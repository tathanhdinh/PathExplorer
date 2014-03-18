#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <functional>

#include "../util/stuffs.h"
#include "common.h"
#include "instrumentation.h"
#include "capturing_phase.h"
#include "tainting_phase.h"
#include "rollbacking_phase.h"

namespace instrumentation
{
typedef std::function<void(RTN&)> instrument_func_t;
static std::map<std::string, instrument_func_t> intercept_func_of_name;
static std::map<std::string, instrument_func_t> replace_func_of_name;

/**
 * @brief exec_capturing_phase
 */
static inline auto exec_capturing_phase (INS& ins) -> void
{
  if (INS_IsMemoryRead(ins))
  {
    INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)capturing::mem_read_instruction,
                             IARG_INST_PTR, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE,
                             IARG_CONST_CONTEXT, IARG_THREAD_ID, IARG_END);

    if (INS_HasMemoryRead2(ins))
    {
      INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)capturing::mem_read_instruction,
                               IARG_INST_PTR, IARG_MEMORYREAD2_EA, IARG_MEMORYREAD_SIZE,
                               IARG_CONST_CONTEXT, IARG_THREAD_ID, IARG_END);
    }
  }
  return;
}


/**
 * @brief exec_tainting_phase
 */
static inline auto exec_tainting_phase (INS& ins, ptr_instruction_t examined_ins) -> void
{
  /* taint logging */
  if (examined_ins->is_mapped_from_kernel)
  {
    INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)tainting::kernel_mapped_instruction,
                             IARG_INST_PTR, IARG_THREAD_ID, IARG_END);
  }
  else
  {
    // general logging
    INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)tainting::generic_instruction,
                             IARG_INST_PTR, IARG_CONST_CONTEXT, IARG_THREAD_ID, IARG_END);

    if (examined_ins->is_mem_read)
    {
      // memory read logging
      INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)tainting::mem_read_instruction,
                               IARG_INST_PTR, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE,
                               IARG_CONST_CONTEXT, IARG_THREAD_ID, IARG_END);

      if (examined_ins->has_mem_read2)
      {
        // memory read2 (e.g. rep cmpsb instruction)
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)tainting::mem_read_instruction,
                                 IARG_INST_PTR, IARG_MEMORYREAD2_EA, IARG_MEMORYREAD_SIZE,
                                 IARG_CONST_CONTEXT, IARG_THREAD_ID, IARG_END);
      }
    }

    if (examined_ins->is_mem_write)
    {
      // memory written logging
      INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)tainting::mem_write_instruction,
                               IARG_INST_PTR, IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE,
                               IARG_THREAD_ID, IARG_END);
    }
  }

  /* taint propagating */
  INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)tainting::graphical_propagation,
                           IARG_INST_PTR, IARG_THREAD_ID, IARG_END);
  return;
}


/**
 * @brief instrument codes executed in rollbacking phase
 */
static inline auto exec_rollbacking_phase (INS& ins, ptr_instruction_t examined_ins) -> void
{
  INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)rollbacking::generic_instruction,
                           IARG_INST_PTR, IARG_THREAD_ID, IARG_END);

  if (examined_ins->is_cond_direct_cf)
  {
    INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)rollbacking::control_flow_instruction,
                             IARG_INST_PTR, IARG_THREAD_ID, IARG_END);
  }

#if !defined(ENABLE_FAST_ROLLBACK)
  if (examined_ins->is_mem_write)
  {
    INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)rollbacking::mem_write_instruction,
                             IARG_INST_PTR, IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE,
                             IARG_THREAD_ID, IARG_END);
  }
#endif

  return;
}


/**
 * @brief instruction instrumentation: all analysis functions are inserted using
 * INS_InsertPredicatedCall to make sure that the instruction is examined iff it is executed.
 */
