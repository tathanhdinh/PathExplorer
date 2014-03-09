#ifndef COMMON_H
#define COMMON_H

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

#include <fstream>

#include "../base/checkpoint.h"
#include "../base/cond_direct_instruction.h"
#include "../util/tinyformat.h"
#if defined(ENABLE_FSA)
#include "../base/explorer_graph.h"
#endif

typedef enum
{
  capturing_phase   = 0,
  tainting_phase    = 1,
  rollbacking_phase = 2
}                                     running_phase;

typedef enum
{
  syscall_inexist  = 0,
  syscall_sendto   = 44,
  syscall_recvfrom = 45
}                                     syscall_id;

//#if defined(ENABLE_FSA)
//typedef std::map<UINT, path_code_t>   order_path_code_map_t;
//#endif

extern addr_ins_map_t                 ins_at_addr;
extern order_ins_map_t                ins_at_order;

extern UINT32                         total_rollback_times;
extern UINT32                         local_rollback_times;
extern UINT32                         trace_size;

extern UINT32                         max_total_rollback_times;
extern UINT32                         max_local_rollback_times;
extern UINT32                         max_trace_size;

extern ptr_checkpoints_t              saved_checkpoints;

extern ptr_cond_direct_inss_t         detected_input_dep_cfis;
extern ptr_cond_direct_ins_t          exploring_cfi;

extern UINT32                         current_exec_order;

#if defined(ENABLE_FSA)
extern path_code_t                    current_path_code;
//extern order_path_code_map_t          path_code_at_order;
extern ptr_explorer_graph_t           explored_fsa;
#endif

extern ADDRINT                        received_msg_addr;
extern UINT32                         received_msg_size;
extern UINT32                         received_msg_order;

extern THREADID                       traced_thread_id;
extern bool                           traced_thread_is_fixed;

#if defined(__gnu_linux__)
extern ADDRINT                        logged_syscall_index;   // logged syscall index
extern ADDRINT                        logged_syscall_args[6]; // logged syscall arguments
#endif

extern running_phase                  current_running_phase;

extern UINT64                         executed_ins_number;
extern UINT64                         econed_ins_number;

extern KNOB<UINT32>                   max_total_rollback;
extern KNOB<UINT32>                   max_local_rollback;
extern KNOB<UINT32>                   max_trace_length;

extern std::ofstream                  log_file;

#endif // COMMON_H
