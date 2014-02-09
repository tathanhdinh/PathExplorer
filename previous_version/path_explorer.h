#ifndef PATH_EXPLORER_H
#define PATH_EXPLORER_H

#include <pin.H>

#include <map>

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

extern std::map<ADDRINT, instruction>                addr_ins_static_map;    // statically examined instructions
extern std::map<UINT32, instruction>                 order_ins_dynamic_map;  // dynamically examined instructions

extern ADDRINT                                       logged_syscall_index;   // logged syscall index
extern ADDRINT                                       logged_syscall_args[6]; // logged syscall arguments

extern UINT32                                        total_rollback_times;
extern UINT32                                        local_rollback_times;
extern UINT32                                        trace_size;
extern UINT32                                        used_checkpoint_number;

extern UINT32                                        max_total_rollback_times;
extern UINT32                                        max_local_rollback_times;
extern UINT32                                        max_trace_size;

extern bool                                          in_tainting;

extern df_diagram                                    dta_graph;
extern map_ins_io                                    dta_inss_io;
extern df_vertex_desc_set                            dta_outer_vertices;

extern std::vector<ptr_checkpoint>                   saved_ptr_checkpoints;
extern ptr_checkpoint                                master_ptr_checkpoint;
extern ptr_checkpoint                                last_active_ptr_checkpoint;

extern std::set<ADDRINT>                             active_input_dep_addrs;

extern std::pair< ptr_checkpoint, 
                  std::set<ADDRINT> >                active_nearest_checkpoint;

extern std::map< UINT32,
                 std::vector<ptr_checkpoint> >       exepoint_checkpoints_map;

extern std::map<UINT32, ptr_branch>                  order_input_dep_ptr_branch_map;
extern std::map<UINT32, ptr_branch>                  order_input_indep_ptr_branch_map;
extern std::map<UINT32, ptr_branch>                  order_tainted_ptr_branch_map;

extern std::vector<ptr_branch>                       found_new_ptr_branches;
extern std::vector<ptr_branch>                       total_resolved_ptr_branches;
extern std::vector<ptr_branch>                       total_input_dep_ptr_branches;

extern ptr_branch                                    active_ptr_branch;
extern ptr_branch                                    last_active_ptr_branch;
extern ptr_branch                                    exploring_ptr_branch;

extern std::vector<ADDRINT>                          explored_trace;

extern UINT32                                        received_msg_num;
extern ADDRINT                                       received_msg_addr;
extern UINT32                                        received_msg_size;
extern ADDRINT                                       received_msg_struct_addr;

extern UINT64                                        executed_ins_number;
extern UINT64                                        econed_ins_number;

namespace btime = boost::posix_time;
extern boost::shared_ptr<btime::ptime>               start_ptr_time;
extern boost::shared_ptr<btime::ptime>               stop_ptr_time;

namespace logging = boost::log;
namespace sinks   = boost::log::sinks;
namespace sources = boost::log::sources;
typedef sinks::text_file_backend text_backend;
typedef sinks::synchronous_sink<text_backend> sink_file_backend;
typedef logging::trivial::severity_level      log_level;

extern sources::severity_logger<log_level>           log_instance;
extern boost::shared_ptr<sink_file_backend>          log_sink;

extern KNOB<BOOL>    print_debug_text;
extern KNOB<UINT32>  max_local_rollback;
extern KNOB<UINT32>  max_total_rollback;
extern KNOB<UINT32>  max_trace_length;

#endif
