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

#include "trace_analyzer.h"

namespace instrumentation 
{

extern UINT32 received_message_number;


/**
 * @brief verify if the need message is received.
 * 
 * @return bool
 */
static inline bool is_needed_message_received()
{
  bool result = false;
  // by default, the first received message will be analyzed
  if (received_message_number == 1) 
  {
    result = true;
  }
  return result;
}


/**
 * @brief the function will be called immediately before the execution of an instruction.
 * 
 * @param instruction instrumented instruction
 * @param data not used
 * @return void
 */
void trace_analyzer::instrument_instruction_before(INS instruction, VOID* data)
{
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
void trace_analyzer::instrument_syscall_enter(THREADID thread_id, CONTEXT* context, 
                                              SYSCALL_STANDARD syscall_std, VOID* data)
{
  if (!is_needed_message_received()) 
  {
    
  }
  return;
}

}