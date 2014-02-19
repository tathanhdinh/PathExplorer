#include <pin.H>

#include <fstream>
#include <vector>
#include <map>
#include <set>

#include "../util/stuffs.h"
#include "../base/branch.h"
#include "../base/instruction.h"
#include "../base/cond_direct_instruction.h"
#include "instrumentation_functions.h"
#include "common.h"
#include "tainting_functions.h"
#include "resolving_functions.h"

/*================================================================================================*/

inline static void exec_tainting_phase(INS& ins, ptr_instruction_t examined_ins)
{
  /* taint logging */
  if (examined_ins->is_syscall)
  {
    INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)tainting::syscall_instruction,
                             IARG_INST_PTR, IARG_END);
  }
  else
  {
    // general logging
    INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)tainting::general_instruction,
                             IARG_INST_PTR, IARG_END);

    if (examined_ins->is_mem_read)
    {
      // memory read logging
      INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)tainting::mem_read_instruction,
                               IARG_INST_PTR, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE,
                               IARG_CONTEXT, IARG_END);

      if (examined_ins->has_mem_read2)
      {
        // memory read2 (e.g. rep cmpsb instruction)
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)tainting::mem_read_instruction,
                                 IARG_INST_PTR, IARG_MEMORYREAD2_EA, IARG_MEMORYREAD_SIZE,
                                 IARG_CONTEXT, IARG_END);
      }
    }

    if (examined_ins->is_mem_write)
    {
      // memory written logging
      INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)tainting::mem_write_instruction,
                               IARG_INST_PTR, IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE, IARG_END);
    }

//    if (examined_ins->is_cond_direct_cf)
//    {
//      // conditional branch logging
//      INS_InsertPredicatedCall(ins, IPOINT_BEFORE,
//                               (AFUNPTR)tainting::cond_branch_instruction,
//                               IARG_INST_PTR,
//                               IARG_BRANCH_TAKEN,
//                               IARG_END);
//    }
//    else
//    {
//    }
  }
  /* taint propagating */
  INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)tainting::graphical_propagation,
                           IARG_INST_PTR, IARG_END);
  return;
}

/*================================================================================================*/

inline static void exec_rollbacking_state(INS& ins, ptr_instruction_t examined_ins)
{
  INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)resolving_ins_count_analyzer,
                           IARG_INST_PTR, IARG_END);

  if (examined_ins->is_mem_write)
  {
    INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)resolving_st_to_mem_analyzer,
                             IARG_INST_PTR, IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE, IARG_END);
  }
  else
  {
    // note that conditional branches are always direct
    if (examined_ins->is_cond_direct_cf)
    {
      INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)resolving_cond_branch_analyzer,
                               IARG_INST_PTR, IARG_BRANCH_TAKEN, IARG_END);
    }
    else
    {
      if (examined_ins->is_uncond_indirect_cf)
      {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)resolving_indirect_branch_call_analyzer,
                                 IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_END);
      }
    }
  }
  return;
}

/*================================================================================================*/

/**
 * @brief instrumentation function: all analysis functions are inserted using
 * INS_InsertPredicatedCall to make sure that the instruction is examined iff it is executed.
 * 
 * @param ins current examined instruction.
 * @param data not used.
 * @return VOID
 */
VOID ins_instrumenter(INS ins, VOID *data)
{
  if (current_running_state != capturing_state)
  {
    // examining statically instructions
    ptr_instruction_t examined_ins(new instruction(ins));
    ins_at_addr[examined_ins->address] = examined_ins;
    if (examined_ins->is_cond_direct_cf)
    {
      ins_at_addr[examined_ins->address].reset(new cond_direct_instruction(*examined_ins));
    }

    switch (current_running_state)
    {
    case tainting_state:
      exec_tainting_phase(ins, examined_ins);
      break;

    case rollbacking_state:
      exec_rollbacking_state(ins, examined_ins);
      break;

    default:
      break;
    }
  }

  return;
}

