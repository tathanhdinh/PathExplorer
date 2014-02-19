#include <pin.H>

#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <stack>
#include <sstream>
#include <fstream>
#include <string>
#include <iostream>
#include <limits>

#include <boost/predef.h>
#include <boost/timer.hpp>
#include <boost/random.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/format.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>

#include "base/instruction.h"
#include "base/checkpoint.h"
#include "base/branch.h"
#include "base/cond_direct_instruction.h"
#include "operation/instrumentation_functions.h"
#include "operation/tainting_functions.h"
#include "operation/common.h"
#include "util/stuffs.h"

extern "C" 
{
#include <xed-interface.h>
}

/* ---------------------------------------------------------------------------------------------- */
/*                                             global variables                                   */
/* ---------------------------------------------------------------------------------------------- */
std::map<ADDRINT, ptr_instruction_t>            ins_at_addr;   // statically examined instructions
std::map<UINT32, ptr_instruction_t>             ins_at_order;  // dynamically examined instructions

UINT32                                          total_rollback_times;
UINT32                                          local_rollback_times;
UINT32                                          trace_size;
UINT32                                          used_checkpoint_number;

UINT32                                          max_total_rollback_times;
UINT32                                          max_local_rollback_times;
UINT32                                          max_trace_size;

std::vector<ptr_checkpoint_t>                   saved_checkpoints;

ptr_cond_direct_instructions_t                  detected_input_dep_cfis;
ptr_cond_direct_instruction_t                   exploring_cfi;
UINT32                                          exploring_cfi_exec_order;

std::vector<ADDRINT>                            explored_trace;
UINT32                                          current_exec_order;

UINT32                                          received_msg_num;
ADDRINT                                         received_msg_addr;
UINT32                                          received_msg_size;

#if BOOST_OS_LINUX
ADDRINT                                         logged_syscall_index;   // logged syscall index
ADDRINT                                         logged_syscall_args[6]; // logged syscall arguments
#endif

running_state                                   current_running_state;

UINT64                                          executed_ins_number;
UINT64                                          econed_ins_number;

namespace btime = boost::posix_time;
boost::shared_ptr<btime::ptime>                 start_ptr_time;
boost::shared_ptr<btime::ptime>                 stop_ptr_time;

namespace logging = boost::log;
namespace sinks   = boost::log::sinks;
namespace sources = boost::log::sources;
typedef sinks::text_file_backend text_backend;
typedef sinks::synchronous_sink<text_backend>   sink_file_backend;
typedef logging::trivial::severity_level        log_level;
sources::severity_logger<log_level>             log_instance;
boost::shared_ptr<sink_file_backend>            log_sink;

namespace bran = boost::random;
bran::taus88                                    rgen(static_cast<unsigned int>(std::time(0)));
bran::uniform_int_distribution<UINT8>           uint8_uniform;

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
VOID start_tracing(VOID *data)
{
  start_ptr_time.reset(new btime::ptime(btime::microsec_clock::local_time()));

  max_trace_size            = max_trace_length.Value();
  trace_size                = 0;
  current_exec_order        = 0;

  total_rollback_times      = 0;
  local_rollback_times      = 0;
  used_checkpoint_number    = 0;

  max_total_rollback_times  = max_total_rollback.Value();
  total_rollback_times      = 0;
  
  max_local_rollback_times  = max_local_rollback.Value();
  local_rollback_times      = 0;
  
  executed_ins_number       = 0;
  econed_ins_number         = 0;
  
  received_msg_num          = 0;
  logged_syscall_index      = syscall_inexist;

  ::srand(static_cast<uint32_t>(::time(0)));

  return;
}


/**
 * @brief stop_tracing
 * @param code
 * @param data
 * @return
 */
VOID stop_tracing(INT32 code, VOID *data)
{
  stop_ptr_time.reset(new btime::ptime(btime::microsec_clock::local_time()));

  boost::posix_time::time_duration elapsed_time = *stop_ptr_time - *start_ptr_time;
  uint64_t elapsed_millisec = elapsed_time.total_milliseconds();

  journal_static_trace("static_trace.log");
  
  UINT32 resolved_cfi_num = 0;
  ptr_cond_direct_instructions_t::iterator cfi_iter = detected_input_dep_cfis.begin();
  for (; cfi_iter != detected_input_dep_cfis.end(); ++cfi_iter)
  {
    if ((*cfi_iter)->is_resolved) resolved_cfi_num++;
  }

  BOOST_LOG_SEV(log_instance, logging::trivial::info)
      << boost::format("stop %d milli-seconds elapsed, %d rollbacks used, %d/%d branches resolved")
         % elapsed_millisec % total_rollback_times
         % resolved_cfi_num % detected_input_dep_cfis.size();

        
//  BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
//    << boost::format("economized/total executed instruction number %d/%d")
//        % econed_ins_number % executed_ins_number;

  log_sink->flush();
                              
  return;
}


/**
 * @brief initialize_logging
 * @param log_filename
 */
inline static void initialize_logging(std::string log_filename)
{
  log_sink = logging::add_file_log(log_filename.c_str());
  logging::add_common_attributes();

  return;
}

/* ---------------------------------------------------------------------------------------------- */
/*                                          main function                                         */
/* ---------------------------------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
  initialize_logging("path_explorer.log");

  BOOST_LOG_SEV(log_instance, logging::trivial::info) << "initialize image symbol tables";
  PIN_InitSymbols();

  BOOST_LOG_SEV(log_instance, logging::trivial::info) << "initialize Pin";
  if (PIN_Init(argc, argv))
  {
    BOOST_LOG_SEV(log_instance, logging::trivial::fatal) << "Pin initialization failed";
    log_sink->flush();
  }
  else
  {
    BOOST_LOG_SEV(log_instance, logging::trivial::info) << "Pin initialization success";

    BOOST_LOG_SEV(log_instance, logging::trivial::info) << "activate Pintool data-initialization";
    PIN_AddApplicationStartFunction(start_tracing, 0);  // 0 is the (unused) input data

    BOOST_LOG_SEV(log_instance, logging::trivial::info) << "activate image-load instrumenter";
    IMG_AddInstrumentFunction(image_load_instrumenter, 0);
    
    BOOST_LOG_SEV(log_instance, logging::trivial::info) << "activate process-fork instrumenter";
    PIN_AddFollowChildProcessFunction(process_create_instrumenter, 0);

    BOOST_LOG_SEV(log_instance, logging::trivial::info) << "activate instruction instrumenters";
    INS_AddInstrumentFunction(ins_instrumenter, 0);

#if BOOST_OS_LINUX
    PIN_AddSyscallEntryFunction(tainting::syscall_entry_analyzer, 0);
    PIN_AddSyscallExitFunction(tainting::syscall_exit_analyzer, 0);
#elif
    // In Windows environment, the input tracing is through socket api instead of system call
#endif

    BOOST_LOG_SEV(log_instance, boost::log::trivial::info) << "activate Pintool data-finalization";
    PIN_AddFiniFunction(stop_tracing, 0);

    // now the control is passed to pin, so the main function will never return
    PIN_StartProgram();
  }

  // in fact, it is reached only if the Pin initialization fails
  return 0;
}
