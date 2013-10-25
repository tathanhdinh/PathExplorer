#include <pin.H>

#include <fstream>
#include <vector>
#include <map>
#include <set>

#include <boost/format.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "stuffs.h"
#include "branch.h"
#include "instruction.h"
#include "analysis_functions.h"
#include "tainting_functions.h"
#include "resolving_functions.h"
#include "logging_functions.h"

/*====================================================================================================================*/

extern std::map< ADDRINT, 
                 instruction >                      addr_ins_static_map;
                 
extern std::vector<ADDRINT>                         explored_trace;

extern bool                                         in_tainting;

extern map_ins_io                                   dta_inss_io;

extern UINT8                                        received_msg_num;

extern std::ofstream                                tainting_log_file;

extern boost::shared_ptr<boost::posix_time::ptime>  start_ptr_time;
extern boost::shared_ptr<boost::posix_time::ptime>  stop_ptr_time;

/*====================================================================================================================*/

inline VOID extract_ins_operands(INS ins, ADDRINT ins_addr) 
{
  std::set<UINT32>  src_oprs, dst_oprs;
  std::set<REG>     src_regs, dst_regs;
  std::set<ADDRINT> src_mems, dst_mems;
  std::set<UINT32>  src_imms, dst_imms;
  
  UINT32 reg_id;
  REG    reg;
  
  UINT32 max_num_rregs = INS_MaxNumRRegs(ins);
  for (reg_id = 0; reg_id < max_num_rregs; ++reg_id) 
  {
    reg = INS_RegR(ins, reg_id);
    if (reg != REG_INST_PTR)
    {
      src_regs.insert(reg);
    }
  }
  
  UINT32 max_num_wregs = INS_MaxNumWRegs(ins);
  for (reg_id = 0; reg_id < max_num_wregs; ++reg_id) 
  {
    reg = INS_RegW(ins, reg_id);
    if ((reg != REG_INST_PTR) || INS_IsBranchOrCall(ins) || INS_IsRet(ins))
    {
      dst_regs.insert(reg);
    }
  }
  
  bool is_mov_or_lea = INS_IsMov(ins) || INS_IsLea(ins);
  
  dta_inss_io[ins_addr] = boost::make_tuple(std::make_pair(src_regs, dst_regs), 
                                            std::make_pair(src_imms, dst_imms), 
                                            std::make_pair(src_mems, dst_mems), 
                                            is_mov_or_lea);
  return;
}

/*====================================================================================================================*/

VOID ins_instrumenter(INS ins, VOID *data)
{
  // logging the parsed instructions statically
  ADDRINT ins_addr = INS_Address(ins);
  addr_ins_static_map[ins_addr] = instruction(ins);
//   assign_image_name(ins_addr);
  assign_image_name(ins_addr, addr_ins_static_map[ins_addr].img);
  
  if (
      false 
//       || INS_IsCall(ins) 
//       || INS_IsSyscall(ins) 
//       || INS_IsSysret(ins) 
//       || INS_IsNop(ins)
     )
  {
    // omit these instructions
  }
  else 
  {
    if (received_msg_num == 1) 
    {
      if (!start_ptr_time)
      {
        start_ptr_time.reset(new boost::posix_time::ptime(boost::posix_time::microsec_clock::local_time()));
      }
      
//       if (rollback_times == 0) // branch exploring
      if (in_tainting)
      {
        if (INS_IsSyscall(ins)) 
        {
          INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)logging_ins_syscall, 
                                   IARG_INST_PTR, IARG_END);
        }
        else 
        {
          /* 
           * memory read/write tainting and logging
           * =======================================*/
          extract_ins_operands(ins, ins_addr);
          
          INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)logging_ins_count_analyzer, 
                                  IARG_INST_PTR, 
                                  IARG_END);
          
          if (INS_IsMemoryRead(ins))
          {
            // memory read tainting
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)tainting_mem_to_st_analyzer, 
                                    IARG_INST_PTR, 
                                    IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE,
                                    IARG_END);
            
            // memory read logging
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)logging_mem_to_st_analyzer,
                                    IARG_INST_PTR,
                                    IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, 
                                    IARG_CONTEXT,
                                    IARG_END);
          }
          else 
          {
            if (INS_IsMemoryWrite(ins))
            {
              // memory written tainting
              INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)tainting_st_to_mem_analyzer, 
                                      IARG_INST_PTR, 
                                      IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE,
                                      IARG_END);
              
              // memory written logging
              INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)logging_st_to_mem_analyzer, 
                                      IARG_INST_PTR,
                                      IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE, 
                                      IARG_END);
            }
            else 
            {
              // register read/written tainting
              INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)tainting_st_to_st_analyzer, 
                                       IARG_INST_PTR, 
                                       IARG_END);
              
              if (addr_ins_static_map[ins_addr].category != XED_CATEGORY_COND_BR) 
              {
                INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)logging_st_to_st_analyzer, 
                                        IARG_INST_PTR, 
                                        IARG_END);
              }
            }
          }
          
          if (addr_ins_static_map[ins_addr].category == XED_CATEGORY_COND_BR)
          {
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)logging_cond_br_analyzer, 
                                    IARG_INST_PTR, 
                                    IARG_BRANCH_TAKEN,
                                    IARG_END);
          }
        }
      }
      else // in rollbacking
      {
        /* branch resolving
         * ================= */
        
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)resolving_ins_count_analyzer, 
                                 IARG_INST_PTR, 
                                 IARG_END);
        
        if (INS_IsMemoryRead(ins))
        {
//           INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)resolving_mem_to_st_analyzer, 
//                                    IARG_INST_PTR, 
//                                    IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE,
//                                    IARG_END);
        }
        else 
        {
          if (INS_IsMemoryWrite(ins))
          {
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)resolving_st_to_mem_analyzer, 
                                     IARG_INST_PTR, 
                                     IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE,
                                     IARG_END);
          }            
        }
        
        if (addr_ins_static_map[ins_addr].category == XED_CATEGORY_COND_BR)
        {
          INS_InsertPredicatedCall(ins, IPOINT_BEFORE, 
                                   (AFUNPTR)resolving_cond_branch_analyzer, 
                                   IARG_INST_PTR, 
                                   IARG_BRANCH_TAKEN,
                                   IARG_END);
        }
      }
    }
  }
  
  return;
}
