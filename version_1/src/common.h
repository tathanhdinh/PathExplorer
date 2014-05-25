#ifndef COMMON_H
#define COMMON_H

#include "parsing_helper.h"
#include <pin.H>

#include <fstream>
#include <random>

#include "base/checkpoint.h"
#include "base/cond_direct_instruction.h"
#include "base/execution_path.h"
#include "util/tinyformat.h"
#include "base/explorer_graph.h"
#include "base/execution_dfa.h"

typedef enum
{
  capturing_phase   = 0,
  tainting_phase    = 1,
  rollbacking_phase = 2
}                               running_phase;

typedef enum
{
  syscall_inexist  = 0,
  syscall_sendto   = 44,
  syscall_recvfrom = 45
}                               syscall_id;

extern addr_ins_map_t           ins_at_addr;
extern order_ins_map_t          ins_at_order;

extern UINT32                   total_rollback_times;
extern UINT32                   local_rollback_times;
extern UINT32                   trace_size;

extern UINT32                   max_total_rollback_times;
extern UINT32                   max_local_rollback_times;
extern UINT32                   max_trace_size;

extern ptr_checkpoints_t        saved_checkpoints;

extern ptr_cond_direct_inss_t   detected_input_dep_cfis;
extern ptr_cond_direct_ins_t    exploring_cfi;

extern UINT32                   current_exec_order;

extern path_code_t              current_path_code;
extern ptr_explorer_graph_t     explored_fsa;

extern ptr_exec_dfa_t           abstracted_dfa;

extern ptr_exec_path_t          current_exec_path;
extern ptr_exec_paths_t         explored_exec_paths;

extern ADDRINT                  received_msg_addr;
extern UINT32                   received_msg_size;
extern UINT32                   received_msg_order;
extern bool                     interested_msg_is_received;
extern ptr_uint8_t              fresh_input;

extern INT                      process_id;
extern std::string              process_id_str;
extern THREADID                 traced_thread_id;
extern bool                     traced_thread_is_fixed;

#if defined(__gnu_linux__)
extern ADDRINT                  logged_syscall_index;   // logged syscall index
extern ADDRINT                  logged_syscall_args[6]; // logged syscall arguments
#endif

extern running_phase            current_running_phase;

extern UINT64                   executed_ins_number;
extern UINT64                   econed_ins_number;

extern KNOB<UINT32>             max_total_rollback_knob;
extern KNOB<UINT32>             max_local_rollback_knob;
extern KNOB<UINT32>             max_trace_length_knob;

extern std::ofstream            log_file;

typedef std::shared_ptr<std::default_random_engine> ptr_random_engine_t;
extern ptr_random_engine_t      ptr_rand_engine;

#endif // COMMON_H
