#include <fstream>
#include <vector>
#include <map>
#include <set>

#include "../util/stuffs.h"
#include "common.h"
#include "instrumentation.h"
#include "capturing_phase.h"
#include "tainting_phase.h"
#include "rollbacking_phase.h"

//#if defined(_WIN32) || defined(_WIN64)
//#include <stdlib.h>
//#endif

/*================================================================================================*/

static inline void exec_tainting_phase(INS& ins, ptr_instruction_t examined_ins)
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
    INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)tainting::general_instruction,
                             IARG_INST_PTR, IARG_THREAD_ID, IARG_END);

    if (examined_ins->is_mem_read)
    {
      // memory read logging
      INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)tainting::mem_read_instruction,
                               IARG_INST_PTR, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE,
                               IARG_CONTEXT, IARG_THREAD_ID, IARG_END);

      if (examined_ins->has_mem_read2)
      {
        // memory read2 (e.g. rep cmpsb instruction)
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)tainting::mem_read_instruction,
                                 IARG_INST_PTR, IARG_MEMORYREAD2_EA, IARG_MEMORYREAD_SIZE,
                                 IARG_CONTEXT, IARG_THREAD_ID, IARG_END);
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
static inline void exec_rollbacking_phase(INS& ins, ptr_instruction_t examined_ins)
{
//  if (examined_ins->is_mapped_from_kernel)
//  {
//    tfm::format(log_file,
//                "unsafe rollbacking: a kernel mapped instruction (%s %s) will be executed at %d\n",
//                addrint_to_hexstring(examined_ins->address), examined_ins->disassembled_name,
//                current_exec_order + 1);
//    PIN_ExitApplication(1);
//  }
//  else
//  {
//  }

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
VOID ins_instrumenter(INS ins, VOID *data)
{
  if (current_running_phase != capturing_phase)
  {
    // examining statically instructions
    ptr_instruction_t examined_ins(new instruction(ins));
    ADDRINT examined_addr = examined_ins->address;
    ins_at_addr[examined_addr] = examined_ins;
    if (examined_ins->is_cond_direct_cf)
    {
      ins_at_addr[examined_addr].reset(new cond_direct_instruction(*examined_ins));
    }
#if defined(ENABLE_FSA)
//    tfm::format(std::cerr, "instrumented (%s: %s)\n", addrint_to_hexstring(examined_ins->address),
//                examined_ins->disassembled_name);
    explored_fsa->add_vertex(ins_at_addr[examined_addr]);
#endif

    switch (current_running_phase)
    {
    case tainting_phase:
      exec_tainting_phase(ins, examined_ins);
      break;

    case rollbacking_phase:
      exec_rollbacking_phase(ins, examined_ins);
      break;

    default:
      break;
    }
  }

  return;
}


/**
 * @brief instrument recv and recvfrom functions
 */
static inline void instrument_recvs(RTN& recv_function)
{
  RTN_Open(recv_function);

  RTN_InsertCall(recv_function, IPOINT_BEFORE, (AFUNPTR)capturing::before_recvs,
                 IARG_FUNCARG_ENTRYPOINT_VALUE, 1, IARG_THREAD_ID, IARG_END);

  RTN_InsertCall(recv_function, IPOINT_AFTER, (AFUNPTR)capturing::after_recvs,
                 IARG_FUNCRET_EXITPOINT_VALUE, IARG_THREAD_ID, IARG_END);

  RTN_Close(recv_function);
  return;
}


/**
 * @brief instrument WSARecv and WSARecvFrom functions
 */
static inline void instrument_wsarecvs(RTN& wsarecv_function)
{
  RTN_Open(wsarecv_function);

  RTN_InsertCall(wsarecv_function, IPOINT_BEFORE, (AFUNPTR)capturing::before_wsarecvs,
                 IARG_FUNCARG_ENTRYPOINT_VALUE, 1, IARG_THREAD_ID, IARG_END);

  RTN_InsertCall(wsarecv_function, IPOINT_AFTER, (AFUNPTR)capturing::after_wsarecvs, IARG_THREAD_ID,
                 IARG_END);

  RTN_Close(wsarecv_function);
  return;
}


/**
 * @brief detect loaded images
 */
VOID image_load_instrumenter(IMG loaded_img, VOID *data)
{
#if !defined(NDEBUG)
  tfm::format(log_file, "module %s is loaded at %s\n",
              IMG_Name(loaded_img), addrint_to_hexstring(IMG_StartAddress(loaded_img)));
#endif

#if defined(_WIN32) || defined(_WIN64)
  // verify if the winsock2 module is loaded
  std::string loaded_img_full_name = IMG_Name(loaded_img);
  if (loaded_img_full_name.find("WS2_32.dll") != std::string::npos)
  {
#if !defined(NDEBUG)
    log_file << "winsock2 module is found, instrumenting message receiving functions\n";
#endif

    RTN recv_func = RTN_FindByName(loaded_img, "recv");
    if (RTN_Valid(recv_func))
    {
#if !defined(NDEBUG)
      tfm::format(log_file, "recv is located at %s\n",
                  addrint_to_hexstring(RTN_Address(recv_func)));
#endif
      instrument_recvs(recv_func);
    }

    RTN recvfrom_func = RTN_FindByName(loaded_img, "recvfrom");
    if (RTN_Valid(recvfrom_func))
    {
#if !defined(NDEBUG)
      tfm::format(log_file, "recvfrom is located at %s\n",
                  addrint_to_hexstring(RTN_Address(recvfrom_func)));
#endif
      instrument_recvs(recvfrom_func);
    }

    RTN wsarecv_func = RTN_FindByName(loaded_img, "WSARecv");
    if (RTN_Valid(wsarecv_func))
    {
#if !defined(NDEBUG)
      tfm::format(log_file, "WSARecv is located at %s\n",
                  addrint_to_hexstring(RTN_Address(wsarecv_func)));
#endif
      instrument_wsarecvs(wsarecv_func);
    }

    RTN wsarecvfrom_func = RTN_FindByName(loaded_img, "WSARecvFrom");
    if (RTN_Valid(wsarecvfrom_func))
    {
#if !defined(NDEBUG)
      tfm::format(log_file, "WSARecvFrom is located at %s\n",
                  addrint_to_hexstring(RTN_Address(wsarecvfrom_func)));
#endif
      instrument_wsarecvs(wsarecvfrom_func);
    }

    current_running_phase = capturing_phase; capturing::initialize();
  }
#endif
  return;
}

/*================================================================================================*/

//BOOL process_create_instrumenter(CHILD_PROCESS created_process, VOID* data)
//{
//#if !defined(NDEBUG)
//  tfm::format(log_file, "new process created with id %d\n", CHILD_PROCESS_GetId(created_process));
//#endif
//  return TRUE;
//}
