#ifndef COMMON_H
#define COMMON_H

#include <pin.H>

#include <fstream>

#include "../base/checkpoint.h"
#include "../base/cond_direct_instruction.h"
#include "../util/tinyformat.h"

typedef enum
{
  capturing_state   = 0,
  tainting_state    = 1,
  rollbacking_state = 2
} running_phase;

typedef enum
{
  syscall_inexist  = 0,
  syscall_sendto   = 44,
  syscall_recvfrom = 45
} syscall_id;


extern addr_ins_map_t                   ins_at_addr;
extern order_ins_map_t                  ins_at_order;

extern UINT32                           total_rollback_times;
extern UINT32                           local_rollback_times;
extern UINT32                           trace_size;
extern UINT32                           used_checkpoint_number;

extern UINT32                           max_total_rollback_times;
extern UINT32                           max_local_rollback_times;
extern UINT32                           max_trace_size;

extern ptr_checkpoints_t                saved_checkpoints;

extern ptr_cond_direct_inss_t   detected_input_dep_cfis;
extern ptr_cond_direct_ins_t    exploring_cfi;

extern UINT32                           current_exec_order;

extern UINT32                           received_msg_num;
extern ADDRINT                          received_msg_addr;
extern UINT32                           received_msg_size;

#if defined(__gnu_linux__)
extern ADDRINT                          logged_syscall_index;   // logged syscall index
extern ADDRINT                          logged_syscall_args[6]; // logged syscall arguments
#endif

extern running_phase                    current_running_phase;

extern UINT64                           executed_ins_number;
extern UINT64                           econed_ins_number;

extern KNOB<UINT32>                     max_total_rollback;
extern KNOB<UINT32>                     max_local_rollback;
extern KNOB<UINT32>                     max_trace_length;

extern std::ofstream                    log_file;

#endif // COMMON_H
