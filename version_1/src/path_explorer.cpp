#include "operation/instrumentation.h"
#include "operation/tainting_phase.h"
#include "operation/capturing_phase.h"
#include "operation/common.h"
#include "util/stuffs.h"
#include <ctime>

/* ---------------------------------------------------------------------------------------------- */
/*                                        global variables                                        */
/* ---------------------------------------------------------------------------------------------- */
std::map<ADDRINT, ptr_instruction_t>  ins_at_addr;   // statically examined instructions
std::map<UINT32, ptr_instruction_t>   ins_at_order;  // dynamically examined instructions

UINT32                                total_rollback_times;
UINT32                                local_rollback_times;
UINT32                                trace_size;
UINT32                                used_checkpoint_number;

UINT32                                max_total_rollback_times;
UINT32                                max_local_rollback_times;
UINT32                                max_trace_size;

//std::vector<ptr_checkpoint_t>         saved_checkpoints;
ptr_checkpoints_t                     saved_checkpoints;

ptr_cond_direct_inss_t                detected_input_dep_cfis;
ptr_cond_direct_ins_t                 exploring_cfi;

UINT32                                current_exec_order;

UINT32                                received_msg_num;
ADDRINT                               received_msg_addr;
UINT32                                received_msg_size;

#if defined(__gnu_linux__)
ADDRINT                               logged_syscall_index;   // logged syscall index
ADDRINT                               logged_syscall_args[6]; // logged syscall arguments
#endif

running_phase                         current_running_phase;

UINT64                                executed_ins_number;
UINT64                                econed_ins_number;

time_t                                start_time;
time_t                                stop_time;

std::ofstream                         log_file;

/* ---------------------------------------------------------------------------------------------- */
/*                                         input handler functions                                */
/* ---------------------------------------------------------------------------------------------- */
KNOB<UINT32>  max_local_rollback  (KNOB_MODE_WRITEONCE, "pintool",
                                   "r", "7000",
                                   "specify the maximum local number of rollback" );

KNOB<UINT32>  max_total_rollback  (KNOB_MODE_WRITEONCE, "pintool",
                                   "t", "4000000000",
                                   "specify the maximum total number of rollback" );

KNOB<UINT32>  max_trace_length    (KNOB_MODE_WRITEONCE, "pintool", 
                                   "l", "100", "specify the length of the longest trace" );

/* ---------------------------------------------------------------------------------------------- */
/*                                  basic instrumentation functions                               */
/* ---------------------------------------------------------------------------------------------- */
/**
 * @brief initialize input variables
 */
VOID start_exploring(VOID *data)
{
  start_time                = std::time(0);

  max_trace_size            = max_trace_length.Value();
  trace_size                = 0;
  current_exec_order        = 0;

  total_rollback_times      = 0;
  local_rollback_times      = 0;
  used_checkpoint_number    = 0;

  max_total_rollback_times  = max_total_rollback.Value();
  max_local_rollback_times  = max_local_rollback.Value();
  
  executed_ins_number       = 0;
  econed_ins_number         = 0;
  
  received_msg_num          = 0;
#if defined(__gnu_linux__)
  logged_syscall_index      = syscall_inexist;
#endif

  exploring_cfi.reset();
  current_running_phase     = capturing_phase;

  log_file.open("path_explorer.log", std::ofstream::out | std::ofstream::trunc);

  ::srand(static_cast<uint32_t>(::time(0)));

#if !defined(ENABLE_FAST_ROLLBACK)
  tfm::format(log_file, "local rollback %d, trace depth %d, fast rollback disabled\n",
              max_local_rollback_times, max_trace_size);
#else
  tfm::format(log_file, "local rollback %d, trace depth %d, fast rollback enabled\n",
              max_local_rollback_times, max_trace_size);
#endif

  return;
}


/**
 * @brief collect explored results
 */
VOID stop_exploring(INT32 code, VOID *data)
{
  stop_time = std::time(0);

  save_static_trace("static_trace.log");
  
  UINT32 resolved_cfi_num = 0;
  ptr_cond_direct_inss_t::iterator cfi_iter = detected_input_dep_cfis.begin();
  for (; cfi_iter != detected_input_dep_cfis.end(); ++cfi_iter)
  {
    if ((*cfi_iter)->is_resolved) resolved_cfi_num++;
    total_rollback_times += (*cfi_iter)->used_rollback_num;
  }

  tfm::format(log_file, "%d seconds elapsed, %d rollbacks used, %d/%d branches resolved\n",
              (stop_time - start_time),
              total_rollback_times, resolved_cfi_num, detected_input_dep_cfis.size());

  log_file.close();
  return;
}


/* ---------------------------------------------------------------------------------------------- */
/*                                          main function                                         */
/* ---------------------------------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
  log_file << "initialize image symbol tables\n";
  PIN_InitSymbols();

  log_file << "initialize Pin";
  if (PIN_Init(argc, argv))
  {
    log_file << "Pin initialization failed\n";
    log_file.close();
  }
  else
  {
    log_file << "Pin initialization success\n";

    log_file << "activate Pintool data-initialization\n";
    PIN_AddApplicationStartFunction(start_exploring, 0);  // 0 is the (unused) input data

    log_file << "activate image-load instrumenter\n";
    IMG_AddInstrumentFunction(image_load_instrumenter, 0);
    
    log_file << "activate process-fork instrumenter\n";
    PIN_AddFollowChildProcessFunction(process_create_instrumenter, 0);

    log_file << "activate instruction instrumenters\n";
    INS_AddInstrumentFunction(ins_instrumenter, 0);

    // In Windows environment, the input tracing is through socket api instead of system call
#if defined(__gnu_linux__)
    PIN_AddSyscallEntryFunction(capturing::syscall_entry_analyzer, 0);
    PIN_AddSyscallExitFunction(capturing::syscall_exit_analyzer, 0);
#endif

    log_file << "activate Pintool data-finalization\n";
    PIN_AddFiniFunction(stop_exploring, 0);

    // now the control is passed to pin, so the main function will never return
    PIN_StartProgram();
  }

  // in fact, it is reached only if the Pin initialization fails
  return 0;
}
