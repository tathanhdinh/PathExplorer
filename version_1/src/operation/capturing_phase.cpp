#include "capturing_phase.h"
#include "common.h"
#include "../util/stuffs.h"

#include <algorithm>
#include <numeric>

namespace capturing 
{

/*================================================================================================*/

static UINT32   received_msg_number;

#if defined(_WIN32) || defined(_WIN64)
static ADDRINT  received_msg_struct_addr;
static bool     recv_is_locked;
static bool     recvfrom_is_locked;
static bool     wsarecv_is_locked;
static bool     wsarecvfrom_is_locked;
static bool     InternetReadFile_is_locked;
static std::map<std::string, bool> is_locked;
#endif


/*================================================================================================*/
/**
 * @brief initialize a new capturing phase
 */
auto initialize() -> void
{
#if defined(_WIN32) || defined(_WIN64)
  recv_is_locked = false; recvfrom_is_locked = false; wsarecv_is_locked = false;
  wsarecvfrom_is_locked = false;

  is_locked["recv"] = false; is_locked["recvfrom"] = false; is_locked["WSARecv"] = false;
  is_locked["WSARecvFrom"] = false; is_locked["InternetReadFile"] = false;
  is_locked["InternetReadFileEx"] = false;
#endif
  received_msg_number = 0; interested_msg_is_received = false;
  return;
}


/**
 * @brief prepare switching to a new tainting phase
 */
static inline auto prepare_new_tainting_phase() -> void
{
  // save a fresh copy of the input
  fresh_input.reset(new UINT8[received_msg_size]);
  std::copy(reinterpret_cast<UINT8*>(received_msg_addr),
            reinterpret_cast<UINT8*>(received_msg_addr) + received_msg_size, fresh_input.get());

  // switch to the tainting state
  current_running_phase = tainting_phase; PIN_RemoveInstrumentation();
#if !defined(NDEBUG)
  tfm::format(log_file, "%s\nthe message of order %d saved at %s with size %d bytes\n",
              "================================================================================",
              received_msg_number, addrint_to_hexstring(received_msg_addr), received_msg_size);
#endif
  return;
}


/**
 * @brief handle_received_message
 */
static inline auto handle_received_message() -> void
{
  auto all_lock_released = [&]() -> bool
  {
    auto func_locked_iter = is_locked.begin();
    for (; func_locked_iter != is_locked.end(); ++func_locked_iter)
    {
      if (func_locked_iter->second) break;
    }
    return (func_locked_iter == is_locked.end());

  };

  if ((received_msg_size > 0) && all_lock_released() /*!recv_is_locked && !wsarecv_is_locked*/)
  {
    received_msg_number++;
    // verify if the received message is the interesting message
    if (received_msg_number == received_msg_order)
    {
      // yes, then remove the current instrumentation for message receiving functions
      interested_msg_is_received = true; PIN_RemoveInstrumentation();
    }
  }
  return;
}


#if defined(_WIN32) || defined(_WIN64)
/**
 * @brief determine the received message address of recv or recvfrom
 */
auto recvs_interceptor_before(ADDRINT msg_addr, THREADID thread_id) -> VOID
{
  if (!traced_thread_is_fixed || (traced_thread_is_fixed && (traced_thread_id == thread_id)))
  {
    recv_is_locked = true; is_locked["recv"] = true; is_locked["recvfrom"] = false;

    traced_thread_id = thread_id; traced_thread_is_fixed = true; received_msg_addr = msg_addr;
  }
  return;
}


/**
 * @brief determine the received message length of recv or recvfrom
 */
auto recvs_interceptor_after(UINT32 msg_length, THREADID thread_id) -> VOID
{
  if (traced_thread_is_fixed && (thread_id == traced_thread_id) && recv_is_locked)
  {
    recv_is_locked = false; is_locked["recv"] = false; is_locked["recvfrom"] = false;

#if !defined(NDEBUG)
    tfm::format(log_file, "message is obtained from recv or recvfrom at thread id %d\n",
                thread_id);
#endif
    received_msg_size = msg_length; handle_received_message();
  }
  return;
}


/**
 * @brief determine the address a type LPWSABUF containing the address of the received message
 */
auto wsarecvs_interceptor_before(ADDRINT msg_struct_addr, THREADID thread_id) -> VOID
{
  if (!traced_thread_is_fixed || (traced_thread_is_fixed && (traced_thread_id == thread_id)))
  {
    // lock the message receiving with WSARecv or WSARecvFrom
    wsarecv_is_locked = true; is_locked["WSARecv"] = true; is_locked["WSARecvFrom"] = true;

    received_msg_struct_addr = msg_struct_addr;
    received_msg_addr = reinterpret_cast<ADDRINT>((reinterpret_cast<windows::LPWSABUF>(
                                                     received_msg_struct_addr))->buf);
    traced_thread_id = thread_id; traced_thread_is_fixed = true;
  }
  return;
}


/**
 * @brief determine the received message length or WSARecv or WSARecvFrom
 */
auto wsarecvs_interceptor_after(THREADID thread_id) -> VOID
{
  if (traced_thread_is_fixed && (thread_id == traced_thread_id) && wsarecv_is_locked)
  {
    // unlock the message receiving with WSARecv or WSARecvFrom
    wsarecv_is_locked = false; is_locked["WSARecv"] = false; is_locked["WSARecvFrom"] = false;

#if !defined(NDEBUG)
    tfm::format(log_file, "message is obtained from WSARecv or WSARecvFrom at thread id %d\n",
                thread_id);
#endif
    received_msg_size = (reinterpret_cast<windows::LPWSABUF>(received_msg_struct_addr))->len;
    handle_received_message();
  }
  return;
}


auto InternetReadFile_inserter_before(ADDRINT hFile,                 // HINTERNET
                                      ADDRINT lpBuffer,              // LPVOID
                                      ADDRINT dwNumberOfBytesToRead, // DWORD
                                      ADDRINT lpdwNumberOfBytesRead, // LPDWORD
                                      THREADID thread_id) -> VOID
{
  if (!traced_thread_is_fixed || (traced_thread_is_fixed && (traced_thread_id == thread_id)))
  {
    // lock the message receiving with InternetReadFile
    is_locked["InternetReadFile"] = true;

    received_msg_addr = lpBuffer;
    received_msg_struct_addr = lpdwNumberOfBytesRead;

    traced_thread_id = thread_id; traced_thread_is_fixed = true;
  }
  return;
}


auto InternetReadFile_inserter_after(ADDRINT is_successful, // BOOL
                                     THREADID thread_id) -> VOID
{
  if (traced_thread_is_fixed && (thread_id == traced_thread_id) && is_locked["InternetReadFile"])
  {
    // unlock the message receiving with InternetReadFile
    is_locked["InternetReadFile"] = false;

#if !defined(NDEBUG)
    tfm::format(log_file, "message is obtained from InternetReadFile at thread id %d\n", thread_id);
#endif

    if (static_cast<InternetReadFile_traits_t::result_type>/*BOOL*/(is_successful))
    {
      received_msg_size = *(reinterpret_cast<InternetReadFile_traits_t::arg4_type>/*LPDWORD*/(
                              received_msg_struct_addr));
      handle_received_message();
    }
  }
  return;
}


auto InternetReadFileEx_inserter_before(ADDRINT hFile,        // HINTERNET
                                        ADDRINT lpBuffersOut, // LPINTERNET_BUFFERS
                                        ADDRINT dwFlags,      // DWORD
                                        ADDRINT dwContext,    // DWORD_PTR
                                        THREADID thread_id) -> VOID
{
  if (!traced_thread_is_fixed || (traced_thread_is_fixed && (traced_thread_id == thread_id)))
  {
    // lock the message receiving with InternetReadFileEx
    is_locked["InternetReadFileEx"] = true;

    received_msg_addr = reinterpret_cast<ADDRINT>(
          (reinterpret_cast<InternetReadFileEx_traits_t::arg2_type>/*LPINTERNET_BUFFERS*/(
             lpBuffersOut))->lpvBuffer);
    received_msg_struct_addr = lpBuffersOut;
//    received_msg_size = (reinterpret_cast<InternetReadFileEx_traits_t::arg2_type>(lpBuffersOut))->dwBufferLength;

    traced_thread_id = thread_id; traced_thread_is_fixed = true;
  }
  return;
}


auto InternetReadFileEx_inserter_after(ADDRINT is_successful, THREADID thread_id) -> VOID
{
  if (traced_thread_is_fixed && (thread_id == traced_thread_id) && is_locked["InternetReadFileEx"])
  {
    // unlock the message receiving with InternetReadFileEx
    is_locked["InternetReadFileEx"] = false;

#if !defined(NDEBUG)
    tfm::format(log_file, "message is obtained from InternetReadFileEx at thread id %d\n", thread_id);
#endif

    if (static_cast<InternetReadFileEx_traits_t::result_type>/*BOOL*/(is_successful))
    {
      received_msg_size = (reinterpret_cast<InternetReadFileEx_traits_t::arg2_type>(
                             received_msg_struct_addr))->dwBufferLength;
      handle_received_message();
    }
  }
  return;
}

/**
 * @brief recv wrapper
 */
auto recv_wrapper(AFUNPTR recv_origin,
                  recv_traits_t::arg1_type s,          // windows::SOCKET
                  recv_traits_t::arg2_type buf,        // char*
                  recv_traits_t::arg3_type len,        // int
                  recv_traits_t::arg4_type flags,      // int
                  CONTEXT* p_ctxt, THREADID thread_id) -> recv_traits_t::result_type
{
  recv_traits_t::result_type result;

  if (!traced_thread_is_fixed || (traced_thread_is_fixed && (traced_thread_id == thread_id)))
  {
    received_msg_addr = reinterpret_cast<ADDRINT>(buf);
    traced_thread_id = thread_id; traced_thread_is_fixed = true; recv_is_locked = true;
  }

  std::cerr << "recv wrapper\n";
  PIN_CallApplicationFunction(p_ctxt, thread_id, CALLINGSTD_DEFAULT, recv_origin,
                              PIN_PARG(recv_traits_t::result_type), &result,  // int
                              PIN_PARG(recv_traits_t::arg1_type), s,          // windows::SOCKET
                              PIN_PARG(recv_traits_t::arg2_type), buf,        // char*
                              PIN_PARG(recv_traits_t::arg3_type), len,        // int
                              PIN_PARG(recv_traits_t::arg4_type), flags,      // int
                              PIN_PARG_END());

  if (traced_thread_is_fixed && (thread_id == traced_thread_id) && recv_is_locked)
  {
#if !defined(NDEBUG)
    tfm::format(log_file, "message is obtained from recv at thread id %d\n", thread_id);
#endif
    received_msg_size = result; recv_is_locked = false; handle_received_message();
  }

  return result;
}


/**
 * @brief recvfrom wrapper
 */
auto recvfrom_wrapper(AFUNPTR recvfrom_origin,
                      recvfrom_traits_t::arg1_type s,       // windows::SOCKET
                      recvfrom_traits_t::arg2_type buf,     // char*
                      recvfrom_traits_t::arg3_type len,     // int
                      recvfrom_traits_t::arg4_type flags,   // int
                      recvfrom_traits_t::arg5_type from,    // windows::sockaddr*
                      recvfrom_traits_t::arg6_type fromlen, // int*
                      CONTEXT* p_ctxt, THREADID thread_id) -> recvfrom_traits_t::result_type
{
  recvfrom_traits_t::result_type result;

  std::cerr << "recvfrom wrapper\n";
  if (!traced_thread_is_fixed || (traced_thread_is_fixed && (traced_thread_id == thread_id)))
  {
    received_msg_addr = reinterpret_cast<ADDRINT>(buf);
    traced_thread_id = thread_id; traced_thread_is_fixed = true; recv_is_locked = true;
  }

  PIN_CallApplicationFunction(p_ctxt, thread_id, CALLINGSTD_DEFAULT, recvfrom_origin,
                              PIN_PARG(recvfrom_traits_t::result_type), &result,  // int
                              PIN_PARG(recvfrom_traits_t::arg1_type), s,          // windows::SOCKET
                              PIN_PARG(recvfrom_traits_t::arg2_type), buf,        // char*
                              PIN_PARG(recvfrom_traits_t::arg3_type), len,        // int
                              PIN_PARG(recvfrom_traits_t::arg4_type), flags,      // int
                              PIN_PARG(recvfrom_traits_t::arg5_type), from,       // windows::sockaddr*
                              PIN_PARG(recvfrom_traits_t::arg6_type), fromlen,    // int*
                              PIN_PARG_END());

  if (traced_thread_is_fixed && (thread_id == traced_thread_id) && recv_is_locked)
  {
#if !defined(NDEBUG)
    tfm::format(log_file, "message is obtained from recvfrom at thread id %d\n", thread_id);
#endif
    received_msg_size = result; recv_is_locked = false; handle_received_message();
  }

  return result;
}

/**
 * @brief WSARecv wrapper
 */
auto WSARecv_wrapper(AFUNPTR wsarecv_origin,
                     WSARecv_traits_t::arg1_type s,                     // windows::SOCKET
                     WSARecv_traits_t::arg2_type lpBuffers,             // windows::LPWSABUF
                     WSARecv_traits_t::arg3_type dwBufferCount,         // ewindows::DWORD
                     WSARecv_traits_t::arg4_type lpNumberOfBytesRecvd,  // windows::LPDWORD
                     WSARecv_traits_t::arg5_type lpFlags,               // windows::LPDWORD
                     WSARecv_traits_t::arg6_type lpOverlapped,          // windows::LPWSAOVERLAPPED
                     WSARecv_traits_t::arg7_type lpCompletionRoutine,   // windows::LPWSAOVERLAPPED_COMPLETION_ROUTINE
                     CONTEXT* p_ctxt, THREADID thread_id) -> WSARecv_traits_t::result_type
{
  WSARecv_traits_t::result_type result;

  std::cerr << "wsarecv wrapper\n";
  if (!traced_thread_is_fixed || (traced_thread_is_fixed && (traced_thread_id == thread_id)))
  {
    received_msg_addr = reinterpret_cast<ADDRINT>(lpBuffers->buf);
    traced_thread_id = thread_id; traced_thread_is_fixed = true; wsarecv_is_locked = true;
  }

  PIN_CallApplicationFunction(p_ctxt, thread_id, CALLINGSTD_DEFAULT, wsarecv_origin,
                              PIN_PARG(WSARecv_traits_t::result_type), &result,
                              PIN_PARG(WSARecv_traits_t::arg1_type), s,
                              PIN_PARG(WSARecv_traits_t::arg2_type), lpBuffers,
                              PIN_PARG(WSARecv_traits_t::arg3_type), dwBufferCount,
                              PIN_PARG(WSARecv_traits_t::arg4_type), lpNumberOfBytesRecvd,
                              PIN_PARG(WSARecv_traits_t::arg5_type), lpFlags,
                              PIN_PARG(WSARecv_traits_t::arg6_type), lpOverlapped,
                              PIN_PARG(WSARecv_traits_t::arg7_type), lpCompletionRoutine,
                              PIN_PARG_END());

  if (traced_thread_is_fixed && (thread_id == traced_thread_id) && wsarecv_is_locked)
  {
#if !defined(NDEBUG)
    tfm::format(log_file, "message is obtained from WSARecv at thread id %d\n", thread_id);
#endif
    received_msg_size = *lpNumberOfBytesRecvd; wsarecv_is_locked = false; handle_received_message();
  }

  return result;
}


/**
 * @brief WSARecvFrom wrapper
 */
auto WSARecvFrom_wrapper(AFUNPTR origin_func,
                         WSARecvFrom_traits_t::arg1_type s,                     // windows::SOCKET
                         WSARecvFrom_traits_t::arg2_type lpBuffers,             // windows::LPWSABUF
                         WSARecvFrom_traits_t::arg3_type dwBufferCount,         // windows::DWORD
                         WSARecvFrom_traits_t::arg4_type lpNumberOfBytesRecvd,  // windows::LPDWORD
                         WSARecvFrom_traits_t::arg5_type lpFlags,               // windows::LPDWORD
                         WSARecvFrom_traits_t::arg6_type lpFrom,                // windows::sockaddr*
                         WSARecvFrom_traits_t::arg7_type lpFromlen,             // windows::LPINT
                         WSARecvFrom_traits_t::arg8_type lpOverlapped,          // windows::LPWSAOVERLAPPED
                         WSARecvFrom_traits_t::arg9_type lpCompletionRoutine,   // windows::LPWSAOVERLAPPED_COMPLETION_ROUTINE
                         CONTEXT* p_ctxt, THREADID thread_id) -> WSARecvFrom_traits_t::result_type
{
  WSARecvFrom_traits_t::result_type result;

  if (!traced_thread_is_fixed || (traced_thread_is_fixed && (traced_thread_id == thread_id)))
  {
    wsarecv_is_locked = true; is_locked["WSARecv"] = true;
    traced_thread_id = thread_id; traced_thread_is_fixed = true;
  }

  PIN_CallApplicationFunction(p_ctxt, thread_id, CALLINGSTD_DEFAULT, origin_func,
                              PIN_PARG(WSARecvFrom_traits_t::result_type), &result,
                              PIN_PARG(WSARecvFrom_traits_t::arg1_type), s,
                              PIN_PARG(WSARecvFrom_traits_t::arg2_type), lpBuffers,
                              PIN_PARG(WSARecvFrom_traits_t::arg3_type), dwBufferCount,
                              PIN_PARG(WSARecvFrom_traits_t::arg4_type), lpNumberOfBytesRecvd,
                              PIN_PARG(WSARecvFrom_traits_t::arg5_type), lpFlags,
                              PIN_PARG(WSARecvFrom_traits_t::arg6_type), lpFrom,
                              PIN_PARG(WSARecvFrom_traits_t::arg7_type), lpFromlen,
                              PIN_PARG(WSARecvFrom_traits_t::arg8_type), lpOverlapped,
                              PIN_PARG(WSARecvFrom_traits_t::arg9_type), lpCompletionRoutine,
                              PIN_PARG_END());

  if (traced_thread_is_fixed && (thread_id == traced_thread_id) && wsarecv_is_locked)
  {
#if !defined(NDEBUG)
    tfm::format(log_file, "message is obtained from WSARecvFrom at thread id %d\n", thread_id);
#endif
    wsarecv_is_locked = false; is_locked["WSARecvFrom"] = false;
    received_msg_addr = reinterpret_cast<ADDRINT>(lpBuffers->buf);
    received_msg_size = *lpNumberOfBytesRecvd;
    handle_received_message();
  }

  return result;
}


/**
 * @brief InternetReadFile wrapper
 */
auto InternetReadFile_wrapper(AFUNPTR origin_func,
                              InternetReadFile_traits_t::arg1_type hFile,                 // windows::HINTERNET
                              InternetReadFile_traits_t::arg2_type lpBuffer,              // windows::LPVOID
                              InternetReadFile_traits_t::arg3_type dwNumberOfBytesToRead, // windows::DWORD
                              InternetReadFile_traits_t::arg4_type lpdwNumberOfBytesRead, // windows::LPWORD
                              CONTEXT *p_ctxt, THREADID thread_id) -> InternetReadFile_traits_t::result_type
{
  InternetReadFile_traits_t::result_type result;

  if (!traced_thread_is_fixed || (traced_thread_is_fixed && (traced_thread_id == thread_id)))
  {
    is_locked["InternetReadFile"] = true;
    traced_thread_id = thread_id; traced_thread_is_fixed = true;
  }

  PIN_CallApplicationFunction(p_ctxt, thread_id, CALLINGSTD_DEFAULT, origin_func,
                              PIN_PARG(InternetReadFile_traits_t::result_type), &result,
                              PIN_PARG(InternetReadFile_traits_t::arg1_type), hFile,
                              PIN_PARG(InternetReadFile_traits_t::arg2_type), lpBuffer,
                              PIN_PARG(InternetReadFile_traits_t::arg3_type), dwNumberOfBytesToRead,
                              PIN_PARG(InternetReadFile_traits_t::arg4_type), lpdwNumberOfBytesRead,
                              PIN_PARG_END());

  if (traced_thread_is_fixed && (thread_id == traced_thread_id) && is_locked["InternetReadFile"])
  {
#if !defined(NDEBUG)
    tfm::format(log_file, "message is obtained from InternetReadFile at thread id %d\n", thread_id);
#endif
    is_locked["InternetReadFile"] = false;
    received_msg_addr = reinterpret_cast<ADDRINT>(lpBuffer);
    received_msg_size = *lpdwNumberOfBytesRead;
    handle_received_message();
  }

  return result;
}

#elif defined(__gnu_linux__)

VOID syscall_entry_analyzer(THREADID thread_id, CONTEXT* p_ctxt, SYSCALL_STANDARD syscall_std,
                            VOID *data)
{
  if (current_running_phase == capturing_phase)
  {
    logged_syscall_index = PIN_GetSyscallNumber(p_ctxt, syscall_std);
    if (logged_syscall_index == syscall_recvfrom)
    {
      for (UINT8 arg_id = 0; arg_id < 6; ++arg_id)
      {
        logged_syscall_args[arg_id] = PIN_GetSyscallArgument(p_ctxt, syscall_std, arg_id);
      }
    }
  }
  return;
}


/**
 * @brief syscall_exit_analyzer
 */
VOID syscall_exit_analyzer(THREADID thread_id, CONTEXT* p_ctxt, SYSCALL_STANDARD syscall_std,
                           VOID *data)
{
  if (current_running_phase == capturing_phase)
  {
    if (logged_syscall_index == syscall_recvfrom)
    {
      ADDRINT returned_value = PIN_GetSyscallReturn(p_ctxt, syscall_std);
      if (returned_value > 0)
      {
        received_msg_num++;
        received_msg_addr = logged_syscall_args[1]; received_msg_size = returned_value;
#if !defined(NDEBUG)
        tfm::format(log_file, "the first message saved at %s with size %d bytes\nstart tainting \
                    the first time with trace size %d\n", addrint_to_hexstring(received_msg_addr),
                    received_msg_size, max_trace_size);
#endif
        // the first received message is the considered input
        if (received_msg_num == 1)
        {
          // switch to the tainting state
          current_running_phase = tainting_state; PIN_RemoveInstrumentation();
        }
      }
    }
  }
  return;
}

#endif // defined(__gnu_linux__)


/**
 * @brief handle memory read instructions in the capturing phase
 */
auto mem_read_instruction (ADDRINT ins_addr, ADDRINT r_mem_addr, UINT32 r_mem_size, CONTEXT* p_ctxt,
                           THREADID thread_id) -> VOID
{
  if (thread_id == traced_thread_id)
  {
    // verify if the instruction read some addresses in the input buffer
    if (std::max(r_mem_addr, received_msg_addr) <
        std::min(r_mem_addr + r_mem_size, received_msg_addr + received_msg_size))
    {
      // yes, then start the tainting phase
      prepare_new_tainting_phase(); PIN_ExecuteAt(p_ctxt);
    }
  }
  return;
}

} // end of capturing namespace