auto instruction_executing (INS ins, VOID *data) -> VOID
{
  if (current_running_phase != capturing_phase)
  {
    // examining statically instructions
    auto ins_addr = INS_Address(ins);

    // verify if the instruction has been examined
    if (ins_at_addr.find(ins_addr) == ins_at_addr.end())
    {
      // not yet, then create a new instruction object
      ins_at_addr[ins_addr] = std::make_shared<instruction>(ins);
      if (ins_at_addr[ins_addr]->is_cond_direct_cf)
      {
        ins_at_addr[ins_addr] = std::make_shared<cond_direct_instruction>(*ins_at_addr[ins_addr]);
      }

#if !defined(DISABLE_FSA)
      explored_fsa->add_vertex(ins_addr);
#endif
    }

    switch (current_running_phase)
    {
    case tainting_phase:
      exec_tainting_phase(ins, ins_at_addr[ins_addr]); break;

    case rollbacking_phase:
      exec_rollbacking_phase(ins, ins_at_addr[ins_addr]); break;

    default:
      break;
    }
  }
  else
  {
    if (interested_msg_is_received) exec_capturing_phase(ins);
  }

  return;
}


#if defined(_WIN32) || defined(_WIN64)
/**
 * @brief instrument recv and recvfrom functions
 */
static inline auto recvs_interceptor (RTN& func) -> void
{
  // insertion approach
  RTN_Open(func);
  RTN_InsertCall(func, IPOINT_BEFORE, (AFUNPTR)capturing::recvs_interceptor_before,
                 IARG_FUNCARG_ENTRYPOINT_VALUE, 1, IARG_THREAD_ID, IARG_END);
  RTN_InsertCall(func, IPOINT_AFTER, (AFUNPTR)capturing::recvs_interceptor_after,
                 IARG_FUNCRET_EXITPOINT_VALUE, IARG_THREAD_ID, IARG_END);
  RTN_Close(func);
  return;
}


/**
 * @brief instrument WSARecv and WSARecvFrom functions
 */
static inline auto wsarecvs_interceptor (RTN& rtn) -> void
{
  // insertion approach
  RTN_Open(rtn);
  RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)capturing::wsarecvs_interceptor_before,
                 IARG_FUNCARG_ENTRYPOINT_VALUE, 1, IARG_THREAD_ID, IARG_END);
  RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)capturing::wsarecvs_interceptor_after, IARG_THREAD_ID,
                 IARG_END);
  RTN_Close(rtn);

  return;
}


/**
 * @brief replace recv function
 */
static inline auto recv_replacer (RTN& rtn) -> void
{
  // replacement approach
  auto recv_proto = PROTO_Allocate(PIN_PARG(int), CALLINGSTD_DEFAULT, "recv",
                                   PIN_PARG(windows::SOCKET), PIN_PARG(char*), PIN_PARG(int),
                                   PIN_PARG(int), PIN_PARG_END());
  RTN_ReplaceSignature(rtn, AFUNPTR(capturing::recv_wrapper), IARG_PROTOTYPE, recv_proto, IARG_ORIG_FUNCPTR,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 2, IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
                       IARG_CONST_CONTEXT, IARG_THREAD_ID, IARG_END);
  PROTO_Free(recv_proto);

  return;
}


static inline auto recvfrom_replacer (RTN& rtn) -> void
{
  // replacement approach
  auto recvfrom_proto = PROTO_Allocate(PIN_PARG(int), CALLINGSTD_DEFAULT, "recvfrom",
                                       PIN_PARG(windows::SOCKET), PIN_PARG(char*), PIN_PARG(int),
                                       PIN_PARG(int), PIN_PARG(windows::sockaddr*), PIN_PARG(int*),
                                       PIN_PARG_END());
  RTN_ReplaceSignature(rtn, AFUNPTR(capturing::recvfrom_wrapper), IARG_PROTOTYPE, recvfrom_proto, IARG_ORIG_FUNCPTR,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 2, IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 4, IARG_FUNCARG_ENTRYPOINT_VALUE, 5,
                       IARG_CONST_CONTEXT, IARG_THREAD_ID, IARG_END);
  PROTO_Free(recvfrom_proto);
}

