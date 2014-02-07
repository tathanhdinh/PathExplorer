/*
 * <one line to give the program's name and a brief idea of what it does.>
 * Copyright (C) 2013  Ta Thanh Dinh <thanhdinh.ta@inria.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "resolver.h"
#include "../utilities/utils.h"
#include "../engine/fast_execution.h"
#include "../main.h"

#include <boost/unordered_map.hpp>
#include <boost/integer_traits.hpp>
#include <boost/log/trivial.hpp>
#include <boost/format.hpp>

namespace instrumentation 
{
  
using namespace utilities;  
using namespace engine;

static UINT32 pivot_checkpoint_execorder;
static UINT32 focused_cbranch_execorder;
static UINT32 local_reexec_number = 0;

typedef enum 
{
  backward = 0,
  forward  = 1,
  stop     = 2
} exec_direction_t;

/**
 * @brief Calculate the execution order of the first focused branch in this resolving phase.
 * This branch is the first one after the previous resolved branch (fixed in the analyzing phase) 
 * whose decision depends on the input.
 * 
 * @param previous_resolved_cbranch_execorder the execution order of the previous resolved branch 
 * fixed in the analyzing phase
 * @return void
 */
void resolver::set_first_focused_cbranch_execorder(UINT32 previous_resolved_cbranch_execorder)
{
  boost::unordered_map<UINT32, ptr_cbranch_t>::iterator cbranch_iter;
  UINT32 cbranch_execorder;
  
  focused_cbranch_execorder = boost::integer_traits<UINT32>::const_max;
  for (cbranch_iter = cbranch_at_execorder.begin();
       cbranch_iter != cbranch_at_execorder.end(); ++cbranch_iter)
  {
    cbranch_execorder = cbranch_iter->first;
    if ((cbranch_execorder > previous_resolved_cbranch_execorder) &&
        (!checkpoint_execorders_of_cbranch_at_execorder[cbranch_execorder].empty()) && 
        (cbranch_execorder < focused_cbranch_execorder))
    {
      focused_cbranch_execorder = cbranch_execorder;
    }
  }
  
  return;
}


/**
 * @brief Generic callback applied for all instructions. 
 * Principally it is very similar to the "generic normal instruction" callback in the trace-analyzer 
 * class. But in the trace-resolving state, it does not have to handle the system call and vdso 
 * instructions (all of them do not exist in this state), moreover it does not have to log the 
 * executed instructions; so its semantics is much more simple.
 * 
 * @param instruction_address address of the instrumented instruction
 * @return void
 */
void resolver::generic_instruction_callback(ADDRINT instruction_address)
{
  current_execorder++;
  // fast execution
  if ((pivot_checkpoint_execorder != 0) && 
      (checkpoint_at_execorder[pivot_checkpoint_execorder]->jumping_point == current_execorder)) 
  {
    //
  }
  
  // debug enabled
  if (debug_enabled) 
  {
    if (instruction_at_execorder[current_execorder]->address != instruction_address) 
    {
       BOOST_LOG_TRIVIAL(fatal) 
        << boost::format("meet a wrong instruction at address %s and at execution order %d") 
            % utils::addrint2hexstring(instruction_address) % current_execorder;
        PIN_ExitApplication(0);
    }
  }
  
  return;
}


/**
 * @brief Callback applied for a conditional branch. 
 * The semantics of this function is quite sophisticated because each examined branch can fall into 
 * one of 36 different states combined from 4 categories:
 *  1. resolving (resolved, bypassed, neither resolved nor bypassed):                    3 values
 *  2. re-execution number in comparison with the max value N (< N-1, = N-1, > N-1):     3 values
 *  3. examined branch is the focused one (yes, no):                                     2 values
 *  4. new branch taken (yes, no):                                                       2 values 
 * so the total is 3 x 3 x 2 x 2 = 36 states. The mean idea to treat with this sophistication is to 
 * imagine that the examination, goes along with the execution, and when it meets a conditional 
 * branch then it can make this branch take a new decision or keep the old decision, depending on 
 * the current status of the examined branch, the execution will continue or back to a previous 
 * checkpoint. So that leads to a "functional" view as follows:
 * 
 * (new decision is taken or not) {0,1} ------> current status -------> {0,1} (continue or back)
 *
 * We devise the callback into several smaller functions:  
 */
static exec_direction_t unfocused_newtaken_branch_handler (ptr_cbranch_t examined_cbranch);
static exec_direction_t focused_newtaken_branch_handler   (ptr_cbranch_t examined_cbranch);
static exec_direction_t unfocused_oldtaken_branch_handler (ptr_cbranch_t examined_branch);
static exec_direction_t focused_oldtaken_branch_handler   (ptr_cbranch_t examined_branch);
/**
 * @param is_branch_taken the branch will be taken or not
 * @return void
 */
