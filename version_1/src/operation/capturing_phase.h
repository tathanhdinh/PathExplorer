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

//#include <functional>
//#include <boost/type_traits/function_traits.hpp>


#if defined(_WIN32) || defined(_WIN64)
namespace windows
{
#define NOMINMAX
#include <WinSock2.h>
#include <Windows.h>

// workaround since the calling convention __stdcall is added always for Windows APIs
// the follows should be declared more elegantly by: typedef decltype(function_name) function_name_t
typedef int recv_t        (SOCKET, char*, int, int);
typedef int recvfrom_t    (SOCKET, char*, int, int, struct sockaddr*, int*);
typedef int wsarecv_t     (SOCKET, LPWSABUF, DWORD, LPDWORD, LPDWORD, LPWSAOVERLAPPED,
                           LPWSAOVERLAPPED_COMPLETION_ROUTINE);
typedef int wsarecvfrom_t (SOCKET, LPWSABUF, DWORD, LPDWORD, LPDWORD, struct sockaddr*, LPINT,
                           LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);
};
#endif

template <typename F> struct wrapper;

template <typename R, typename T1, typename T2, typename T3, typename T4>
struct wrapper<R(T1, T2, T3, T4)>
{
  typedef R result_type;
  typedef R (type)(AFUNPTR, T1, T2, T3, T4, CONTEXT*, THREADID);
};


template <typename R, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
struct wrapper<R(T1, T2, T3, T4, T5, T6)>
{
  typedef R result_type;
  typedef R (type)(AFUNPTR, T1, T2, T3, T4, T5, T6, CONTEXT*, THREADID);
};

template <typename R, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
          typename T7>
struct wrapper<R(T1, T2, T3, T4, T5, T6, T7)>
{
  typedef R result_type;
  typedef R (type)(AFUNPTR, T1, T2, T3, T4, T5, T6, T7, CONTEXT*, THREADID);
};

template <typename R, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
          typename T7, typename T8, typename T9>
struct wrapper<R(T1, T2, T3, T4, T5, T6, T7, T8, T9)>
{
  typedef R result_type;
  typedef R (type)(AFUNPTR, T1, T2, T3, T4, T5, T6, T7, T8, T9, CONTEXT*, THREADID);
};


namespace capturing
{
extern auto initialize                  ()                                                -> void;

#if defined(_WIN32) || defined(_WIN64)

extern auto recvs_interceptor_before    (ADDRINT msg_addr, THREADID thread_id)            -> VOID;

extern auto recvs_interceptor_after     (UINT32 msg_length, THREADID thread_id)           -> VOID;

extern auto wsarecvs_interceptor_before (ADDRINT msg_struct_addr, THREADID thread_id)     -> VOID;

extern auto wsarecvs_interceptor_after  (THREADID thread_id)                              -> VOID;

//typedef boost::function_traits<windows::recv_t> recv_traits;
//extern auto recv_wrapper                (AFUNPTR recv_origin,
//                                         recv_traits::arg1_type s,            // windows::SOCKET
//                                         recv_traits::arg2_type buf,          // char*
//                                         recv_traits::arg3_type len,          // int
//                                         recv_traits::arg4_type flags,        // int
//                                         CONTEXT* p_ctxt, THREADID thread_id) -> recv_traits::result_type;
extern wrapper<windows::recv_t>::type recv_wrapper;
typedef boost::function_traits<windows::recv_t> recv_traits_t;


//typedef boost::function_traits<windows::recvfrom_t> recvfrom_traits;
//extern auto recvfrom_wrapper            (AFUNPTR recvfrom_origin,
//                                         recvfrom_traits::arg1_type s,        // windows::SOCKET
//                                         recvfrom_traits::arg2_type buf,      // char*
//                                         recvfrom_traits::arg3_type len,      // int
//                                         recvfrom_traits::arg4_type flags,    // int
//                                         recvfrom_traits::arg5_type from,     // windows::sockaddr*
//                                         recvfrom_traits::arg6_type fromlen,  // int*
//                                         CONTEXT* p_ctxt, THREADID thread_id) -> recvfrom_traits::result_type;
extern wrapper<windows::recvfrom_t>::type recvfrom_wrapper;
typedef boost::function_traits<windows::recvfrom_t> recvfrom_traits_t;

//typedef boost::function_traits<windows::wsarecv_t> wsarecvfrom_traits;
//extern auto wsarecv_wrapper             (AFUNPTR wsarecv_origin,
//                                         wsarecvfrom_traits::arg1_type s,                     // windows::SOCKET
//                                         wsarecvfrom_traits::arg2_type lpBuffers,             // windows::LPWSABUF
//                                         wsarecvfrom_traits::arg3_type dwBufferCount,         // windows::DWORD
//                                         wsarecvfrom_traits::arg4_type lpNumberOfBytesRecvd,  // windows::LPDWORD
//                                         wsarecvfrom_traits::arg5_type lpFlags,               // windows::LPDWORD
//                                         wsarecvfrom_traits::arg6_type lpOverlapped,          // windows::LPWSAOVERLAPPED
//                                         wsarecvfrom_traits::arg7_type lpCompletionRoutine,   // windows::LPWSAOVERLAPPED_COMPLETION_ROUTINE
//                                         CONTEXT* p_ctxt, THREADID thread_id)             -> int;
extern wrapper<windows::wsarecv_t>::type wsarecv_wrapper;
typedef boost::function_traits<windows::wsarecv_t> wsarecv_traits_t;

//extern auto wsarecvfrom_wrapper         (AFUNPTR wsarecvfrom_origin,
//                                         windows::SOCKET s,
//                                         windows::LPWSABUF lpBuffers,
//                                         windows::DWORD dwBufferCount,
//                                         windows::LPDWORD lpNumberOfBytesRecvd,
//                                         windows::LPDWORD lpFlags,
//                                         windows::sockaddr* lpFrom,
//                                         windows::LPINT lpFromlen,
//                                         windows::LPWSAOVERLAPPED lpOverlapped,
//                                         windows::LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine,
//                                         CONTEXT* p_ctxt, THREADID thread_id)             -> int;
extern wrapper<windows::wsarecvfrom_t>::type wsarecvfrom_wrapper;
typedef boost::function_traits<windows::wsarecvfrom_t> wsarecvfrom_traits_t;

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
