#include "capturing_phase.h"
#include "common.h"
#include "../util/stuffs.h"

namespace capturing 
{

/*================================================================================================*/

static UINT32   received_msg_number;

#if defined(_WIN32) || defined(_WIN64)
static ADDRINT  received_msg_struct_addr;
static std::map<std::string, bool> is_locked;
#endif


/*================================================================================================*/
/**
 * @brief initialize a new capturing phase
 */
auto initialize() -> void
{
#if defined(_WIN32) || defined(_WIN64)
  is_locked["recv"] = false;
  is_locked["recvfrom"] = false;
  is_locked["WSARecv"] = false;
  is_locked["WSARecvFrom"] = false;
  is_locked["InternetReadFile"] = false;
  is_locked["InternetReadFileEx"] = false;
#endif
  received_msg_number = 0; interested_msg_is_received = false;
  traced_thread_id = 0; traced_thread_is_fixed = true;
  return;
}


/**
 * @brief prepare switching to a new tainting phase
 */
static inline auto prepare_new_tainting_phase() -> void
{
  // save a fresh copy of the input
//  fresh_input.reset(new UINT8[received_msg_size]);
  fresh_input.reset(new UINT8[received_msg_size], std::default_delete<UINT8[]>());
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


static inline auto all_lock_released () -> bool
{
  auto func_locked_iter = is_locked.begin();
  for (; func_locked_iter != is_locked.end(); ++func_locked_iter)
  {
    if (func_locked_iter->second) break;
  }
  return (func_locked_iter == is_locked.end());
}


/**
 * @brief handle_received_message
 */
static inline auto handle_received_message () -> void
{
  if ((received_msg_size > 0) && all_lock_released())
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

///**
// * @brief replace recv function (quite boring C++ type traits get type of parameter)
// */
//static inline auto recv_replacer (RTN& rtn) -> void
//{
//  auto recv_proto = PROTO_Allocate(PIN_PARG(capturing::recv_traits_t::result_type),
//                                   CALLINGSTD_DEFAULT, "recv",
//                                   PIN_PARG(capturing::recv_traits_t::arg1_type),  // windows::SOCKET
//                                   PIN_PARG(capturing::recv_traits_t::arg2_type),  // char*
//                                   PIN_PARG(capturing::recv_traits_t::arg3_type),  // int
//                                   PIN_PARG(capturing::recv_traits_t::arg4_type),  // int
//                                   PIN_PARG_END());

//  RTN_ReplaceSignature(rtn, AFUNPTR(capturing::recv_wrapper),
//                       IARG_PROTOTYPE, recv_proto,
//                       IARG_ORIG_FUNCPTR,
//                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
//                       IARG_FUNCARG_ENTRYPOINT_VALUE, 2, IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
//                       IARG_CONST_CONTEXT, IARG_THREAD_ID, IARG_END);
//  PROTO_Free(recv_proto);

//  return;
//}


//static inline auto recvfrom_replacer (RTN& rtn) -> void
//{
//  auto recvfrom_proto = PROTO_Allocate(PIN_PARG(capturing::recvfrom_traits_t::result_type),
//                                       CALLINGSTD_DEFAULT, "recvfrom",
//                                       PIN_PARG(capturing::recvfrom_traits_t::arg1_type),  // windows::SOCKET
//                                       PIN_PARG(capturing::recvfrom_traits_t::arg2_type),  // char*
//                                       PIN_PARG(capturing::recvfrom_traits_t::arg3_type),  // int
//                                       PIN_PARG(capturing::recvfrom_traits_t::arg4_type),  // int
//                                       PIN_PARG(capturing::recvfrom_traits_t::arg5_type),  // windows::sockaddr*
//                                       PIN_PARG(capturing::recvfrom_traits_t::arg6_type),  // int*
//                                       PIN_PARG_END());

//  RTN_ReplaceSignature(rtn, AFUNPTR(capturing::recvfrom_wrapper),
//                       IARG_PROTOTYPE, recvfrom_proto,
//                       IARG_ORIG_FUNCPTR,
//                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
//                       IARG_FUNCARG_ENTRYPOINT_VALUE, 2, IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
//                       IARG_FUNCARG_ENTRYPOINT_VALUE, 4, IARG_FUNCARG_ENTRYPOINT_VALUE, 5,
//                       IARG_CONST_CONTEXT, IARG_THREAD_ID, IARG_END);
//  PROTO_Free(recvfrom_proto);
//}

///**
// * @brief replace WSARecv function
// */
//static inline auto WSARecv_replacer (RTN& rtn) -> void
//{
//  auto WSARecv_proto = PROTO_Allocate(PIN_PARG(capturing::WSARecv_traits_t::result_type),
//                                      CALLINGSTD_DEFAULT, "WSARecv",
//                                      PIN_PARG(capturing::WSARecv_traits_t::arg1_type),
//                                      PIN_PARG(capturing::WSARecv_traits_t::arg2_type),
//                                      PIN_PARG(capturing::WSARecv_traits_t::arg3_type),
//                                      PIN_PARG(capturing::WSARecv_traits_t::arg4_type),
//                                      PIN_PARG(capturing::WSARecv_traits_t::arg5_type),
//                                      PIN_PARG(capturing::WSARecv_traits_t::arg6_type),
//                                      PIN_PARG(capturing::WSARecv_traits_t::arg7_type),
//                                      PIN_PARG_END());

//  RTN_ReplaceSignature(rtn, AFUNPTR(capturing::WSARecv_wrapper),
//                       IARG_PROTOTYPE, WSARecv_proto,
//                       IARG_ORIG_FUNCPTR,
//                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
//                       IARG_FUNCARG_ENTRYPOINT_VALUE, 2, IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
//                       IARG_FUNCARG_ENTRYPOINT_VALUE, 4, IARG_FUNCARG_ENTRYPOINT_VALUE, 5,
//                       IARG_FUNCARG_ENTRYPOINT_VALUE, 6,
//                       IARG_CONST_CONTEXT, IARG_THREAD_ID, IARG_END);
//  PROTO_Free(WSARecv_proto);

//  return;
//}


///**
// * @brief WSARecvFrom replacer
// */
//static inline auto WSARecvFrom_replacer (RTN& rtn) -> void
//{
//  auto WSARecvFrom_proto = PROTO_Allocate(PIN_PARG(capturing::WSARecvFrom_traits_t::result_type),
//                                          CALLINGSTD_DEFAULT, "WSARecvFrom",
//                                          PIN_PARG(capturing::WSARecvFrom_traits_t::arg1_type),
//                                          PIN_PARG(capturing::WSARecvFrom_traits_t::arg2_type),
//                                          PIN_PARG(capturing::WSARecvFrom_traits_t::arg3_type),
//                                          PIN_PARG(capturing::WSARecvFrom_traits_t::arg4_type),
//                                          PIN_PARG(capturing::WSARecvFrom_traits_t::arg5_type),
//                                          PIN_PARG(capturing::WSARecvFrom_traits_t::arg6_type),
//                                          PIN_PARG(capturing::WSARecvFrom_traits_t::arg7_type),
//                                          PIN_PARG(capturing::WSARecvFrom_traits_t::arg8_type),
//                                          PIN_PARG(capturing::WSARecvFrom_traits_t::arg9_type),
//                                          PIN_PARG_END());

//  RTN_ReplaceSignature(rtn, AFUNPTR(capturing::WSARecvFrom_wrapper),
//                       IARG_PROTOTYPE, WSARecvFrom_proto,
//                       IARG_ORIG_FUNCPTR,
//                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
//                       IARG_FUNCARG_ENTRYPOINT_VALUE, 2, IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
//                       IARG_FUNCARG_ENTRYPOINT_VALUE, 4, IARG_FUNCARG_ENTRYPOINT_VALUE, 5,
//                       IARG_FUNCARG_ENTRYPOINT_VALUE, 6, IARG_FUNCARG_ENTRYPOINT_VALUE, 7,
//                       IARG_FUNCARG_ENTRYPOINT_VALUE, 8,
//                       IARG_CONST_CONTEXT, IARG_THREAD_ID, IARG_END);
//  PROTO_Free(WSARecvFrom_proto);
//  return;
//}


///**
// * @brief InternetReadFile replacer
// */
//static inline auto InternetReadFile_replacer (RTN& rtn) -> void
//{
//  auto InternetReadFile_proto = PROTO_Allocate(PIN_PARG(capturing::InternetReadFile_traits_t::result_type),
//                                               CALLINGSTD_DEFAULT, "InternetReadFile",
//                                               PIN_PARG(capturing::InternetReadFile_traits_t::arg1_type),
//                                               PIN_PARG(capturing::InternetReadFile_traits_t::arg2_type),
//                                               PIN_PARG(capturing::InternetReadFile_traits_t::arg3_type),
//                                               PIN_PARG(capturing::InternetReadFile_traits_t::arg4_type),
//                                               PIN_PARG_END());
//  RTN_ReplaceSignature(rtn, AFUNPTR(capturing::InternetReadFile_wrapper),
//                       IARG_PROTOTYPE, InternetReadFile_proto,
//                       IARG_ORIG_FUNCPTR,
//                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
//                       IARG_FUNCARG_ENTRYPOINT_VALUE, 2, IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
//                       IARG_CONST_CONTEXT, IARG_THREAD_ID, IARG_END);
//  PROTO_Free(InternetReadFile_proto);
//  return;
//}

static auto recv_inserter_before (ADDRINT s,      // SOCKET
                                  ADDRINT buf,    // char*
                                  ADDRINT len,    // int
                                  ADDRINT flags,  // int
                                  THREADID thread_id) -> VOID
{
  if (!traced_thread_is_fixed || (traced_thread_is_fixed && (traced_thread_id == thread_id)))
  {
    // verify if the recv is not in any calling chain of another function
    if (all_lock_released()) received_msg_addr = buf;

    // lock recv to note that it is called
    is_locked["recv"] = true;
    traced_thread_id = thread_id; traced_thread_is_fixed = true;
  }
  return;
}


static auto recv_inserter_after (ADDRINT numberOfBytesReceived, // int
                                 THREADID thread_id) -> VOID
{
  if (traced_thread_is_fixed && (thread_id == traced_thread_id) && is_locked["recv"])
  {
    is_locked["recv"] = false;

    // verify if the recv is not in any calling chain of another function
    if (all_lock_released() && (static_cast<recv_traits_t::result_type>(numberOfBytesReceived) > 0))
    {
#if !defined(NDEBUG)
      tfm::format(log_file, "message is received from recv at thread id %d\n", thread_id);
#endif
      received_msg_size = numberOfBytesReceived; handle_received_message();
    }
  }
  return;
}


/**
 * @brief recv interception
 */
auto recv_routine (RTN& rtn) -> void
{
  // insertion approach
  RTN_Open(rtn);
  RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)recv_inserter_before,
                 IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                 IARG_FUNCARG_ENTRYPOINT_VALUE, 2, IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
                 IARG_THREAD_ID, IARG_END);
  RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)recv_inserter_after,
                 IARG_FUNCRET_EXITPOINT_VALUE,
                 IARG_THREAD_ID, IARG_END);
  RTN_Close(rtn);
  return;
}


