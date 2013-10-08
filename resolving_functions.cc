#include <pin.H>

#include "variable.h"
#include "instruction.h"
#include "checkpoint.h"
#include "branch.h"
#include "stuffs.h"

extern std::map< ADDRINT, 
                 instruction >                    addr_ins_static_map;

extern bool                                       in_tainting;

extern vdep_graph                                 dta_graph;

extern std::vector<ADDRINT>                       explored_trace;

// extern ptr_checkpoint                             active_ptr_checkpoint;
extern ptr_checkpoint                             master_ptr_checkpoint;
extern std::vector<ptr_checkpoint>                saved_ptr_checkpoints;

extern std::map< UINT32, 
                 std::vector<ptr_checkpoint> >    exepoint_checkpoints_map;

extern UINT32                                     total_rollback_times;

extern ADDRINT                                    received_msg_addr;
extern UINT32                                     received_msg_size;

extern std::vector<ptr_branch>                    input_dep_ptr_branches;
extern std::vector<ptr_branch>                    input_indep_ptr_branches;
extern std::vector<ptr_branch>                    tainted_ptr_branches;
extern std::vector<ptr_branch>                    found_new_ptr_branches;
extern std::vector<ptr_branch>                    resolved_ptr_branches;

extern ptr_branch                                 active_ptr_branch;
extern ptr_branch                                 exploring_ptr_branch; 

extern UINT32                                     input_dep_branch_num;
extern UINT32                                     resolved_branch_num;

extern KNOB<UINT32>                               max_total_rollback;
extern KNOB<UINT32>                               max_local_rollback;
extern KNOB<UINT32>                               max_trace_length;
extern KNOB<BOOL>                                 print_debug_text;

/*====================================================================================================================*/

VOID resolving_ins_count_analyzer(ADDRINT ins_addr)
{
  explored_trace.push_back(ins_addr);
  
  return;
}

/*====================================================================================================================*/

VOID resolving_mem_to_st_analyzer(ADDRINT ins_addr, ADDRINT mem_read_addr, UINT32 mem_read_size) // memory read
{
  // copy memory read values
//   copy_instruction_mem_access(ins_addr, mem_read_addr, mem_read_size, 0);
  
  return;
}

/*====================================================================================================================*/

VOID resolving_st_to_mem_analyzer(ADDRINT ins_addr, ADDRINT mem_written_addr, UINT32 mem_written_size) // memory written
{ 
  // copy memory written values
//   copy_instruction_mem_access(ins_addr, mem_written_addr, mem_written_size, 1);
  
  if (active_ptr_branch) // in resolving
  {
    active_ptr_branch->chkpnt->mem_written_logging(ins_addr, mem_written_addr, mem_written_size);
  }
  else // in forwarding
  { 
    std::vector<ptr_checkpoint>::iterator ptr_chkpt_iter = exepoint_checkpoints_map[explored_trace.size()].begin();
    for (; ptr_chkpt_iter != exepoint_checkpoints_map[explored_trace.size()].end(); ++ptr_chkpt_iter) 
    {
      (*ptr_chkpt_iter)->mem_written_logging(ins_addr, mem_written_addr, mem_written_size);
    }
    
    master_ptr_checkpoint->mem_written_logging(ins_addr, mem_written_addr, mem_written_size);
  }
  
  return;
}

/*====================================================================================================================*/

inline void prepare_new_tainting_phase(ptr_branch& unexplored_ptr_branch)
{        
  in_tainting = true;
  exploring_ptr_branch = unexplored_ptr_branch;
          
  vdep_graph().swap(dta_graph);
  std::vector<ptr_branch>().swap(input_dep_ptr_branches);
  std::vector<ptr_branch>().swap(input_indep_ptr_branches);
  std::vector<ptr_checkpoint>().swap(saved_ptr_checkpoints);
  std::map< UINT32, std::vector<ptr_checkpoint> >().swap(exepoint_checkpoints_map);
  
  unexplored_ptr_branch->is_explored = true;
  
  if (active_ptr_branch) 
  {
    active_ptr_branch.reset();
  }
  
  return;
}

/*====================================================================================================================*/

inline std::vector<ptr_branch>::iterator search_in(std::vector<ptr_branch>& ptr_branches, ADDRINT ins_addr)
{
  std::vector<ptr_branch>::iterator ptr_branch_iter = ptr_branches.begin();
  for (; ptr_branch_iter != ptr_branches.end(); ++ptr_branch_iter) 
  {
    if (((*ptr_branch_iter)->trace.size() == explored_trace.size()) && ((*ptr_branch_iter)->addr == ins_addr)) 
    {
      break;
    }
  }
  
  return ptr_branch_iter;
}

/*====================================================================================================================*/

inline std::vector<ptr_branch>::iterator first_unexplored_branch() 
{
  std::vector<ptr_branch>::iterator unexplored_ptr_branch_iter = resolved_ptr_branches.begin();
  for (; unexplored_ptr_branch_iter != resolved_ptr_branches.end(); ++unexplored_ptr_branch_iter)
  {
    if (!(*unexplored_ptr_branch_iter)->is_explored) 
    {
      break;
    }
  }
  return unexplored_ptr_branch_iter;
}

