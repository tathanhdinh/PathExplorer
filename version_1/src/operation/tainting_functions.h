#ifndef TAINTING_FUNCTIONS_H
#define TAINTING_FUNCTIONS_H

#include <pin.H>
#include <boost/predef.h>

namespace tainting
{
extern void initalize_tainting_phase();
extern VOID syscall_instruction(ADDRINT ins_addr);
extern VOID general_instruction(ADDRINT ins_addr);
extern VOID mem_read_instruction(ADDRINT ins_addr,
                                 ADDRINT mem_read_addr, UINT32 mem_read_size, CONTEXT* p_ctxt);
extern VOID mem_write_instruction(ADDRINT ins_addr,
                                  ADDRINT mem_written_addr, UINT32 mem_written_size);
#if BOOST_OS_WINDOWS
// instrument recv and recvfrom functions
extern VOID logging_before_recv_functions_analyzer(ADDRINT msg_addr);
extern VOID logging_after_recv_functions_analyzer(UINT32 msg_length);
// instrument WSARecv and WSARecvFrom
extern VOID logging_before_wsarecv_functions_analyzer(ADDRINT msg_struct_addr);
extern VOID logging_after_wsarecv_funtions_analyzer();
#elif BOOST_OS_LINUX
extern VOID syscall_entry_analyzer(THREADID thread_id, CONTEXT* p_ctxt,
                                   SYSCALL_STANDARD syscall_std, VOID *data);
extern VOID syscall_exit_analyzer(THREADID thread_id, CONTEXT* p_ctxt,
                                  SYSCALL_STANDARD syscall_std, VOID *data);
#endif
extern VOID graphical_propagation(ADDRINT ins_addr);

} // end of tainting namespace
#endif