static auto recvfrom_inserter_before (ADDRINT s,        // SOCKET
                                      ADDRINT buf,      // char*
                                      ADDRINT len,      // len
                                      ADDRINT flags,    // flags
                                      ADDRINT from,     // struct sockaddr*
                                      ADDRINT fromlen,  // int*
                                      THREADID thread_id) -> VOID
{
  if (!traced_thread_is_fixed || (traced_thread_is_fixed && (traced_thread_id == thread_id)))
  {
    // verify if the recvfrom is not in the calling chain of another function
    if (all_lock_released()) received_msg_addr = buf;

    // lock recvfrom to note that it is called
    is_locked["recvfrom"] = true;
    traced_thread_id = thread_id; traced_thread_is_fixed = true;
  }
  return;
}


static auto recvfrom_inserter_after (ADDRINT numberOfBytesReceived, // int
                                     THREADID thread_id) -> VOID
{
  if (traced_thread_is_fixed && (thread_id == traced_thread_id) && is_locked["recvfrom"])
  {
    is_locked["recvfrom"] = false;

    // verify if the recvfrom is not in any calling chain of another function
    if (all_lock_released() && (static_cast<recv_traits_t::result_type>(numberOfBytesReceived) > 0))
    {
#if !defined(NDEBUG)
      tfm::format(log_file, "message is received from recvfrom at thread id %d\n", thread_id);
#endif
      received_msg_size = numberOfBytesReceived; handle_received_message();
    }
  }
  return;
}


