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

#include "checkpoint.h"
#include <pin.H>

extern boost::container::vector<ADDRINT> explored_trace;

namespace engine
{

boost::unordered_map<ADDRINT, 
                     boost::compressed_pair<UINT8, UINT8>
                     > current_memory_state;

/**
 * @brief a checkpoint is created before the instruction (pointed by the current address) executes. 
 * 
 * @param current_address the address pointed by the instruction pointer (IP).
 * @param current_context the cpu context (values of registers) when the IP is at this address.
 */
checkpoint::checkpoint(ADDRINT current_address, CONTEXT* current_context)
{
  this->address = current_address;
  
  // save the current cpu context
  PIN_SaveContext(current_context, &(this->cpu_context));
  
  // save the current memory state
  this->memory_state = current_memory_state;
  
  this->trace = explored_trace;
}


/**
 * @brief the checkpoint stores the original values at memory addresses before the executed 
 * instruction overwrites these values.
 * 
 * @param memory_written_address the beginning address that will be written.
 * @param memory_written_length the length of written addresses.
 * @return void
 */
void checkpoint::log_before_execution(ADDRINT memory_written_address, UINT8 memory_written_length)
{
  ADDRINT upper_bound_address = memory_written_address + memory_written_length;
  ADDRINT address;
  
  for (address = memory_written_address; address < upper_bound_address; ++address) 
  {
    // log the original value at this written address
    if (this->memory_log.find(address) == this->memory_log.end()) 
    {
      this->memory_log[address] = *(reinterpret_cast<UINT8*>(address));
    }
  
    // update the total memory state
    if (current_memory_state.find(address) == current_memory_state.end()) 
    {
      current_memory_state[address].first() = *(reinterpret_cast<UINT8*>(address));
    }
    current_memory_state[address].second() = *(reinterpret_cast<UINT8*>(address));
  }
  
  return;
}

} // end of reverse_execution_engine namespace