void resolver::cbranch_instruction_callback(bool is_branch_taken)
{
  exec_direction_t exec_direction;
  ptr_cbranch_t curr_examined_cbranch;
  
  // the examined branch takes a different decision (category 4)
  if (curr_examined_cbranch->is_taken != is_branch_taken) 
  {
    // the examined branch is the focused one (category 3)
    if (current_execorder == focused_cbranch_execorder) 
    {
      exec_direction = focused_newtaken_branch_handler(curr_examined_cbranch); 
    }
    else // the examined branch is not the focused one (category 3)
    {
      exec_direction = unfocused_newtaken_branch_handler(curr_examined_cbranch);
    }
  }
  else // the examined branch keeps the old decision (category 4)
  {
    // the examined branch is the focused one (category 3)
    if (current_execorder == focused_cbranch_execorder) 
    {
      exec_direction = focused_oldtaken_branch_handler(curr_examined_cbranch);
    }
    else // the examined branch is not the focused one (category 3)
    {
      exec_direction = unfocused_oldtaken_branch_handler(curr_examined_cbranch);
    }
  }
  
  // the execution will back or continue depending on the direction and the current value of the 
  // local re-execution number
  switch (exec_direction) 
  {
    case backward:
      ++total_reexec_times;
      ++local_reexec_number;
      if (local_reexec_number < max_local_reexec_number) 
      {
        checkpoint_at_execorder[pivot_checkpoint_execorder]->modify_input();
      }
      else // that means local_reexec_number == max_local_reexec_number 
      {
        checkpoint_at_execorder[pivot_checkpoint_execorder]->restore_input();
      }
      fast_execution::move_backward(pivot_checkpoint_execorder);
      break;
      
    case forward:
      // move forward is the default
      break;
      
    case stop:
      BOOST_LOG_TRIVIAL(info) 
        << boost::format("there is no more branch to resolve, stop at execution order %d") 
            % current_execorder;
      break;
      
    default:
      break;
  }
  
  return;
}


/**
 * @brief Handle the case where the examined branch is unfocused and a new decision is taken. 
 * The situation is as follows: in re-executing, a conditional branch is met, the current value of 
 * input makes the branch take a different decision.
 * 
 * @param examined_branch the examined branch
 * @return exec_direction_t
 */
static inline exec_direction_t unfocused_newtaken_branch_handler(ptr_cbranch_t examined_branch) 
{
  // if the examined branch is not resolved yet
  if (!examined_branch->is_resolved) 
  {
    // then set it as resolved
    examined_branch->is_resolved = true; examined_branch->is_bypassed = false;
    // and save the current input
    examined_branch->save_current_input(!examined_branch->is_taken);
  }
  
  // because the branch will take a different target if the execution continue, so that implicitly 
  // means that the local_reexec_number is less than max_local_reexec_number, we increase the local 
  // execution number and back.
  return backward;
}


/**
 * @brief Handle the case where the examined branch is focused and a new decision is taken.
 * The situation is as follows: in re-executing, the branch needed to resolve is met, the current 
 * value of input makes the branch take a different decision.
 * 
 * @param examined_branch the examined branch
 * @return exec_direction_t
 */
static inline exec_direction_t focused_newtaken_branch_handler(ptr_cbranch_t examined_branch)
{
  // set it as resolved
  examined_branch->is_resolved = true;
  // and save the current input
  examined_branch->save_current_input(!examined_branch->is_taken);
  
  // because the branch will take a different target if the execution continue, so that implicitly 
  // means that the local_reexec_number is less than max_local_reexec_number, we increase the local 
  // execution number and back.  
  return backward;
}


/**
 * @brief Handle the case where the examined branch is unfocused and the current value of the input 
 * keeps the decision of the branch.
 * 
 * @param examined_branch the examined branch
 * @return exec_direction_t
 */
static inline exec_direction_t unfocused_oldtaken_branch_handler(ptr_cbranch_t examined_branch)
{
  // just forward
  return forward;
}


/**
 * @brief Handle the case where the examined branch is focused and the current value of the input 
 * keeps the decision of the branch.
 * 
 * @param examined_branch the examined branch
 * @return exec_direction_t
 */
