#include "checkpoint.h"

#include <limits>
#include <cstdlib>
#include <ctime>
#include <map>

#include <boost/random.hpp>

#include "../util/stuffs.h"

extern ADDRINT                                received_msg_addr;
extern UINT32                                 received_msg_size;
extern UINT32                                 current_exec_order;

checkpoint::checkpoint(CONTEXT* p_ctxt, ADDRINT input_mem_read_addr, UINT32 input_mem_read_size)
{
  this->context.reset(new CONTEXT);
  PIN_SaveContext(p_ctxt, this->context.get());

  this->exec_order = current_exec_order;

  UINT32 mem_offset; UINT8 single_byte;
  for (mem_offset = 0; mem_offset < input_mem_read_size; ++mem_offset)
  {
    PIN_SafeCopy(&single_byte,
                 reinterpret_cast<UINT8*>(input_mem_read_addr + mem_offset), sizeof(UINT8));
    this->input_dep_original_values[input_mem_read_addr + mem_offset] = single_byte;
  }
}


/**
 * @brief tracking instructions that write memory
 */
void checkpoint::mem_write_tracking(ADDRINT mem_addr, UINT32 mem_size)
{
  UINT8 single_byte;
  for (UINT32 offset = 0; offset < mem_size; ++offset)
  {
    // this address is written for the first time,
    if (mem_written_log.find(mem_addr + offset) == mem_written_log.end())
    {
      // then the original value is logged.
//       mem_written_log[mem_addr + offset] = *(reinterpret_cast<UINT8*>(mem_addr + offset));
      PIN_SafeCopy(&single_byte, reinterpret_cast<UINT8*>(mem_addr + offset), sizeof(UINT8));
      mem_written_log[mem_addr + offset] = single_byte;
    }
  }

  return;
}


static inline void restore_exec_order_and_written_mem(UINT32& existing_exec_order,
                                                      UINT32 checkpoint_exec_order,
                                                      addrint_value_map_t& checkpoint_mem_written_log)
{
  // restore the existing execution order (-1 because the instruction at the checkpoint will
  // be re-executed)
  existing_exec_order = checkpoint_exec_order - 1;

  // restore values of written memory addresses
  addrint_value_map_t::iterator mem_iter = checkpoint_mem_written_log.begin();
  for (; mem_iter != checkpoint_mem_written_log.end(); ++mem_iter)
  {
    PIN_SafeCopy(reinterpret_cast<UINT8*>(mem_iter->first), &mem_iter->second, sizeof(UINT8));
  }
  return;
}


/**
 * @brief restore the original input and rollback
 */
void checkpoint::rollback_with_original_input(UINT32& existing_exec_order)
{
  std::cerr << "rollback with original input\n";
  restore_exec_order_and_written_mem(existing_exec_order, this->exec_order, this->mem_written_log);

  // restore the original input
  addrint_value_map_t::iterator mem_iter = this->input_dep_original_values.begin();
  for (; mem_iter != this->input_dep_original_values.end(); ++mem_iter)
  {
    PIN_SafeCopy(reinterpret_cast<UINT8*>(mem_iter->first), &mem_iter->second, sizeof(UINT8));
  }

  // restore the values of registers
  PIN_ExecuteAt(this->context.get());
  return;
}


/**
 * @brief keep the current input and rollback
 */
void checkpoint::rollback_with_current_input(UINT32& existing_exec_order)
{
  std::cerr << "rollback with current input\n";

  restore_exec_order_and_written_mem(existing_exec_order, this->exec_order, this->mem_written_log);

  std::cerr << "hahaha\n";
  // restore values of registers
  PIN_ExecuteAt(this->context.get());
  return;
}


/**
 * @brief replace the current input by a new input and rollback
 */
void checkpoint::rollback_with_new_input(UINT32& existing_exec_order, ADDRINT input_buffer_addr,
                                         UINT32 input_buffer_size, UINT8* new_buffer)
{
  std::cerr << "rollback with new input\n";

  restore_exec_order_and_written_mem(existing_exec_order, this->exec_order, this->mem_written_log);

  std::cerr << existing_exec_order << "\n";

  // replace the current input
  PIN_SafeCopy(reinterpret_cast<UINT8*>(input_buffer_addr), new_buffer, input_buffer_size);

  std::cerr << "hahahasdfsdfsdf\n";
  // restore values of registers
  PIN_ExecuteAt(this->context.get());
  return;
}


/**
 * @brief modify the current input and rollback
 */
void checkpoint::rollback_with_modified_input(UINT32& existing_exec_order,
                                              addrint_value_map_t& modified_addrs_values)
{
  std::cerr << "rollback with modified input " << modified_addrs_values.size() << "\n" ;

  restore_exec_order_and_written_mem(existing_exec_order, this->exec_order, this->mem_written_log);

  // modify the current input
  addrint_value_map_t::iterator mem_iter = modified_addrs_values.begin();
  for (; mem_iter != modified_addrs_values.end(); ++mem_iter)
  {
    PIN_SafeCopy(reinterpret_cast<UINT8*>(mem_iter->first), &mem_iter->second, sizeof(UINT8));
  }

  // restore values of registers
  PIN_ExecuteAt(this->context.get());

  return;
}


/**
 * @brief keep the current input and rollback
 */
void rollback_with_current_input(ptr_checkpoint_t destination, UINT32& existing_exec_order)
{
  std::cerr << "rollback with current input\n";

  restore_exec_order_and_written_mem(existing_exec_order, destination->exec_order,
                                     destination->mem_written_log);

  std::cerr << "hahaha\n";
  // restore values of registers
  PIN_ExecuteAt(destination->context.get());
  return;
}


