#ifndef CHECKPOINT_H
#define CHECKPOINT_H

// these definitions are not necessary (defined already in the CMakeLists),
// they are added just to help qt-creator parsing headers
#if defined(_WIN32) || defined(_WIN64)
#ifndef TARGET_IA32
#define TARGET_IA32
#endif
#ifndef HOST_IA32
#define HOST_IA32
#endif
#ifndef TARGET_WINDOWS
#define TARGET_WINDOWS
#endif
#ifndef USING_XED
#define USING_XED
#endif
#endif
#include <pin.H>

#include <map>
#include <set>
#include <vector>
#include <memory>

//#if __cplusplus < 199711L
//#include <boost/shared_ptr.hpp>
//#define pept boost
//#else
//#include <memory>
//#define pept std
//#endif

typedef std::map<ADDRINT, UINT8> addrint_value_map_t;

class checkpoint
{
public:
  std::shared_ptr<CONTEXT>     context;

  // maps between written memory addresses and original values
  addrint_value_map_t         mem_written_log;
  
  addrint_value_map_t         input_dep_original_values;
  UINT32                      exec_order;
    
public:
  checkpoint(UINT32 existing_exec_order, CONTEXT* ptr_context,
             ADDRINT input_mem_read_addr, UINT32 input_mem_read_size);

  void mem_write_tracking(ADDRINT mem_addr, UINT32 mem_length);
};

typedef std::shared_ptr<checkpoint> ptr_checkpoint_t;
typedef std::vector<ptr_checkpoint_t> ptr_checkpoints_t;

extern void rollback_with_current_input   (const ptr_checkpoint_t& destination,
                                           UINT32& existing_exec_order);

extern void rollback_with_original_input  (const ptr_checkpoint_t& destination,
                                           UINT32& existing_exec_order);

extern void rollback_with_new_input       (const ptr_checkpoint_t& destination,
                                           UINT32& existing_exec_order, ADDRINT input_buffer_addr,
                                           UINT32 input_buffer_size, UINT8* new_buffer);

extern void rollback_with_modified_input  (const ptr_checkpoint_t& destination,
                                           UINT32& existing_exec_order,
                                           addrint_value_map_t& modified_addrs_values);


//void rollback_and_restore(ptr_checkpoint_t& ptr_chkpnt, UINT8* backup_input_addr);

//void rollback_and_restore(ptr_checkpoint_t& ptr_chkpnt, std::set<ADDRINT>& mem_addrs);

//void rollback_and_modify(ptr_checkpoint_t& ptr_chkpnt, std::set<ADDRINT>& dep_mems);

#endif // CHECKPOINT_H