static UINT32 next_checkpoint_execorder();
static UINT32 next_focused_cbranch_execorder();
inline static exec_direction_t focused_oldtaken_branch_handler(ptr_cbranch_t examined_branch)
{
  exec_direction_t exec_direction;
  
  if (local_reexec_number < max_local_reexec_number - 1) 
  {
    // back again
    exec_direction = backward;
  }
  else 
  {
    if (local_reexec_number == max_local_reexec_number - 1) 
    {
      // set the examined branch as bypassed and back to the original trace
      examined_branch->is_bypassed = true; exec_direction = backward;
    }
    else // that means local_reexec_number == max_local_reexec_number
    {
      UINT32 chkpnt_execorder = next_checkpoint_execorder();
      // if the next checkpoint exists
      if (chkpnt_execorder != 0) 
      {
        // then back to the next checkpoint
        pivot_checkpoint_execorder = chkpnt_execorder; local_reexec_number = 0;
        exec_direction = backward;        
      }
      else // the next checkpoint does not exist
      {
        UINT32 cbranch_execorder = next_focused_cbranch_execorder();
        // if there exist some conditional branch to resolve
        if (cbranch_execorder != boost::integer_traits<UINT32>::const_max) 
        {
          // then continue executing
          focused_cbranch_execorder = cbranch_execorder; local_reexec_number = 0;
          exec_direction = forward;
        }
        else 
        {
          // else stop
          exec_direction = stop;
        }
      }
    }
  }
  
  return exec_direction;
}


/**
 * @brief Get the execution order of the next checkpoint (with respect to the pivot checkpoint) in 
 * the branch's checkpoint list.
 * A branch's decision may depend on the several computations each starts from some checkpoint. In 
 * the "trace analyzing state", the list of checkpoints for each branch has been computed, so the 
 * execution will be restarted from each checkpoint in this list, together with the input 
 * modification until the branch changes its decision or the re-execution number for the checkpoint 
 * exceeds its bounded value. In this case, the next checkpoint (in this list) will be extracted 
 * and the execution will restart from this checkpoint. 
 * 
 * @param examined_branch the examined branch
 * @return UINT32
 */
inline static UINT32 next_checkpoint_execorder()
{
  UINT32 nearest_chkpnt_execorder = 0;
  exeorders_t::iterator chkpt_iter;
  for (chkpt_iter = checkpoint_execorders_of_cbranch_at_execorder[current_execorder].begin(); 
       chkpt_iter != checkpoint_execorders_of_cbranch_at_execorder[current_execorder].end(); ++chkpt_iter) 
  {
    if ((*chkpt_iter < pivot_checkpoint_execorder) && (nearest_chkpnt_execorder < *chkpt_iter))
    {
      nearest_chkpnt_execorder = *chkpt_iter;
    }
  }
  
  return nearest_chkpnt_execorder;
}


/**
 * @brief Get the execution order of the next conditional branch needed to resolve.
 * 
 * @return UINT32
 */
inline static UINT32 next_focused_cbranch_execorder()
{
  UINT32 nearest_cbranch_execorder = boost::integer_traits<UINT32>::const_max;
  UINT32 cbranch_execorder;
  boost::unordered_map<UINT32, ptr_cbranch_t>::iterator cbranch_iter;
  for (cbranch_iter = cbranch_at_execorder.begin(); 
       cbranch_iter != cbranch_at_execorder.end(); ++cbranch_iter)
  {
    cbranch_execorder = cbranch_iter->first;
    if ((cbranch_execorder > current_execorder) && 
        (nearest_cbranch_execorder > cbranch_execorder) &&
        !checkpoint_execorders_of_cbranch_at_execorder[cbranch_execorder].empty())
    {
      nearest_cbranch_execorder = cbranch_execorder;
    }
  }
  
  return nearest_cbranch_execorder;
}


/**
 * @brief Callback applied for an indirect branch or call. 
 * It exists only in the trace-resolving state because the re-execution trace must be kept to not 
 * go to a different target (than one in the execution trace logged in the trace-analyzing state).
 * 
 * @param instruction_address address of the instrumented instruction
 * @return void
 */
void resolver::indirectBrOrCall_instruction_callback(ADDRINT target_address)
{
  // in x86-64 architecture, an indirect branch (or call) instruction is always unconditional so 
  // the target address must be the next executed instruction, let's verify that
  if (instruction_at_execorder[current_execorder + 1]->address != target_address) 
  {
    ++local_reexec_number;
    if (local_reexec_number < max_local_reexec_number) 
    {
      // modify the input to try to pass this branch
      checkpoint_at_execorder[pivot_checkpoint_execorder]->modify_input();
    }
    else // that means local_reexec_number == max_local_reexec_number 
    {
      // restore the input to back to the original trace
      checkpoint_at_execorder[pivot_checkpoint_execorder]->restore_input();
    }
    // and back
    fast_execution::move_backward(pivot_checkpoint_execorder);
  }
  
  return;
}

  
} // end of instrumentation namespace
