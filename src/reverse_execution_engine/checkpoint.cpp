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

using namespace reverse_execution_engine;

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
  this->local_memory_state = total_memory_state;
  
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
 * @brief the checkpoint needs to update the total memory state after the executed instruction 
 * overwrites some memory addresses.
 * 
 * @param memory_written_address the beginning addresses that was written.
 * @param memory_written_length the length of written addresses
 * @return void
 */
void log_after_execution(ADDRINT memory_written_address, UINT8 memory_written_length)
{
  ADDRINT upper_bound_address = memory_written_address + memory_written_length;
  ADDRINT address = memory_written_address;
  
  for (; address < upper_bound_address; ++address) 
  {
    total_memory_state[address] = *(reinterpret_cast<UINT8*>(address));
  }
  
  return;
}


/**
 * @brief the control moves back to a previously logged checkpoint; this operation can always be 
 * invoked safely (in the program's space) if the checkpoint is logged fully (by invoking the log 
 * function just before the execution of a memory written instruction). Note that the instruction 
 * (pointed by the IP in the checkpoint's cpu context) will be re-executed.
 * 
 * @param target_checkpoint the checkpoint in the past.
 * @return void
 */
void move_backward(boost::shared_ptr<checkpoint>& target_checkpoint)
{
  // restore the explored trace: because the instruction will be re-executed so the last instruction 
  // in the trace must be removed.
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
  
  // restore the cpu context
  // note that the instruction (pointed by the EIP register in the cpu context) will be re-executed.
  PIN_ExecuteAt(&(target_checkpoint->cpu_context));

  return;
}


/**
 * @brief the control moves forward to a previously logged checkpoint. Note that the instruction 
 * (pointed by the IP in the checkpoint's cpu context) will be re-executed.
 * 
 * @param target_checkpoint the checkpoint in the future.
 * @return void
 */
void move_forward(boost::shared_ptr<checkpoint>& target_checkpoint)
{
  // restore the explored trace: because the instruction will be re-executed so the last instruction 
  // in the trace must be removed.
  explored_trace = target_checkpoint->trace;
  explored_trace.pop_back();
  
  // restore the total memory state
  boost::unordered_map<ADDRINT, UINT8>::iterator total_memory_state_iter 
                                                  = target_checkpoint->local_memory_state.begin();
  for (; total_memory_state_iter != target_checkpoint->local_memory_state.end(); 
       ++total_memory_state_iter)
  {
    *(reinterpret_cast<UINT8*>(total_memory_state_iter->first)) = total_memory_state_iter->second;
  }
  
  // restore the cpu context
  PIN_ExecuteAt(&(target_checkpoint->cpu_context));
  
  return;
}