/*====================================================================================================================*/

inline void error_lost_in_forwarding(ADDRINT ins_addr, ptr_branch& err_ptr_branch) 
{
  journal_buffer("failed_rollback_msg", reinterpret_cast<UINT8*>(received_msg_addr), received_msg_size);
  print_debug_lost_forward(ins_addr, err_ptr_branch);
  
  return;
}

/*====================================================================================================================*/

inline void accept_branch(ptr_branch& accepted_ptr_branch) 
{
  accepted_ptr_branch->is_resolved      = true;
  accepted_ptr_branch->is_just_resolved = true;
  accepted_ptr_branch->is_bypassed      = false;
  
  resolved_ptr_branches.push_back(accepted_ptr_branch);
  return;
}

/*====================================================================================================================*/

inline void bypass_branch(ptr_branch& bypassed_ptr_branch) 
{
  bypassed_ptr_branch->is_resolved      = true;
  bypassed_ptr_branch->is_just_resolved = true;
  bypassed_ptr_branch->is_bypassed      = true;
  return;
}

/*====================================================================================================================*/

inline void disable_active_branch() 
{
  active_ptr_branch.reset();
  return;
}

/*====================================================================================================================*/

inline void enable_active_branch(ptr_branch& new_branch) 
{
  active_ptr_branch = new_branch;
  return;
}

/*====================================================================================================================*/

inline void exploring_new_branch_or_stop() 
{
  std::vector<ptr_branch>::iterator unexplored_ptr_branch_iter = first_unexplored_branch();
  if (unexplored_ptr_branch_iter != resolved_ptr_branches.end())   // unexplored branch found
  {
    print_debug_rollbacking_stop(*unexplored_ptr_branch_iter);

    // rollback to the first unexplored branch
    prepare_new_tainting_phase(*unexplored_ptr_branch_iter);
    
    total_rollback_times++;
    PIN_RemoveInstrumentation();
    bool new_br_taken = !exploring_ptr_branch->br_taken;
    rollback_with_input_replacement(master_ptr_checkpoint, exploring_ptr_branch->inputs[new_br_taken][0].get());
  }
  else // all branches are explored
  {
    PIN_ExitApplication(0);
  }
  
  return;
}

/*====================================================================================================================*/

inline void process_tainted_and_resolved_branch(ADDRINT ins_addr, bool br_taken, ptr_branch& tainted_ptr_branch) 
{
  if (tainted_ptr_branch->br_taken != br_taken) // new branch taken
  { 
    // the branch has been marked as "bypassed" before, then is resolved accidentally
    if (tainted_ptr_branch->is_bypassed) 
    {
//       print_debug_met_again(ins_addr, tainted_ptr_branch);
      accept_branch(tainted_ptr_branch);
    }
    
    // we will lost out of the original trace if go further, so we must rollback
    total_rollback_times++;
    rollback_with_input_random_modification(active_ptr_branch->chkpnt, active_ptr_branch->dep_mems);
  }
  
  return;
}

/*====================================================================================================================*/

inline void new_branch_taken_processing(ADDRINT ins_addr, bool br_taken, ptr_branch& tainted_ptr_branch) 
{
  ptr_branch tmp_ptr_branch;
  
  if (active_ptr_branch) // so this branch is resolved
  {
//     print_debug_succeed(ins_addr, tainted_ptr_branch);
    
    accept_branch(active_ptr_branch);
    
    // this branch is resolved, now restore the input to take a clean rollback
    tmp_ptr_branch = active_ptr_branch;
    disable_active_branch();
    total_rollback_times++;
    
    bool current_br_taken = tmp_ptr_branch->br_taken;
    rollback_with_input_replacement(tmp_ptr_branch->chkpnt, tmp_ptr_branch->inputs[current_br_taken][0].get());
  }
  else // error: in forward but new branch taken found 
  {
    error_lost_in_forwarding(ins_addr, tainted_ptr_branch);
    PIN_ExitApplication(0);
  }  
  
  return;
}

/*====================================================================================================================*/

