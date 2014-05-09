#include "checkpoint.h"

#include "../util/stuffs.h"

checkpoint::checkpoint(UINT32 existing_exec_order, const CONTEXT* p_ctxt,
                       ADDRINT input_mem_read_addr, UINT32 input_mem_read_size)
{
//  this->context.reset(new CONTEXT);
  this->context = std::make_shared<CONTEXT>(); PIN_SaveContext(p_ctxt, this->context.get());

  this->exec_order = existing_exec_order;

  UINT8 mem_buffer[sizeof(std::size_t)];
  PIN_SafeCopy(mem_buffer, reinterpret_cast<UINT8*>(input_mem_read_addr), input_mem_read_size);
  for (auto mem_idx = 0; mem_idx < input_mem_read_size; ++mem_idx)
  {
    this->input_dep_original_values[input_mem_read_addr + mem_idx] = mem_buffer[mem_idx];
  }

//  /*UINT32 mem_offset;*/ UINT8 single_byte;
//  for (auto mem_offset = 0; mem_offset < input_mem_read_size; ++mem_offset)
//  {
//    PIN_SafeCopy(&single_byte,
//                 reinterpret_cast<UINT8*>(input_mem_read_addr + mem_offset), sizeof(UINT8));
//    this->input_dep_original_values[input_mem_read_addr + mem_offset] = single_byte;
//  }
}


/**
 * @brief tracking instructions that write memory
 */
void checkpoint::mem_write_tracking(ADDRINT mem_addr, UINT32 mem_size)
{
  for (auto mem_idx = 0; mem_idx < mem_size; ++mem_idx)
  {
    // this address is written for the first time,
    if (mem_written_log.find(mem_addr + mem_idx) == mem_written_log.end())
    {
      // then the original value is logged.
//       mem_written_log[mem_addr + offset] = *(reinterpret_cast<UINT8*>(mem_addr + offset));
      UINT8 single_byte;
      PIN_SafeCopy(&single_byte, reinterpret_cast<UINT8*>(mem_addr + mem_idx), sizeof(UINT8));
      mem_written_log[mem_addr + mem_idx] = single_byte;
    }
  }
  return;
}


/**
 * @brief restore the execution order and over-written memory addresses
 */
static inline void generic_restore(UINT32& existing_exec_order, UINT32 checkpoint_exec_order,
                                   addrint_value_map_t& checkpoint_mem_written_log)
{
  // restore the existing execution order (-1 because the instruction at the checkpoint will
  // be re-executed)
  existing_exec_order = checkpoint_exec_order - 1;

  // restore values of written memory addresses
  /*addrint_value_map_t::iterator*/auto mem_iter = checkpoint_mem_written_log.begin();
  for (; mem_iter != checkpoint_mem_written_log.end(); ++mem_iter)
  {
    PIN_SafeCopy(reinterpret_cast<UINT8*>(mem_iter->first), &mem_iter->second, sizeof(UINT8));
  }
#if !defined(ENABLE_FAST_ROLLBACK)
  checkpoint_mem_written_log.clear();
#endif
  return;
}


/**
 * @brief keep the current input and rollback
 */
void rollback_with_current_input(const ptr_checkpoint_t& dest, UINT32& existing_exec_order)
{
  generic_restore(existing_exec_order, dest->exec_order, dest->mem_written_log);

  // restore values of registers
  PIN_ExecuteAt(dest->context.get());
  return;
}


/**
 * @brief restore the original input and rollback
 */
void rollback_with_original_input(const ptr_checkpoint_t& dest, UINT32& existing_exec_order)
{
  generic_restore(existing_exec_order, dest->exec_order, dest->mem_written_log);

  // restore the original input
  addrint_value_map_t::iterator mem_iter = dest->input_dep_original_values.begin();
  for (; mem_iter != dest->input_dep_original_values.end(); ++mem_iter)
  {
    PIN_SafeCopy(reinterpret_cast<UINT8*>(mem_iter->first), &mem_iter->second, sizeof(UINT8));
  }

  // restore the values of registers
  PIN_ExecuteAt(dest->context.get());
  return;
}


/**
 * @brief replace the current input by a new input and rollback
 */
void rollback_with_new_input(const ptr_checkpoint_t& dest, UINT32& existing_exec_order,
                             ADDRINT input_addr, UINT32 input_size, UINT8* new_buffer)
{
  generic_restore(existing_exec_order, dest->exec_order, dest->mem_written_log);

  // replace the current input
  PIN_SafeCopy(reinterpret_cast<UINT8*>(input_addr), new_buffer, input_size);

  // restore values of registers
  PIN_ExecuteAt(dest->context.get());
  return;
}


/**
 * @brief modify the current input and rollback
 */
void rollback_with_modified_input(const ptr_checkpoint_t& dest, UINT32& existing_exec_order,
                                  addrint_value_map_t& modified_addrs_values)
{
  generic_restore(existing_exec_order, dest->exec_order, dest->mem_written_log);

  // modify the current input
  addrint_value_map_t::iterator mem_iter = modified_addrs_values.begin();
  for (; mem_iter != modified_addrs_values.end(); ++mem_iter)
  {
    PIN_SafeCopy(reinterpret_cast<UINT8*>(mem_iter->first), &mem_iter->second, sizeof(UINT8));
  }

  // restore values of registers
  PIN_ExecuteAt(dest->context.get());
  return;
}
