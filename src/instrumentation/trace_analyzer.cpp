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
#include "../main.h"
#include "../engine/checkpoint.h"
#include "../analysis/dataflow.h"
#include "../instrumentation/dbi.h"
#include "../utilities/utils.h"
#include <boost/log/trivial.hpp>
#include <boost/format.hpp>
#include <boost/unordered_map.hpp>

namespace instrumentation 
{

using namespace analysis;
using namespace engine;
using namespace utilities;

/**
 * @brief verify if the current analyzed trace has some branches needed to resolve.
 * 
 * @return bool
 */
// static bool have_branches_to_resolve()
// {
//   return true;
// }

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
//   dataflow::extract_inputs_instructions_dependance_maps();
  dbi::set_instrumentation_state(trace_resolving_state);
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
    << boost::format("meet a system call at %s after %d instructions executed.") 
        % utils::addrint2hexstring(instruction_address) % current_execution_order;
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
    << boost::format("meet a vdso instruction at %s after %d instructions executed.") 
        % utils::addrint2hexstring(instruction_address) % current_execution_order;
  return;
}


/**
 * @brief as it named, this is the generic callback applied for a normal instruction (i.e. 
 * neither a system call nor a vdso).
 * 
 * @param instruction_address address of the instrumented instruction
 * @return void
 */
void trace_analyzer::normal_instruction_callback(ADDRINT instruction_address)
{
  if (current_execution_order < execution_trace_max_length) 
  {
    // log the instruction
    current_execution_order++;
    // first get its static information
    ptr_instruction_t curr_ins = instruction_at_address[instruction_address];
    // then duplicate it using copy constructor instead of constructing from a PIN instruction 
    if (curr_ins->is_cbranch) 
    {
      cbranch* curr_branch = new cbranch(*curr_ins);
      instruction_at_execorder[current_execution_order].reset(curr_branch);
      branch_at_exeorder[current_execution_order].reset(curr_branch);
    }
    else 
    {
      instruction_at_execorder[current_execution_order].reset(new instruction(*curr_ins));
    }
    
  }
  else 
  {
    // stop the trace-analyzing state and go into the trace-resolving state
    switch_to_trace_resolving_state();
  }
}


/**
 * @brief callback for a conditional branch.
 * 
 * @param instruction_address address of the instrumented conditional branch
 * @param is_branch_taken the branch will be taken or not
 * @return void
 */
void trace_analyzer::cbranch_instruction_callback(bool is_branch_taken)
{
  // update dynamic information: the branch is taken or not
  static_cast<cbranch*>(
    instruction_at_execorder[current_execution_order].get())->is_taken = is_branch_taken;
  return;
}


/**
 * @brief callback for a memory read instruction, beside the address/size of memory, the cpu 
 * context is passed because a checkpoint need to be stored when the instruction read some 
 * addresses in the input buffer.
 * 
 * @param instruction_address address of the instrumented instruction
 * @param memory_read_address beginning of the read address
 * @param memory_read_size size of the read address
 * @param cpu_context cpu context
 * @return void
 */
void trace_analyzer::mread_instruction_callback(ADDRINT memory_read_address,
                                                UINT32 memory_read_size)
{
  // update dynamic information: read memory addresses
  ptr_instruction_t curr_ins = instruction_at_execorder[current_execution_order];
  curr_ins->update_memory_access_info(memory_read_address, memory_read_size, MEMORY_READ);  
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
void trace_analyzer::mwrite_instruction_callback(ADDRINT memory_written_address,
                                                 UINT32 memory_written_size)
{
  // update dynamic information: written memory addresses
  ptr_instruction_t curr_ins = instruction_at_execorder[current_execution_order];
  curr_ins->update_memory_access_info(memory_written_address, memory_written_size, MEMORY_WRITE);
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
void trace_analyzer::dataflow_propagation_callback()
{
  dataflow::propagate_along_instruction(current_execution_order);
  return;
}


/**
 * @brief callback for storing checkpoint.
 * 
 * @param cpu_context cpu registers along with their values
 * @return void
 */
void trace_analyzer::checkpoint_storing_callback(CONTEXT* cpu_context)
{
  ptr_instruction_t curr_ins = instruction_at_execorder[current_execution_order];
  boost::unordered_set<ptr_insoperand_t>::iterator operand_iter;

  // iterate over the set of the current instruction's source operands
  for (operand_iter = curr_ins->source_operands.begin();
       operand_iter != curr_ins->source_operands.end(); ++operand_iter)
  {
    // verify if the operand is a memory address and this address is in the input buffer
    if (((*operand_iter)->value.type() == typeid(ADDRINT)) && 
        utils::is_in_input_buffer(boost::get<ADDRINT>((*operand_iter)->value)))
    {
      // then capture a checkpoint
      checkpoint_at_exeorder[current_execution_order].reset(new checkpoint(cpu_context));
      break;
    }
  }
  
  return;
}

} // end of instrumentation namespace