/**
 * @brief replace WSARecv function
 */
static inline auto wsarecv_replacer (RTN& rtn) -> void
{
  // replacement approach
  auto wsarecv_proto = PROTO_Allocate(PIN_PARG(int), CALLINGSTD_DEFAULT, "WSARecv",
                                      PIN_PARG(windows::SOCKET), PIN_PARG(windows::LPWSABUF),
                                      PIN_PARG(windows::DWORD), PIN_PARG(windows::LPDWORD),
                                      PIN_PARG(windows::LPDWORD),
                                      PIN_PARG(windows::LPWSAOVERLAPPED),
                                      PIN_PARG(windows::LPWSAOVERLAPPED_COMPLETION_ROUTINE),
                                      PIN_PARG_END());
  RTN_ReplaceSignature(rtn, AFUNPTR(capturing::wsarecv_wrapper), IARG_PROTOTYPE, wsarecv_proto, IARG_ORIG_FUNCPTR,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 2, IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 4, IARG_FUNCARG_ENTRYPOINT_VALUE, 5,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 6,
                       IARG_CONST_CONTEXT, IARG_THREAD_ID, IARG_END);
  PROTO_Free(wsarecv_proto);

  return;
}


/**
 * @brief replace WSARecvFrom function
 */
static inline auto wsarecvfrom_replacer (RTN& rtn) -> void
{
  // replacement approach
  auto wsarecvfrom_proto = PROTO_Allocate(PIN_PARG(int), CALLINGSTD_DEFAULT, "WSARecvFrom",
                                          PIN_PARG(windows::SOCKET), PIN_PARG(windows::LPWSABUF),
                                          PIN_PARG(windows::DWORD), PIN_PARG(windows::LPDWORD),
                                          PIN_PARG(windows::LPDWORD),
                                          PIN_PARG(windows::sockaddr*), PIN_PARG(windows::LPINT),
                                          PIN_PARG(windows::LPWSAOVERLAPPED),
                                          PIN_PARG(windows::LPWSAOVERLAPPED_COMPLETION_ROUTINE),
                                          PIN_PARG_END());
  RTN_ReplaceSignature(rtn, AFUNPTR(capturing::wsarecvfrom_wrapper), IARG_PROTOTYPE,
                       wsarecvfrom_proto, IARG_ORIG_FUNCPTR,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 2, IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 4, IARG_FUNCARG_ENTRYPOINT_VALUE, 5,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 6, IARG_FUNCARG_ENTRYPOINT_VALUE, 7,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 8,
                       IARG_CONST_CONTEXT, IARG_THREAD_ID, IARG_END);
  PROTO_Free(wsarecvfrom_proto);

  return;
}


//#if !defined(NDEBUG)
//static std::map<ADDRINT, std::string> routine_at_addr;

//static auto before_other(ADDRINT rtn_addr) -> VOID
//{
//  tfm::format(log_file, "%s is called\n", routine_at_addr[rtn_addr]);
//  return;
//}


//static auto after_other(ADDRINT rtn_addr) -> VOID
//{
//  tfm::format(log_file, "%s returns\n", routine_at_addr[rtn_addr]);
//  return;
//}

//static inline auto instrument_generic (RTN& rtn) -> void
//{
//  RTN_Open(rtn);
//  RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)before_other, IARG_ADDRINT, RTN_Address(rtn), IARG_END);
//  RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)after_other, IARG_ADDRINT, RTN_Address(rtn), IARG_END);
//  RTN_Close(rtn);
//  return;
//}
//#endif


/**
 * @brief routine instrumentation
 */
auto routine_calling(RTN rtn, VOID* data) -> VOID
{
  if ((current_running_phase == capturing_phase) && !interested_msg_is_received)
  {
    auto rtn_name = RTN_Name(rtn);
    if (intercept_func_of_name.find(rtn_name) != intercept_func_of_name.end())
      intercept_func_of_name[rtn_name](rtn);
  }
  return;
}


