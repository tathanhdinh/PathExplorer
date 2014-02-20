#include "capturing_phase.h"
#include "common.h"
#include "../util/stuffs.h"

namespace capturing
{
#if BOOST_OS_WINDOWS
static bool function_has_been_called = false;
VOID logging_before_recv_functions_analyzer(ADDRINT msg_addr)
{
  received_msg_addr = msg_addr;
  function_has_been_called = true;

  BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
    << "recv or recvfrom called, received msg buffer: "
    << remove_leading_zeros(StringFromAddrint(received_msg_addr));
  return;
}

VOID logging_after_recv_functions_analyzer(UINT32 msg_length)
{
  if (function_has_been_called)
  {
    if (msg_length > 0)
    {
      received_msg_num++; received_msg_size = msg_length;

      BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
        << "recv or recvfrom returned, received msg size: " << received_msg_size;
    }

    function_has_been_called = false;
  }
  return;
}

namespace WINDOWS
{
#include <WinSock2.h>
#include <Windows.h>
};
VOID logging_before_wsarecv_functions_analyzer(ADDRINT msg_struct_adddr)
{
  received_msg_struct_addr = msg_struct_adddr;
  received_msg_addr = reinterpret_cast<ADDRINT>(
        (reinterpret_cast<WINDOWS::LPWSABUF>(received_msg_struct_addr))->buf);
  function_has_been_called = true;

  BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
    << "WSARecv or WSARecvFrom called, received msg buffer: "
    << remove_leading_zeros(StringFromAddrint(received_msg_addr));

  return;
}

VOID logging_after_wsarecv_funtions_analyzer()
{
  if (function_has_been_called)
  {
    received_msg_size = (reinterpret_cast<WINDOWS::LPWSABUF>(received_msg_struct_addr))->len;
    if (received_msg_size > 0)
    {
      ++received_msg_num;

      BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
        << "WSARecv or WSARecvFrom returned, received msg size: " << received_msg_size;

      BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
        << boost::format("the first message saved at %s with size %d bytes")
            % remove_leading_zeros(StringFromAddrint(received_msg_addr)) % received_msg_size
        << "\n-------------------------------------------------------------------------------------------------\n"
        << boost::format("start tainting the first time with trace size %d")
            % max_trace_size;

      PIN_RemoveInstrumentation();
    }
    function_has_been_called = false;
  }

  return;
}
#elif BOOST_OS_LINUX
VOID syscall_entry_analyzer(THREADID thread_id,
                            CONTEXT* p_ctxt, SYSCALL_STANDARD syscall_std, VOID *data)
{
  if (current_running_state == capturing_state)
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
VOID syscall_exit_analyzer(THREADID thread_id,
                           CONTEXT* p_ctxt, SYSCALL_STANDARD syscall_std, VOID *data)
{
  if (current_running_state == capturing_state)
  {
    if (logged_syscall_index == syscall_recvfrom)
    {
      ADDRINT returned_value = PIN_GetSyscallReturn(p_ctxt, syscall_std);
      if (returned_value > 0)
      {
        received_msg_num++;
        received_msg_addr = logged_syscall_args[1]; received_msg_size = returned_value;
#if !defined(NDEBUG)
        BOOST_LOG_TRIVIAL(info)
          << boost::format("the first message saved at %s with size %d bytes\n%s\n%s %d")
             % addrint_to_hexstring(received_msg_addr) % received_msg_size
             % "-----------------------------------------------------------------------------------"
             % "start tainting the first time with trace size" % max_trace_size;
#endif
        // the first received message is the considered input
        if (received_msg_num == 1)
        {
          current_running_state = tainting_state; PIN_RemoveInstrumentation();
        }
      }
    }
  }
  return;
}
#endif
} // end of capturing namespace
