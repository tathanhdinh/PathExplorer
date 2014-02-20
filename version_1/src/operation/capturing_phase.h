#ifndef CAPTURING_PHASE_H
#define CAPTURING_PHASE_H

#include <pin.H>

namespace capturing
{
#if defined(_WIN32) || defined(_WIN64)
// instrument recv and recvfrom functions
extern VOID logging_before_recv_functions_analyzer(ADDRINT msg_addr);
extern VOID logging_after_recv_functions_analyzer(UINT32 msg_length);
// instrument WSARecv and WSARecvFrom
extern VOID logging_before_wsarecv_functions_analyzer(ADDRINT msg_struct_addr);
extern VOID logging_after_wsarecv_funtions_analyzer();
#elif defined(__gnu_linux__)
extern VOID syscall_entry_analyzer(THREADID thread_id, CONTEXT* p_ctxt,
                                   SYSCALL_STANDARD syscall_std, VOID *data);
extern VOID syscall_exit_analyzer(THREADID thread_id, CONTEXT* p_ctxt,
                                  SYSCALL_STANDARD syscall_std, VOID *data);
#endif
} // end of capturing namespace

#endif // CAPTURING_PHASE_H
