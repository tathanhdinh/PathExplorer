#ifndef CHECKPOINT_H
#define CHECKPOINT_H

#include <pin.H>

#include <map>
#include <set>
#include <vector>

#include <boost/shared_ptr.hpp>

typedef std::map<ADDRINT, UINT8> addrint_value_map_t;

class checkpoint
{
public:
  boost::shared_ptr<CONTEXT>  context;
  std::map<ADDRINT, UINT8>    mem_written_log; // maps between written memory addresses and original values
  
  std::set<ADDRINT>           input_dep_addrs;
  UINT32                      exec_order;
    
public:
  checkpoint(CONTEXT* ptr_context, ADDRINT input_mem_read_addr, UINT32 input_mem_read_size);
  void mem_write_tracking(ADDRINT mem_addr, UINT32 mem_length);
  void rollback(UINT32& existing_exec_order);
  void rollback_with_new_input(UINT32& existing_exec_order, ADDRINT input_buffer_addr,
                               UINT32 input_buffer_size, UINT8* new_buffer);
};

typedef boost::shared_ptr<checkpoint> ptr_checkpoint_t;
typedef std::vector<ptr_checkpoint_t> ptr_checkpoints_t;

/*================================================================================================*/

void rollback_and_restore(ptr_checkpoint_t& ptr_chkpnt, UINT8* backup_input_addr);

void rollback_and_restore(ptr_checkpoint_t& ptr_chkpnt, std::set<ADDRINT>& mem_addrs);

void rollback_and_modify(ptr_checkpoint_t& ptr_chkpnt, std::set<ADDRINT>& dep_mems);

#endif // CHECKPOINT_H