/**
 * @brief recvfrom interception
 */
auto recvfrom_routine (RTN& rtn) -> void
{
  // insertion approach
  RTN_Open(rtn);
  RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)recvfrom_inserter_before,
                 IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                 IARG_FUNCARG_ENTRYPOINT_VALUE, 2, IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
                 IARG_FUNCARG_ENTRYPOINT_VALUE, 4, IARG_FUNCARG_ENTRYPOINT_VALUE, 5,
                 IARG_THREAD_ID, IARG_END);
  RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)recvfrom_inserter_after,
                 IARG_FUNCRET_EXITPOINT_VALUE,
                 IARG_THREAD_ID, IARG_END);
  RTN_Close(rtn);
  return;
}


static auto WSARecv_inserter_before (ADDRINT s,                     // SOCKET
                                     ADDRINT lpBuffers,             // LPWSABUF
                                     ADDRINT dwBufferCount,         // DWORD
                                     ADDRINT lpNumberOfBytesRecvd,  // LPDWORD
                                     ADDRINT lpFlags,               // LPDWORD
                                     ADDRINT lpOverlapped,          // LPWSAOVERLAPPED
                                     ADDRINT lpCompletionRoutine,   // LPWSAOVERLAPPED_COMPLETION_ROUTINE
                                     THREADID thread_id) -> VOID
{
  if (!traced_thread_is_fixed || (traced_thread_is_fixed && (traced_thread_id == thread_id)))
  {
    // verify if the WSARecv is not in any calling chain of another function
    if (all_lock_released())
    {
      received_msg_addr = reinterpret_cast<ADDRINT>((reinterpret_cast<WSARecv_traits_t::arg2_type>(
                                                       lpBuffers)->buf));
      received_msg_struct_addr = lpBuffers;
    }

    // lock WSARecv to note that it is called
    is_locked["WSARecv"] = true;
    traced_thread_id = thread_id; traced_thread_is_fixed = true;
  }
  return;
}


