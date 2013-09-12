#include <pin.H>

#include <iostream>
#include <fstream>
#include <boost/format.hpp>

#include "stuffs.h"
#include "instruction.h"
#include "checkpoint.h"
#include "branch.h"

/*====================================================================================================================*/

extern bool                                     in_tainting;

extern std::map< ADDRINT, 
                 instruction >                  addr_ins_static_map;

extern UINT32                                   total_rollback_times;

extern std::vector<ADDRINT>                     explored_trace;

extern std::vector<ptr_branch>                  input_dep_ptr_branches;
extern std::vector<ptr_branch>                  input_indep_ptr_branches;
extern std::vector<ptr_branch>                  tainted_ptr_branches;

extern ptr_branch                               exploring_ptr_branch; 

extern UINT32                                   input_dep_branch_num;

extern std::vector<ptr_checkpoint>              saved_ptr_checkpoints;
extern ptr_checkpoint                           master_ptr_checkpoint;

extern std::map< UINT32, 
                 std::vector<ptr_checkpoint> >  exepoint_checkpoints_map;

extern ADDRINT                                  received_msg_addr;
extern UINT32                                   received_msg_size;

extern std::ofstream trace_file;

extern KNOB<BOOL>                               print_debug_text;
extern KNOB<UINT32>                             max_trace_length;

/*====================================================================================================================*/

inline void omit_branch(ptr_branch& omitted_ptr_branch) 
{
  omitted_ptr_branch->is_resolved = true;
  omitted_ptr_branch->is_bypassed = false;
  omitted_ptr_branch->is_just_resolved = false;
  
  return;
}

/*====================================================================================================================*/

void prepare_new_rollbacking_phase() 
{
  print_debug_start_rollbacking();
  
  in_tainting = false;
  PIN_RemoveInstrumentation();
  
  if (exploring_ptr_branch) 
  {
    rollback_with_input_replacement(master_ptr_checkpoint, 
                                    exploring_ptr_branch->inputs[!exploring_ptr_branch->br_taken][0].get());
  }
  else 
  {
    if (!input_dep_ptr_branches.empty()) 
    {
      rollback_with_input_replacement(master_ptr_checkpoint, 
                                      input_dep_ptr_branches[0]->inputs[input_dep_ptr_branches[0]->br_taken][0].get());
    }
    else 
    {
      PIN_ExitApplication(0);
    }
  }
  
  return;
}

/*====================================================================================================================*/

VOID logging_ins_count_analyzer(ADDRINT ins_addr)
{  
  ptr_branch tmp_ptr_branch;
  
  if (explored_trace.size() < max_trace_length.Value())
  {
    explored_trace.push_back(ins_addr);
        
    if (print_debug_text)
    {
      trace_file << boost::format("%-5i %-20s %-35s (R: %-i, W: %-i)\n") 
                    % explored_trace.size() % StringFromAddrint(ins_addr) % addr_ins_static_map[ins_addr].disass 
                    % addr_ins_static_map[ins_addr].mem_read_size % addr_ins_static_map[ins_addr].mem_written_size;
    }
  }
  else // trace length limit reached
  {
    prepare_new_rollbacking_phase();
  }
  
  return;
}

/*====================================================================================================================*/
// memmory read
VOID logging_mem_to_st_analyzer(ADDRINT ins_addr, ADDRINT mem_read_addr, UINT32 mem_read_size, CONTEXT* p_ctxt) 
{
  if (
      std::max(mem_read_addr, received_msg_addr) < 
      std::min(mem_read_addr + mem_read_size, received_msg_addr + received_msg_size)
     )
  {
    ptr_checkpoint new_ptr_checkpoint(new checkpoint(ins_addr, p_ctxt, explored_trace, mem_read_addr, mem_read_size));
    saved_ptr_checkpoints.push_back(new_ptr_checkpoint);
    
    if (!master_ptr_checkpoint) 
    {
      master_ptr_checkpoint = saved_ptr_checkpoints[0];
    }
   
    print_debug_new_checkpoint(ins_addr);
  }
  
  return;
}

/*====================================================================================================================*/
// memory written
VOID logging_st_to_mem_analyzer(ADDRINT ins_addr, ADDRINT mem_written_addr, UINT32 mem_written_size) 
{  
  if (master_ptr_checkpoint) 
  {
    master_ptr_checkpoint->mem_written_logging(ins_addr, mem_written_addr, mem_written_size);
  }
  
  exepoint_checkpoints_map[explored_trace.size()] = saved_ptr_checkpoints;
  
  return;
}

/*====================================================================================================================*/

VOID logging_cond_br_analyzer(ADDRINT ins_addr, bool br_taken)
{ 
  ptr_branch new_ptr_branch(new branch(ins_addr, br_taken));
  
  // save the first input
  store_input(new_ptr_branch, br_taken);
  
  // verify if the branch is a new tainted branch
  if (exploring_ptr_branch) 
  {
    if (new_ptr_branch->trace.size() <= exploring_ptr_branch->trace.size()) // it is not
    {
      omit_branch(new_ptr_branch); // then omit it
    }
    else 
    {
      tainted_ptr_branches.push_back(new_ptr_branch);
    }
  }
  else // it is always a new tainted branch in the first tainting phase
  {
    tainted_ptr_branches.push_back(new_ptr_branch);
  }
  
  if (!new_ptr_branch->dep_mems.empty()) // input dependent branch
  {
    input_dep_ptr_branches.push_back(new_ptr_branch);    
    print_debug_new_branch(ins_addr, new_ptr_branch);
  }
  else // input independent branch
  {
    input_indep_ptr_branches.push_back(new_ptr_branch);
  }

  return;
}
