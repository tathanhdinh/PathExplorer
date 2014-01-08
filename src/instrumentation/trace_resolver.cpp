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

#include "trace_resolver.h"
#include "../main.h"
#include <boost/log/trivial.hpp>
#include <boost/format.hpp>

namespace instrumentation 
{
  
typedef enum 
{
  original_execution = 0, // execution with the original input
  modified_execution = 1  // execution with some modified input
} execution_state;

static execution_state current_execution_state;
    
/**
 * @brief generic callback applied for all instructions, principally it is very similar to the 
 * "generic normal instruction" callback in the trace-analyzer class. But in the trace-resolving 
 * state, it does not have to handle the system call and vdso instructions (all of them do not 
 * exist in this state), moreover it does not have to log the executed instructions; so its 
 * semantics is much more simple.
 * 
 * @param instruction_address address of the instrumented instruction
 * @return void
 */
void trace_resolver::generic_instruction_callback(ADDRINT instruction_address)
{
  current_execution_order++;
  return;
}


/**
 * @brief callback for a conditional branch.
 * 
 * @param instruction_address address of the instrumented branch
 * @param is_branch_taken the branch will be taken or not
 * @return void
 */
void trace_resolver::cbranch_instruction_callback(bool is_branch_taken)
{
  return;
}


/**
 * @brief callback for an indirect branch or call, it exists only in the trace-resolving state 
 * because the re-execution trace must be kept to not go to a different target (than one in the 
 * execution trace logged in the trace-analyzing state).
 * 
 * @param instruction_address address of the instrumented instruction
 * @return void
 */
void trace_resolver::indirectBrOrCall_instruction_callback(ADDRINT target_address)
{
  // in x86-64 architecture, an indirect branch (or call) instruction is always unconditional so 
  // the target address is always the next executed instruction, let's verify that
  if (instruction_at_exeorder[current_execution_order + 1]->address != target_address) 
  {
    switch (current_execution_state) 
    {
      case original_execution:
        break;
        
      case modified_execution:
        break;
        
      default:
        
        break;
    }
  }
  
  return;
}

  
} // end of instrumentation namespace