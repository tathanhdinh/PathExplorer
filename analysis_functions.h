#ifndef ANALYSIS_FUNCTIONS_H
#define ANALYSIS_FUNCTIONS_H

#include <pin.H>

VOID unhandled_analyzer(ADDRINT ins_addr, UINT32 dst_opr_id, UINT32 src_opr_id);

VOID syscall_entry_analyzer(THREADID thread_id, CONTEXT* p_ctxt, SYSCALL_STANDARD syscall_std, VOID *data);

VOID syscall_exit_analyzer(THREADID thread_id, CONTEXT* p_ctxt, SYSCALL_STANDARD syscall_std, VOID *data);

#endif