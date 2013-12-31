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

#include "trace_analyzer.h"
#include "../analysis/instruction.h"
#include "../analysis/conditional_branch.h"
#include "../engine/checkpoint.h"
#include "../analysis/dataflow.h"
#include <algorithm>
#include <boost/log/trivial.hpp>
#include <boost/format.hpp>
#include <boost/unordered_map.hpp>

namespace instrumentation 
{

using namespace analysis;
using namespace engine;
  
extern ADDRINT received_message_address;
extern INT32 received_message_length;
extern UINT32 current_execution_order;
extern UINT32 execution_trace_max_length;
extern boost::unordered_map<UINT32, ptr_instruction_t> execution_order_instruction_map;
extern boost::unordered_map<ADDRINT, ptr_instruction_t> address_instruction_map;
extern boost::unordered_map<UINT32, ptr_checkpoint_t> execution_order_checkpoint_map;


/**
 * @brief called in the trace-analyzing state in the following cases:
 *   1. the analyzed instruction is a system call
 *   2. the analyzed instruction is mapped from the kernel space
 *   3. the number of analyzed instructions has exceeded its bound value
 * to switch to the trace-resolving state.
 * 
 * @return void
 */
static void switch_to_trace_resolving_state()
{
  dataflow::extract_inputs_instructions_dependance_maps();
  PIN_RemoveInstrumentation();
  return;
}
  
/**
 * @brief when a system call instruction is met in the trace-analyzing state, stop (without 
 * executing this instruction) executing more instruction, and prepare to analyze executed trace. 
 * Note that the callback functions are invoked in running time.
 * 
 * @param instruction_address address of the instrumented instruction.
 * @return void
 */
void trace_analyzer::syscall_instruction_callback(ADDRINT instruction_address)
{
  BOOST_LOG_TRIVIAL(warning) 
    << boost::format("meet a system call after %d executed instruction.") 
        % current_execution_order;
        
  // stop the trace-analyzing state and go into the trace-resolving state
  switch_to_trace_resolving_state();
  return;
}


/**
 * @brief a vdso instruction is mapped from the kernel space so its accessed information may not in 
 * the user space, stop (without executing this instruction) executing more instruction, and prepare 
 * to analyze executed trace.
 * 
 * @param instruction_address address of the instrumented instruction
 * @return void
 */
void trace_analyzer::vdso_instruction_callback(ADDRINT instruction_address)
{
  BOOST_LOG_TRIVIAL(warning) 
    << boost::format("meet a vdso instruction after %d executed instruction.") 
        % current_execution_order;
   
  // stop the trace-analyzing state and go into the trace-resolving state
  switch_to_trace_resolving_state();
  return;
}


/**
 * @brief as it named, this is the generic callback applied for a normal instruction (i.e. 
 * neither a system call nor a vdso).
 * 
 * @param instruction_address address of the instrumented instruction
 * @return void
 */
void trace_analyzer::generic_normal_instruction_callback(ADDRINT instruction_address)
{
  if (current_execution_order < execution_trace_max_length) 
  {
    // log the instruction
    current_execution_order++;
    ptr_instruction_t curr_ins = address_instruction_map[instruction_address];
    if (curr_ins->is_conditional_branch) 
    {
      // using copy constructor (faster) instead of instructor from a PIN instruction
      execution_order_instruction_map[current_execution_order].reset(new conditional_branch(*curr_ins));
    }
    else 
    {
      // using copy constructor (faster) instead of instructor from a PIN instruction
      execution_order_instruction_map[current_execution_order].reset(new instruction(*curr_ins));
    }
    
  }
  else 
  {
    // stop the trace-analyzing state and go into the trace-resolving state
    switch_to_trace_resolving_state();
  }
}


/**
 * @brief callback for a conditional branch. Note that the parameter instruction address is actually 
 * not necessary because of using the running-time information "current execution order".
 * 
 * @param instruction_address address of the instrumented conditional branch
 * @param is_branch_taken the branch will be taken or not
 * @return void
 */
void trace_analyzer::conditional_branch_callback(ADDRINT instruction_address, bool is_branch_taken)
{
  static_cast<conditional_branch*>(
    execution_order_instruction_map[current_execution_order].get())->is_taken = is_branch_taken;
  return;
}


/**
 * @brief callback for a memory read instruction, beside the address/size of memory, the cpu context 
 * is passed because a checkpoint need to be stored when the instruction read someinput-related 
 * addresses.
 * 
 * @param instruction_address address of the instrumented instruction
 * @param memory_read_address beginning of the read address
 * @param memory_read_size size of the read address
 * @param cpu_context cpu context
 * @return void
 */
void trace_analyzer::memory_read_instruction_callback(ADDRINT instruction_address, 
                                                      ADDRINT memory_read_address, 
                                                      UINT32 memory_read_size, 
                                                      CONTEXT* cpu_context)
{
  // update dynamic information: read memory addresses
  ptr_instruction_t curr_ins = execution_order_instruction_map[current_execution_order];
  curr_ins->update_memory(memory_read_address, memory_read_size, MEMORY_READ);
  
  // if the read memory address range has an intersection with the input buffer,
  if (std::max(memory_read_address, received_message_address) < 
      std::min(memory_read_address + memory_read_size, 
               received_message_address + received_message_length))
  {
    // namely the instruction read some byte of the input, then take a checkpoint
    execution_order_checkpoint_map[current_execution_order].reset(
      new checkpoint(current_execution_order, cpu_context));
  }
  
  return;
}


/**
 * @brief callback for a memory write instruction.
 * 
 * @param instruction_address address of the instrumented instruction
 * @param memory_written_address beginning of the written address
 * @param memory_written_size size of the written address
 * @return void
 */
void trace_analyzer::memory_write_instruction_callback(ADDRINT instruction_address, 
                                                       ADDRINT memory_written_address, 
                                                       UINT32 memory_written_size)
{
  // update dynamic information: written memory addresses
  ptr_instruction_t curr_ins = execution_order_instruction_map[current_execution_order];
  curr_ins->update_memory(memory_written_address, memory_written_size, MEMORY_WRITE);
  return;
}


/**
 * @brief callback for propagating dynamic information along the execution of an instruction, the 
 * parameter "instruction address" is actually not necessary because of using the running-time 
 * information "current execution order". Note that the callback has an important side-effect: it 
 * modifies the map "original value at address" which will be used in storing checkpoints.
 * 
 * @param instruction_address address of the instrumented instruction
 * @return void
 */
void trace_analyzer::dataflow_propagating_callback(ADDRINT instruction_address)
{
  dataflow::propagate_along_instruction(current_execution_order);
  return;
}

} // end of instrumentation namespace