/*================================================================================================*/

VOID image_load_instrumenter(IMG loaded_img, VOID *data)
{
  boost::filesystem::path loaded_image_path(IMG_Name(loaded_img));

  BOOST_LOG_SEV(log_instance, logging::trivial::info) 
    << boost::format("module %s loaded") % loaded_image_path.filename();

#if BOOST_OS_WINDOWS
  // verify whether the winsock2 module is loaded
  const static std::string winsock_dll_name("WS2_32.dll");
  if (loaded_image_path.filename() == winsock_dll_name)
  {
    BOOST_LOG_SEV(log_instance, logging::trivial::info)
      << "winsock module found";

    RTN recv_function = RTN_FindByName(loaded_img, "recv");
    if (RTN_Valid(recv_function))
    {
      BOOST_LOG_SEV(log_instance, boost::log::trivial::info) << "recv instrumented";

      RTN_Open(recv_function);

      RTN_InsertCall(recv_function, IPOINT_BEFORE, 
                     (AFUNPTR)logging_before_recv_functions_analyzer,
                     IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                     IARG_END);

      RTN_InsertCall(recv_function, IPOINT_AFTER, 
                     (AFUNPTR)logging_after_recv_functions_analyzer,
                     IARG_FUNCRET_EXITPOINT_VALUE,
                     IARG_END);

      RTN_Close(recv_function);
    }

    RTN recvfrom_function = RTN_FindByName(loaded_img, "recvfrom");
    if (RTN_Valid(recvfrom_function))
    {
      BOOST_LOG_SEV(log_instance, boost::log::trivial::info) 
        << "recvfrom instrumented";

      RTN_Open(recvfrom_function);

      RTN_InsertCall(recvfrom_function, IPOINT_BEFORE,
                     (AFUNPTR)logging_before_recv_functions_analyzer,
                     IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                     IARG_END);

      RTN_InsertCall(recvfrom_function, IPOINT_AFTER,
                     (AFUNPTR)logging_after_recv_functions_analyzer,
                     IARG_FUNCRET_EXITPOINT_VALUE,
                     IARG_END);

      RTN_Close(recvfrom_function);
    }

    RTN wsarecv_function = RTN_FindByName(loaded_img, "WSARecv");
    if (RTN_Valid(wsarecv_function))
    {
      BOOST_LOG_SEV(log_instance, boost::log::trivial::info) 
        << "WSARecv instrumented";

      RTN_Open(wsarecv_function);

      RTN_InsertCall(wsarecv_function, IPOINT_BEFORE,
        (AFUNPTR)logging_before_wsarecv_functions_analyzer,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
        IARG_END);
      RTN_InsertCall(wsarecv_function, IPOINT_AFTER,
        (AFUNPTR)logging_after_wsarecv_funtions_analyzer,
        IARG_END);

      RTN_Close(wsarecv_function);
    }

    RTN wsarecvfrom_function = RTN_FindByName(loaded_img, "WSARecvFrom");
    if (RTN_Valid(wsarecvfrom_function))
    {
      BOOST_LOG_SEV(log_instance, boost::log::trivial::info) << "WSARecvFrom instrumented";

      RTN_Open(wsarecvfrom_function);

      RTN_InsertCall(wsarecvfrom_function, IPOINT_BEFORE,
        (AFUNPTR)logging_before_wsarecv_functions_analyzer,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
        IARG_END);
      RTN_InsertCall(wsarecvfrom_function, IPOINT_AFTER,
        (AFUNPTR)logging_after_wsarecv_funtions_analyzer,
        IARG_END);

      RTN_Close(wsarecvfrom_function);
    }
  }
#endif
  return;
}

/*================================================================================================*/

BOOL process_create_instrumenter(CHILD_PROCESS created_process, VOID* data)
{
  BOOST_LOG_SEV(log_instance, logging::trivial::warning) 
    << boost::format("new process created with id %d") % CHILD_PROCESS_GetId(created_process);
  return TRUE;
}
