#ifndef CAPTURING_PHASE_H
#define CAPTURING_PHASE_H

// these definitions are not necessary (since they are defined already in the CMakeLists),
// they are added just to make qt-creator parsing headers
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
extern void initialize();

#if defined(_WIN32) || defined(_WIN64)
extern VOID before_recvs      (ADDRINT msg_addr, THREADID thread_id);

extern VOID after_recvs       (UINT32 msg_length, THREADID thread_id);

extern VOID before_wsarecvs   (ADDRINT msg_struct_addr, THREADID thread_id);

extern VOID after_wsarecvs    (THREADID thread_id);
#elif defined(__gnu_linux__)
extern VOID syscall_entry_analyzer(THREADID thread_id, CONTEXT* p_ctxt,
                                   SYSCALL_STANDARD syscall_std, VOID *data);

extern VOID syscall_exit_analyzer(THREADID thread_id, CONTEXT* p_ctxt,
                                  SYSCALL_STANDARD syscall_std, VOID *data);
#endif
} // end of capturing namespace

#endif // CAPTURING_PHASE_H
