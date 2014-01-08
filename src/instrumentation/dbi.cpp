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
#include "../main.h"
#include "trace_analyzer.h"
#include "trace_resolver.h"
#include "../analysis/instruction.h"
#include "../analysis/cbranch.h"
#include <boost/unordered_map.hpp>
#include <boost/log/trivial.hpp>
#include <boost/format.hpp>

namespace instrumentation 
{

using namespace analysis;

#define SYSCALL_SENDTO            44
#define SYSCALL_RECVFROM          45
#define MESSAGE_ADDRESS_ARGUMENT  1

static instrumentation_state current_instrumentation_state;
static bool required_message_received = false; 

/**
 * @brief change current running state.
 * 
 * @param new_state state to change
 * @return void
 */
void dbi::set_instrumentation_state(instrumentation_state new_state)
{
  current_instrumentation_state = new_state;
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
 * @brief get a copy of the original received message.
 * 
 * @return void
 */
static void copy_original_message()
{
  UINT32 idx;
  for (idx = 0; idx < received_message_length; ++idx) 
  {
    original_msg_at[idx + received_message_address] = 
      *(reinterpret_cast<UINT8*>(idx + received_message_address));
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
      current_instrumentation_state = trace_analyzing_state;
    }
    else 
    {
      copy_original_message();
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
  ptr_instruction_t curr_ptr_ins = instruction_at_address[curr_ins_addr];
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
      // generic callback for normal instruction
      INS_InsertPredicatedCall(curr_ins, IPOINT_BEFORE, 
                               (AFUNPTR)trace_analyzer::normal_instruction_callback,
                               IARG_INST_PTR, IARG_END);
  
      // update running time information for normal instructions, note that the first 3 callbacks 
      // below are mutually exclusive so they can be used separately
      if (curr_ptr_ins->is_cbranch) 
      {
        INS_InsertPredicatedCall(curr_ins, IPOINT_BEFORE, 
                                 (AFUNPTR)trace_analyzer::cbranch_instruction_callback,
                                 IARG_BRANCH_TAKEN, IARG_END);
      }
      
      // update the instruction's memory source operands
      if (curr_ptr_ins->is_memread) 
      {
        INS_InsertPredicatedCall(curr_ins, IPOINT_BEFORE, 
                                 (AFUNPTR)trace_analyzer::mread_instruction_callback,
                                 IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_END);
      }
      
      // update the instruction's memory target operands
      if (curr_ptr_ins->is_memwrite) 
      {
        INS_InsertPredicatedCall(curr_ins, IPOINT_BEFORE, 
                                 (AFUNPTR)trace_analyzer::mwrite_instruction_callback,
                                 IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE, IARG_END);
      }
      
      // propagate the running time information along the instruction's execution
      INS_InsertPredicatedCall(curr_ins, IPOINT_BEFORE, 
                               (AFUNPTR)trace_analyzer::dataflow_propagation_callback, IARG_END);
      
      
      // capture a callback whenever the instruction read some bytes of the input
      if (curr_ptr_ins->is_memread) 
      {
        INS_InsertPredicatedCall(curr_ins, IPOINT_BEFORE, 
                                 (AFUNPTR)trace_analyzer::checkpoint_storing_callback, 
                                 IARG_CONTEXT, IARG_END);
      }
    }
  }
  return;
}


/**
 * @brief handle an instruction in the trace resolving state, in this state the vdso and system 
 * call instructions will never be met because in the trace analyzing state, the execution has been 
 * stopped before meeting a such kind of instructions.
 * 
 * @param instruction handled instruction
 * @param data unused
 * @return void
 */
static void trace_resolving_state_handler(const INS& curr_ins, ADDRINT curr_ins_addr)
{
  // insert generic callback
  INS_InsertPredicatedCall(curr_ins, IPOINT_BEFORE, 
                           (AFUNPTR)trace_resolver::generic_instruction_callback, IARG_INST_PTR, 
                           IARG_END);
  
  // insert callbacks for conditional branch and indirect one, note that the following conditions 
  // are mutually exclusive
  ptr_instruction_t curr_ptr_ins = instruction_at_address[curr_ins_addr];
  if (curr_ptr_ins->is_cbranch)
  {
    INS_InsertPredicatedCall(curr_ins, IPOINT_BEFORE, 
                             (AFUNPTR)trace_resolver::cbranch_instruction_callback, IARG_INST_PTR, 
                             IARG_BRANCH_TAKEN, IARG_END);
  }
  
  if (curr_ptr_ins->is_indirectBrOrCall) 
  {
    INS_InsertPredicatedCall(curr_ins, IPOINT_BEFORE, 
                             (AFUNPTR)trace_resolver::indirectBrOrCall_instruction_callback, 
                             IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_END);
  }
  
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
void dbi::instrument_instruction_before(INS current_instruction, VOID* data)
{
  // create an instruction from the current analyzed PIN instruction
  ADDRINT current_address = INS_Address(current_instruction);
  ptr_instruction_t curr_ptr_ins(new instruction(current_instruction));
  
  // if it is a conditional branch 
  if (curr_ptr_ins->is_cbranch) 
  {
    // then copy it as a conditional branch
    instruction_at_address[current_address].reset(new cbranch(*curr_ptr_ins));
  }
  else 
  {
    // else copy it as a normal instruction
    instruction_at_address[current_address] = curr_ptr_ins;
  }
  
  // place handlers
  switch (current_instrumentation_state)
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
        << boost::format("instrumentation falls into a unknown running state %d") 
            % current_instrumentation_state;
      PIN_ExitApplication(current_instrumentation_state);
      break;
  }
  
  return;
}
  
}
