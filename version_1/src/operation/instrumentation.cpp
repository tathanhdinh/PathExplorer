#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <functional>
#include <locale>

#include "../util/stuffs.h"
#include "common.h"
#include "instrumentation.h"
#include "capturing_phase.h"
#include "tainting_phase.h"
#include "rollbacking_phase.h"

namespace instrumentation
{
typedef std::function</*void(RTN&)*/capturing::interceptor_t> instrument_func_t;
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
  if (examined_ins->is_mapped_from_kernel
    #if defined(_WIN32) || defined(_WIN64)
      || examined_ins->is_in_msg_receiving
    #endif
      )
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

///**
// * @brief routine instrumentation
// */
//auto routine_calling(RTN rtn, VOID* data) -> VOID
//{
//  if ((current_running_phase == capturing_phase) && !interested_msg_is_received)
//  {
//    auto rtn_name = RTN_Name(rtn);
////    routine_at_addr[RTN_Address(rtn)] = rtn_name; generic_routine_interceptor(rtn);

//    if (intercept_func_of_name.find(rtn_name) != intercept_func_of_name.end())
//    {
//#if !defined(NDEBUG)
//      tfm::format(log_file, "intercepting function %s\n", rtn_name);
//#endif
//      intercept_func_of_name[rtn_name](rtn);
//    }
//  }
//  return;
//}

static inline auto generic_intercept (IMG& loaded_img) -> void
{
  // iterate over sections of the loaded image
  for (auto sec = IMG_SecHead(loaded_img); SEC_Valid(sec); sec = SEC_Next(sec))
  {
    // iterate over routines of the section
    for (auto rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn))
    {
#if !defined(NDEBUG)
      tfm::format(log_file, "intercepting %s located at %s\n", RTN_Name(rtn),
                  addrint_to_hexstring(RTN_Address(rtn)));
#endif
      capturing::generic_routine(rtn);
    }
  }
  return;
}


static inline auto network_related_intercept (IMG& loaded_img) -> void
{
  // verify if the winsock or wininet module is loaded
  static std::locale current_loc;
  static auto lowercase_convertion = [&](char c) -> char { return std::tolower(c, current_loc); };

  auto img_name = IMG_Name(loaded_img); std::transform(img_name.begin(), img_name.end(),
                                                       img_name.begin(), lowercase_convertion);
  if ((img_name.find("ws2_32.dll") != std::string::npos) ||
      (img_name.find("wininet.dll") != std::string::npos))
  {
    typedef decltype(intercept_func_of_name) intercept_func_of_name_t;
    std::for_each(intercept_func_of_name.begin(), intercept_func_of_name.end(),
                  [&](intercept_func_of_name_t::value_type origin_interceptor)
    {
      PIN_LockClient();
      // look for the routine corresponding with the name of the original function
      auto rtn = RTN_FindByName(loaded_img, origin_interceptor.first.c_str());
      if (RTN_Valid(rtn))
      {
#if !defined(NDEBUG)
        tfm::format(log_file, "intercepting %s mapped at %s\n", RTN_Name(rtn),
                    addrint_to_hexstring((RTN_Address(rtn))));
#endif
//          capturing::generic_routine(rtn);
        origin_interceptor.second(rtn);
      }
      PIN_UnlockClient();
    });
  }
  return;
}


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
    network_related_intercept(loaded_img);
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
  intercept_func_of_name["recv"] = capturing::recv_routine;
  intercept_func_of_name["recvfrom"] = capturing::recvfrom_routine;
  intercept_func_of_name["WSARecv"] = capturing::WSARecv_routine;
  intercept_func_of_name["WSARecvFrom"] = capturing::WSARecvFrom_routine;
//  intercept_func_of_name["InternetReadFile"] = capturing::InternetReadFile_routine;
//  intercept_func_of_name["InternetReadFileEx"] = capturing::InternetReadFileEx_routine;
  intercept_func_of_name["InternetReadFileExA"] = capturing::InternetReadFileEx_routine;
//  intercept_func_of_name["InternetReadFileExW"] = capturing::InternetReadFileEx_routine;

//  replace_func_of_name["recv"] = recv_replacer;
//  replace_func_of_name["recvfrom"] = recvfrom_replacer;
//  replace_func_of_name["WSARecv"] = WSARecv_replacer;
//  replace_func_of_name["WSARecvFrom"] = WSARecvFrom_replacer;
//  replace_func_of_name["InternetReadFile"] = InternetReadFile_replacer;
#endif // defined(_WIN32) || defined(_WIN64)

  current_running_phase = capturing_phase; capturing::initialize();
  return;
}

} // end of instrumentation namespace