static auto WSARecv_inserter_after (ADDRINT errorCode, // int
                                    THREADID thread_id) -> VOID
{
  if (traced_thread_is_fixed && (thread_id == traced_thread_id) && is_locked["WSARecv"])
  {
    is_locked["WSARecv"] = false;

    // verify if the WSARecv is not in any calling chain of another function
    if (all_lock_released() && (static_cast<WSARecv_traits_t::result_type>(errorCode) == 0))
    {
#if !defined(NDEBUG)
      tfm::format(log_file, "message is received from WSARecv at thread id %d\n", thread_id);
#endif
      received_msg_size = (reinterpret_cast<WSARecv_traits_t::arg2_type>(
                             received_msg_struct_addr))->len;
      handle_received_message();
    }
  }
  return;
}


/**
 * @brief WSARecv interception
 */
auto WSARecv_routine (RTN& rtn) -> void
{
  // insertion approach
  RTN_Open(rtn);
  RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)WSARecv_inserter_before,
                 IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                 IARG_FUNCARG_ENTRYPOINT_VALUE, 2, IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
                 IARG_FUNCARG_ENTRYPOINT_VALUE, 4, IARG_FUNCARG_ENTRYPOINT_VALUE, 5,
                 IARG_FUNCARG_ENTRYPOINT_VALUE, 6,
                 IARG_THREAD_ID, IARG_END);
  RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)WSARecv_inserter_after,
                 IARG_FUNCRET_EXITPOINT_VALUE,
                 IARG_THREAD_ID, IARG_END);
  RTN_Close(rtn);
  return;
}


static auto WSARecvFrom_inserter_before(ADDRINT s,                    // SOCKET
                                        ADDRINT lpBuffers,            // LPWSABUF
                                        ADDRINT dwBufferCount,        // DWORD
                                        ADDRINT lpNumberOfBytesRecvd, // LPDWORD
                                        ADDRINT lpFlags,              // LPDWORD
                                        ADDRINT lpFrom,               // struct sockaddr*
                                        ADDRINT lpFromlen,            // LPINT
                                        ADDRINT lpOverlapped,         // LPWSAOVERLAPPED
                                        ADDRINT lpCompletionRoutine,  // LPWSAOVERLAPPED_COMPLETION_ROUTINE
                                        THREADID thread_id) -> VOID
{
  if (!traced_thread_is_fixed || (traced_thread_is_fixed && (traced_thread_id == thread_id)))
  {
    // verify if WSARecvFrom is not in any calling chain of another function
    if (all_lock_released())
    {
      received_msg_addr = reinterpret_cast<ADDRINT>((reinterpret_cast<windows::LPWSABUF>(
                                                       lpBuffers)->buf));
      received_msg_struct_addr = lpBuffers;
    }

    // lock WSARecvFrom to note that it is called
    is_locked["WSARecvFrom"] = true;
    traced_thread_id = thread_id; traced_thread_is_fixed = true;
  }
  return;
}


static auto WSARecvFrom_inserter_after (ADDRINT errorCode, // int
                                        THREADID thread_id) -> VOID
{
  if (traced_thread_is_fixed && (thread_id == traced_thread_id) && is_locked["WSARecvFrom"])
  {
    is_locked["WSARecvFrom"] = false;

    // verify if the WSARecvFrom is not in any calling chain of another function
    if (all_lock_released() && (static_cast<WSARecvFrom_traits_t::result_type>(errorCode) == 0))
    {
#if !defined(NDEBUG)
      tfm::format(log_file, "message is received from WSARecvFrom at thread id %d\n", thread_id);
#endif
      received_msg_size = (reinterpret_cast<WSARecvFrom_traits_t::arg2_type>(
                             received_msg_struct_addr))->len;
      handle_received_message();
    }
  }
  return;
}


/**
 * @brief WSARecvFrom interception
 */
