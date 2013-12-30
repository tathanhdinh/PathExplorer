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
#include <boost/log/trivial.hpp>
#include <boost/format.hpp>
#include <boost/unordered_map.hpp>

namespace instrumentation 
{

using namespace analysis;
  
extern UINT32 current_execution_order;
extern UINT32 execution_trace_max_length;
// extern boost::unordered_map<UINT32, ADDRINT> execution_order_address_map;
extern boost::unordered_map<UINT32, ptr_instruction_t> execution_order_instruction_map;
extern boost::unordered_map<ADDRINT, ptr_instruction_t> address_instruction_map;
  
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
    execution_order_instruction_map[current_execution_order].reset(new instruction(*curr_ins));
  }
  else 
  {
    // stop the trace-analyzing state
  }
}


/**
 * @brief callback for a memory read instruction, beside the address/size of memory, the 
 * cpu context is passed because a checkpoint need to be stored when the instruction read some
 * input-related addresses.
 * 
 * @param instruction_address ...
 * @param memory_read_address ...
 * @param memory_read_size ...
 * @param cpu_context ...
 * @return void
 */
void trace_analyzer::memory_read_instruction_callback(ADDRINT instruction_address, 
                                                      ADDRINT memory_read_address, 
                                                      UINT32 memory_read_size, 
                                                      CONTEXT* cpu_context)
{
  // determine dynamic information: read memory addresses
  ptr_instruction_t curr_ins = execution_order_instruction_map[current_execution_order];
  curr_ins->update_memory(memory_read_address, memory_read_size, MEMORY_READ);
  
  // verify if the 
  return;
}



} // end of instrumentation namespace