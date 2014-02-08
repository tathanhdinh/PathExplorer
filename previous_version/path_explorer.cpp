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

#include <boost/timer.hpp>
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

#include "instruction.h"
#include "checkpoint.h"
#include "stuffs.h"
#include "variable.h"
#include "branch.h"
#include "instrumentation_functions.h"
#include "analysis_functions.h"

extern "C" 
{
#include <xed-interface.h>
}

/* ---------------------------------------------------------------------------------------------- */
/*                                             global variables                                   */
/* ---------------------------------------------------------------------------------------------- */
std::map<ADDRINT, instruction>                addr_ins_static_map;    // statically examined instructions
std::map<UINT32, instruction>                 order_ins_dynamic_map;  // dynamically examined instructions

ADDRINT                                       logged_syscall_index;   // logged syscall index
ADDRINT                                       logged_syscall_args[6]; // logged syscall arguments

UINT32                                        total_rollback_times;
UINT32                                        local_rollback_times;
UINT32                                        trace_size;
UINT32                                        used_checkpoint_number;

UINT32                                        max_total_rollback_times;
UINT32                                        max_local_rollback_times;
UINT32                                        max_trace_size;


bool                                          in_tainting;

df_diagram                                dta_graph;
map_ins_io                                    dta_inss_io;
df_vertex_desc_set                          dta_outer_vertices;

std::vector<ptr_checkpoint>                   saved_ptr_checkpoints;
ptr_checkpoint                                master_ptr_checkpoint;
ptr_checkpoint                                last_active_ptr_checkpoint;

std::set<ADDRINT>                             active_input_dep_addrs;

std::pair< ptr_checkpoint, 
           std::set<ADDRINT> >                active_nearest_checkpoint;

std::map< UINT32,
          std::vector<ptr_checkpoint> >       exepoint_checkpoints_map;

std::map<UINT32, ptr_branch>                  order_input_dep_ptr_branch_map;
std::map<UINT32, ptr_branch>                  order_input_indep_ptr_branch_map;
std::map<UINT32, ptr_branch>                  order_tainted_ptr_branch_map;

std::vector<ptr_branch>                       found_new_ptr_branches;
std::vector<ptr_branch>                       total_resolved_ptr_branches;
std::vector<ptr_branch>                       total_input_dep_ptr_branches;

ptr_branch                                    active_ptr_branch;
ptr_branch                                    last_active_ptr_branch;
ptr_branch                                    exploring_ptr_branch;

std::vector<ADDRINT>                          explored_trace;

UINT32                                        received_msg_num;
ADDRINT                                       received_msg_addr;
UINT32                                        received_msg_size;
ADDRINT																				received_msg_struct_addr;

UINT64                                        executed_ins_number;
UINT64                                        econed_ins_number;

boost::shared_ptr<boost::posix_time::ptime>   start_ptr_time;
boost::shared_ptr<boost::posix_time::ptime>   stop_ptr_time;

namespace logging = boost::log;
namespace sinks   = boost::log::sinks;
namespace sources = boost::log::sources;
typedef sinks::text_file_backend text_backend;
typedef sinks::synchronous_sink<text_backend> sink_file_backend;
typedef logging::trivial::severity_level      log_level;

sources::severity_logger<log_level>           log_instance;
boost::shared_ptr<sink_file_backend>          log_sink;


/* ---------------------------------------------------------------------------------------------- */
/*                                         input handler functions                                */
/* ---------------------------------------------------------------------------------------------- */
KNOB<BOOL>    print_debug_text    (KNOB_MODE_WRITEONCE, "pintool",
                                   "d", "1",
                                   "print debug text" );

KNOB<UINT32>  max_local_rollback  (KNOB_MODE_WRITEONCE, "pintool",
                                   "r", "7000",
                                   "specify the maximum local number of rollback" );

KNOB<UINT32>  max_total_rollback  (KNOB_MODE_WRITEONCE, "pintool",
                                   "t", "4000000000",
                                   "specify the maximum total number of rollback" );

KNOB<UINT32>  max_trace_length    (KNOB_MODE_WRITEONCE, "pintool", 
                                   "l", "100", "specify the length of the longest trace" );

