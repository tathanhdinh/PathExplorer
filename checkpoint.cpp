#include "checkpoint.h"

#include <boost/format.hpp>
#include <boost/log/trivial.hpp>

#include <limits>
#include <cstdlib>
#include <ctime>
#include <map>

#include "stuffs.h"

extern ADDRINT                  received_msg_addr;
extern UINT32                   received_msg_size;
extern std::vector<ADDRINT>     explored_trace;
extern std::map< ADDRINT, 
                 instruction >  addr_ins_static_map;

/*====================================================================================================================*/

checkpoint::checkpoint()
{
  std::map<ADDRINT, UINT8>().swap(this->mem_read_log);
  std::map<ADDRINT, UINT8>().swap(this->mem_written_log);

  std::set<ADDRINT>().swap(this->dep_mems);

  this->rollback_times = 0;
}

/*====================================================================================================================*/

checkpoint::checkpoint(ADDRINT ip_addr, 
                       CONTEXT* p_ctxt, 
                       const std::vector<ADDRINT>& current_trace,
                       ADDRINT mem_read_addr, UINT32 mem_read_size)
{
  this->addr = ip_addr;
  this->ptr_ctxt.reset(new CONTEXT);
  PIN_SaveContext(p_ctxt, this->ptr_ctxt.get());

//   this->ptr_ctxt.reset(new_ptr_ctxt);

//   std::map<ADDRINT, UINT8>().swap(this->mem_read_log);
//   std::map<ADDRINT, UINT8>().swap(this->mem_written_log);

  this->trace = current_trace;

  for (UINT32 idx = 0; idx < mem_read_size; ++idx)
  {
    if (
        (received_msg_addr <= mem_read_addr + idx) && (mem_read_addr + idx < received_msg_addr + received_msg_size)
       )
    {
      this->dep_mems.insert(mem_read_addr + idx);
    }
  }

  this->rollback_times = 0;
}

/*====================================================================================================================*/

checkpoint& checkpoint::operator=(checkpoint const& other_chkpnt)
{
  this->addr            = other_chkpnt.addr;
  this->ptr_ctxt        = other_chkpnt.ptr_ctxt;

  this->mem_read_log    = other_chkpnt.mem_read_log;
  this->mem_written_log = other_chkpnt.mem_written_log;

  this->dep_mems        = other_chkpnt.dep_mems;
  this->trace           = other_chkpnt.trace;

  this->rollback_times        = other_chkpnt.rollback_times;

  return *this;
}

/*====================================================================================================================*/

// void checkpoint::mem_read_logging(ADDRINT ins_addr, ADDRINT mem_addr, UINT32 mem_size)
// {
//   for (UINT32 offset = 0; offset < mem_size; ++offset)
//   {
//     if (// this address is read for the first time,
//         mem_read_log.find(mem_addr + offset) == mem_read_log.end()
//        )
//     {
//       // then the original value is logged
//       mem_read_log[mem_addr + offset] = *(reinterpret_cast<UINT8*>(mem_addr + offset));
//     }
//   }
//
//   return;
// }

/*====================================================================================================================*/

void checkpoint::mem_written_logging(ADDRINT ins_addr, ADDRINT mem_addr, UINT32 mem_size)
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

/*====================================================================================================================*/

void rollback_with_input_replacement(ptr_checkpoint& dest_ptr_checkpoint, UINT8* backup_input_addr)
{
  // restore the current trace
  explored_trace = dest_ptr_checkpoint->trace;
  explored_trace.pop_back();

  // restore written memories
//   UINT8 single_byte;
  
  std::map<ADDRINT, UINT8>::iterator mem_map_iter = dest_ptr_checkpoint->mem_written_log.begin();
  for (; mem_map_iter != dest_ptr_checkpoint->mem_written_log.end(); ++mem_map_iter)
  {
//     single_byte = mem_map_iter->second;
//     PIN_SafeCopy(reinterpret_cast<UINT8*>(mem_map_iter->first), &single_byte, 1);
    *(reinterpret_cast<UINT8*>(mem_map_iter->first)) = mem_map_iter->second;
  }
  dest_ptr_checkpoint->mem_read_log.clear();
//   std::map<ADDRINT, UINT8>().swap(dest_ptr_checkpoint->mem_written_log);

  // replace input and go back
//   PIN_SafeCopy(reinterpret_cast<UINT8*>(received_msg_addr), backup_input_addr, received_msg_size);
  for (UINT32 idx = 0; idx < received_msg_size; ++idx) 
  {
    reinterpret_cast<UINT8*>(received_msg_addr)[idx] = backup_input_addr[idx];
  }
  
  // go back
  PIN_ExecuteAt(dest_ptr_checkpoint->ptr_ctxt.get());

  return;
}

/*====================================================================================================================*/

void rollback_with_input_random_modification(ptr_checkpoint& ptr_chkpnt, std::set<ADDRINT>& dep_mems)
{
  // restore the current trace
  explored_trace = ptr_chkpnt->trace;
  explored_trace.pop_back();

  // increase rollback times
//   ptr_chkpnt->rollback_times++;

  // restore written memories
//   UINT8 single_byte;
  std::map<ADDRINT, UINT8>::iterator mem_map_iter = ptr_chkpnt->mem_written_log.begin();
  for (; mem_map_iter != ptr_chkpnt->mem_written_log.end(); ++mem_map_iter)
  {
//     single_byte = mem_map_iter->second;
//     PIN_SafeCopy(reinterpret_cast<UINT8*>(mem_map_iter->first), &single_byte, 1);
    *(reinterpret_cast<UINT8*>(mem_map_iter->first)) = mem_map_iter->second;
  }
  std::map<ADDRINT, UINT8>().swap(ptr_chkpnt->mem_written_log);

  // modify input
  std::set<ADDRINT>::iterator mem_set_iter = dep_mems.begin();
  for (; mem_set_iter != dep_mems.end(); ++mem_set_iter)
  {
//     single_byte = rand() % std::numeric_limits<UINT8>::max();
//     PIN_SafeCopy(reinterpret_cast<UINT8*>(*mem_set_iter), &single_byte, 1);
    *(reinterpret_cast<UINT8*>(*mem_set_iter)) = rand() % std::numeric_limits<UINT8>::max();
  }

  // go back
  PIN_ExecuteAt(ptr_chkpnt->ptr_ctxt.get());

  return;
}
