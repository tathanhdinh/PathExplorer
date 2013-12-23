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

#include <pin.H>
#include "engine.h"

extern boost::container::vector<ADDRINT> explored_trace;

using namespace reverse_execution_engine;

extern boost::unordered_map<ADDRINT, 
                            boost::compressed_pair<UINT8, UINT8>
                           > current_memory_state;
                           
// boost::shared_ptr<checkpoint> current_checkpoint;

/**
 * @brief the control moves back to a previously logged checkpoint; this operation can always be 
 * invoked safely (in the program's space) if the checkpoint is logged fully (by invoking the log 
 * function just before the execution of a memory written instruction). Note that the instruction 
 * (pointed by the IP in the checkpoint's cpu context) will be re-executed.
 * 
 * @param target_checkpoint the checkpoint in the past.
 * @return void
 */
void engine::move_backward(boost::shared_ptr<checkpoint>& past_checkpoint)
{
  // update the explored trace: because the instruction will be re-executed, the last instruction 
  // in the trace must be removed.
  explored_trace = past_checkpoint->trace;
  explored_trace.pop_back();
  
  // update the logged values of the written addresses
  boost::unordered_map<ADDRINT, UINT8>::iterator mem_iter = past_checkpoint->memory_log.begin();
  for (; mem_iter != past_checkpoint->memory_log.end(); ++mem_iter) 
  {
    *(reinterpret_cast<UINT8*>(mem_iter->first)) = mem_iter->second;
  }
  // then clear the set of logged values
  past_checkpoint->memory_log.clear();
  
  // update the global memory state
  boost::unordered_map<ADDRINT, 
                       boost::compressed_pair<UINT8, UINT8>
                       >::iterator current_mem_iter = current_memory_state.begin();
  boost::unordered_map<ADDRINT, 
                       boost::compressed_pair<UINT8, UINT8>
                       >::iterator past_mem_iter;
  for (; current_mem_iter != current_memory_state.end(); ++current_mem_iter) 
  {
    // verify if an element in the current state is also an element in the past state
    past_mem_iter = past_checkpoint->memory_state.find(current_mem_iter->first);
    if (past_mem_iter != past_checkpoint->memory_state.end()) 
    {
      // if it is then the memory value at its address needs to be restored by the last value in 
      // the past state
      *(reinterpret_cast<UINT8*>(current_mem_iter->first)) = past_mem_iter->second.second();
    }
    else 
    {
      // if it is not then the memory value at its address needs to be restored to the original 
      // value in the past state
      *(reinterpret_cast<UINT8*>(current_mem_iter->first)) = current_mem_iter->second.first();
    }
  }
  current_memory_state = past_checkpoint->memory_state;
  
  // update the cpu context: the instruction (pointed by the IP register in the cpu context) will 
  // be re-executed.
  PIN_ExecuteAt(&(past_checkpoint->cpu_context));

  return;
}


/**
 * @brief the control moves forward to a previously logged checkpoint: the global memory state is 
 * now a subset of the local memory state of the target checkpoint. Note that the instruction 
 * (determined by the IP in the checkpoint's cpu context) will be re-executed.
 * 
 * @param target_checkpoint the future checkpoint.
 * @return void
 */
void engine::move_forward(boost::shared_ptr<checkpoint>& future_checkpoint)
{
  // update the explored trace: because the instruction will be re-executed, the last instruction 
  // in the trace must be removed.
  explored_trace = future_checkpoint->trace;
  explored_trace.pop_back();
  
  // update the memory state
  boost::unordered_map<ADDRINT, 
                       boost::compressed_pair<UINT8, UINT8>
                       >::iterator futur_mem_iter = future_checkpoint->memory_state.begin();
  boost::unordered_map<ADDRINT, 
                       boost::compressed_pair<UINT8, UINT8>
                       >::iterator curr_mem_iter;
  boost::unordered_map<ADDRINT, UINT8>::iterator mem_iter;
  for (; futur_mem_iter != future_checkpoint->memory_state.end(); ++futur_mem_iter) 
  {
    // verify if an element in the future memory state is also an element in the current state
    curr_mem_iter = current_memory_state.find(futur_mem_iter->first);
    if (curr_mem_iter != current_memory_state.end()) 
    {
      // if it is then verify if it is modified
    }
    else 
    {
      // if it is not then the value at its address nees to be restored by the last value in the 
      // current state
      *(reinterpret_cast<UINT8*>(futur_mem_iter->first)) = futur_mem_iter->second.second();
    }
  }
    
  // restore the cpu context
  PIN_ExecuteAt(&(future_checkpoint->cpu_context));
  
  return;
}
