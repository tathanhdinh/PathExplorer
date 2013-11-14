#include <pin.H>

#include <boost/format.hpp>
#include <boost/log/trivial.hpp>

#include <algorithm>

#include "variable.h"
#include "instruction.h"
#include "checkpoint.h"
#include "branch.h"
#include "stuffs.h"

extern std::map< ADDRINT,
                 instruction >                    addr_ins_static_map;
extern std::map< UINT32, 
                 instruction >                    order_ins_dynamic_map;

extern bool                                       in_tainting;

extern vdep_graph                                 dta_graph;

extern std::vector<ADDRINT>                       explored_trace;

extern ptr_checkpoint                             active_ptr_checkpoint;
extern ptr_checkpoint                             master_ptr_checkpoint;
extern std::vector<ptr_checkpoint>                saved_ptr_checkpoints;

extern std::pair< ptr_checkpoint, 
                  std::set<ADDRINT> >             active_nearest_checkpoint;

extern std::map< UINT32,
       std::vector<ptr_checkpoint> >              exepoint_checkpoints_map;

extern UINT32                                     total_rollback_times;
extern UINT32                                     local_rollback_times;
extern UINT32                                     max_total_rollback_times;
extern UINT32                                     max_local_rollback_times;

extern ADDRINT                                    received_msg_addr;
extern UINT32                                     received_msg_size;

extern std::map<UINT32, ptr_branch>               order_input_dep_ptr_branch_map;
extern std::map<UINT32, ptr_branch>               order_input_indep_ptr_branch_map;
extern std::map<UINT32, ptr_branch>               order_tainted_ptr_branch_map;

extern std::vector<ptr_branch>                    found_new_ptr_branches;
extern std::vector<ptr_branch>                    resolved_ptr_branches;

extern ptr_branch                                 active_ptr_branch;
extern ptr_branch                                 exploring_ptr_branch;

extern std::set<ADDRINT>                          active_input_dep_addrs;

extern UINT32                                     input_dep_branch_num;
extern UINT32                                     resolved_branch_num;

extern KNOB<UINT32>                               max_total_rollback;
extern KNOB<UINT32>                               max_local_rollback;
extern KNOB<UINT32>                               max_trace_length;
extern KNOB<BOOL>                                 print_debug_text;

/*====================================================================================================================*/

inline void print_debug_unknown_branch(ADDRINT ins_addr/*, ptr_branch& unknown_ptr_branch*/)
{
  if (print_debug_text) 
  {
    std::cerr << boost::format("\033[31mLost at     %-5i %-20s %-35s (unknown branch met)\033[0m\n")
                  % explored_trace.size() % remove_leading_zeros(StringFromAddrint(ins_addr))
                  % addr_ins_static_map[ins_addr].disass;
  }
  return;
}

/*====================================================================================================================*/

inline void print_debug_lost_forward(ADDRINT ins_addr/*, ptr_branch& lost_ptr_branch*/)
{
  if (print_debug_text) 
  {
    std::cerr << boost::format("\033[31mBranch at   %-5i %-20s %-35s (lost: new branch taken in forward)\033[0m\n")
//     boost::format("\033[31mBranch  %-1i   %-5i %-20s %-35s (lost: new branch taken in forward)\033[0m\n")
                  % /*!lost_ptr_branch->br_taken %*/ explored_trace.size()
                  % remove_leading_zeros(StringFromAddrint(ins_addr))
                  % addr_ins_static_map[ins_addr].disass;
  }
  return;
}

/*====================================================================================================================*/

inline void print_debug_failed_active_forward (ADDRINT ins_addr, ptr_branch& failed_ptr_branch)
{
  if (print_debug_text) 
  {
    std::cerr << boost::format("\033[31mFailed      %-5i %-20s %-35s (active branch not met in forward)\033[0m\n")
                  % explored_trace.size() % remove_leading_zeros(StringFromAddrint (ins_addr))
                  % addr_ins_static_map[ins_addr].disass;
  }
  return;
}

/*====================================================================================================================*/

inline void print_debug_met_again(ADDRINT ins_addr, ptr_branch &met_ptr_branch)
{
  if (print_debug_text) 
  {
    std::cerr << boost::format ("\033[36mMet again   %-5i %-20s %-35s (%-10i rollbacks)\033[0m\n")
                  % explored_trace.size() % remove_leading_zeros(StringFromAddrint(ins_addr)) 
                  % addr_ins_static_map[ins_addr].disass % met_ptr_branch->checkpoint->rollback_times;
  }
  return;
}

