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

/*================================================================================================*/

extern std::map<ADDRINT, ptr_instruction_t>       addr_ins_static_map;
extern std::map<UINT32,  ptr_instruction_t>       order_ins_dynamic_map;

extern bool                                       in_tainting;

extern df_diagram                                 dta_graph;
extern df_vertex_desc_set                         dta_outer_vertices;

extern std::vector<ADDRINT>                       explored_trace;
extern UINT32                                     current_execution_order;

extern ptr_checkpoint_t                             master_ptr_checkpoint;
extern ptr_checkpoint_t                             last_active_ptr_checkpoint;
extern std::vector<ptr_checkpoint_t>                saved_ptr_checkpoints;

extern std::pair< ptr_checkpoint_t, 
                  std::set<ADDRINT> >             active_nearest_checkpoint;

extern std::map< UINT32,
       std::vector<ptr_checkpoint_t> >              exepoint_checkpoints_map;

extern UINT32                                     total_rollback_times;
extern UINT32                                     local_rollback_times;
extern UINT32                                     max_total_rollback_times;
extern UINT32                                     max_local_rollback_times;
extern UINT32                                     used_checkpoint_number;

extern ADDRINT                                    received_msg_addr;
extern UINT32                                     received_msg_size;

extern std::map<UINT32, ptr_branch_t>               order_input_dep_ptr_branch_map;
extern std::map<UINT32, ptr_branch_t>               order_input_indep_ptr_branch_map;
extern std::map<UINT32, ptr_branch_t>               order_tainted_ptr_branch_map;

extern std::vector<ptr_branch_t>                    found_new_ptr_branches;
extern std::vector<ptr_branch_t>                    total_resolved_ptr_branches;
extern std::vector<ptr_branch_t>                    total_input_dep_ptr_branches;

extern ptr_branch_t                                 active_ptr_branch;
extern ptr_branch_t                                 last_active_ptr_branch;
extern ptr_branch_t                                 exploring_ptr_branch;

extern std::set<ADDRINT>                          active_input_dep_addrs;

extern UINT64                                     executed_ins_number;
extern UINT64                                     econed_ins_number;

namespace logging = boost::log;
namespace sinks   = boost::log::sinks;
namespace sources = boost::log::sources;
typedef sinks::text_file_backend text_backend;
typedef sinks::synchronous_sink<text_backend>     sink_file_backend;
typedef logging::trivial::severity_level          log_level;

extern sources::severity_logger<log_level>        log_instance;
extern boost::shared_ptr<sink_file_backend>       log_sink;

extern KNOB<UINT32>                               max_total_rollback;
extern KNOB<UINT32>                               max_local_rollback;
extern KNOB<UINT32>                               max_trace_length;
extern KNOB<BOOL>                                 print_debug_text;

/*================================================================================================*/

VOID resolving_ins_count_analyzer(ADDRINT ins_addr)
{
  current_execution_order++;
  executed_ins_number++;

  if (order_ins_dynamic_map[current_execution_order]->address == ins_addr)
  {
    // do nothing
  }
  else
  {
    if (local_rollback_times < max_local_rollback)
    {
      // the original trace will be lost if go further, so rollback
      total_rollback_times++;
      local_rollback_times++;
      rollback_and_modify(active_nearest_checkpoint.first, active_nearest_checkpoint.second);
    }
    else
    {
      if (local_rollback_times > max_local_rollback + 1)
      {
        BOOST_LOG_SEV(log_instance, boost::log::trivial::info) << "stop because of unknow bug";
        PIN_ExitApplication(0);
      }

      // back to the original trace
      local_rollback_times++;
      rollback_and_restore(active_nearest_checkpoint.first,
                           active_ptr_branch->inputs[active_ptr_branch->br_taken][0].get());
    }
  }
  
  return;
}

/*================================================================================================*/
// memory read
VOID resolving_mem_to_st_analyzer(ADDRINT ins_addr, 
                                  ADDRINT mem_read_addr, 
                                  UINT32 mem_read_size) 
{
  return;
}

