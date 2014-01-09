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
#include "../utilities/utils.h"

namespace engine
{

using namespace utilities;

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
  ptr_checkpoint_t past_chkpnt = checkpoint_at_execorder[checkpoint_exeorder];
  
  // PREVIOUS APPROACH: always safe but with high overhead
  boost::unordered_map<ADDRINT, UINT8>::iterator mem_iter;
  
  // update the logged values of the written addresses
  for (mem_iter = past_chkpnt->memory_change_log.begin(); 
       mem_iter != past_chkpnt->memory_change_log.end(); ++mem_iter) 
  {
    *(reinterpret_cast<UINT8*>(mem_iter->first)) = mem_iter->second;
  }
  // then clear the set of logged values
  past_chkpnt->memory_change_log.clear();
  
  // NEW APPROACH: not always safe but with low overhead
  // the global memory state will be restored to reflect the state at the past checkpoint
  boost::unordered_set<ptr_insoperand_t>::iterator insoperand_iter;
  ADDRINT mem_addr;
  for (insoperand_iter = outerface_at_execorder[current_execorder].begin(); 
       insoperand_iter != outerface_at_execorder[current_execorder].end(); ++insoperand_iter) 
  {
    // if the operand is a memory address
    if ((*insoperand_iter)->value.type() == typeid(ADDRINT)) 
    {
      mem_addr = boost::get<ADDRINT>((*insoperand_iter)->value);
      // and this address has been accessed at the target checkpoint
      if (past_chkpnt->memory_state_at.find(mem_addr) != past_chkpnt->memory_state_at.end()) 
      {
        // then restore it by the value stored in the checkpoint
        *(reinterpret_cast<UINT8*>(mem_addr)) = past_chkpnt->memory_state_at[mem_addr];
      }
      else 
      {
        // otherwise restore it by the original value
        *(reinterpret_cast<UINT8*>(mem_addr)) = original_memstate_at_address[mem_addr];
      }
    }
  }
  
  // restore the current memory state
  current_memstate_at_address = past_chkpnt->memory_state_at;
  
  //restore the cpu context
  PIN_ExecuteAt(&(past_chkpnt->cpu_context));

  return;
}


/**
 * @brief the control moves forward to a previously logged checkpoint: this operation is NOT ALWAYS 
 * SAFE, it should be called only from jumping point of the current checkpoint to move to the next 
 * checkpoint. The current memory state is now a subset of the memory state saved at the future 
 * checkpoint. Note that the instruction (determined by the instruction pointer in the checkpoint's 
 * cpu context) will be re-executed.
 * 
 * @param checkpoint_exeorder the future checkpoint's execution order
 * @return void
 */
void fast_execution::move_forward(UINT32 checkpoint_exeorder)
{
  ptr_checkpoint_t futur_chkpnt = checkpoint_at_execorder[checkpoint_exeorder];
  
  // the global memory state will be updated to reflect the state at the future checkpoint
  boost::unordered_set<ptr_insoperand_t>::iterator insoperand_iter;
  ADDRINT mem_addr;
  // iterate over the list of alive operands at the target checkpoint
  for (insoperand_iter = outerface_at_execorder[checkpoint_exeorder].begin(); 
       insoperand_iter != outerface_at_execorder[checkpoint_exeorder].end(); ++insoperand_iter) 
  {
    // if the operand is a memory address
    if ((*insoperand_iter)->value.type() == typeid(ADDRINT)) 
    {
      mem_addr = boost::get<ADDRINT>((*insoperand_iter)->value);
      // and it is also alive at the current execution
      if (outerface_at_execorder[current_execorder].find(*insoperand_iter) != 
          outerface_at_execorder[current_execorder].end()) 
      {
        // then the memory state at the checkpoint will be updated by the current value
        *(reinterpret_cast<UINT8*>(mem_addr)) = current_memstate_at_address[mem_addr];
      }
      else 
      {
        // otherwise the memory state at the checkpoint will be kept
        *(reinterpret_cast<UINT8*>(mem_addr)) = futur_chkpnt->memory_state_at[mem_addr];
      }
    }
  }

  // restore the cpu context
  PIN_ExecuteAt(&(futur_chkpnt->cpu_context));
  
  return;
}


/**
 * @brief modify randomly memory values (at the input buffer) which read at the checkpoint.
 * 
 * @param checkpoint_exeorder the checkpoint's execution order
 * @return void
 */
inline static void modify_input_at_checkpoint(UINT32 checkpoint_exeorder) 
{
  ptr_checkpoint_t curr_chkpnt = checkpoint_at_execorder[checkpoint_exeorder];
  boost::unordered_set<ADDRINT>::iterator mem_addr_iter;
  for (mem_addr_iter = curr_chkpnt->memory_addresses_to_modify.begin(); 
       mem_addr_iter != curr_chkpnt->memory_addresses_to_modify.end(); ++mem_addr_iter) 
  {
    *(reinterpret_cast<UINT8*>(*mem_addr_iter)) = utils::random_uint8();
  }

  return;
}


/**
 * @brief restore original memory values (at the input buffer) which read at the checkpoint.
 * 
 * @param checkpoint_exeorder the checkpoint's execution order
 * @return void
 */
inline static void restore_input_at_checkpoint(UINT32 checkpoint_exeorder) 
{
  ptr_checkpoint_t curr_chkpnt = checkpoint_at_execorder[checkpoint_exeorder];
  boost::unordered_set<ADDRINT>::iterator mem_addr_iter;
  for (mem_addr_iter = curr_chkpnt->memory_addresses_to_modify.begin(); 
       mem_addr_iter != curr_chkpnt->memory_addresses_to_modify.end(); ++mem_addr_iter) 
  {
    *(reinterpret_cast<UINT8*>(*mem_addr_iter)) = original_msgstate_at_address[*mem_addr_iter];
  }
  
  return;
}


/**
 * @brief back to a checkpoint and modify randomly its read values (in the input buffer).
 * 
 * @param checkpoint_exeorder the target checkpoint's execution order
 * @return void
 */
void fast_execution::move_backward_and_modify_input(UINT32 checkpoint_exeorder)
{
  modify_input_at_checkpoint(checkpoint_exeorder);
  move_backward(checkpoint_exeorder);
  return;
}


/**
 * @brief back to a checkpoint and restore its read value (in the input buffer).
 * 
 * @param checkpoint_exeorder the target checkpoint's execution order
 * @return void
 */
void fast_execution::move_backward_and_restore_input(UINT32 checkpoint_exeorder)
{
  restore_input_at_checkpoint(checkpoint_exeorder);
  move_backward(checkpoint_exeorder);
  return;
}


/**
 * @brief forward to a checkpoint and modify randomly its read values (in the input buffer).
 * 
 * @param checkpoint_exeorder the target checkpoint's execution order
 * @return void
 */
void fast_execution::move_forward_and_modify_input(UINT32 checkpoint_exeorder)
{
  modify_input_at_checkpoint(checkpoint_exeorder);
  move_forward(checkpoint_exeorder);
  return;
}


/**
 * @brief forward to a checkpoint and restore its read values (in the input buffer).
 * 
 * @param checkpoint_exeorder the target checkpoint's execution order
 * @return void
 */
void fast_execution::move_forward_and_restore_input(UINT32 checkpoint_exeorder)
{
  restore_input_at_checkpoint(checkpoint_exeorder);
  move_forward(checkpoint_exeorder);
  return;
}

} // end of engine namespace
