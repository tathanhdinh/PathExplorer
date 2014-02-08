#include <pin.H>

#include <boost/format.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>

#include <algorithm>

#include "variable.h"
#include "instruction.h"
#include "checkpoint.h"
#include "branch.h"
#include "stuffs.h"

/*====================================================================================================================*/

extern std::map< ADDRINT,
                 instruction >                    addr_ins_static_map;
extern std::map< UINT32, 
                 instruction >                    order_ins_dynamic_map;

extern bool                                       in_tainting;

extern df_diagram                                 dta_graph;

extern std::vector<ADDRINT>                       explored_trace;

extern ptr_checkpoint                             master_ptr_checkpoint;
extern ptr_checkpoint                             last_active_ptr_checkpoint;
extern std::vector<ptr_checkpoint>                saved_ptr_checkpoints;

extern std::pair< ptr_checkpoint, 
                  std::set<ADDRINT> >             active_nearest_checkpoint;

extern std::map< UINT32,
       std::vector<ptr_checkpoint> >              exepoint_checkpoints_map;

extern UINT32                                     total_rollback_times;
extern UINT32                                     local_rollback_times;
extern UINT32                                     max_total_rollback_times;
extern UINT32                                     max_local_rollback_times;
extern UINT32                                     used_checkpoint_number;

extern ADDRINT                                    received_msg_addr;
extern UINT32                                     received_msg_size;

extern std::map<UINT32, ptr_branch>               order_input_dep_ptr_branch_map;
extern std::map<UINT32, ptr_branch>               order_input_indep_ptr_branch_map;
extern std::map<UINT32, ptr_branch>               order_tainted_ptr_branch_map;

extern std::vector<ptr_branch>                    found_new_ptr_branches;
extern std::vector<ptr_branch>                    total_resolved_ptr_branches;
extern std::vector<ptr_branch>                    total_input_dep_ptr_branches;

extern ptr_branch                                 active_ptr_branch;
extern ptr_branch                                 last_active_ptr_branch;
extern ptr_branch                                 exploring_ptr_branch;

extern std::set<ADDRINT>                          active_input_dep_addrs;

extern UINT64                                     executed_ins_number;
extern UINT64                                     econed_ins_number;
// extern UINT32                                     input_dep_branch_num;
// extern UINT32                                     resolved_branch_num;

extern boost::log::sources::severity_logger<boost::log::trivial::severity_level> log_instance;
extern boost::shared_ptr< boost::log::sinks::synchronous_sink<boost::log::sinks::text_file_backend> > log_sink;

extern KNOB<UINT32>                               max_total_rollback;
extern KNOB<UINT32>                               max_local_rollback;
extern KNOB<UINT32>                               max_trace_length;
extern KNOB<BOOL>                                 print_debug_text;

/*====================================================================================================================*/