auto WSARecvFrom_routine (RTN& rtn) -> void
{
  // insertion approach
  RTN_Open(rtn);
  RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)WSARecvFrom_inserter_before,
                 IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                 IARG_FUNCARG_ENTRYPOINT_VALUE, 2, IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
                 IARG_FUNCARG_ENTRYPOINT_VALUE, 4, IARG_FUNCARG_ENTRYPOINT_VALUE, 5,
                 IARG_FUNCARG_ENTRYPOINT_VALUE, 6, IARG_FUNCARG_ENTRYPOINT_VALUE, 7,
                 IARG_FUNCARG_ENTRYPOINT_VALUE, 8,
                 IARG_THREAD_ID, IARG_END);
  RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)WSARecvFrom_inserter_after,
                 IARG_FUNCRET_EXITPOINT_VALUE,
                 IARG_THREAD_ID, IARG_END);
  RTN_Close(rtn);
  return;
}


/**
 * @brief handle InternetReadFile before its execution
 */
static auto InternetReadFile_inserter_before(ADDRINT hFile,                 // HINTERNET
                                             ADDRINT lpBuffer,              // LPVOID
                                             ADDRINT dwNumberOfBytesToRead, // DWORD
                                             ADDRINT lpdwNumberOfBytesRead, // LPDWORD
                                             THREADID thread_id) -> VOID
{
  if (!traced_thread_is_fixed || (traced_thread_is_fixed && (traced_thread_id == thread_id)))
  {
    // verify if InternetReadFile is not any calling chain of another function
    if (all_lock_released())
    {
      received_msg_addr = lpBuffer;
      received_msg_struct_addr = lpdwNumberOfBytesRead;
    }

    // lock InternetReadFile to note that it is called
    is_locked["InternetReadFile"] = true;
    traced_thread_id = thread_id; traced_thread_is_fixed = true;
  }
  return;
}


static auto InternetReadFile_inserter_after(ADDRINT is_successful, // BOOL
                                            THREADID thread_id) -> VOID
{
  if (traced_thread_is_fixed && (thread_id == traced_thread_id) && is_locked["InternetReadFile"])
  {
    // unlock the message receiving with InternetReadFile
    is_locked["InternetReadFile"] = false;

    // verify if InternetReadFile is not any calling chain of another function
    if (all_lock_released() && static_cast<InternetReadFile_traits_t::result_type>(is_successful))
    {
#if !defined(NDEBUG)
      tfm::format(log_file, "message is received from InternetReadFile at thread id %d\n", thread_id);
#endif
      received_msg_size = *(reinterpret_cast<InternetReadFile_traits_t::arg4_type>(
                              received_msg_struct_addr));
      handle_received_message();
    }
  }
  return;
}


/**
 * @brief InternetReadFile interception
 */
auto InternetReadFile_routine (RTN& rtn) -> void
{
  // insertion approach
  RTN_Open(rtn);
  RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)InternetReadFile_inserter_before,
                 IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                 IARG_FUNCARG_ENTRYPOINT_VALUE, 2, IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
                 IARG_THREAD_ID, IARG_END);
  RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)InternetReadFile_inserter_after,
                 IARG_FUNCRET_EXITPOINT_VALUE, IARG_THREAD_ID, IARG_END);
  RTN_Close(rtn);
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
    // verify if InternetReadFileEx is not any calling chain of another function
    if (all_lock_released())
    {
      received_msg_addr = reinterpret_cast<ADDRINT>(
            (reinterpret_cast<InternetReadFileEx_traits_t::arg2_type>(lpBuffersOut))->lpvBuffer);
      received_msg_struct_addr = lpBuffersOut;
    }

    // lock InternetReadFileEx to note that it is called
    is_locked["InternetReadFileEx"] = true;
    traced_thread_id = thread_id; traced_thread_is_fixed = true;
  }
  return;
}


auto InternetReadFileEx_inserter_after(ADDRINT is_successful, // BOOL
                                       THREADID thread_id) -> VOID
{
  if (traced_thread_is_fixed && (thread_id == traced_thread_id) && is_locked["InternetReadFileEx"])
  {
    // unlock the message receiving with InternetReadFileEx
    is_locked["InternetReadFileEx"] = false;

    // verify if InternetReadFile is not any calling chain of another function
    if (all_lock_released() && static_cast<InternetReadFileEx_traits_t::result_type>(is_successful))
    {
#if !defined(NDEBUG)
      tfm::format(log_file, "message is received from InternetReadFileEx at thread id %d\n", thread_id);
#endif
      received_msg_size = (reinterpret_cast<InternetReadFileEx_traits_t::arg2_type>(
                             received_msg_struct_addr))->dwBufferLength;
      handle_received_message();
    }
  }
  return;
}


/**
 * @brief InternetReadFileEx interception
 */
