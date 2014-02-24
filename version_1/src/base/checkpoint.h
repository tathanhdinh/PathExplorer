#ifndef CHECKPOINT_H
#define CHECKPOINT_H

#include <pin.H>

#include <map>
#include <set>
#include <vector>

#if __cplusplus <= 199711L
#include <boost/shared_ptr.hpp>
#define pept boost
#else
#include <memory>
#define pept std
#endif

typedef std::map<ADDRINT, UINT8> addrint_value_map_t;

class checkpoint
{
public:
  pept::shared_ptr<CONTEXT>     context;

  // maps between written memory addresses and original values
  addrint_value_map_t         mem_written_log;
  
  addrint_value_map_t         input_dep_original_values;
  UINT32                      exec_order;
    
public:
  checkpoint(CONTEXT* ptr_context, ADDRINT input_mem_read_addr, UINT32 input_mem_read_size);

  void mem_write_tracking(ADDRINT mem_addr, UINT32 mem_length);

//  void rollback_with_current_input(UINT32& existing_exec_order);
//  void rollback_with_original_input(UINT32& existing_exec_order);
//  void rollback_with_new_input(UINT32& existing_exec_order, ADDRINT input_buffer_addr,
//                               UINT32 input_buffer_size, UINT8* new_buffer);
//  void rollback_with_modified_input(UINT32& existing_exec_order,
//                                    addrint_value_map_t& modified_addrs_values);
};

typedef pept::shared_ptr<checkpoint> ptr_checkpoint_t;
typedef std::vector<ptr_checkpoint_t> ptr_checkpoints_t;

extern void rollback_with_current_input(ptr_checkpoint_t destination, UINT32& existing_exec_order);
extern void rollback_with_original_input(ptr_checkpoint_t destination, UINT32& existing_exec_order);
extern void rollback_with_new_input(ptr_checkpoint_t destination, UINT32& existing_exec_order,
                                    ADDRINT input_buffer_addr, UINT32 input_buffer_size,
                                    UINT8* new_buffer);
extern void rollback_with_modified_input(ptr_checkpoint_t destination, UINT32& existing_exec_order,
                                         addrint_value_map_t& modified_addrs_values);


//void rollback_and_restore(ptr_checkpoint_t& ptr_chkpnt, UINT8* backup_input_addr);

//void rollback_and_restore(ptr_checkpoint_t& ptr_chkpnt, std::set<ADDRINT>& mem_addrs);

//void rollback_and_modify(ptr_checkpoint_t& ptr_chkpnt, std::set<ADDRINT>& dep_mems);

#endif // CHECKPOINT_H

