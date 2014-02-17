#ifndef TAINTING_FUNCTIONS_H
#define TAINTING_FUNCTIONS_H

#include <pin.H>
#include <boost/predef.h>

VOID logging_syscall_instruction_analyzer(ADDRINT ins_addr);

VOID logging_general_instruction_analyzer(ADDRINT ins_addr);

VOID logging_mem_read_instruction_analyzer(ADDRINT ins_addr,
                                           ADDRINT mem_read_addr, UINT32 mem_read_size,
                                           CONTEXT* p_ctxt);

// VOID logging_mem_read2_instruction_analyzer(ADDRINT ins_addr,
//                                             ADDRINT mem_read_addr, UINT32 mem_read_size,
//                                             CONTEXT* p_ctxt);

VOID logging_mem_write_instruction_analyzer(ADDRINT ins_addr,
                                            ADDRINT mem_written_addr, UINT32 mem_written_size);

VOID logging_cond_br_analyzer(ADDRINT ins_addr, bool br_taken);

#if BOOST_OS_WINDOWS
// instrument recv and recvfrom functions
VOID logging_before_recv_functions_analyzer(ADDRINT msg_addr);
VOID logging_after_recv_functions_analyzer(UINT32 msg_length);

// instrument WSARecv and WSARecvFrom
VOID logging_before_wsarecv_functions_analyzer(ADDRINT msg_struct_addr);
VOID logging_after_wsarecv_funtions_analyzer();
#elif BOOST_OS_LINUX
extern VOID syscall_entry_analyzer(THREADID thread_id, CONTEXT* p_ctxt,
                                   SYSCALL_STANDARD syscall_std, VOID *data);

extern VOID syscall_exit_analyzer(THREADID thread_id, CONTEXT* p_ctxt,
                                  SYSCALL_STANDARD syscall_std, VOID *data);
#endif

extern VOID tainting_general_instruction_analyzer(ADDRINT ins_addr);

// VOID tainting_mem_to_st_analyzer(ADDRINT ins_addr, ADDRINT mem_read_addr, UINT32 mem_read_size);

// VOID tainting_st_to_mem_analyzer(ADDRINT ins_addr, ADDRINT mem_written_addr, UINT32 mem_written_size);

#endif