auto InternetReadFileEx_routine (RTN& rtn) -> void
{
  // insertion approach
  RTN_Open(rtn);
  RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)InternetReadFileEx_inserter_before,
                 IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                 IARG_FUNCARG_ENTRYPOINT_VALUE, 2, IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
                 IARG_THREAD_ID, IARG_END);
  RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)InternetReadFileEx_inserter_after,
                 IARG_FUNCRET_EXITPOINT_VALUE,
                 IARG_THREAD_ID, IARG_END);
  RTN_Close(rtn);
  return;
}


/**
 * @brief generic inserter
 */
static auto generic_insertion (ADDRINT rtn_addr, THREADID thread_id, bool before_or_after) -> VOID
{
#if !defined(NDEBUG)
  tfm::format(log_file, "<%d: %s> %s ", thread_id, addrint_to_hexstring(rtn_addr),
              RTN_FindNameByAddress(rtn_addr));
  if (before_or_after)
    tfm::format(log_file, "is called\n");
  else
    tfm::format(log_file, "returns\n");
#endif
  return;
};


/**
 * @brief generic routine interception
 */
auto generic_routine (RTN& rtn) -> void
{
  // insertion approach
  RTN_Open(rtn);
  auto rtn_addr = RTN_Address(rtn);
  RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)generic_insertion, IARG_ADDRINT, rtn_addr,
                 IARG_THREAD_ID, IARG_BOOL, true, IARG_END);
  RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)generic_insertion, IARG_ADDRINT, rtn_addr,
                 IARG_THREAD_ID, IARG_BOOL, false, IARG_END);
  RTN_Close(rtn);
  return;
}


///**
// * @brief recv wrapper
// */
//auto recv_wrapper(AFUNPTR recv_origin,
//                  recv_traits_t::arg1_type s,          // windows::SOCKET
//                  recv_traits_t::arg2_type buf,        // char*
//                  recv_traits_t::arg3_type len,        // int
//                  recv_traits_t::arg4_type flags,      // int
//                  CONTEXT* p_ctxt, THREADID thread_id) -> recv_traits_t::result_type
//{
//  recv_traits_t::result_type result;

//  if (!traced_thread_is_fixed || (traced_thread_is_fixed && (traced_thread_id == thread_id)))
//  {
//    is_locked["recv"] = true;
//    traced_thread_id = thread_id; traced_thread_is_fixed = true;
//  }

//  PIN_CallApplicationFunction(p_ctxt, thread_id, CALLINGSTD_DEFAULT, recv_origin,
//                              PIN_PARG(recv_traits_t::result_type), &result,  // int
//                              PIN_PARG(recv_traits_t::arg1_type), s,          // windows::SOCKET
//                              PIN_PARG(recv_traits_t::arg2_type), buf,        // char*
//                              PIN_PARG(recv_traits_t::arg3_type), len,        // int
//                              PIN_PARG(recv_traits_t::arg4_type), flags,      // int
//                              PIN_PARG_END());

//  if (traced_thread_is_fixed && (thread_id == traced_thread_id) && is_locked["recv"])
//  {
//#if !defined(NDEBUG)
//    tfm::format(log_file, "message is obtained from recv at thread id %d\n", thread_id);
//#endif
//    is_locked["recv"] = false;
//    received_msg_addr = reinterpret_cast<ADDRINT>(buf); received_msg_size = result;
//    handle_received_message();
//  }

//  return result;
//}


///**
// * @brief recvfrom wrapper
// */
//auto recvfrom_wrapper(AFUNPTR recvfrom_origin,
//                      recvfrom_traits_t::arg1_type s,       // windows::SOCKET
//                      recvfrom_traits_t::arg2_type buf,     // char*
//                      recvfrom_traits_t::arg3_type len,     // int
//                      recvfrom_traits_t::arg4_type flags,   // int
//                      recvfrom_traits_t::arg5_type from,    // windows::sockaddr*
//                      recvfrom_traits_t::arg6_type fromlen, // int*
//                      CONTEXT* p_ctxt, THREADID thread_id) -> recvfrom_traits_t::result_type
//{
//  recvfrom_traits_t::result_type result;

//  if (!traced_thread_is_fixed || (traced_thread_is_fixed && (traced_thread_id == thread_id)))
//  {
//    is_locked["recv"] = true;
//    traced_thread_id = thread_id; traced_thread_is_fixed = true;
//  }

