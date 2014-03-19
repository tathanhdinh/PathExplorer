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

#if defined(_WIN32) || defined(_WIN64)
namespace windows
{
#define NOMINMAX
#define INCL_WINSOCK_API_PROTOTYPES 1
#define INCL_WINSOCK_API_TYPEDEFS   1
#include <WinSock2.h>
#include <Windows.h>

typedef int recv_t(SOCKET, char*, int, int);
typedef int recvfrom_t(SOCKET, char*, int, int, struct sockaddr*, int*);
};
#endif

#include <boost/type_traits.hpp>

namespace capturing
{
extern auto initialize                  ()                                                -> void;

#if defined(_WIN32) || defined(_WIN64)

extern auto recvs_interceptor_before    (ADDRINT msg_addr, THREADID thread_id)            -> VOID;

extern auto recvs_interceptor_after     (UINT32 msg_length, THREADID thread_id)           -> VOID;

extern auto wsarecvs_interceptor_before (ADDRINT msg_struct_addr, THREADID thread_id)     -> VOID;

extern auto wsarecvs_interceptor_after  (THREADID thread_id)                              -> VOID;

typedef boost::function_traits<windows::recv_t> recv_traits;
extern auto recv_wrapper                (AFUNPTR recv_origin,
                                         /*windows::SOCKET*/recv_traits::arg1_type s,
                                         /*char**/recv_traits::arg2_type buf,
                                         /*int*/recv_traits::arg3_type len,
                                         /*int*/recv_traits::arg4_type flags,
                                         CONTEXT* p_ctxt, THREADID thread_id)             -> recv_traits::result_type;

typedef boost::function_traits<windows::recvfrom_t> recvfrom_traits;
extern auto recvfrom_wrapper            (AFUNPTR recvfrom_origin,
                                         /*windows::SOCKET*/recvfrom_traits::arg1_type s,
                                         /*char**/recvfrom_traits::arg2_type buf,
                                         /*int*/recvfrom_traits::arg3_type len,
                                         /*int*/recvfrom_traits::arg4_type flags,
                                         /*windows::sockaddr**/recvfrom_traits::arg5_type from,
                                         /*int**/recvfrom_traits::arg6_type fromlen,
                                         CONTEXT* p_ctxt, THREADID thread_id)              -> recvfrom_traits::result_type;

extern auto wsarecv_wrapper             (AFUNPTR wsarecv_origin,
                                         windows::SOCKET s,
                                         windows::LPWSABUF lpBuffers,
                                         windows::DWORD dwBufferCount,
                                         windows::LPDWORD lpNumberOfBytesRecvd,
                                         windows::LPDWORD lpFlags,
                                         windows::LPWSAOVERLAPPED lpOverlapped,
                                         windows::LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine,
                                         CONTEXT* p_ctxt, THREADID thread_id)             -> int;

extern auto wsarecvfrom_wrapper         (AFUNPTR wsarecvfrom_origin,
                                         windows::SOCKET s,
                                         windows::LPWSABUF lpBuffers,
                                         windows::DWORD dwBufferCount,
                                         windows::LPDWORD lpNumberOfBytesRecvd,
                                         windows::LPDWORD lpFlags,
                                         windows::sockaddr* lpFrom,
                                         windows::LPINT lpFromlen,
                                         windows::LPWSAOVERLAPPED lpOverlapped,
                                         windows::LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine,
                                         CONTEXT* p_ctxt, THREADID thread_id)             -> int;

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
