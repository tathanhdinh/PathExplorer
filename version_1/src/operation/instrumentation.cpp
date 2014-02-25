#include <pin.H>

#include <fstream>
#include <vector>
#include <map>
#include <set>

#include "../util/stuffs.h"
#include "common.h"
#include "instrumentation.h"
#include "tainting_phase.h"
#include "rollbacking_phase.h"

/*================================================================================================*/

static inline void exec_tainting_phase(INS& ins, ptr_instruction_t examined_ins)
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
  }

  /* taint propagating */
  INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)tainting::graphical_propagation,
                           IARG_INST_PTR, IARG_END);
  return;
}

/*================================================================================================*/

static inline void exec_rollbacking_phase(INS& ins, ptr_instruction_t examined_ins)
{
  INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)rollbacking::generic_instruction,
                           IARG_INST_PTR, IARG_END);

  if (examined_ins->is_cond_direct_cf)
  {
    INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)rollbacking::control_flow_instruction,
                             IARG_INST_PTR, IARG_END);
  }

  if (examined_ins->is_mem_write)
  {
    INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)rollbacking::mem_write_instruction,
                             IARG_INST_PTR, IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE, IARG_END);
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
  if (current_running_phase != capturing_state)
  {
    // examining statically instructions
    ptr_instruction_t examined_ins(new instruction(ins));
    ins_at_addr[examined_ins->address] = examined_ins;
    if (examined_ins->is_cond_direct_cf)
    {
      ins_at_addr[examined_ins->address].reset(new cond_direct_instruction(*examined_ins));
    }

    switch (current_running_phase)
    {
    case tainting_state:
      exec_tainting_phase(ins, examined_ins);
      break;

    case rollbacking_state:
      exec_rollbacking_phase(ins, examined_ins);
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
#if !defined(NDEBUG)
  tfm::format(log_file, "module loaded %s\n", IMG_Name(loaded_img));
#endif

#if defined(_WIN32) || defined(_WIN64)
  // verify whether the winsock2 module is loaded
  const static std::string winsock_dll_name("WS2_32.dll");
  if (loaded_image_path.filename() == winsock_dll_name)
  {
    log_file << "winsock module found\n";

    RTN recv_function = RTN_FindByName(loaded_img, "recv");
    if (RTN_Valid(recv_function))
    {
      log_file << "recv instrumented\n";

      RTN_Open(recv_function);

      RTN_InsertCall(recv_function, IPOINT_BEFORE, (AFUNPTR)logging_before_recv_functions_analyzer,
                     IARG_FUNCARG_ENTRYPOINT_VALUE, 1, IARG_END);

      RTN_InsertCall(recv_function, IPOINT_AFTER, (AFUNPTR)logging_after_recv_functions_analyzer,
                     IARG_FUNCRET_EXITPOINT_VALUE, IARG_END);

      RTN_Close(recv_function);
    }

    RTN recvfrom_function = RTN_FindByName(loaded_img, "recvfrom");
    if (RTN_Valid(recvfrom_function))
    {
      log_file << "recvfrom instrumented\n";

      RTN_Open(recvfrom_function);

      RTN_InsertCall(recvfrom_function, IPOINT_BEFORE,
                     (AFUNPTR)logging_before_recv_functions_analyzer,
                     IARG_FUNCARG_ENTRYPOINT_VALUE, 1, IARG_END);

      RTN_InsertCall(recvfrom_function, IPOINT_AFTER,
                     (AFUNPTR)logging_after_recv_functions_analyzer,
                     IARG_FUNCRET_EXITPOINT_VALUE, IARG_END);

      RTN_Close(recvfrom_function);
    }

    RTN wsarecv_function = RTN_FindByName(loaded_img, "WSARecv");
    if (RTN_Valid(wsarecv_function))
    {
      log_file << "WSARecv instrumented\n";

      RTN_Open(wsarecv_function);

      RTN_InsertCall(wsarecv_function, IPOINT_BEFORE,
                     (AFUNPTR)logging_before_wsarecv_functions_analyzer,
                     IARG_FUNCARG_ENTRYPOINT_VALUE, 1, IARG_END);

      RTN_InsertCall(wsarecv_function, IPOINT_AFTER,
                     (AFUNPTR)logging_after_wsarecv_funtions_analyzer, IARG_END);

      RTN_Close(wsarecv_function);
    }

    RTN wsarecvfrom_function = RTN_FindByName(loaded_img, "WSARecvFrom");
    if (RTN_Valid(wsarecvfrom_function))
    {
      log_file << "WSARecvFrom instrumented\n";

      RTN_Open(wsarecvfrom_function);

      RTN_InsertCall(wsarecvfrom_function, IPOINT_BEFORE,
                     (AFUNPTR)logging_before_wsarecv_functions_analyzer,
                     IARG_FUNCARG_ENTRYPOINT_VALUE, 1, IARG_END);

      RTN_InsertCall(wsarecvfrom_function, IPOINT_AFTER,
                     (AFUNPTR)logging_after_wsarecv_funtions_analyzer, IARG_END);

      RTN_Close(wsarecvfrom_function);
    }
  }
#endif
  return;
}

/*================================================================================================*/

BOOL process_create_instrumenter(CHILD_PROCESS created_process, VOID* data)
{
#if !defined(NDEBUG)
  tfm::format(log_file, "new process created with id %d\n", CHILD_PROCESS_GetId(created_process));
#endif
  return TRUE;
}