/**
 * @brief restore the original input and rollback
 */
void rollback_with_original_input(ptr_checkpoint_t destination, UINT32& existing_exec_order)
{
  std::cerr << "rollback with original input\n";
  restore_exec_order_and_written_mem(existing_exec_order, destination->exec_order,
                                     destination->mem_written_log);

  // restore the original input
  addrint_value_map_t::iterator mem_iter = destination->input_dep_original_values.begin();
  for (; mem_iter != destination->input_dep_original_values.end(); ++mem_iter)
  {
    PIN_SafeCopy(reinterpret_cast<UINT8*>(mem_iter->first), &mem_iter->second, sizeof(UINT8));
  }

  // restore the values of registers
  PIN_ExecuteAt(destination->context.get());
  return;
}


/**
 * @brief replace the current input by a new input and rollback
 */
void rollback_with_new_input(ptr_checkpoint_t destination, UINT32& existing_exec_order,
                             ADDRINT input_buffer_addr, UINT32 input_buffer_size, UINT8* new_buffer)
{
  std::cerr << "rollback with new input\n";

  restore_exec_order_and_written_mem(existing_exec_order, destination->exec_order,
                                     destination->mem_written_log);

  std::cerr << existing_exec_order << "\n";

  // replace the current input
  PIN_SafeCopy(reinterpret_cast<UINT8*>(input_buffer_addr), new_buffer, input_buffer_size);

  std::cerr << "hahahasdfsdfsdf\n";
  // restore values of registers
  PIN_ExecuteAt(destination->context.get());
  return;
}

//void rollback_and_restore(ptr_checkpoint_t& dest_ptr_checkpoint, UINT8* backup_input_addr)
//{
//  // restore the input
////  PIN_SafeCopy(reinterpret_cast<UINT8*>(received_msg_addr),
////               dest_ptr_checkpoint->curr_input.get(), received_msg_size);

//  // restore the current execution order (-1 again because the instruction at the checkpoint will
//  // be re-executed)
//  current_exec_order = dest_ptr_checkpoint->exec_order;
//  current_exec_order--;

//  // restore written memories
//   UINT8 single_byte;
  
//  std::map<ADDRINT, UINT8>::iterator mem_map_iter = dest_ptr_checkpoint->mem_written_log.begin();
//  for (; mem_map_iter != dest_ptr_checkpoint->mem_written_log.end(); ++mem_map_iter)
//  {
//     single_byte = mem_map_iter->second;
//     PIN_SafeCopy(reinterpret_cast<UINT8*>(mem_map_iter->first), &single_byte, 1);
//    //*(reinterpret_cast<UINT8*>(mem_map_iter->first)) = mem_map_iter->second;
//  }
////  dest_ptr_checkpoint->mem_read_log.clear();
////   std::map<ADDRINT, UINT8>().swap(dest_ptr_checkpoint->mem_written_log);

//  // replace input and go back
//  PIN_SafeCopy(reinterpret_cast<UINT8*>(received_msg_addr), backup_input_addr, received_msg_size);

//  /*std::cout << remove_leading_zeros(StringFromAddrint(received_msg_addr))
//    << reinterpret_cast<UINT8*>(backup_input_addr)[0] << std::endl;;*/

//  /*for (UINT32 idx = 0; idx < received_msg_size; ++idx)
//  {
//    reinterpret_cast<UINT8*>(received_msg_addr)[idx] = backup_input_addr[idx];
//  }*/
  
//  // go back
//  PIN_ExecuteAt(dest_ptr_checkpoint->context.get());

//  return;
//}


////void rollback_and_restore(ptr_checkpoint_t& current_chkpt, std::set<ADDRINT>& mem_addrs)
////{
////  return;
////}


//void rollback_and_modify(ptr_checkpoint_t& current_ptr_checkpoint, std::set<ADDRINT>& dep_mems)
//{
//  // restore the current trace
////  explored_trace = current_ptr_checkpoint->trace;
////  explored_trace.pop_back();

//  current_exec_order = current_ptr_checkpoint->exec_order;
//  current_exec_order--;

//  // restore written memories
//  UINT8 single_byte;
//  std::map<ADDRINT, UINT8>::iterator mem_map_iter;
//  for (mem_map_iter = current_ptr_checkpoint->mem_written_log.begin();
//       mem_map_iter != current_ptr_checkpoint->mem_written_log.end(); ++mem_map_iter)
//  {
//     single_byte = mem_map_iter->second;
//     PIN_SafeCopy(reinterpret_cast<UINT8*>(mem_map_iter->first), &single_byte, 1);
//    //*(reinterpret_cast<UINT8*>(mem_map_iter->first)) = mem_map_iter->second;
//  }
//  std::map<ADDRINT, UINT8>().swap(current_ptr_checkpoint->mem_written_log);

//  // modify input
//  std::set<ADDRINT>::iterator mem_set_iter = dep_mems.begin();
//  for (; mem_set_iter != dep_mems.end(); ++mem_set_iter)
//  {
//     single_byte = rand() % std::numeric_limits<UINT8>::max();
////    single_byte = uint8_uniform(rgen);
//    PIN_SafeCopy(reinterpret_cast<UINT8*>(*mem_set_iter), &single_byte, 1);
//    //*(reinterpret_cast<UINT8*>(*mem_set_iter)) = rand() % std::numeric_limits<UINT8>::max();
//  }

//  // go back
//  PIN_ExecuteAt(current_ptr_checkpoint->context.get());

//  return;
//}
