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

#include "dbi.h"
#include "trace_analyzer.h"
#include <boost/log/trivial.hpp>
#include <boost/format.hpp>

namespace instrumentation 
{

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
 * @brief the function will be called immediately before the execution of a system call.
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
 * @brief the function will be called immediately after the execution of a system call.
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
 * are not executed yet.
 * 
 * @param instruction handled instruction
 * @param data unused
 * @return void
 */
static void trace_analyzing_state_handler(INS instruction, VOID* data)
{
  if (INS_IsSyscall(instruction)) 
  {
    INS_InsertPredicatedCall(instruction, IPOINT_BEFORE, 
                             (AFUNPTR)trace_analyzer::syscall_instruction_callback, IARG_INST_PTR, 
                             IARG_END);
  }
  else 
  {
    //
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
static void trace_resolving_state_handler(INS instruction, VOID* data)
{
  return;
}


/**
 * @brief the function will be called immediately before the execution of an instruction.
 * 
 * @param instruction instrumented instruction
 * @param data not used
 * @return void
 */
void dbi::instrument_instruction_before(INS instruction, VOID* data)
{
  switch (current_running_state)
  {
    case message_receiving_state:
      // currently do nothing
      break;
      
    case trace_analyzing_state:
      trace_analyzing_state_handler(instruction, data);
      break;
      
    case trace_resolving_state:
      trace_resolving_state_handler(instruction, data);
      break;
      
    default:
      BOOST_LOG_TRIVIAL(fatal) 
        << boost::format("instrumentation falls into a unknown running state %d") % current_running_state;
      PIN_ExitApplication(current_running_state);
      break;
  }
  
  return;
}
  
}
