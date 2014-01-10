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
#include <boost/log/trivial.hpp>
#include <boost/format.hpp>

namespace instrumentation 
{
  
using namespace utilities;  
using namespace engine;

static UINT32 focused_checkpoint_execorder;
static UINT32 focused_cbranch_execorder;
static UINT32 local_reexec_number = 0;
static resolving_state current_resolving_state = execution_with_orig_input;

/**
 * @brief set a new resolving state.
 * 
 * @param new_resolving_state 
 * @return void
 */
void resolver::set_resolving_state(resolving_state new_resolving_state)
{
  current_resolving_state = new_resolving_state;
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
  if (instruction_at_execorder[current_execorder]->address == instruction_address)
  {
    // better performance because of branch prediction ?!!
    current_execorder++;
  }
  else 
  {
    BOOST_LOG_TRIVIAL(fatal) 
      << boost::format("in trace resolving state: meet a wrong instruction at %s after %d instruction executed.") 
          % utils::addrint2hexstring(instruction_address) % current_execorder;
    PIN_ExitApplication(current_resolving_state);
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
 * checkpoint.
 * 
 * (new decision is taken or not) {0,1} ------> current status -------> {0,1} (continue or back)
 *
 * We devise the callback into several smaller functions:  
 */
static void unfocused_newtaken_branch_handler (ptr_cbranch_t examined_cbranch);
static void focused_newtaken_branch_handler   (ptr_cbranch_t examined_cbranch);
static void unfocused_oldtaken_branch_handler (ptr_cbranch_t examined_branch);
static void focused_oldtaken_branch_handler   (ptr_cbranch_t examined_branch);
/**
 * @param is_branch_taken the branch will be taken or not
 * @return void
 */
void resolver::cbranch_instruction_callback(bool is_branch_taken)
{
  // verify if the current examined instruction is branch
  if (instruction_at_execorder[current_execorder]->is_cbranch) 
  {
    ptr_cbranch_t curr_examined_cbranch;
    // the examined branch takes a different decision (category 4)
    if (curr_examined_cbranch->is_taken != is_branch_taken) 
    {
      // the examined branch is the focused one (category 3)
      if (current_execorder == focused_cbranch_execorder) 
      {
        focused_newtaken_branch_handler(curr_examined_cbranch); 
      }
      else // the examined branch is not the focused one (category 3)
      {
        unfocused_newtaken_branch_handler(curr_examined_cbranch);
      }
    }
    else // the examined branch keeps the old decision (category 4)
    {
      // the examined branch is the focused one (category 3)
      if (current_execorder == focused_cbranch_execorder) 
      {
        focused_oldtaken_branch_handler(curr_examined_cbranch);
      }
      else // the examined branch is not the focused one (category 3)
      {
        unfocused_oldtaken_branch_handler(curr_examined_cbranch);
      }
    }
  }
  else 
  {
    BOOST_LOG_TRIVIAL(fatal) 
      << boost::format("in trace resolving state: meet a wrong branch at execution order %d") 
          % current_execorder;
  }
  return;
}


/**
 * @brief Handle the case where the examined branch is un-focused and a new decision is taken. 
 * The situation is as follows: in re-executing, a conditional branch is met, the current value of 
 * input makes the branch take a different decision.
 * 
 * @param examined_branch the met branch
 * @return void
 */
inline static void unfocused_newtaken_branch_handler(ptr_cbranch_t examined_branch) 
{
  // if the examined branch is not resolved yet
  if (!examined_branch->is_resolved) 
  {
    // then set it as resolved
    examined_branch->is_resolved = true;
    examined_branch->is_bypassed = false;
    // and save the current input
    examined_branch->save_current_input(!examined_branch->is_taken);
  }
  
  // because the branch will take a different target if the execution continue, so that implicitly 
  // means that the local_reexec_number is less than max_local_reexec_number, we increase the local 
  // execution number and back.
  local_reexec_number++;
  if (local_reexec_number < max_local_reexec_number) 
  {
    fast_execution::move_backward_and_modify_input(focused_checkpoint_execorder);
  }
  else // must be local_reexec_number == max_local_reexec_number
  {
    fast_execution::move_backward_and_restore_input(focused_checkpoint_execorder);
  }
  
  return;
}


/**
 * @brief Handle the case where the examined is focused and a new decision is taken.
 * The situation is as follows: in re-executing, the branch needed to resolve is met, the current 
 * value of input makes the branch take a different decision.
 * 
 * @param examined_branch the met branch
 * @return void
 */
inline static void focused_newtaken_branch_handler(ptr_cbranch_t examined_branch)
{
  // set it as resolved
  examined_branch->is_resolved = true;
  // and save the current input
  examined_branch->save_current_input(!examined_branch->is_taken);
  
  // because the branch will take a different target if the execution continue, so that implicitly 
  // means that the local_reexec_number is less than max_local_reexec_number, we increase the local 
  // execution number and back.
  local_reexec_number++;
  if (local_reexec_number < max_local_reexec_number) 
  {
    fast_execution::move_backward_and_modify_input(focused_checkpoint_execorder);
  }
  else 
  {
    fast_execution::move_backward_and_restore_input(focused_checkpoint_execorder);
  }
  
  return;
}

inline static void unfocused_oldtaken_branch_handler(ptr_cbranch_t examined_branch)
{
  return;
}

inline static void focused_oldtaken_branch_handler(ptr_cbranch_t examined_branch)
{
  return;
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
    switch (current_resolving_state)
    {
      case execution_with_orig_input:
        BOOST_LOG_TRIVIAL(fatal) 
          << boost::format("indirect branch at %d will take new target in executing with the original input.")
              % current_execorder;
        PIN_ExitApplication(current_resolving_state);
        break;
        
      case execution_with_modif_input:
        fast_execution::move_backward_and_modify_input(focused_checkpoint_execorder);
        break;
        
      default:
        BOOST_LOG_TRIVIAL(fatal)
          << boost::format("trace resolver falls into a unknown running state %d")
              % current_resolving_state;
        PIN_ExitApplication(current_resolving_state);
        break;
    }
  }
  
  return;
}

  
} // end of instrumentation namespace