/* ---------------------------------------------------------------------------------------------- */
/*                                                instrumental functions                          */
/* -------------------------------------------------------+-------------------------------------- */
VOID start_tracing(VOID *data)
{
  max_trace_size            = max_trace_length.Value();
  trace_size                = 0;

  total_rollback_times      = 0;
  local_rollback_times      = 0;
  used_checkpoint_number    = 0;
  
  max_total_rollback_times  = max_total_rollback.Value();
  total_rollback_times      = 0;
  
  max_local_rollback_times  = max_local_rollback.Value();
  local_rollback_times      = 0;
  
  executed_ins_number       = 0;
  econed_ins_number         = 0;
  
  in_tainting               = true;
  received_msg_num          = 0;
  logged_syscall_index      = syscall_inexist;

  ::srand(static_cast<uint32_t>(::time(0)));

  return;
}

/*====================================================================================================================*/

VOID stop_tracing(INT32 code, VOID *data)
{
  using namespace boost::posix_time;
  if (!stop_ptr_time) 
  {
    stop_ptr_time.reset(new ptime(microsec_clock::local_time()));
  }

  boost::posix_time::time_duration elapsed_time = *stop_ptr_time - *start_ptr_time;
  uint64_t elapsed_millisec = elapsed_time.total_milliseconds();

  journal_static_trace("static_trace.log");
  
  BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
    << boost::format("stop examining, %d milli-seconds elapsed, %d rollbacks used, and %d/%d branches resolved") 
        % elapsed_millisec % total_rollback_times
        % (total_resolved_ptr_branches.size() + found_new_ptr_branches.size()) 
        % total_input_dep_ptr_branches.size();
        
  BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
    << boost::format("economized/total executed instruction number %d/%d") 
        % econed_ins_number % executed_ins_number;

  log_sink->flush();
                              
  return;
}

/*====================================================================================================================*/

inline static void initialize_logging(std::string log_filename)
{
//   log_sink = logging::add_file_log
//   (
//     keywords::file_name = log_filename.c_str()
//     keywords::format = expressions::format("<%1%> %2%") 
//       % trivial::severity % expressions::smessage
//   );

  log_sink = logging::add_file_log(log_filename.c_str());
  logging::core::get()->set_filter
  (
    logging::trivial::severity >= logging::trivial::info
  );
  logging::add_common_attributes();

  return;
}

/* ------------------------------------------------------------------------------------------------------------------ */
/*                                                        main function                                               */
/* ------------------------------------------------------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
  initialize_logging("path_explorer.log");

  BOOST_LOG_SEV(log_instance, boost::log::trivial::info) << "initialize image symbol tables";
  PIN_InitSymbols();

  BOOST_LOG_SEV(log_instance, boost::log::trivial::info) << "initialize Pin";
  if (PIN_Init(argc, argv))
  {
    BOOST_LOG_SEV(log_instance, boost::log::trivial::fatal) << "Pin initialization failed";
    log_sink->flush();
  }
  else
  {
    BOOST_LOG_SEV(log_instance, boost::log::trivial::info) << "Pin initialization success";

    BOOST_LOG_SEV(log_instance, boost::log::trivial::info) << "activate Pintool data-initialization";
    PIN_AddApplicationStartFunction(start_tracing, 0);  // 0 is the (unused) input data

    BOOST_LOG_SEV(log_instance, boost::log::trivial::info) << "activate image-load instrumenter";
    IMG_AddInstrumentFunction(image_load_instrumenter, 0);
    
    BOOST_LOG_SEV(log_instance, boost::log::trivial::info) << "activate process-fork instrumenter";
    PIN_AddFollowChildProcessFunction(process_create_instrumenter, 0);

    BOOST_LOG_SEV(log_instance, boost::log::trivial::info) << "activate instruction instrumenters";
    INS_AddInstrumentFunction(ins_instrumenter, 0);

#if defined(__linux__)
    PIN_AddSyscallEntryFunction(syscall_entry_analyzer, 0);
    PIN_AddSyscallExitFunction(syscall_exit_analyzer, 0);
#elif
    // In Windows environment, the input tracing is through socket api instead of system call
#endif

    BOOST_LOG_SEV(log_instance, boost::log::trivial::info) << "activate Pintool data-finalization";
    PIN_AddFiniFunction(stop_tracing, 0);

    log_sink->flush();

    // now the control is passed to pin, so the main function will never return
    PIN_StartProgram();
  }

  // in fact, it is never reached
  return 0;
}
