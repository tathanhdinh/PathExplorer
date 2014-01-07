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

#include "fast_execution.h"
#include "../main.h"

namespace engine
{

/**
 * @brief the control moves back to a previously logged checkpoint; this operation can always be 
 * invoked safely (in the program's space) if the checkpoint is logged fully (by invoking the log 
 * function just before the execution of a memory written instruction). Note that the instruction 
 * (pointed by the IP in the checkpoint's cpu context) will be re-executed.
 * 
 * @param checkpoint_exeorder the execution order of the target past checkpoint.
 * @return void
 */
void fast_execution::move_backward(UINT32 checkpoint_exeorder)
{
  ptr_checkpoint_t past_chkpnt = checkpoint_at_exeorder[checkpoint_exeorder];
  boost::unordered_map<ADDRINT, UINT8>::iterator mem_iter;
  
  // update the logged values of the written addresses
  for (mem_iter = past_chkpnt->memory_change_log.begin(); 
       mem_iter != past_chkpnt->memory_change_log.end(); ++mem_iter) 
  {
    *(reinterpret_cast<UINT8*>(mem_iter->first)) = mem_iter->second;
  }
  // then clear the set of logged values
  past_chkpnt->memory_change_log.clear();
  
  // NEW APPROACH
  // the global memory state will be restored to the state at the past checkpoint (before the 
  // execution of the checkpoint's instruction)
  boost::unordered_map<ADDRINT, state_t>::iterator curr_mem_iter, past_mem_iter;
  for (curr_mem_iter = current_memory_state.begin(); curr_mem_iter != current_memory_state.end(); 
       ++curr_mem_iter) 
  {
    // verify if an element in the current state is also an element in the past state
    past_mem_iter = past_chkpnt->memory_state.find(curr_mem_iter->first);
    if (past_mem_iter != past_chkpnt->memory_state.end()) 
    {
      // if it is then the memory value at its address needs to be restored by the last value in 
      // the past state
      *(reinterpret_cast<UINT8*>(curr_mem_iter->first)) = past_mem_iter->second.second();
    }
    else 
    {
      // if it is not then the memory value at its address needs to be restored to the original 
      // value in the past state
      *(reinterpret_cast<UINT8*>(curr_mem_iter->first)) = curr_mem_iter->second.first();
    }
  }
  current_memory_state = past_chkpnt->memory_state;
  
  // update the cpu context: the instruction (pointed by the IP register in the cpu context) will 
  // be re-executed.
  PIN_ExecuteAt(&(past_chkpnt->cpu_context));

  return;
}


/**
 * @brief the control moves forward to a previously logged checkpoint: this operation is NOT ALWAYS 
 * SAFE, it should be called only from jumping point of the current checkpoint to move to the next 
 * checkpoint. The current memory state is now a subset of the memory state saved at the future 
 * checkpoint. Note that the instruction (determined by the IP in the checkpoint's cpu context) 
 * will be re-executed.
 * 
 * @param target_checkpoint the future checkpoint
 * @return void
 */
void fast_execution::move_forward(UINT32 checkpoint_exeorder)
{
  ptr_checkpoint_t futur_chkpnt = checkpoint_at_exeorder[checkpoint_exeorder];
  
  // the global memory state will be updated to reflect the state at the future checkpoint
  boost::unordered_set<ptr_insoperand_t>::iterator insoperand_iter;
  
  for (insoperand_iter = futur_chkpnt->alive_operands.begin(); 
       insoperand_iter != futur_chkpnt->alive_operands.end(); ++insoperand_iter) 
  {
    if ((*insoperand_iter)->value.type() == typeid(ADDRINT)) 
    {
      if ()
    }
  }
  
  
//   // update the memory state
//   boost::unordered_map<ADDRINT, state_t>::iterator futur_mem_iter;
//   boost::unordered_map<ADDRINT, state_t>::iterator curr_mem_iter;
//   boost::unordered_map<ADDRINT, UINT8>::iterator mem_iter;
//   
//   for (futur_mem_iter = future_checkpoint->memory_state.begin(); 
//        futur_mem_iter != future_checkpoint->memory_state.end(); ++futur_mem_iter) 
//   {
//     *(reinterpret_cast<UINT8*>(futur_mem_iter->first)) = futur_mem_iter->second.second();
//   }
    
  // restore the cpu context
  PIN_ExecuteAt(&(futur_chkpnt->cpu_context));
  
  return;
}

} // end of engine namespace
