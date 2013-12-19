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

extern boost::container::vector<ADDRINT> explored_trace;

/**
 * @brief a checkpoint is created at when the instruction pointer pointes to some address.
 * 
 * @param current_address the address pointed by the instruction pointer (IP).
 * @param current_context the cpu context (values of registers) when the IP is at this address.
 */
checkpoint::checkpoint(ADDRINT current_address, 
                       CONTEXT* current_context)
{
  this->address = current_address;
  PIN_SaveContext(current_context, &(this->cpu_context));
  this->trace = explored_trace;
}

/**
 * @brief the checkpoint stores the original value at memory addresses before they are written.
 * 
 * @param memory_written_address the beginning address that will be written.
 * @param memory_written_length the length of written addresses.
 * @return void
 */
void checkpoint::log(ADDRINT memory_written_address, UINT8 memory_written_length)
{
  ADDRINT upper_bound_address = memory_written_address + memory_written_length;
  ADDRINT address = memory_written_address;
  
  for (; address < upper_bound_address; ++address) 
  {
    if (this->memory_log.find(address) == this->memory_log.end()) 
    {
      // log the original value at this written address
      this->memory_log[address] = *(reinterpret_cast<UINT8*>(address));
    }
  }
  
  return;
}

/**
 * @brief the control moves back to a past checkpoint; this operation can always be invoked safely 
 * (in the program's space) if the past checkpoint is logged fully (by invoking the log function 
 * just before the execution of a memory written instruction).
 * 
 * @param target_checkpoint the past checkpoint.
 * @return void
 */
void checkpoint::move_backward(ptr_checkpoint& target_checkpoint)
{
  // restore the explored trace
  explored_trace = target_checkpoint->trace;
  explored_trace.pop_back();
  
  // restore the logged values of the written addresses
  boost::unordered_map<ADDRINT, UINT8>::iterator memory_log_iter 
                                                          = target_checkpoint->memory_log.begin();
  for (; memory_log_iter != target_checkpoint->memory_log.end(); ++memory_log_iter) 
  {
    *(reinterpret_cast<UINT8*>(memory_log_iter->first)) = memory_log_iter->second;
  }
  
  // clear the set of logged values
  target_checkpoint->memory_log.clear();
  
  // move backward
  PIN_ExecuteAt(&(target_checkpoint->cpu_context));

  return;
}
