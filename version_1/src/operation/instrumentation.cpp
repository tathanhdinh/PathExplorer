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

/*================================================================================================*/

typedef std::function<void(RTN&)> instrument_func_t;
static std::map<std::string, instrument_func_t> instrument_func_of_name;

/*================================================================================================*/
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
                             IARG_INST_PTR, IARG_THREAD_ID, IARG_END);

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

/*================================================================================================*/

/**
 * @brief instrumentation function: all analysis functions are inserted using
 * INS_InsertPredicatedCall to make sure that the instruction is examined iff it is executed.
 * 
 * @param ins current examined instruction (data is not used)
 */
auto ins_instrumenter (INS ins, VOID *data) -> VOID
{
  if (current_running_phase != capturing_phase)
  {
    // examining statically instructions
    /*ptr_instruction_t examined_ins;*/ auto ins_addr = INS_Address(ins);

    // verify if the instruction has been examined
    if (ins_at_addr.find(ins_addr) == ins_at_addr.end())
    {
      // not yet, then create a new instruction object
//      examined_ins.reset(new instruction(ins)); ins_at_addr[ins_addr] = examined_ins;
      ins_at_addr[ins_addr] = std::make_shared<instruction>(ins);
      if (/*examined_ins*/ins_at_addr[ins_addr]->is_cond_direct_cf)
      {
//        ins_at_addr[ins_addr].reset(new cond_direct_instruction(*examined_ins));
        ins_at_addr[ins_addr] = std::make_shared<cond_direct_instruction>(*ins_at_addr[ins_addr]);
      }

#if !defined(DISABLE_FSA)
      explored_fsa->add_vertex(ins_addr);
#endif
    }
//    else
//    {
//      // the instruction has been examined, then simply get the existed instance
//      examined_ins = ins_at_addr[ins_addr];
//    }

    switch (current_running_phase)
    {
    case tainting_phase:
      exec_tainting_phase(ins, /*examined_ins*/ins_at_addr[ins_addr]); break;

    case rollbacking_phase:
      exec_rollbacking_phase(ins, /*examined_ins*/ins_at_addr[ins_addr]); break;

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


/**
 * @brief instrument recv and recvfrom functions
 */
static inline auto instrument_recvs (RTN& func) -> void
{
  RTN_Open(func);
  RTN_InsertCall(func, IPOINT_BEFORE, (AFUNPTR)capturing::before_recvs,
                 IARG_FUNCARG_ENTRYPOINT_VALUE, 1, IARG_THREAD_ID, IARG_END);
  RTN_InsertCall(func, IPOINT_AFTER, (AFUNPTR)capturing::after_recvs,
                 IARG_FUNCRET_EXITPOINT_VALUE, IARG_THREAD_ID, IARG_END);
  RTN_Close(func);
  return;
}


/**
 * @brief instrument WSARecv and WSARecvFrom functions
 */
static inline auto instrument_wsarecvs (RTN& func) -> void
{
  RTN_Open(func);
  RTN_InsertCall(func, IPOINT_BEFORE, (AFUNPTR)capturing::before_wsarecvs,
                 IARG_FUNCARG_ENTRYPOINT_VALUE, 1, IARG_THREAD_ID, IARG_END);
  RTN_InsertCall(func, IPOINT_AFTER, (AFUNPTR)capturing::after_wsarecvs, IARG_THREAD_ID, IARG_END);
  RTN_Close(func);
  return;
}


static inline auto instrument_function (IMG loaded_img, std::string func_name) -> void
{
  auto func = RTN_FindByName(loaded_img, func_name.c_str());
  if (RTN_Valid(func))
  {
#if !defined(NDEBUG)
    tfm::format(log_file, "instrumenting function %s (mapped at %s, size %d)\n", func_name,
                addrint_to_hexstring(RTN_Address(func)), RTN_Size(func));
#endif
    instrument_func_of_name[func_name](func);
//    if ((func_name == "recv") || (func_name == "recvfrom")) instrument_recvs(func);
//    else if ((func_name == "WSARecv") || (func_name == "WSARecvFrom")) instrument_wsarecvs(func);
  }
  else
#if !defined(NDEBUG)
    tfm::format(log_file, "function %s cannot found in %s\n", func_name, IMG_Name(loaded_img));
#endif
  return;
}


/**
 * @brief detect loaded images and instrument recv, recvfrom, WSARecv, WSARecvFrom functions
 */
auto image_load_instrumenter (IMG loaded_img, VOID *data) -> VOID
{
  if (current_running_phase == capturing_phase)
  {
#if !defined(NDEBUG)
    tfm::format(log_file, "module %s is mapped at %s with size %d\n",
                IMG_Name(loaded_img), addrint_to_hexstring(IMG_StartAddress(loaded_img)),
                IMG_SizeMapped(loaded_img));
#endif

#if defined(_WIN32) || defined(_WIN64)
    // verify if the winsock2 module is loaded
    auto loaded_img_full_name = IMG_Name(loaded_img);
    if (loaded_img_full_name.find("WS2_32.dll") != std::string::npos)
    {
#if !defined(NDEBUG)
      log_file << "winsock2 module is found, instrumenting message receiving functions\n";
#endif
      instrument_function(loaded_img, "recv"); instrument_function(loaded_img, "recvfrom");
      instrument_function(loaded_img, "WSARecv"); instrument_function(loaded_img, "WSARecvFrom");

      /*current_running_phase = capturing_phase;*/ /*capturing::initialize();*/
    }
#endif // defined(_WIN32) || defined(_WIN64)
  }

  return;
}


/**
 * @brief process_create_instrumenter
 */
auto process_create_instrumenter (CHILD_PROCESS created_process, VOID* data) -> BOOL
{
#if !defined(NDEBUG)
  tfm::format(log_file, "new process created with id %d\n", CHILD_PROCESS_GetId(created_process));
#endif
  return TRUE;
}


/**
 * @brief initialize_instrumenter
 */
auto initialize_instrumenter () -> void
{
  instrument_func_of_name["recv"] = instrument_recvs;
  instrument_func_of_name["recvfrom"] = instrument_recvs;
  instrument_func_of_name["WSARecv"] = instrument_wsarecvs;
  instrument_func_of_name["WSARecvFrom"] = instrument_wsarecvs;

  current_running_phase = capturing_phase; capturing::initialize();
  return;
}
