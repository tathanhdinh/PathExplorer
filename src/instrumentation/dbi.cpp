/*
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

#include "dbi.h"
#include "trace_analyzer.h"
#include "../analysis/instruction.h"
#include "../analysis/conditional_branch.h"
#include <boost/unordered_map.hpp>
#include <boost/log/trivial.hpp>
#include <boost/format.hpp>

namespace instrumentation 
{

using namespace analysis;
extern boost::unordered_map<ADDRINT, ptr_instruction_t> address_instruction_map;
  
extern ADDRINT received_message_address;
extern INT32   received_message_length;

static running_state current_running_state; 

#define SYSCALL_SENDTO            44
#define SYSCALL_RECVFROM          45
#define MESSAGE_ADDRESS_ARGUMENT  1

static bool required_message_received = false; 

/**
 * @brief change current running state.
 * 
 * @param new_state state to change
 * @return void
 */
void dbi::change_running_state(running_state new_state)
{
  current_running_state = new_state;
  return;
}


/**
 * @brief the function is placed to be called immediately before the execution of a system call.
 * 
 * @param thread_id ID of the thread calling the system call
 * @param context cpu context
 * @param syscall_std system calling standard
 * @param data unused
 * @return void
 */
void dbi::instrument_syscall_enter(THREADID thread_id, CONTEXT* context, 
                                   SYSCALL_STANDARD syscall_std, VOID* data)
{
  if (!required_message_received) 
  {
    if (PIN_GetSyscallNumber(context, syscall_std) == SYSCALL_RECVFROM) 
    {
      // this PIN function is effective only when it is called before the execution of the syscall 
      received_message_address = PIN_GetSyscallArgument(context, syscall_std, 
                                                        MESSAGE_ADDRESS_ARGUMENT);
      required_message_received = true;
    }
  }
  
  return;
}


/**
 * @brief the function is placed to be called immediately after the execution of a system call.
 * 
 * @param thread_id ID of the thread calling the system call
 * @param context cpu context
 * @param syscall_std system calling standard
 * @param data unused
 * @return void
 */
void dbi::instrument_syscall_exit(THREADID thread_id, CONTEXT* context, 
                                  SYSCALL_STANDARD syscall_std, VOID* data)
{
  if (!required_message_received) 
  {
    // this PIN function is effective only when it is called after the execution of the syscall
    received_message_length = PIN_GetSyscallReturn(context, syscall_std);
    if (received_message_length <= 0) 
    {
      required_message_received = false;
      current_running_state = trace_analyzing_state;
    }
  }
  return;
}


/**
 * @brief statically analyze the instrumented program to put different callbacks for each type of 
 * instruction. Note that the handler will be called in "loading time", i.e. when the instructions
 * are not executed yet. The complexity of the trace analyzing is O(trace length).
 * 
 * @param instruction handled instruction
 * @param data unused
 * @return void
 */
static void trace_analyzing_state_handler(const INS& curr_ins, ADDRINT curr_ins_addr)
{
  ptr_instruction_t curr_ptr_ins = address_instruction_map[curr_ins_addr];
  if (curr_ptr_ins->is_syscall)
  {
    INS_InsertPredicatedCall(curr_ins, IPOINT_BEFORE, 
                             (AFUNPTR)trace_analyzer::syscall_instruction_callback, IARG_INST_PTR, 
                             IARG_END);
  }
  else 
  {
    if (curr_ptr_ins->is_vdso) 
    {
      INS_InsertPredicatedCall(curr_ins, IPOINT_BEFORE, 
                               (AFUNPTR)trace_analyzer::vdso_instruction_callback, IARG_INST_PTR, 
                               IARG_END);
    }
    else 
    {
      // update running time information for instructions
      // the first 3 condition below are mutually exclusive so they can be used separately
      if (curr_ptr_ins->is_conditional_branch) 
      {
        INS_InsertPredicatedCall(curr_ins, IPOINT_BEFORE, 
                                 (AFUNPTR)trace_analyzer::conditional_branch_callback, 
                                 IARG_INST_PTR, IARG_BRANCH_TAKEN, IARG_END);
      }
      
      if (curr_ptr_ins->is_memory_read) 
      {
        INS_InsertPredicatedCall(curr_ins, IPOINT_BEFORE, 
                                 (AFUNPTR)trace_analyzer::memory_read_instruction_callback, 
                                 IARG_INST_PTR, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_END);
      }
      
      if (curr_ptr_ins->is_memory_write) 
      {
        INS_InsertPredicatedCall(curr_ins, IPOINT_BEFORE, 
                                 (AFUNPTR)trace_analyzer::memory_write_instruction_callback, 
                                 IARG_INST_PTR, IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE, IARG_END);
      }
      
      // propagate the running time information along the execution
      INS_InsertPredicatedCall(curr_ins, IPOINT_BEFORE, 
                               (AFUNPTR)trace_analyzer::dataflow_propagation_along_instruction_callback, 
                               IARG_INST_PTR, IARG_END);
      
    }
  }
  return;
}


/**
 * @brief handle an instruction in the trace resolving state.
 * 
 * @param instruction handled instruction
 * @param data unused
 * @return void
 */
static void trace_resolving_state_handler(const INS& curr_ins, ADDRINT curr_ins_addr)
{
  return;
}


/**
 * @brief the function is placed to be called before the execution of an instruction. Note that the 
 * placement is realized in "loading time", i.e. before the execution of instructions.
 * 
 * @param instruction instrumented instruction
 * @param data not used
 * @return void
 */
void dbi::instrument_instruction_before(const INS& current_instruction, VOID* data)
{
  // create an instruction from the current analyzed PIN instruction
  ADDRINT current_address = INS_Address(current_instruction);
  ptr_instruction_t curr_ptr_ins(new instruction(current_instruction));
  
  // if it is a conditional branch 
  if (curr_ptr_ins->is_conditional_branch) 
  {
    // then copy it as a conditional branch
    address_instruction_map[current_address].reset(new conditional_branch(*curr_ptr_ins));
  }
  else 
  {
    // else copy it as a normal instruction
    address_instruction_map[current_address] = curr_ptr_ins;
  }
  
  // place handlers
  switch (current_running_state)
  {
    case message_receiving_state:
      // currently do nothing
      break;
      
    case trace_analyzing_state:
      trace_analyzing_state_handler(current_instruction, current_address);
      break;
      
    case trace_resolving_state:
      trace_resolving_state_handler(current_instruction, current_address);
      break;
      
    default:
      BOOST_LOG_TRIVIAL(fatal) 
        << boost::format("instrumentation is currently falls into a unknown running state %d") 
            % current_running_state;
      PIN_ExitApplication(current_running_state);
      break;
  }
  
  return;
}
  
}