VOID resolving_ins_count_analyzer(ADDRINT ins_addr)
{
  explored_trace.push_back(ins_addr);
  executed_ins_number++;

  if (addr_ins_static_map[ins_addr].has_mem_read2)
  {
//     std::set<ADDRINT>::iterator addr_iter;
//     for (addr_iter = order_ins_dynamic_map[explored_trace.size()].src_mems.begin();
//       addr_iter != order_ins_dynamic_map[explored_trace.size()].src_mems.end(); ++addr_iter)
//     {
//       BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
//         << boost::format("%s %d")
//         % remove_leading_zeros(StringFromAddrint(*addr_iter)) 
//         % *(reinterpret_cast<UINT8*>(*addr_iter));
//     }
  }

  if (order_ins_dynamic_map[explored_trace.size()].address == ins_addr)
  {
    /*BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
      << boost::format("instruction at %d (%s: %s) will be executed")
      % explored_trace.size()
      % remove_leading_zeros(StringFromAddrint(ins_addr))
      % addr_ins_static_map[ins_addr].disass;*/
  }
  else
  {
    if (local_rollback_times < max_local_rollback)
    {
      /*BOOST_LOG_SEV(log_instance, boost::log::trivial::info) 
        << boost::format("random-rollback from %d (%s: %s) to %d (%s: %s)")
        % explored_trace.size()
        % remove_leading_zeros(StringFromAddrint(ins_addr))
        % addr_ins_static_map[ins_addr].disass
        % active_nearest_checkpoint.first->trace.size()
        % remove_leading_zeros(StringFromAddrint(active_nearest_checkpoint.first->addr))
        % addr_ins_static_map[active_nearest_checkpoint.first->addr].disass;*/

      // the original trace will be lost if go further, so rollback
      total_rollback_times++;
      local_rollback_times++;
      rollback_with_input_random_modification(active_nearest_checkpoint.first, 
                                              active_nearest_checkpoint.second);
    }
    else
    {
      if (local_rollback_times > max_local_rollback + 1)
      {
        BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
          << "stop because of unknow bug";
        PIN_ExitApplication(0);
      }

      /*BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
        << boost::format("replace-rollback from %d (%s: %s) to %d (%s: %s)")
        % explored_trace.size()
        % remove_leading_zeros(StringFromAddrint(ins_addr))
        % addr_ins_static_map[ins_addr].disass
        % active_nearest_checkpoint.first->trace.size()
        % remove_leading_zeros(StringFromAddrint(active_nearest_checkpoint.first->addr))
        % addr_ins_static_map[active_nearest_checkpoint.first->addr].disass;*/

      // back to the original trace
      local_rollback_times++;
      /*std::cerr << "first value of the stored buffer at branch " << active_ptr_branch->trace.size() << " : "
        << active_ptr_branch->inputs[active_ptr_branch->br_taken][0].get()[0] << "\n";*/
      rollback_with_input_replacement(active_nearest_checkpoint.first, 
                                      active_ptr_branch->inputs[active_ptr_branch->br_taken][0].get());
      /*rollback_with_input_replacement(saved_ptr_checkpoints[0], 
                                      active_ptr_branch->inputs[active_ptr_branch->br_taken][0].get());*/
    }
  }

  
  //log_sink->flush();
  return;
}

/*====================================================================================================================*/
// memory read
VOID resolving_mem_to_st_analyzer(ADDRINT ins_addr, 
                                  ADDRINT mem_read_addr, 
                                  UINT32 mem_read_size) 
{
  return;
}