/*====================================================================================================================*/



/*====================================================================================================================*/

inline void print_debug_resolving_failed(ADDRINT ins_addr, ptr_branch& failed_ptr_branch)
{
  if (print_debug_text) 
  {
    std::cout << boost::format("\033[31mFailed      %-5i %-20s %-35s (%i rollbacks)\033[0m\n")
                  % failed_ptr_branch->trace.size() /*explored_trace.size()*/
                  % remove_leading_zeros(StringFromAddrint(ins_addr)) % addr_ins_static_map[ins_addr].disass 
                  % local_rollback_times;
//                   % failed_ptr_branch->checkpoint->rollback_times;
  }
  return;
}

/*====================================================================================================================*/

inline void print_debug_succeed(ADDRINT ins_addr, ptr_branch& succeed_ptr_branch)
{
  if (print_debug_text) 
  {
    std::cout << boost::format ("\033[32mSucceeded   %-5i %-20s %-35s (%i rollbacks)\033[0m\n")
                  % explored_trace.size() % remove_leading_zeros(StringFromAddrint(ins_addr))
                  % addr_ins_static_map[ins_addr].disass
                  % local_rollback_times;
//                   % succeed_ptr_branch->checkpoint->rollback_times;
  }
  return;
}

/*====================================================================================================================*/

inline void print_debug_resolving_rollback(ADDRINT ins_addr, ptr_branch& new_ptr_branch)
{
  if (print_debug_text) 
  {
    std::cerr << boost::format("Resolving   %-5i %-20s %-35s (rollback to %i)\n")
                  % explored_trace.size() % remove_leading_zeros(StringFromAddrint(ins_addr))
                  % addr_ins_static_map[ins_addr].disass
                  % active_ptr_branch->checkpoint->trace.size();
  }
  return;
}

/*====================================================================================================================*/

inline void print_debug_found_new(ADDRINT ins_addr, ptr_branch& found_ptr_branch)
{
  if (print_debug_text) 
  {
    std::cout << boost::format("\033[35mFound new   %-5i %-20s %-35s\033[0m\n")
                  % explored_trace.size() % remove_leading_zeros(StringFromAddrint(ins_addr)) 
                  % addr_ins_static_map[ins_addr].disass;
  }
  return;
}

/*====================================================================================================================*/

VOID resolving_ins_count_analyzer(ADDRINT ins_addr)
{
  explored_trace.push_back(ins_addr);
  BOOST_LOG_TRIVIAL(trace) << explored_trace.size() << "  " 
                           << remove_leading_zeros(StringFromAddrint(ins_addr)) << " "
                           << addr_ins_static_map[ins_addr].disass;
   
  return;
}

/*====================================================================================================================*/

VOID resolving_mem_to_st_analyzer(ADDRINT ins_addr, ADDRINT mem_read_addr, UINT32 mem_read_size) // memory read
{
  return;
}

/*====================================================================================================================*/