//  PIN_CallApplicationFunction(p_ctxt, thread_id, CALLINGSTD_DEFAULT, recvfrom_origin,
//                              PIN_PARG(recvfrom_traits_t::result_type), &result,  // int
//                              PIN_PARG(recvfrom_traits_t::arg1_type), s,          // windows::SOCKET
//                              PIN_PARG(recvfrom_traits_t::arg2_type), buf,        // char*
//                              PIN_PARG(recvfrom_traits_t::arg3_type), len,        // int
//                              PIN_PARG(recvfrom_traits_t::arg4_type), flags,      // int
//                              PIN_PARG(recvfrom_traits_t::arg5_type), from,       // windows::sockaddr*
//                              PIN_PARG(recvfrom_traits_t::arg6_type), fromlen,    // int*
//                              PIN_PARG_END());

//  if (traced_thread_is_fixed && (thread_id == traced_thread_id) && is_locked["recv"])
//  {
//#if !defined(NDEBUG)
//    tfm::format(log_file, "message is obtained from recvfrom at thread id %d\n", thread_id);
//#endif
//    is_locked["recv"] = false;
//    received_msg_addr = reinterpret_cast<ADDRINT>(buf); received_msg_size = result;
//    handle_received_message();
//  }

//  return result;
//}

///**
// * @brief WSARecv wrapper
// */
//auto WSARecv_wrapper(AFUNPTR wsarecv_origin,
//                     WSARecv_traits_t::arg1_type s,                     // windows::SOCKET
//                     WSARecv_traits_t::arg2_type lpBuffers,             // windows::LPWSABUF
//                     WSARecv_traits_t::arg3_type dwBufferCount,         // windows::DWORD
//                     WSARecv_traits_t::arg4_type lpNumberOfBytesRecvd,  // windows::LPDWORD
//                     WSARecv_traits_t::arg5_type lpFlags,               // windows::LPDWORD
//                     WSARecv_traits_t::arg6_type lpOverlapped,          // windows::LPWSAOVERLAPPED
//                     WSARecv_traits_t::arg7_type lpCompletionRoutine,   // windows::LPWSAOVERLAPPED_COMPLETION_ROUTINE
//                     CONTEXT* p_ctxt, THREADID thread_id) -> WSARecv_traits_t::result_type
//{
//  WSARecv_traits_t::result_type result;

//  if (!traced_thread_is_fixed || (traced_thread_is_fixed && (traced_thread_id == thread_id)))
//  {
//    is_locked["WSARecv"] = true;
//    traced_thread_id = thread_id; traced_thread_is_fixed = true;
//  }

//  PIN_CallApplicationFunction(p_ctxt, thread_id, CALLINGSTD_DEFAULT, wsarecv_origin,
//                              PIN_PARG(WSARecv_traits_t::result_type), &result,
//                              PIN_PARG(WSARecv_traits_t::arg1_type), s,
//                              PIN_PARG(WSARecv_traits_t::arg2_type), lpBuffers,
//                              PIN_PARG(WSARecv_traits_t::arg3_type), dwBufferCount,
//                              PIN_PARG(WSARecv_traits_t::arg4_type), lpNumberOfBytesRecvd,
//                              PIN_PARG(WSARecv_traits_t::arg5_type), lpFlags,
//                              PIN_PARG(WSARecv_traits_t::arg6_type), lpOverlapped,
//                              PIN_PARG(WSARecv_traits_t::arg7_type), lpCompletionRoutine,
//                              PIN_PARG_END());

//  if (traced_thread_is_fixed && (thread_id == traced_thread_id) && is_locked["WSARecv"])
//  {
//#if !defined(NDEBUG)
//    tfm::format(log_file, "message is obtained from WSARecv at thread id %d\n", thread_id);
//#endif
//    log_file.flush();
//    is_locked["WSARecv"] = false;
//    received_msg_addr = reinterpret_cast<ADDRINT>(lpBuffers->buf);
//    received_msg_size = *lpNumberOfBytesRecvd; handle_received_message();
//  }
//  std::cerr << "WSARecv exists\n";

//  return result;
//}


///**
// * @brief WSARecvFrom wrapper
// */
//auto WSARecvFrom_wrapper(AFUNPTR origin_func,
//                         WSARecvFrom_traits_t::arg1_type s,                     // windows::SOCKET
//                         WSARecvFrom_traits_t::arg2_type lpBuffers,             // windows::LPWSABUF
//                         WSARecvFrom_traits_t::arg3_type dwBufferCount,         // windows::DWORD
//                         WSARecvFrom_traits_t::arg4_type lpNumberOfBytesRecvd,  // windows::LPDWORD
//                         WSARecvFrom_traits_t::arg5_type lpFlags,               // windows::LPDWORD
//                         WSARecvFrom_traits_t::arg6_type lpFrom,                // windows::sockaddr*
//                         WSARecvFrom_traits_t::arg7_type lpFromlen,             // windows::LPINT
//                         WSARecvFrom_traits_t::arg8_type lpOverlapped,          // windows::LPWSAOVERLAPPED
//                         WSARecvFrom_traits_t::arg9_type lpCompletionRoutine,   // windows::LPWSAOVERLAPPED_COMPLETION_ROUTINE
//                         CONTEXT* p_ctxt, THREADID thread_id) -> WSARecvFrom_traits_t::result_type
//{
//  WSARecvFrom_traits_t::result_type result;

