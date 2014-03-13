#ifndef CAPTURING_PHASE_H
#define CAPTURING_PHASE_H

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

namespace capturing
{
extern auto initialize            ()                                            -> void;

#if defined(_WIN32) || defined(_WIN64)

extern auto before_recvs          (ADDRINT msg_addr, THREADID thread_id)        -> VOID;

extern auto after_recvs           (UINT32 msg_length, THREADID thread_id)       -> VOID;

extern auto before_wsarecvs       (ADDRINT msg_struct_addr, THREADID thread_id) -> VOID;

extern auto after_wsarecvs        (THREADID thread_id)                          -> VOID;

#elif defined(__gnu_linux__)

extern VOID syscall_entry_analyzer(THREADID thread_id, CONTEXT* p_ctxt,
                                   SYSCALL_STANDARD syscall_std, VOID *data);

extern VOID syscall_exit_analyzer(THREADID thread_id, CONTEXT* p_ctxt,
                                  SYSCALL_STANDARD syscall_std, VOID *data);

#endif

extern auto mem_read_instruction  (ADDRINT ins_addr, ADDRINT r_mem_addr, UINT32 r_mem_size,
                                   CONTEXT* p_ctxt, THREADID thread_id)         -> VOID;
} // end of capturing namespace

#endif // CAPTURING_PHASE_H
