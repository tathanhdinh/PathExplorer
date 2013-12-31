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

namespace instrumentation 
{
  
extern UINT32 current_execution_order;
  
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
void trace_resolver::conditional_branch_callback(ADDRINT instruction_address, bool is_branch_taken)
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
void trace_resolver::indirect_branch_or_call_callback(ADDRINT instruction_address, 
                                                      ADDRINT target_address)
{
  return;
}

  
} // end of instrumentation namespace