//  if (!traced_thread_is_fixed || (traced_thread_is_fixed && (traced_thread_id == thread_id)))
//  {
//    is_locked["WSARecvFrom"] = true;
//    traced_thread_id = thread_id; traced_thread_is_fixed = true;
//  }

//  PIN_CallApplicationFunction(p_ctxt, thread_id, CALLINGSTD_DEFAULT, origin_func,
//                              PIN_PARG(WSARecvFrom_traits_t::result_type), &result,
//                              PIN_PARG(WSARecvFrom_traits_t::arg1_type), s,
//                              PIN_PARG(WSARecvFrom_traits_t::arg2_type), lpBuffers,
//                              PIN_PARG(WSARecvFrom_traits_t::arg3_type), dwBufferCount,
//                              PIN_PARG(WSARecvFrom_traits_t::arg4_type), lpNumberOfBytesRecvd,
//                              PIN_PARG(WSARecvFrom_traits_t::arg5_type), lpFlags,
//                              PIN_PARG(WSARecvFrom_traits_t::arg6_type), lpFrom,
//                              PIN_PARG(WSARecvFrom_traits_t::arg7_type), lpFromlen,
//                              PIN_PARG(WSARecvFrom_traits_t::arg8_type), lpOverlapped,
//                              PIN_PARG(WSARecvFrom_traits_t::arg9_type), lpCompletionRoutine,
//                              PIN_PARG_END());

//  if (traced_thread_is_fixed && (thread_id == traced_thread_id) && is_locked["WSARecvFrom"])
//  {
//#if !defined(NDEBUG)
//    tfm::format(log_file, "message is obtained from WSARecvFrom at thread id %d\n", thread_id);
//#endif
//    is_locked["WSARecvFrom"] = false;
//    received_msg_addr = reinterpret_cast<ADDRINT>(lpBuffers->buf);
//    received_msg_size = *lpNumberOfBytesRecvd; handle_received_message();
//  }

//  return result;
//}


///**
// * @brief InternetReadFile wrapper
// */
//auto InternetReadFile_wrapper(AFUNPTR origin_func,
//                              InternetReadFile_traits_t::arg1_type hFile,                 // windows::HINTERNET
//                              InternetReadFile_traits_t::arg2_type lpBuffer,              // windows::LPVOID
//                              InternetReadFile_traits_t::arg3_type dwNumberOfBytesToRead, // windows::DWORD
//                              InternetReadFile_traits_t::arg4_type lpdwNumberOfBytesRead, // windows::LPWORD
//                              CONTEXT *p_ctxt, THREADID thread_id) -> InternetReadFile_traits_t::result_type
//{
//#if !defined(NDEBUG)
//  tfm::format(log_file, "InternetReadFile wrapper called\n");
//#endif
//  InternetReadFile_traits_t::result_type result;

//  if (!traced_thread_is_fixed || (traced_thread_is_fixed && (traced_thread_id == thread_id)))
//  {
//    is_locked["InternetReadFile"] = true;
//    traced_thread_id = thread_id; traced_thread_is_fixed = true;
//  }

//  PIN_CallApplicationFunction(p_ctxt, thread_id, CALLINGSTD_DEFAULT, origin_func,
//                              PIN_PARG(InternetReadFile_traits_t::result_type), &result,
//                              PIN_PARG(InternetReadFile_traits_t::arg1_type), hFile,
//                              PIN_PARG(InternetReadFile_traits_t::arg2_type), lpBuffer,
//                              PIN_PARG(InternetReadFile_traits_t::arg3_type), dwNumberOfBytesToRead,
//                              PIN_PARG(InternetReadFile_traits_t::arg4_type), lpdwNumberOfBytesRead,
//                              PIN_PARG_END());

//  if (traced_thread_is_fixed && (thread_id == traced_thread_id) && is_locked["InternetReadFile"])
//  {
//#if !defined(NDEBUG)
//    tfm::format(log_file, "message is obtained from InternetReadFile at thread id %d\n", thread_id);
//#endif
//    is_locked["InternetReadFile"] = false;
//    received_msg_addr = reinterpret_cast<ADDRINT>(lpBuffer);
//    received_msg_size = *lpdwNumberOfBytesRead;
//    handle_received_message();
//  }

//  return result;
//}

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