/*================================================================================================*/
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
    std::vector<ptr_checkpoint_t>::iterator
        ptr_checkpoint_iter = exepoint_checkpoints_map[current_execution_order].begin();
    for (; ptr_checkpoint_iter != exepoint_checkpoints_map[current_execution_order].end();
         ++ptr_checkpoint_iter)
    {
      (*ptr_checkpoint_iter)->mem_written_logging(ins_addr, mem_written_addr, mem_written_size);
    }
  }

  return;
}

/*================================================================================================*/

inline void prepare_new_tainting_phase(ptr_branch_t& unexplored_ptr_branch)
{
  in_tainting = true;
  exploring_ptr_branch = unexplored_ptr_branch;
  
  master_ptr_checkpoint = *saved_ptr_checkpoints.begin();

  std::map<UINT32, ptr_instruction_t>::iterator order_ins_map_iter;
  order_ins_map_iter = order_ins_dynamic_map.find(master_ptr_checkpoint->execution_order);
  order_ins_dynamic_map.erase(order_ins_map_iter, order_ins_dynamic_map.end());
    
  dta_graph.clear();
  dta_outer_vertices.clear();
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

/*================================================================================================*/

inline ptr_branch_t next_unexplored_branch()
{
  ptr_branch_t unexplored_ptr_branch;
  
  std::vector<ptr_branch_t>::iterator 
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

/*================================================================================================*/

inline void accept_branch(ptr_branch_t& accepted_ptr_branch)
{
  accepted_ptr_branch->is_resolved      = true;
  accepted_ptr_branch->is_just_resolved = true;
  accepted_ptr_branch->is_bypassed      = false;

  total_resolved_ptr_branches.push_back(accepted_ptr_branch);
  return;
}

/*================================================================================================*/

inline void bypass_branch(ptr_branch_t& bypassed_ptr_branch)
{
  bypassed_ptr_branch->is_resolved      = true;
  bypassed_ptr_branch->is_just_resolved = true;
  bypassed_ptr_branch->is_bypassed      = true;
  return;
}

/*================================================================================================*/

/**
 * @brief set the active_nearest_checkpoint.
 * 
 * @param current_ptr_branch ...
 * @return void
 */
inline void set_next_active_nearest_checkpoint(ptr_branch_t& current_ptr_branch) 
{
  std::map<ptr_checkpoint_t, 
           std::set<ADDRINT>, 
           ptr_checkpoint_less>::iterator nearest_checkpoint_iter;
  
  std::map<ptr_checkpoint_t, 
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
            % __FUNCTION__ % current_ptr_branch->execution_order;

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
  ptr_branch_t unexplored_ptr_branch = next_unexplored_branch();
  if (unexplored_ptr_branch) 
  {
    BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
    << boost::format("economized/total executed instruction number %d/%d") 
        % econed_ins_number % executed_ins_number;
        
    BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
      << boost::format("exploring the branch at %d (%s) by start tainting a new path")
          % unexplored_ptr_branch->execution_order
          % addr_ins_static_map[unexplored_ptr_branch->addr]->disassembled_name;
    
    // the MASTER checkpoint is the first element of saved_ptr_checkpoints, 
    // it will be update here because the prepare_new_tainting_phase will 
    // reset all branches and checkpoints saved in the current exploration.
    prepare_new_tainting_phase(unexplored_ptr_branch);
    
    PIN_RemoveInstrumentation();
    
    // explore new branch
    local_rollback_times++;
    rollback_and_restore(master_ptr_checkpoint,
                         exploring_ptr_branch->inputs[!exploring_ptr_branch->br_taken][0].get());
  }
  else 
  {
    //BOOST_LOG_TRIVIAL(info) 
    BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
      << "stop exploring, all branches are explored.";

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
                                                        ptr_branch_t& examined_ptr_branch)
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
                % examined_ptr_branch->execution_order % active_ptr_branch->execution_order;
                
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
          rollback_and_modify(active_nearest_checkpoint.first, 
                                                  active_nearest_checkpoint.second);
        }
        else
        {
//           econed_ins_number += active_ptr_branch->econ_execution_length[active_nearest_checkpoint.first];
//           std::cout << econed_ins_number << "  " << executed_ins_number << std::endl;
          
          // back to the original trace
          local_rollback_times++;
          rollback_and_restore(active_nearest_checkpoint.first,
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
                                                 ptr_branch_t& examined_ptr_branch)
{
  // active_ptr_branch is enabled, namely in some rollback
  if (active_ptr_branch) 
  {
    // so this branch is resolved
    BOOST_LOG_SEV(log_instance, boost::log::trivial::info) 
      << boost::format("the branch at %d (%s: %s) is successfully resolved after %d rollbacks") 
          % examined_ptr_branch->execution_order
          % remove_leading_zeros(StringFromAddrint(ins_addr))
          % addr_ins_static_map[ins_addr]->disassembled_name
          % (local_rollback_times + (used_checkpoint_number - 1) * max_local_rollback_times);
    
    accept_branch(active_ptr_branch);
    
    // now back to the original trace
    econed_ins_number += active_ptr_branch->econ_execution_length[active_nearest_checkpoint.first];
//     std::cout << econed_ins_number << "  " << executed_ins_number << std::endl;
    
    local_rollback_times++;
    rollback_and_restore(active_nearest_checkpoint.first, 
                                    active_ptr_branch->inputs[active_ptr_branch->br_taken][0].get());
  }
  else // active_ptr_branch is disabled, namely in some forward
  {
    BOOST_LOG_SEV(log_instance, boost::log::trivial::fatal)
      << boost::format("the branch at %d takes a different decision in forwarding.") 
          % current_execution_order;

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
                                                 bool br_taken, ptr_branch_t& examined_ptr_branch)
{
  // active_ptr_branch is enabled, namely in some rollback
  if (active_ptr_branch) 
  {
    // so the unresolved examined_ptr_branch must be the active branch
    if (active_ptr_branch != examined_ptr_branch) 
    {
      BOOST_LOG_SEV(log_instance, boost::log::trivial::fatal)
        << boost::format("in rollback but the tainted (at %d) and the active branch (at %d) are not matched.") 
            % examined_ptr_branch->execution_order % active_ptr_branch->execution_order;

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
        rollback_and_modify(active_nearest_checkpoint.first, active_nearest_checkpoint.second);
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
          rollback_and_restore(active_nearest_checkpoint.first, 
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
              BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
                << boost::format("rollback to the next checkpoint at %d (%s).") 
                    % active_nearest_checkpoint.first->execution_order
                    % order_ins_dynamic_map[active_nearest_checkpoint.first->execution_order]->disassembled_name;
          
              econed_ins_number += active_ptr_branch->econ_execution_length[active_nearest_checkpoint.first];
//               std::cout << econed_ins_number << "  " << executed_ins_number << std::endl;
              
              total_rollback_times++;
              local_rollback_times++;
              rollback_and_modify(active_nearest_checkpoint.first, 
                                                      active_nearest_checkpoint.second);
            }
            else // cannot get any new nearest checkpoint
            {
              // so bypass this branch
              BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
              << boost::format("cannot resolve the branch at %d, bypass it") 
                  % active_ptr_branch->execution_order;
                                          
              bypass_branch(active_ptr_branch);      

              econed_ins_number += active_ptr_branch->econ_execution_length[last_active_ptr_checkpoint];
//               std::cout << econed_ins_number << "  " << executed_ins_number << std::endl;
              
              // and back to the original trace
              local_rollback_times++;
              rollback_and_restore(last_active_ptr_checkpoint, 
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
                  % active_ptr_branch->execution_order;

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
          % active_ptr_branch->execution_order
          % active_ptr_branch->br_taken
          % remove_leading_zeros(StringFromAddrint(active_ptr_branch->addr))
          % order_ins_dynamic_map[active_ptr_branch->execution_order]->disassembled_name
          % active_nearest_checkpoint.first->execution_order
          % order_ins_dynamic_map[active_nearest_checkpoint.first->execution_order]->disassembled_name;
   
    econed_ins_number += active_ptr_branch->econ_execution_length[active_nearest_checkpoint.first];
//     std::cout << econed_ins_number << "  " << executed_ins_number << std::endl;
    
    total_rollback_times++;
    local_rollback_times++;
    rollback_and_modify(active_nearest_checkpoint.first, active_nearest_checkpoint.second);
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
                                                          ptr_branch_t& examined_ptr_branch)
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
                                           ptr_branch_t& examined_ptr_branch)
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
//          % order_input_dep_ptr_branch_map.size()
        << "--------------------------------------------------------------------------------------";
        
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
                                             ptr_branch_t& examined_ptr_branch)
{
  // new taken found
  if (examined_ptr_branch->br_taken != br_taken) 
  {
    // active_ptr_branch is enabled, namely in some rollback or 
    // a re-execution from rollback_with_input_replacement
    if (active_ptr_branch) 
    {
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
        rollback_and_modify(active_nearest_checkpoint.first, 
                                                active_nearest_checkpoint.second);
      }
      else // in a re-execution from rollback_with_input_replacement
      {
        BOOST_LOG_SEV(log_instance, boost::log::trivial::fatal)
          << boost::format("the branch at %d takes a new decision in a clean re-execution") 
              % examined_ptr_branch->execution_order;
      }
    }
    else // active_ptr_branch is disabled, namely in forwarding
    {
      // but new taken found
      BOOST_LOG_SEV(log_instance, boost::log::trivial::fatal)
        << boost::format("the branch at %d takes a new decision in forwarding.") 
            % examined_ptr_branch->execution_order;

      PIN_ExitApplication(0);
    }
  }

  return;
}

/*================================================================================================*/

inline void log_input(ADDRINT ins_addr, bool br_taken)
{
  std::map<UINT32, ptr_branch_t>::iterator order_ptr_branch_iter;
  
  order_ptr_branch_iter = order_tainted_ptr_branch_map.find(current_execution_order);
  if (order_ptr_branch_iter != order_tainted_ptr_branch_map.end()) 
  {
    if (order_ptr_branch_iter->second->inputs[br_taken].empty()) 
    {
      store_input(order_ptr_branch_iter->second, br_taken);
    }
  }
  else 
  {
    BOOST_LOG_SEV(log_instance, boost::log::trivial::fatal)
      << boost::format("the branch at %d (%s: %s) cannot found")
          % current_execution_order
          % remove_leading_zeros(StringFromAddrint(ins_addr)) 
          % addr_ins_static_map[ins_addr]->disassembled_name;

    PIN_ExitApplication(0);
  }

  return;
}

/*====================================================================================================================*/

VOID resolving_cond_branch_analyzer(ADDRINT ins_addr, bool br_taken)
{
  log_input(ins_addr, br_taken);
  
  std::map<UINT32, ptr_branch_t>::iterator order_ptr_branch_iter;
  
  // search in the list of input dependent branches
  order_ptr_branch_iter = order_input_dep_ptr_branch_map.find(current_execution_order);
  if (order_ptr_branch_iter != order_input_dep_ptr_branch_map.end()) 
  {
    process_input_dependent_branch(ins_addr, br_taken, 
                                   order_ptr_branch_iter->second);
  }
  else 
  {
    // search in the list of input independent branches
    order_ptr_branch_iter = order_input_indep_ptr_branch_map.find(current_execution_order);
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
           % current_execution_order
           % remove_leading_zeros(StringFromAddrint(ins_addr))
           % addr_ins_static_map[ins_addr]->disassembled_name;

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
  if (order_ins_dynamic_map[current_execution_order + 1]->address != target_addr)
  {
    // active_ptr_branch is enabled, namely in some rollback
    if (active_ptr_branch) 
    {
//       econed_ins_number += active_ptr_branch->econ_execution_length[active_nearest_checkpoint.first];
//       std::cout << econed_ins_number << "  " << executed_ins_number << std::endl;
      
      // the original trace will lost if go further, so rollback
      total_rollback_times++;
      local_rollback_times++;
      rollback_and_modify(active_nearest_checkpoint.first, active_nearest_checkpoint.second);
    }
    else // active_ptr_branch is empty, namely in forwarding, but new target found
    {
      BOOST_LOG_SEV(log_instance, boost::log::trivial::fatal)
        << boost::format("the indirect branch at %d takes a different decision in forwarding")
           % current_execution_order;
      PIN_ExitApplication(0);
    }    
  }
  return;
}
