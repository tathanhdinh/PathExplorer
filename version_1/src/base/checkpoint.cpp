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

  for (UINT32 idx = 0; idx < input_mem_read_size; ++idx)
  {
    if ((received_msg_addr <= input_mem_read_addr + idx) &&
        (input_mem_read_addr + idx < received_msg_addr + received_msg_size))
    {
      this->input_dep_addrs.insert(input_mem_read_addr + idx);
    }
  }

  this->curr_input.reset(new UINT8[received_msg_size]);
  PIN_SafeCopy(this->curr_input.get(),
               reinterpret_cast<UINT8*>(received_msg_addr), received_msg_size);
}


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
      PIN_SafeCopy(&single_byte, reinterpret_cast<UINT8*>(mem_addr + offset), 1);
      mem_written_log[mem_addr + offset] = single_byte;
    }
  }

  return;
}


/**
 * @brief checkpoint::rollback
 */
void checkpoint::rollback()
{
  std::cerr << "rollback\n";
  // restore values of written memory addresses
  std::map<ADDRINT, UINT8>::iterator mem_iter = this->mem_written_log.begin();
  for (; mem_iter != this->mem_written_log.end(); ++mem_iter)
  {
    PIN_SafeCopy(reinterpret_cast<UINT8*>(mem_iter->first), &mem_iter->second, sizeof(UINT8));
  }
  // restore values of registers
  std::cerr << "rollback\n";
  PIN_ExecuteAt(this->context.get());
  return;
}


/**
 * @brief checkpoint::rollback_with_new_input
 * @param input_addr
 * @param new_input_buffer
 * @param input_size
 */
void checkpoint::rollback_with_new_input(ADDRINT input_addr,
                                         UINT8* new_input_buffer, UINT32 input_size)
{
  // restore values of written memory addresses
  std::map<ADDRINT, UINT8>::iterator mem_iter = this->mem_written_log.begin();
  for (; mem_iter != this->mem_written_log.end(); ++mem_iter)
  {
    PIN_SafeCopy(reinterpret_cast<UINT8*>(mem_iter->first), &mem_iter->second, sizeof(UINT8));
  }

  // replace a new input
  PIN_SafeCopy(reinterpret_cast<UINT8*>(input_addr), new_input_buffer, input_size);

  // restore values of registers
  PIN_ExecuteAt(this->context.get());
  return;
}

/*================================================================================================*/

void rollback_and_restore(ptr_checkpoint_t& dest_ptr_checkpoint, UINT8* backup_input_addr)
{
  // restore the input
  PIN_SafeCopy(reinterpret_cast<UINT8*>(received_msg_addr),
               dest_ptr_checkpoint->curr_input.get(), received_msg_size);

  // restore the current execution order (-1 again because the instruction at the checkpoint will
  // be re-executed)
  current_exec_order = dest_ptr_checkpoint->exec_order;
  current_exec_order--;

  // restore written memories
   UINT8 single_byte;
  
  std::map<ADDRINT, UINT8>::iterator mem_map_iter = dest_ptr_checkpoint->mem_written_log.begin();
  for (; mem_map_iter != dest_ptr_checkpoint->mem_written_log.end(); ++mem_map_iter)
  {
     single_byte = mem_map_iter->second;
     PIN_SafeCopy(reinterpret_cast<UINT8*>(mem_map_iter->first), &single_byte, 1);
    //*(reinterpret_cast<UINT8*>(mem_map_iter->first)) = mem_map_iter->second;
  }
//  dest_ptr_checkpoint->mem_read_log.clear();
//   std::map<ADDRINT, UINT8>().swap(dest_ptr_checkpoint->mem_written_log);

  // replace input and go back
  PIN_SafeCopy(reinterpret_cast<UINT8*>(received_msg_addr), backup_input_addr, received_msg_size);

  /*std::cout << remove_leading_zeros(StringFromAddrint(received_msg_addr))
    << reinterpret_cast<UINT8*>(backup_input_addr)[0] << std::endl;;*/

  /*for (UINT32 idx = 0; idx < received_msg_size; ++idx) 
  {
    reinterpret_cast<UINT8*>(received_msg_addr)[idx] = backup_input_addr[idx];
  }*/
  
  // go back
  PIN_ExecuteAt(dest_ptr_checkpoint->context.get());

  return;
}

/*================================================================================================*/

void rollback_and_restore(ptr_checkpoint_t& current_chkpt, std::set<ADDRINT>& mem_addrs)
{
  return;
}

/*================================================================================================*/

void rollback_and_modify(ptr_checkpoint_t& current_ptr_checkpoint, std::set<ADDRINT>& dep_mems)
{
  // restore the current trace
//  explored_trace = current_ptr_checkpoint->trace;
//  explored_trace.pop_back();

  current_exec_order = current_ptr_checkpoint->exec_order;
  current_exec_order--;

  // restore written memories
  UINT8 single_byte;
  std::map<ADDRINT, UINT8>::iterator mem_map_iter;
  for (mem_map_iter = current_ptr_checkpoint->mem_written_log.begin();
       mem_map_iter != current_ptr_checkpoint->mem_written_log.end(); ++mem_map_iter)
  {
     single_byte = mem_map_iter->second;
     PIN_SafeCopy(reinterpret_cast<UINT8*>(mem_map_iter->first), &single_byte, 1);
    //*(reinterpret_cast<UINT8*>(mem_map_iter->first)) = mem_map_iter->second;
  }
  std::map<ADDRINT, UINT8>().swap(current_ptr_checkpoint->mem_written_log);

  // modify input
  std::set<ADDRINT>::iterator mem_set_iter = dep_mems.begin();
  for (; mem_set_iter != dep_mems.end(); ++mem_set_iter)
  {
     single_byte = rand() % std::numeric_limits<UINT8>::max();
//    single_byte = uint8_uniform(rgen);
    PIN_SafeCopy(reinterpret_cast<UINT8*>(*mem_set_iter), &single_byte, 1);
    //*(reinterpret_cast<UINT8*>(*mem_set_iter)) = rand() % std::numeric_limits<UINT8>::max();
  }

  // go back
  PIN_ExecuteAt(current_ptr_checkpoint->context.get());

  return;
}
