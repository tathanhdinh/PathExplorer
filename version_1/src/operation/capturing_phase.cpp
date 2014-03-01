#include "capturing_phase.h"
#include "common.h"
#include "../util/stuffs.h"

namespace capturing 
{
#if defined(_WIN32) || defined(_WIN64)
#include <WinSock2.h>
#include <Windows.h>

/*================================================================================================*/

static bool     function_has_been_called = false;
static ADDRINT  received_msg_struct_addr;

/*================================================================================================*/

VOID before_recvs(ADDRINT msg_addr)
{
  received_msg_addr = msg_addr;
  function_has_been_called = true;
  return;
}

VOID after_recvs(UINT32 msg_length)
{
  if (function_has_been_called)
  {
    if (msg_length > 0)
    {
      received_msg_num++; received_msg_size = msg_length;

#if !defined(NDEBUG)
      tfm::format(log_file,
        "the first message saved at %s with size %d bytes\nstart tainting the first time with trace size %d\n", 
        addrint_to_hexstring(received_msg_addr), received_msg_size, max_trace_size);
#endif
    }
    function_has_been_called = false;
  }
  return;
}

//namespace WINDOWS
//{
//#include <WinSock2.h>
//#include <Windows.h>
//};
VOID before_wsarecvs(ADDRINT msg_struct_addr)
{
  received_msg_struct_addr = msg_struct_addr;
  received_msg_addr = reinterpret_cast<ADDRINT>(
        (reinterpret_cast<LPWSABUF>(msg_struct_addr))->buf);
  function_has_been_called = true;

  /*log_file << boost::format("WSARecv or WSARecvFrom called, received message buffer: %s\n")
              % addrint_to_hexstring(received_msg_addr);*/

  return;
}

VOID after_wsarecvs()
{
  if (function_has_been_called)
  {
    received_msg_size = (reinterpret_cast<LPWSABUF>(received_msg_struct_addr))->len;
    if (received_msg_size > 0)
    {
      ++received_msg_num;

      /*log_file << boost::format("WSARecv or WSARecvFrom returned, received message size: %d\n")
                  % received_msg_size;
      log_file << boost::format("the first message saved at %s with size %d bytes\nstart tainting with trace size %d\n")
                  % addrint_to_hexstring(received_msg_addr) % received_msg_size % max_trace_size;*/
      tfm::format(log_file, 
        "the first message saved at %s with size %d bytes\nstart tainting with trace size %d\n", 
        addrint_to_hexstring(received_msg_addr), received_msg_size, max_trace_size);

      // the first received message is the considered input
      if (received_msg_num == 1)
      {
        // switch to the tainting state
        current_running_phase = tainting_state; PIN_RemoveInstrumentation();
      }
    }
    function_has_been_called = false;
  }

  return;
}
#elif defined(__gnu_linux__)
VOID syscall_entry_analyzer(THREADID thread_id,
                            CONTEXT* p_ctxt, SYSCALL_STANDARD syscall_std, VOID *data)
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
VOID syscall_exit_analyzer(THREADID thread_id,
                           CONTEXT* p_ctxt, SYSCALL_STANDARD syscall_std, VOID *data)
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
        tfm::format(log_file,
                    "the first message saved at %s with size %d bytes\nstart tainting the first time with trace size %d\n",
                    addrint_to_hexstring(received_msg_addr), received_msg_size, max_trace_size);
//        log_file << boost::format("the first message saved at %s with size %d bytes\nstart tainting the first time with trace size %d\n")
//                    % addrint_to_hexstring(received_msg_addr) % received_msg_size % max_trace_size;
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
#endif
} // end of capturing namespace