inline void same_branch_taken_processing(ADDRINT ins_addr, bool br_taken, ptr_branch& tainted_ptr_branch) 
{
  bool current_br_taken;
  
  if (active_ptr_branch) // in some rollback
  {
    if (active_ptr_branch != tainted_ptr_branch) // error: 
    {
      print_debug_failed_active_forward(ins_addr, tainted_ptr_branch);
      PIN_ExitApplication(0);
    }
  }
  else // in forward and meet a branch to resolve, so enable active_ptr_branch
  {
    enable_active_branch(tainted_ptr_branch);
//     print_debug_resolving_rollback(ins_addr, tainted_ptr_branch);
  }
  
  if (active_ptr_branch->chkpnt->rb_times <= max_local_rollback.Value())
  {
    // this branch is not resolved yet, now modify the input and rollback again
    total_rollback_times++;
    rollback_with_input_random_modification(active_ptr_branch->chkpnt, active_ptr_branch->dep_mems);
  }
  else // the rollback number bypasses the maximum value
  {
    print_debug_resolving_failed(ins_addr, tainted_ptr_branch);
    
    bypass_branch(active_ptr_branch);
    
    ptr_branch tmp_ptr_branch = active_ptr_branch;
    disable_active_branch();
    
    total_rollback_times++;
    current_br_taken = tmp_ptr_branch->br_taken;
    rollback_with_input_replacement(tmp_ptr_branch->chkpnt, tmp_ptr_branch->inputs[current_br_taken][0].get());          
  }
  
  return;
}

/*====================================================================================================================*/

inline void process_tainted_but_unresolved_branch(ADDRINT ins_addr, bool br_taken, ptr_branch& tainted_ptr_branch) 
{
  if (tainted_ptr_branch->br_taken != br_taken) // other branch is taken, 
  {
    new_branch_taken_processing(ins_addr, br_taken, tainted_ptr_branch);
  }
  else // other branch is not taken yet, take a rollback to try to resolve this branch
  {
    same_branch_taken_processing(ins_addr, br_taken, tainted_ptr_branch);
  }
  
  return;
}

/*====================================================================================================================*/

inline void process_tainted_branch(ADDRINT ins_addr, bool br_taken, ptr_branch& tainted_ptr_branch) 
{
  if (total_rollback_times > max_total_rollback.Value())
  {
    PIN_ExitApplication(0);
  }
  
  if (tainted_ptr_branch->is_resolved) // which is resolved 
  {
    if (tainted_ptr_branch == input_dep_ptr_branches.back()) // and is the current last branch
    {
      exploring_new_branch_or_stop();
    }
    else // it is not the last branch
    {
      process_tainted_and_resolved_branch(ins_addr, br_taken, tainted_ptr_branch);
    }
  }
  else // it is not resolved yet
  {
    process_tainted_but_unresolved_branch(ins_addr, br_taken, tainted_ptr_branch);
  }
  
  return;
}

/*====================================================================================================================*/

inline void process_untainted_branch(ADDRINT ins_addr, bool br_taken, ptr_branch& untainted_ptr_branch) 
{
  if (untainted_ptr_branch->br_taken != br_taken) // new taken found
  {
    if (active_ptr_branch) 
    {
      if (!untainted_ptr_branch->is_resolved) 
      {
//         print_debug_found_new(ins_addr, untainted_ptr_branch);
        accept_branch(untainted_ptr_branch);
        found_new_ptr_branches.push_back(untainted_ptr_branch);
      }
      
      // the original trace will lost if go further, so rollback
      total_rollback_times++;
      rollback_with_input_random_modification(active_ptr_branch->chkpnt, active_ptr_branch->dep_mems);
    }
    else // error: active_ptr_branch is empty, namely in forwarding, but new taken found
    {
      print_debug_lost_forward(ins_addr, untainted_ptr_branch);
      PIN_ExitApplication(0);
    }
  }
//   else 
//   {
//     if (untainted_ptr_branch->is_resolved) 
//     {
//       store_input(untainted_ptr_branch, br_taken);
//     }
//   }
  
  return;
}

/*====================================================================================================================*/

inline void log_input(ADDRINT ins_addr, bool br_taken)
{
  std::vector<ptr_branch>::iterator ptr_branch_iter = search_in(tainted_ptr_branches, ins_addr);
  if (ptr_branch_iter != tainted_ptr_branches.end()) 
  {
    store_input(*ptr_branch_iter, br_taken);
  }
  else 
  {
//     print_debug_unknown_branch(ins_addr, *ptr_branch_iter);
    PIN_ExitApplication(0);
  }
  
  return;
}

/*====================================================================================================================*/

VOID resolving_cond_branch_analyzer(ADDRINT ins_addr, bool br_taken)
{ 
  log_input(ins_addr, br_taken);
  
  // search in the list of tainted branches
  std::vector<ptr_branch>::iterator ptr_branch_iter = search_in(input_dep_ptr_branches, ins_addr);
  if (ptr_branch_iter != input_dep_ptr_branches.end()) // found a tainted branch
  {
    process_tainted_branch(ins_addr, br_taken, *ptr_branch_iter);
  }
  else // not found in the list of tainted branches
  {
    // search in the list of untainted branches
    ptr_branch_iter = search_in(input_indep_ptr_branches, ins_addr);
    if (ptr_branch_iter != input_indep_ptr_branches.end()) // a untainted branch found
    {
      process_untainted_branch(ins_addr, br_taken, *ptr_branch_iter);
    }
    else // error: the branch is not tainted neither untainted (normally by indirect jumps)
    {
      print_debug_unknown_branch(ins_addr, *ptr_branch_iter);
      PIN_ExitApplication(0);
    }
  }

  return;
}