///**
// * @brief instrument_function
// */
//static inline auto instrument_function (IMG loaded_img, std::string func_name) -> void
//{
//  auto func = RTN_FindByName(loaded_img, func_name.c_str());
//  if (RTN_Valid(func))
//  {
//#if !defined(NDEBUG)
//    tfm::format(log_file, "instrumenting function %s (mapped at %s, size %d)\n", func_name,
//                addrint_to_hexstring(RTN_Address(func)), RTN_Size(func));
//#endif
//    instrument_func_of_name[func_name](func);
//  }
//  else
//#if !defined(NDEBUG)
//    tfm::format(log_file, "function %s cannot found in %s\n", func_name, IMG_Name(loaded_img));
//#endif
//  return;
//}


/**
 * @brief detect loaded images and instrument recv, recvfrom, WSARecv, WSARecvFrom functions
 */
auto image_loading (IMG loaded_img, VOID *data) -> VOID
{
  if ((current_running_phase == capturing_phase) && !interested_msg_is_received)
  {
#if !defined(NDEBUG)
    tfm::format(log_file, "module %s is mapped at %s with size %d\n", IMG_Name(loaded_img),
                addrint_to_hexstring(IMG_StartAddress(loaded_img)), IMG_SizeMapped(loaded_img));
#endif

    // verify if the winsock2 module is loaded
    auto loaded_img_full_name = IMG_Name(loaded_img);
    if (loaded_img_full_name.find("WS2_32.dll") != std::string::npos)
    {
#if !defined(NDEBUG)
      log_file << "winsock2 module is found, replacing message receiving functions\n";
#endif
      typedef decltype(replace_func_of_name) replace_func_of_name_t;
      std::for_each(replace_func_of_name.begin(), replace_func_of_name.end(),
                    [&](replace_func_of_name_t::value_type name_replacer)
      {
        auto rtn = RTN_FindByName(loaded_img, name_replacer.first.c_str());
        if (RTN_Valid(rtn))
        {
#if !defined(NDEBUG)
          tfm::format(log_file, "replacing function %s <%s: %d>\n", name_replacer.first,
                      addrint_to_hexstring(RTN_Address(rtn)), RTN_Size(rtn));
          name_replacer.second(rtn);
#endif
        }
      });

//      instrument_function(loaded_img, "recv"); instrument_function(loaded_img, "recvfrom");
//      instrument_function(loaded_img, "WSARecv"); instrument_function(loaded_img, "WSARecvFrom");
    }
  }

  return;
}
#endif // defined(_WIN32) || defined(_WIN64)


/**
 * @brief process_create_instrumenter
 */
auto process_creating (CHILD_PROCESS created_process, VOID* data) -> BOOL
{
#if !defined(NDEBUG)
  tfm::format(log_file, "new process created with id %d\n", CHILD_PROCESS_GetId(created_process));
#endif
  return TRUE;
}


/**
 * @brief initialize_instrumenter
 */
auto initialize () -> void
{
#if defined(_WIN32) || defined(_WIN64)
  intercept_func_of_name["recv"] = recvs_interceptor;
  intercept_func_of_name["recvfrom"] = recvs_interceptor;
  intercept_func_of_name["WSARecv"] = wsarecvs_interceptor;
  intercept_func_of_name["WSARecvFrom"] = wsarecvs_interceptor;

  replace_func_of_name["recv"] = recv_replacer;
  replace_func_of_name["recvfrom"] = recvfrom_replacer;
  replace_func_of_name["WSARecv"] = wsarecv_replacer;
  replace_func_of_name["WSARecvFrom"] = wsarecvfrom_replacer;
#endif // defined(_WIN32) || defined(_WIN64)

  current_running_phase = capturing_phase; capturing::initialize();
  return;
}

} // end of instrumentation namespace