/*====================================================================================================================*/
// memory written
VOID resolving_st_to_mem_analyzer(ADDRINT ins_addr, 
                                  ADDRINT mem_written_addr, 
                                  UINT32 mem_written_size) 
{
  if (active_ptr_branch) // in rollbacking
  {
    if (active_nearest_checkpoint.first) 
    {
      active_nearest_checkpoint.first->mem_written_logging(ins_addr, 
                                                           mem_written_addr, 
                                                           mem_written_size);
    }
    else 
    {
      last_active_ptr_checkpoint->mem_written_logging(ins_addr, 
                                                      mem_written_addr, 
                                                      mem_written_size);
    }
  }
  else // in forwarding
  {
    std::vector<ptr_checkpoint>::iterator 
      ptr_checkpoint_iter = exepoint_checkpoints_map[explored_trace.size()].begin();
    for (; ptr_checkpoint_iter != exepoint_checkpoints_map[explored_trace.size()].end(); 
         ++ptr_checkpoint_iter)
    {
      (*ptr_checkpoint_iter)->mem_written_logging(ins_addr, mem_written_addr, mem_written_size);
    }
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
  
  master_ptr_checkpoint = *saved_ptr_checkpoints.begin();

  std::map<UINT32, instruction>::iterator order_ins_map_iter; 
  order_ins_map_iter = order_ins_dynamic_map.find(master_ptr_checkpoint->trace.size());
  order_ins_dynamic_map.erase(order_ins_map_iter, order_ins_dynamic_map.end());
    
  dta_graph.clear();
//   order_ins_dynamic_map.clear();
  order_tainted_ptr_branch_map.clear();
  order_input_dep_ptr_branch_map.clear();
  order_input_indep_ptr_branch_map.clear();
  saved_ptr_checkpoints.clear();
  exepoint_checkpoints_map.clear();
  
  unexplored_ptr_branch->is_explored = true;
  
  active_ptr_branch.reset();
  active_nearest_checkpoint.first.reset();
  active_nearest_checkpoint.second.clear();
  last_active_ptr_checkpoint.reset();
  
  local_rollback_times = 0; 
  
  return;
}

/*====================================================================================================================*/

inline ptr_branch next_unexplored_branch()
{
  ptr_branch unexplored_ptr_branch;
  
  std::vector<ptr_branch>::iterator 
    unexplored_ptr_branch_iter = total_resolved_ptr_branches.begin();
  for (; unexplored_ptr_branch_iter != total_resolved_ptr_branches.end(); 
       ++unexplored_ptr_branch_iter)
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

inline void accept_branch(ptr_branch& accepted_ptr_branch)
{
  accepted_ptr_branch->is_resolved      = true;
  accepted_ptr_branch->is_just_resolved = true;
  accepted_ptr_branch->is_bypassed      = false;

  total_resolved_ptr_branches.push_back(accepted_ptr_branch);
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

/**
 * @brief set the active_nearest_checkpoint.
 * 
 * @param current_ptr_branch ...
 * @return void
 */
inline void set_next_active_nearest_checkpoint(ptr_branch& current_ptr_branch) 
{
  std::map<ptr_checkpoint, 
           std::set<ADDRINT>, 
           ptr_checkpoint_less>::iterator nearest_checkpoint_iter;
  
  std::map<ptr_checkpoint, 
           std::set<ADDRINT>, 
           ptr_checkpoint_less>::iterator next_nearest_checkpoint_iter;
  
  if (active_nearest_checkpoint.first) 
  {
    nearest_checkpoint_iter = current_ptr_branch->nearest_checkpoints.begin();
    next_nearest_checkpoint_iter = current_ptr_branch->nearest_checkpoints.end();
    
    for (; nearest_checkpoint_iter != current_ptr_branch->nearest_checkpoints.end(); 
         ++nearest_checkpoint_iter) 
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
    
    if (nearest_checkpoint_iter != current_ptr_branch->nearest_checkpoints.end()) 
    {
      if (next_nearest_checkpoint_iter != current_ptr_branch->nearest_checkpoints.end()) 
      {
        used_checkpoint_number++;
        active_nearest_checkpoint.first = (*next_nearest_checkpoint_iter).first;
        active_nearest_checkpoint.second.insert((*next_nearest_checkpoint_iter).second.begin(), 
                                                (*next_nearest_checkpoint_iter).second.end());
      }
      else // the next nearest checkpoint cannot found, 
      {
        // so reset the active nearest checkpoint
        active_nearest_checkpoint.first.reset();
        active_nearest_checkpoint.second.clear();
      }
    }
    else 
    {
      //BOOST_LOG_TRIVIAL(fatal) 
      BOOST_LOG_SEV(log_instance, boost::log::trivial::fatal)
        << boost::format("%s: nearest checkpoint for the branch at %d cannot found") 
            % __FUNCTION__ % current_ptr_branch->trace.size();
      log_sink->flush();

      PIN_ExitApplication(4);
    }
  }
  else 
  {
    used_checkpoint_number = 1;
    active_nearest_checkpoint.first = current_ptr_branch->nearest_checkpoints.rbegin()->first;
    active_nearest_checkpoint.second = current_ptr_branch->nearest_checkpoints.rbegin()->second;
  }
  
  return;
}


/**
 * @brief if there exists a unexplored branch then continue resolving it, if not then stop.
 * 
 * @return void
 */
inline void exploring_new_branch_or_stop()
{
  ptr_branch unexplored_ptr_branch = next_unexplored_branch();
  if (unexplored_ptr_branch) 
  {
    //BOOST_LOG_TRIVIAL(info) 
    BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
    << boost::format("economized/total executed instruction number %d/%d") 
        % econed_ins_number % executed_ins_number;
        
    //std::cout 
    BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
      << boost::format("exploring the branch at %d (%s) by start tainting a new path.\n") 
          % unexplored_ptr_branch->trace.size() 
          % addr_ins_static_map[unexplored_ptr_branch->addr].disassembled_name;
    log_sink->flush();
    
    // the MASTER checkpoint is the first element of saved_ptr_checkpoints, 
    // it will be update here because the prepare_new_tainting_phase will 
    // reset all branches and checkpoints saved in the current exploration.
    prepare_new_tainting_phase(unexplored_ptr_branch);
    
    PIN_RemoveInstrumentation();
    
    // explore new branch
    local_rollback_times++;
    rollback_with_input_replacement(master_ptr_checkpoint,
                                    exploring_ptr_branch->inputs[!exploring_ptr_branch->br_taken][0].get());
  }
  else 
  {
    //BOOST_LOG_TRIVIAL(info) 
    BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
      << "stop exploring, all branches are explored.";
    log_sink->flush();

    PIN_ExitApplication(0);
  }
  
  return;
}


/**
 * @brief reset all active branche and checkpoints to continue resolve the next branch.
 * 
 * @return void
 */
inline void reset_current_active_branch_checkpoints() 
{
  active_ptr_branch.reset();
  active_nearest_checkpoint.first.reset();
  active_nearest_checkpoint.second.clear();
  last_active_ptr_checkpoint.reset();
  local_rollback_times = 0; 
  return;
}


/**
 * @brief handle the case where the examined branch is marked as input dependent but it is resolved. 
 * Note that a resolved branch is examined if and only if: in rollback or in some forward which omit it.
 * Moreover in the following procedure the examined branch is not the active branch.
 * 
 * @param ins_addr ...
 * @param br_taken ...
 * @param examined_ptr_branch ...
 * @return void
 */
inline void process_input_dependent_and_resolved_branch(ADDRINT ins_addr, 
                                                        bool br_taken, 
                                                        ptr_branch& examined_ptr_branch)
{
  // consider only when active_ptr_branch is enabled, 
  // namely in some rollback of the current exploration.
  if (active_ptr_branch) 
  {
    // when the examined branch is also the active branch and it is resolved, 
    // that means this is a re-execution from a rollback_with_input_replacement
    if (examined_ptr_branch == active_ptr_branch) 
    {
      // go forward
     reset_current_active_branch_checkpoints();
    }
    else // the examined branch is not the active branch
    {
      // the examined branch takes a new decision
      if (examined_ptr_branch->br_taken != br_taken) 
      {
        if (examined_ptr_branch->is_bypassed)
        {
          //BOOST_LOG_TRIVIAL(trace) 
          BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
            << boost::format("the branch at %d is resolved accidentally in resolving the branch at %d") 
                % examined_ptr_branch->trace.size() % active_ptr_branch->trace.size();
          log_sink->flush();
                
          // the branch has been marked as bypassed before, 
          // then it is resolved accidentally
          accept_branch(examined_ptr_branch);
        }
        
        if (local_rollback_times < max_local_rollback_times)
        {
//           econed_ins_number += active_ptr_branch->econ_execution_length[active_nearest_checkpoint.first];
//           std::cout << econed_ins_number << "  " << executed_ins_number << std::endl;
          
          // the original trace will be lost if go further, so rollback
          total_rollback_times++;
          local_rollback_times++;
          rollback_with_input_random_modification(active_nearest_checkpoint.first, 
                                                  active_nearest_checkpoint.second);
        }
        else
        {
//           econed_ins_number += active_ptr_branch->econ_execution_length[active_nearest_checkpoint.first];
//           std::cout << econed_ins_number << "  " << executed_ins_number << std::endl;
          
          // back to the original trace
          local_rollback_times++;
          rollback_with_input_replacement(active_nearest_checkpoint.first,
                                          active_ptr_branch->inputs[active_ptr_branch->br_taken][0].get());
        }
      }
    }
  }

  return;
}


/**
 * @brief handle the case where the examined branch takes a different target. 
 * Note that this branch is always the active branch because the following procedure is used in 
 * process_input_dependent_but_unresolved_branch.
 * 
 * @param ins_addr ...
 * @param br_taken ...
 * @param examined_ptr_branch ...
 * @return void
 */
inline void unresolved_branch_takes_new_decision(ADDRINT ins_addr, 
                                                 bool br_taken, 
                                                 ptr_branch& examined_ptr_branch)
{
  // active_ptr_branch is enabled, namely in some rollback
  if (active_ptr_branch) 
  {
    // so this branch is resolved
    BOOST_LOG_SEV(log_instance, boost::log::trivial::info) 
      << boost::format("the branch at %d (%s: %s) is successfully resolved after %d rollbacks") 
          % examined_ptr_branch->trace.size() 
          % remove_leading_zeros(StringFromAddrint(ins_addr)) % addr_ins_static_map[ins_addr].disassembled_name
          % (local_rollback_times + (used_checkpoint_number - 1) * max_local_rollback_times);
    
    accept_branch(active_ptr_branch);
    
    // now back to the original trace
    econed_ins_number += active_ptr_branch->econ_execution_length[active_nearest_checkpoint.first];
//     std::cout << econed_ins_number << "  " << executed_ins_number << std::endl;
    
    local_rollback_times++;
    rollback_with_input_replacement(active_nearest_checkpoint.first, 
                                    active_ptr_branch->inputs[active_ptr_branch->br_taken][0].get());
  }
  else // active_ptr_branch is disabled, namely in some forward
  {
    BOOST_LOG_SEV(log_instance, boost::log::trivial::fatal)
      << boost::format("the branch at %d takes a different decision in forwarding.") 
          % explored_trace.size();

    PIN_ExitApplication(2);
  }

  return;
}


/**
 * @brief handle the case where the examined branch takes the same decision. Note that 
 * this branch is always the active branch because the following procedure is used in 
 * process_input_dependent_but_unresolved_branch.
 * 
 * @param ins_addr ...
 * @param br_taken ...
 * @param tainted_ptr_branch ...
 * @return void
 */
inline void unresolved_branch_takes_same_decision(ADDRINT ins_addr, 
                                                 bool br_taken, ptr_branch& examined_ptr_branch)
{
  // active_ptr_branch is enabled, namely in some rollback
  if (active_ptr_branch) 
  {
    // so the unresolved examined_ptr_branch must be the active branch
    if (active_ptr_branch != examined_ptr_branch) 
    {
      //BOOST_LOG_TRIVIAL(fatal) 
      BOOST_LOG_SEV(log_instance, boost::log::trivial::fatal)
        << boost::format("in rollback but the tainted (at %d) and the active branch (at %d) are not matched.") 
            % examined_ptr_branch->trace.size() % active_ptr_branch->trace.size();
      log_sink->flush();

      PIN_ExitApplication(3);
    }
    else 
    {
      if ((local_rollback_times < max_local_rollback_times) && 
          active_nearest_checkpoint.first) 
      {
        econed_ins_number += active_ptr_branch->econ_execution_length[active_nearest_checkpoint.first];
//         std::cout << econed_ins_number << "  " << executed_ins_number << std::endl;
        
        total_rollback_times++;
        local_rollback_times++;
        rollback_with_input_random_modification(active_nearest_checkpoint.first, 
                                                active_nearest_checkpoint.second);
      }
      else 
      {
        // this comes from the last rollback_with_input_random_modification
        if (local_rollback_times == max_local_rollback_times) 
        {
          // back to the original trace
          econed_ins_number += active_ptr_branch->econ_execution_length[active_nearest_checkpoint.first];
//           std::cout << econed_ins_number << "  " << executed_ins_number << std::endl;
          
          local_rollback_times++;
          rollback_with_input_replacement(active_nearest_checkpoint.first, 
                                          active_ptr_branch->inputs[active_ptr_branch->br_taken][0].get());
        }
        else 
        {
          // this comes from the rollback_with_input_replacement
          if (local_rollback_times > max_local_rollback_times) 
          {
            // store the last active checkpoint
            last_active_ptr_checkpoint = active_nearest_checkpoint.first;
            
            // and get the next active checkpoint
            set_next_active_nearest_checkpoint(active_ptr_branch);
            
            if (active_nearest_checkpoint.first) 
            {
              local_rollback_times = 0;
              
              // then rollback to it
              //BOOST_LOG_TRIVIAL(trace) 
              BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
                << boost::format("rollback to the next checkpoint at %d (%s).") 
                    % active_nearest_checkpoint.first->trace.size() 
                    % order_ins_dynamic_map[active_nearest_checkpoint.first->trace.size()].disassembled_name;
              log_sink->flush();
          
              econed_ins_number += active_ptr_branch->econ_execution_length[active_nearest_checkpoint.first];
//               std::cout << econed_ins_number << "  " << executed_ins_number << std::endl;
              
              total_rollback_times++;
              local_rollback_times++;
              rollback_with_input_random_modification(active_nearest_checkpoint.first, 
                                                      active_nearest_checkpoint.second);
            }
            else // cannot get any new nearest checkpoint
            {
              // so bypass this branch
              //BOOST_LOG_TRIVIAL(trace) 
              BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
              << boost::format("cannot resolve the branch at %d, bypass it") 
                  % active_ptr_branch->trace.size();
              log_sink->flush();
                                          
              bypass_branch(active_ptr_branch);      

              econed_ins_number += active_ptr_branch->econ_execution_length[last_active_ptr_checkpoint];
//               std::cout << econed_ins_number << "  " << executed_ins_number << std::endl;
              
              // and back to the original trace
              local_rollback_times++;
              rollback_with_input_replacement(last_active_ptr_checkpoint, 
                                              active_ptr_branch->inputs[active_ptr_branch->br_taken][0].get());
            }
          }
          else // the active_nearest_checkpoint is empty
          {
            // that contradict to the fact that the active_branch is not empty
            // and unresolved
            //BOOST_LOG_TRIVIAL(fatal) 
            BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
              << boost::format("the branch at %d is unresolved but the nearest checkpoint is empty.") 
                  % active_ptr_branch->trace.size();
            log_sink->flush();

            PIN_ExitApplication(5);
          }
        }
      }
    }
  }
  else // active_ptr_branch is disabled, namely in some forward and meet a new input dependent branch
  {
    // so enable active_ptr_branch
    active_ptr_branch = examined_ptr_branch;
    
    // and rollback
    set_next_active_nearest_checkpoint(active_ptr_branch);
    
    // then rollback to it
    //BOOST_LOG_TRIVIAL(trace) 
    BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
      << boost::format("resolve the branch at %d:%d (%s: %s) by rollback to the checkpoint at %d (%s)") 
          % active_ptr_branch->trace.size() 
          % active_ptr_branch->br_taken
          % remove_leading_zeros(StringFromAddrint(active_ptr_branch->addr))
          % order_ins_dynamic_map[active_ptr_branch->trace.size()].disassembled_name 
          % active_nearest_checkpoint.first->trace.size() 
          % order_ins_dynamic_map[active_nearest_checkpoint.first->trace.size()].disassembled_name;
    log_sink->flush();
   
    econed_ins_number += active_ptr_branch->econ_execution_length[active_nearest_checkpoint.first];
//     std::cout << econed_ins_number << "  " << executed_ins_number << std::endl;
    
    total_rollback_times++;
    local_rollback_times++;
    rollback_with_input_random_modification(active_nearest_checkpoint.first, 
                                            active_nearest_checkpoint.second);
  }
  
  return;
}


/**
 * @brief handle the case where the examined branch is an input 
 * 
 * @param ins_addr ...
 * @param br_taken ...
 * @param examined_ptr_branch ...
 * @return void
 */
inline void process_input_dependent_but_unresolved_branch(ADDRINT ins_addr, 
                                                          bool br_taken, 
                                                          ptr_branch& examined_ptr_branch)
{
  // other decision is taken
  if (examined_ptr_branch->br_taken != br_taken) 
  {
    unresolved_branch_takes_new_decision(ins_addr, 
                                         br_taken, 
                                         examined_ptr_branch);
  }
  else // the same decision is taken
  {
    unresolved_branch_takes_same_decision(ins_addr, 
                                          br_taken, 
                                          examined_ptr_branch);
  }

  return;
}


/**
 * @brief handle the case where the examined branch is detected as input dependent.
 * 
 * @param ins_addr ...
 * @param br_taken ...
 * @param tainted_ptr_branch ...
 * @return void
 */
inline void process_input_dependent_branch(ADDRINT ins_addr, 
                                           bool br_taken, 
                                           ptr_branch& examined_ptr_branch)
{  
  if (examined_ptr_branch->is_resolved) // which is resolved
  {
    // and is the current last branch
    if (examined_ptr_branch == order_input_dep_ptr_branch_map.rbegin()->second) 
    {
      /* FOR TESTING ONLY */
//       BOOST_LOG_TRIVIAL(warning) 
//         << "FOR TESTING ONLY: stop at the last branch of the first tainting result.";
//       PIN_ExitApplication(0);

      //BOOST_LOG_TRIVIAL(info) 
      BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
        << boost::format("stop rollbacking, %d/%d branches resolved\n") 
            % total_resolved_ptr_branches.size() 
            % total_input_dep_ptr_branches.size()
//             % order_input_dep_ptr_branch_map.size()
        << "-------------------------------------------------------------------------------------------------";
      log_sink->flush();
        
      exploring_new_branch_or_stop();
    }
    else // it is not the last branch
    {
      process_input_dependent_and_resolved_branch(ins_addr, 
                                                  br_taken, 
                                                  examined_ptr_branch);
    }
  }
  else // it is not resolved yet
  {
    process_input_dependent_but_unresolved_branch(ins_addr, 
                                                  br_taken, 
                                                  examined_ptr_branch);
  }

  return;
}


/**
 * @brief handle the case where the examined branch is detected as input independent
 * 
 * @param ins_addr ...
 * @param br_taken ...
 * @param examined_ptr_branch ...
 * @return void
 */
inline void process_input_independent_branch(ADDRINT ins_addr, 
                                             bool br_taken, 
                                             ptr_branch& examined_ptr_branch)
{
  // new taken found
  if (examined_ptr_branch->br_taken != br_taken) 
  {
    // active_ptr_branch is enabled, namely in some rollback or 
    // a re-execution from rollback_with_input_replacement
    if (active_ptr_branch) 
    {
//       BOOST_LOG_TRIVIAL(warning) 
//         << boost::format("\033[35mThe branch at %d (%s) is input independent, but takes a new decision.\033[0m") 
//             % examined_ptr_branch->trace.size() 
//             % order_ins_dynamic_map[examined_ptr_branch->trace.size()].disass ;
              
      if (!examined_ptr_branch->is_resolved)
      {
        accept_branch(examined_ptr_branch);
        found_new_ptr_branches.push_back(examined_ptr_branch);
      }

      // in some rollback
      if (active_nearest_checkpoint.first) 
      {
//         econed_ins_number += active_ptr_branch->econ_execution_length[active_nearest_checkpoint.first];
//         std::cout << econed_ins_number << "  " << executed_ins_number << std::endl;
        
        // the original trace will lost if go further, so rollback
        total_rollback_times++;
        local_rollback_times++;
        rollback_with_input_random_modification(active_nearest_checkpoint.first, 
                                                active_nearest_checkpoint.second);
      }
      else // in a re-execution from rollback_with_input_replacement
      {
        //BOOST_LOG_TRIVIAL(fatal) 
        BOOST_LOG_SEV(log_instance, boost::log::trivial::fatal)
          << boost::format("the branch at %d takes a new decision in a clean re-execution") 
              % examined_ptr_branch->trace.size();
        log_sink->flush();
      }
    }
    else // active_ptr_branch is disabled, namely in forwarding
    {
      // but new taken found
      //BOOST_LOG_TRIVIAL(fatal) 
      BOOST_LOG_SEV(log_instance, boost::log::trivial::fatal)
        << boost::format("the branch at %d takes a new decision in forwarding.") 
            % examined_ptr_branch->trace.size();
      log_sink->flush();

      PIN_ExitApplication(0);
    }
  }

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
    //BOOST_LOG_TRIVIAL(fatal)
    BOOST_LOG_SEV(log_instance, boost::log::trivial::fatal)
      << boost::format("%s: the branch at %d (%s: %s) cannot found") 
          % __FUNCTION__ % explored_trace.size() 
          % remove_leading_zeros(StringFromAddrint(ins_addr)) 
          % addr_ins_static_map[ins_addr].disassembled_name;
    log_sink->flush();

    PIN_ExitApplication(0);
  }

  return;
}

/*====================================================================================================================*/

VOID resolving_cond_branch_analyzer(ADDRINT ins_addr, bool br_taken)
{
  /*BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
    << boost::format("met a branch at %d (%s: %s)")
        % explored_trace.size()
        % remove_leading_zeros(StringFromAddrint(ins_addr))
        % addr_ins_static_map[ins_addr].disass;*/

  log_input(ins_addr, br_taken);
  
  std::map<UINT32, ptr_branch>::iterator order_ptr_branch_iter;
  
  // search in the list of input dependent branches
  order_ptr_branch_iter = order_input_dep_ptr_branch_map.find(explored_trace.size());
  if (order_ptr_branch_iter != order_input_dep_ptr_branch_map.end()) 
  {
    process_input_dependent_branch(ins_addr, br_taken, 
                                   order_ptr_branch_iter->second);
  }
  else 
  {
    // search in the list of input independent branches
    order_ptr_branch_iter = order_input_indep_ptr_branch_map.find(explored_trace.size());
    if (order_ptr_branch_iter != order_input_indep_ptr_branch_map.end()) 
    {
      process_input_independent_branch(ins_addr, br_taken, 
                                       order_ptr_branch_iter->second);
    }
    else 
    {
      //BOOST_LOG_TRIVIAL(fatal) 
      BOOST_LOG_SEV(log_instance, boost::log::trivial::fatal)
        << boost::format("the branch at %d (%s: %s) cannot found")
        % explored_trace.size() % remove_leading_zeros(StringFromAddrint(ins_addr)) 
        % addr_ins_static_map[ins_addr].disassembled_name;
      log_sink->flush();

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
    // active_ptr_branch is enabled, namely in some rollback
    if (active_ptr_branch) 
    {
//       econed_ins_number += active_ptr_branch->econ_execution_length[active_nearest_checkpoint.first];
//       std::cout << econed_ins_number << "  " << executed_ins_number << std::endl;
      
      // the original trace will lost if go further, so rollback
      total_rollback_times++;
      local_rollback_times++;
      rollback_with_input_random_modification(active_nearest_checkpoint.first, 
                                              active_nearest_checkpoint.second);
    }
    else // active_ptr_branch is empty, namely in forwarding, but new target found
    {
      //BOOST_LOG_TRIVIAL(fatal) 
      BOOST_LOG_SEV(log_instance, boost::log::trivial::fatal)
        << boost::format("the indirect branch at %d takes a different decision in forwarding.") 
            % explored_trace.size();
      PIN_ExitApplication(0);
    }    
  }
  return;
}
