#include <pin.H>

#include <vector>
#include <map>

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

extern bool                                         in_tainting;

extern map_ins_io                                   dta_inss_io;

extern UINT8                                        received_msg_num;

extern boost::shared_ptr<boost::posix_time::ptime>  start_ptr_time;
extern boost::shared_ptr<boost::posix_time::ptime>  stop_ptr_time;

/*====================================================================================================================*/

VOID extract_ins_operands(INS ins) 
{
  std::vector<UINT32>   src_oprs;
  std::vector<UINT32>   dst_oprs;
  
  std::vector<REG>      src_regs;
  std::vector<REG>      dst_regs;
    
  std::vector<ADDRINT>  src_mems;
  std::vector<ADDRINT>  dst_mems;
    
  std::vector<UINT32>   src_imms;
  std::vector<UINT32>   dst_imms;
  
  UINT32 opr_num = INS_OperandCount(ins);
  for (UINT32 opr_id = 0; opr_id < opr_num; ++opr_id)
  {
    if (
        false && 
        INS_OperandIsReg(ins, opr_id) && 
        ((INS_OperandReg(ins, opr_id) == REG_IP) || (INS_OperandReg(ins, opr_id) == REG_EIP))
       )
    {
      // NOTE: never fall into here into because of "false" in the conditional predicate
    }
    else 
    {
      if (INS_OperandRead(ins, opr_id))
      {
        src_oprs.push_back(opr_id);
      }
    
      if (INS_OperandWritten(ins, opr_id))
      {
        dst_oprs.push_back(opr_id);
      }
    } 
  }
  
//   std::cerr << INS_Disassemble(ins) << " Source: " << src_oprs.size() << ", " << "Destination: " << dst_oprs.size() << "\n"; 
  
  for (std::vector<UINT32>::iterator src_iter = src_oprs.begin(); src_iter != src_oprs.end(); ++src_iter)
  {
    if (INS_OperandIsReg(ins, *src_iter))
    {
      src_regs.push_back(INS_OperandReg(ins, *src_iter));
    }
    
    if (INS_OperandIsMemory(ins, *src_iter))
    {
      src_mems.push_back(*src_iter);

      REG base_reg = INS_OperandMemoryBaseReg(ins, *src_iter);
      if (base_reg != REG_INVALID())
      {
        src_regs.push_back(base_reg);
      }
      
      REG idx_reg = INS_OperandMemoryIndexReg(ins, *src_iter);
      if (idx_reg != REG_INVALID())
      {
        src_regs.push_back(base_reg);
      }
      
      REG seg_reg = INS_OperandMemorySegmentReg(ins, *src_iter);
      if (seg_reg != REG_INVALID())
      {
        src_regs.push_back(seg_reg);
      }
    }
    
    if (INS_OperandIsImmediate(ins, *src_iter))
    {
      src_imms.push_back(static_cast<UINT32>(INS_OperandImmediate(ins, *src_iter)));
    }
  }
    
  for (std::vector<UINT32>::iterator dst_iter = dst_oprs.begin(); dst_iter != dst_oprs.end(); ++dst_iter)
  {
    if (INS_OperandIsReg(ins, *dst_iter))
    {
      dst_regs.push_back(INS_OperandReg(ins, *dst_iter));
    }
    
    if (INS_OperandIsMemory(ins, *dst_iter))
    {
      dst_mems.push_back(*dst_iter);
      
      REG base_reg = INS_OperandMemoryBaseReg(ins, *dst_iter);
      if (base_reg != REG_INVALID())
      {
        src_regs.push_back(base_reg);
      }
      
      REG idx_reg = INS_OperandMemoryIndexReg(ins, *dst_iter);
      if (idx_reg != REG_INVALID())
      {
        src_regs.push_back(base_reg);
      }
      
      REG seg_reg = INS_OperandMemorySegmentReg(ins, *dst_iter);
      if (seg_reg != REG_INVALID())
      {
        src_regs.push_back(seg_reg);
      }
    }
    
    if (INS_OperandIsImmediate(ins, *dst_iter))
    {
      dst_imms.push_back(static_cast<UINT32>(INS_OperandImmediate(ins, *dst_iter)));
    }
  }
  
  dta_inss_io[INS_Address(ins)] = boost::make_tuple(std::make_pair(src_regs, dst_regs), 
                                                    std::make_pair(src_imms, dst_imms), 
                                                    std::make_pair(src_mems, dst_mems));
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
          
          INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)logging_ins_count_analyzer, 
                                  IARG_INST_PTR, 
                                  IARG_END);
          
          extract_ins_operands(ins);
          
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