VOID resolving_st_to_mem_analyzer(ADDRINT ins_addr, ADDRINT mem_written_addr, UINT32 mem_written_size) // memory written
{
  if (active_ptr_branch) // in resolving
  {
    active_ptr_branch->checkpoint->mem_written_logging(ins_addr, mem_written_addr, mem_written_size);
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

//   vdep_graph().swap(dta_graph);
//   std::vector<ptr_branch>().swap(input_dep_ptr_branches);
//   std::vector<ptr_branch>().swap(input_indep_ptr_branches);
//   std::vector<ptr_checkpoint>().swap(saved_ptr_checkpoints);
//   std::map< UINT32, std::vector<ptr_checkpoint> >().swap(exepoint_checkpoints_map);
  
  dta_graph.clear();
  order_input_dep_ptr_branch_map.clear();
  order_input_indep_ptr_branch_map.clear();
  saved_ptr_checkpoints.clear();
  exepoint_checkpoints_map.clear();

  unexplored_ptr_branch->is_explored = true;

  if (active_ptr_branch)
  {
    active_ptr_branch.reset();
  }

  return;
}

/*====================================================================================================================*/

inline ptr_branch first_unexplored_branch()
{
  ptr_branch unexplored_ptr_branch;
  
  std::vector<ptr_branch>::iterator unexplored_ptr_branch_iter = resolved_ptr_branches.begin();
  for (; unexplored_ptr_branch_iter != resolved_ptr_branches.end(); ++unexplored_ptr_branch_iter)
  {
    if (!(*unexplored_ptr_branch_iter)->is_explored)
    {
      unexplored_ptr_branch = *unexplored_ptr_branch_iter;
      break;
    }
  }
  return unexplored_ptr_branch;
}

/*====================================================================================================================*/

inline void error_lost_in_forwarding(ADDRINT ins_addr, ptr_branch& err_ptr_branch)
{
  journal_buffer("failed_rollback_msg", reinterpret_cast<UINT8*>(received_msg_addr), received_msg_size);
  print_debug_lost_forward(ins_addr/*, err_ptr_branch*/);

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

inline void get_next_nearest_checkpoint(ptr_branch& input_branch) 
{
  std::map< ptr_checkpoint, std::set<ADDRINT>, ptr_checkpoint_less >::iterator nearest_checkpoint_iter;
  std::map< ptr_checkpoint, std::set<ADDRINT>, ptr_checkpoint_less >::iterator next_nearest_checkpoint_iter;
  
  if (active_nearest_checkpoint.first) 
  {
    nearest_checkpoint_iter = input_branch->nearest_checkpoints.begin();
    next_nearest_checkpoint_iter = input_branch->nearest_checkpoints.end();
    
    for (; nearest_checkpoint_iter != input_branch->nearest_checkpoints.end(); ++nearest_checkpoint_iter) 
    {
      if ((*nearest_checkpoint_iter).first == active_nearest_checkpoint.first) 
      {
        break;
      }
      else 
      {
        next_nearest_checkpoint_iter = nearest_checkpoint_iter;
      }
    }
    
    if (nearest_checkpoint_iter != input_branch->nearest_checkpoints.end()) 
    {
      if (next_nearest_checkpoint_iter != input_branch->nearest_checkpoints.end()) 
      {
        active_nearest_checkpoint.first = (*next_nearest_checkpoint_iter).first;
        active_nearest_checkpoint.second.insert((*next_nearest_checkpoint_iter).second.begin(), 
                                                (*next_nearest_checkpoint_iter).second.end());
      }
      else 
      {
        active_nearest_checkpoint.first.reset();
        active_nearest_checkpoint.second.clear();
//         std::set<ADDRINT>().swap(active_nearest_checkpoint.second);
      }
    }
    else 
    {
      BOOST_LOG_TRIVIAL(fatal) << "Nearest checkpoint cannot found in the branch's nearest checkpoint list";
      PIN_ExitApplication(0);
    }
  }
  else 
  {
    active_nearest_checkpoint.first = input_branch->nearest_checkpoints.rbegin()->first;
    active_nearest_checkpoint.second = input_branch->nearest_checkpoints.rbegin()->second;
  }
  
  return;
}

/*====================================================================================================================*/

inline void exploring_new_branch_or_stop()
{
  ptr_branch unexplored_ptr_branch = first_unexplored_branch();
  if (unexplored_ptr_branch) 
  {
    BOOST_LOG_TRIVIAL(info) << boost::format("Rollbacking phase stop at %d, %d / %d branches resolved") 
                                % resolved_ptr_branches.size() 
                                % resolved_ptr_branches.size() % order_tainted_ptr_branch_map.size();
                                
    prepare_new_tainting_phase(unexplored_ptr_branch);
    
    total_rollback_times++;
    local_rollback_times++;
    
    PIN_RemoveInstrumentation();
    
    bool new_br_taken = !exploring_ptr_branch->br_taken;
    rollback_with_input_replacement(master_ptr_checkpoint, exploring_ptr_branch->inputs[new_br_taken][0].get());
  }
  else 
  {
    BOOST_LOG_TRIVIAL(info) << "Stop exploring, all branches are explored.";
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
      print_debug_met_again(ins_addr, tainted_ptr_branch);
      accept_branch(tainted_ptr_branch);
    }

    if (local_rollback_times < max_local_rollback_times)
    {
      // we will lost out of the original trace if go further, so we must rollback
      total_rollback_times++;
      local_rollback_times++;
      
      active_ptr_checkpoint = active_ptr_branch->nearest_checkpoints.rbegin()->first;
      rollback_with_input_random_modification(/*active_ptr_branch->checkpoint*/
                                              /*active_ptr_branch->dep_input_addrs*/
                                              active_ptr_checkpoint,
                                              active_ptr_branch->nearest_checkpoints[active_ptr_checkpoint]);
    }
    else
    {
      print_debug_resolving_failed(active_ptr_branch->addr, active_ptr_branch);

      bypass_branch(active_ptr_branch);

      ptr_branch tmp_ptr_branch = active_ptr_branch;
      disable_active_branch();

      total_rollback_times++;
      local_rollback_times = 0;
      
      rollback_with_input_replacement(tmp_ptr_branch->checkpoint, 
                                      tmp_ptr_branch->inputs[tmp_ptr_branch->br_taken][0].get());
    }
  }

  return;
}


/**
 * @brief handle the case where the branch (being re-executed) takes a different target.
 * 
 * @param ins_addr ...
 * @param br_taken ...
 * @param tainted_ptr_branch ...
 * @return void
 */
inline void new_branch_taken_processing(ADDRINT ins_addr, bool br_taken, ptr_branch& tainted_ptr_branch)
{
  ptr_branch tmp_ptr_branch;
  
  if (active_ptr_branch) // in some rollback
  {
  }
  else 
  {
  }

  if (active_ptr_branch) // so this branch is resolved
  {
    print_debug_succeed(ins_addr, tainted_ptr_branch);

    accept_branch(active_ptr_branch);

    // this branch is resolved, now restore the input to take a clean rollback
    tmp_ptr_branch = active_ptr_branch;
    disable_active_branch();
    
    total_rollback_times++;
    local_rollback_times = 0;

    bool current_br_taken = tmp_ptr_branch->br_taken;
    rollback_with_input_replacement(tmp_ptr_branch->checkpoint, tmp_ptr_branch->inputs[current_br_taken][0].get());
  }
  else // error: in forward but new branch taken found
  {
//     error_lost_in_forwarding(ins_addr, tainted_ptr_branch);
    print_debug_lost_forward(ins_addr/*, tainted_ptr_branch*/);
    std::cout << "2\n";
    PIN_ExitApplication(0);
  }

  return;
}


/**
 * @brief handle the case where the branch (being re-executed) takes the same target.
 * 
 * @param ins_addr ...
 * @param br_taken ...
 * @param tainted_ptr_branch ...
 * @return void
 */
inline void same_branch_taken_processing(ADDRINT ins_addr, bool br_taken, ptr_branch& tainted_ptr_branch)
{
  if (active_ptr_branch) // active branch is enabled, namely in some rollback
  {
    // so verify that
    if (active_ptr_branch != tainted_ptr_branch) 
    {
      BOOST_LOG_TRIVIAL(fatal) << "In rollback but the tainted and active branches are not matched";
      PIN_ExitApplication(0);
    }
  }
  else // active_ptr_branch is disabled, namely in some forward and meet a new tainted branch
  {
    // so enable active_ptr_branch
    active_ptr_branch = tainted_ptr_branch;
    get_next_nearest_checkpoint(active_ptr_branch); // the active_nearest_checkpoint is updated 
    
    local_rollback_times = 0;
    
    BOOST_LOG_TRIVIAL(trace) << boost::format("Resolve the branch at %d by rollback to the checkpoint at %d") 
                                  % tainted_ptr_branch->trace.size() % active_nearest_checkpoint.first->trace.size();
  }
  
  // resolve the active_ptr_branch
  total_rollback_times++;
  
  if (local_rollback_times <= max_local_rollback_times) 
  {
    local_rollback_times++;
    
    rollback_with_input_random_modification(active_nearest_checkpoint.first, active_nearest_checkpoint.second);
  }
  else // we have reached the limit number of rollback test for the current active_nearest_checkpoint
  {
    // so we get try to get new active_nearest_checkpoint
    get_next_nearest_checkpoint(active_ptr_branch);
    
    local_rollback_times = 0;
    
    if (active_nearest_checkpoint.first) // found a new active_nearest_checkpoint
    {
      // then rollback to it
      BOOST_LOG_TRIVIAL(trace) << boost::format("Resolve the branch at %d by rollback to the checkpoint at %d") 
                                  % active_ptr_branch->trace.size() % active_nearest_checkpoint.first->trace.size();
                                  
      rollback_with_input_random_modification(active_nearest_checkpoint.first, active_nearest_checkpoint.second);
    }
    else // cannot found a new active_nearest_checkpoint
    {
      // so bypass this branch
      BOOST_LOG_TRIVIAL(trace) << boost::format("Cannot resolve the branch at %d") % active_ptr_branch->trace.size();
      
      bypass_branch(active_ptr_branch);
      
      ptr_branch tmp_ptr_branch = active_ptr_branch;
      active_ptr_branch.reset();
      
      rollback_with_input_replacement(tmp_ptr_branch->checkpoint, 
                                      tmp_ptr_branch->inputs[tmp_ptr_branch->br_taken][0].get());
    }
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
  if (total_rollback_times >= max_total_rollback_times)
  {
    BOOST_LOG_TRIVIAL(info) << "Stop exploring, the total rollback number exceeds its limit value.";
    PIN_ExitApplication(0);
  }

  if (tainted_ptr_branch->is_resolved) // which is resolved
  {
    if (tainted_ptr_branch == order_input_indep_ptr_branch_map.rbegin()->second) // and is the current last branch
    {
      /* FOR TESTING ONLY */
//       std::cout << "4\n";
      BOOST_LOG_TRIVIAL(warning) << "FOR TESTING ONLY: stop at the last branch of the first tainting result.";
      PIN_ExitApplication(0);

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
        print_debug_found_new(ins_addr, untainted_ptr_branch);
        accept_branch(untainted_ptr_branch);
        found_new_ptr_branches.push_back(untainted_ptr_branch);
      }

      // the original trace will lost if go further, so rollback
      total_rollback_times++;
      local_rollback_times++;
      
      rollback_with_input_random_modification(active_ptr_branch->checkpoint, 
                                              /*active_ptr_branch->dep_input_addrs*/
                                              active_ptr_branch->nearest_checkpoints[active_ptr_branch->checkpoint]);
    }
    else // error: active_ptr_branch is empty, namely in forwarding, but new taken found
    {
      print_debug_lost_forward(ins_addr/*, untainted_ptr_branch*/);
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
  std::map<UINT32, ptr_branch>::iterator order_ptr_branch_iter;
  
  order_ptr_branch_iter = order_tainted_ptr_branch_map.find(explored_trace.size());
  if (order_ptr_branch_iter != order_tainted_ptr_branch_map.end()) 
  {
    if (order_ptr_branch_iter->second->inputs[br_taken].empty()) 
    {
      store_input(order_ptr_branch_iter->second, br_taken);
    }
  }
  else 
  {
    BOOST_LOG_TRIVIAL(fatal) << boost::format("Branch at %d cannot found.") % explored_trace.size();
    PIN_ExitApplication(0);
  }

  return;
}

/*====================================================================================================================*/

VOID resolving_cond_branch_analyzer(ADDRINT ins_addr, bool br_taken)
{
  log_input(ins_addr, br_taken);
  
  std::map<UINT32, ptr_branch>::iterator order_ptr_branch_iter;
  
  // search in the list of input dependent branches
  order_ptr_branch_iter = order_input_dep_ptr_branch_map.find(explored_trace.size());
  if (order_ptr_branch_iter != order_input_dep_ptr_branch_map.end()) 
  {
    process_tainted_branch(ins_addr, br_taken, order_ptr_branch_iter->second);
  }
  else 
  {
    // search in the list of input independent branches
    order_ptr_branch_iter = order_input_indep_ptr_branch_map.find(explored_trace.size());
    
    if (order_ptr_branch_iter != order_input_indep_ptr_branch_map.end()) 
    {
      process_untainted_branch(ins_addr, br_taken, order_ptr_branch_iter->second);
    }
    else 
    {
      BOOST_LOG_TRIVIAL(fatal) << boost::format("Branch at %d cannot found.") % explored_trace.size();
      PIN_ExitApplication(0);
    }
  }

  return;
}


/**
 * @brief handle the case where the indirect branch may leads to a new target.
 * 
 * @param ins_addr instruction address.
 * @param target_addr target address.
 * @return VOID
 */
VOID resolving_indirect_branch_call_analyzer(ADDRINT ins_addr, ADDRINT target_addr)
{
  if (order_ins_dynamic_map[explored_trace.size() + 1].address != target_addr) 
  {
    if (active_ptr_branch) 
    {
      // the original trace will lost if go further, so rollback
      total_rollback_times++;
      local_rollback_times++;
      
      active_ptr_checkpoint = active_ptr_branch->nearest_checkpoints.rbegin()->first;
      rollback_with_input_random_modification(/*active_ptr_branch->checkpoint, */
                                              /*active_ptr_branch->dep_input_addrs*/
                                              active_ptr_checkpoint,
                                              active_ptr_branch->nearest_checkpoints[active_ptr_checkpoint]);
    }
    else // active_ptr_branch is empty, namely in forwarding, but new target found
    {
      print_debug_lost_forward(ins_addr);
      PIN_ExitApplication(0);
    }    
  }
  return;
}
