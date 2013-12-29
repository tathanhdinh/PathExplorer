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
#include <boost/log/trivial.hpp>
#include <boost/format.hpp>

namespace instrumentation 
{

extern UINT32 current_execution_order;
  
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
 * @param instruction_address address of the instrumented instruction.
 * @return void
 */

void trace_analyzer::vdso_instruction_callback(ADDRINT instruction_address)
{
  BOOST_LOG_TRIVIAL(warning) 
    << boost::format("meet a vdso instruction after %d executed instruction.") 
        % current_execution_order;
  return;
}


} // end of instrumentation namespace