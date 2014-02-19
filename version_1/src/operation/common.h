#ifndef COMMON_H
#define COMMON_H

#include <pin.H>

#include <boost/predef.h>
#include <boost/timer.hpp>
#include <boost/random.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/format.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/lookup_edge.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>

#include "../base/checkpoint.h"
#include "../base/cond_direct_instruction.h"

typedef enum
{
  capturing_state   = 0,
  tainting_state    = 1,
  rollbacking_state = 2
} running_state;

extern std::map<ADDRINT, ptr_instruction_t>         ins_at_addr;
extern std::map<UINT32, ptr_instruction_t>          ins_at_order;

extern UINT32                                       total_rollback_times;
extern UINT32                                       local_rollback_times;
extern UINT32                                       trace_size;
extern UINT32                                       used_checkpoint_number;

extern UINT32                                       max_total_rollback_times;
extern UINT32                                       max_local_rollback_times;
extern UINT32                                       max_trace_size;

extern bool                                         in_tainting;

//extern df_diagram                                   dta_graph;
//extern df_vertex_desc_set                           dta_outer_vertices;

extern std::vector<ptr_checkpoint_t>                saved_checkpoints;
extern ptr_checkpoint_t                             master_ptr_checkpoint;
extern ptr_checkpoint_t                             last_active_ptr_checkpoint;

extern std::pair< ptr_checkpoint_t,
                  std::set<ADDRINT> >               active_nearest_checkpoint;

extern std::map< UINT32,
                 std::vector<ptr_checkpoint_t> >    exepoint_checkpoints_map;

extern ptr_cond_direct_instructions_t               examined_input_dep_cfis;

extern ptr_cond_direct_instruction_t                exploring_cfi;
extern UINT32                                       exploring_cfi_exec_order;
extern UINT32                                       last_input_dep_cfi_exec_order;

extern std::vector<ADDRINT>                         explored_trace;
extern UINT32                                       current_exec_order;

extern UINT32                                       received_msg_num;
extern ADDRINT                                      received_msg_addr;
extern UINT32                                       received_msg_size;

#if BOOST_OS_LINUX
extern ADDRINT                                      logged_syscall_index;   // logged syscall index
extern ADDRINT                                      logged_syscall_args[6]; // logged syscall arguments
#endif

extern running_state                                current_running_state;

extern UINT64                                       executed_ins_number;
extern UINT64                                       econed_ins_number;

extern KNOB<UINT32>                                 max_total_rollback;
extern KNOB<UINT32>                                 max_local_rollback;
extern KNOB<UINT32>                                 max_trace_length;
extern KNOB<BOOL>                                   print_debug_text;

namespace btime = boost::posix_time;
extern boost::shared_ptr<btime::ptime>              start_ptr_time;
extern boost::shared_ptr<btime::ptime>              stop_ptr_time;

namespace logging = boost::log;
namespace sinks   = boost::log::sinks;
namespace sources = boost::log::sources;
typedef sinks::text_file_backend text_backend;
typedef sinks::synchronous_sink<text_backend>       sink_file_backend;
typedef logging::trivial::severity_level            log_level;
extern sources::severity_logger<log_level>          log_instance;
extern boost::shared_ptr<sink_file_backend>         log_sink;

namespace bran = boost::random;
extern bran::taus88                                 rgen;
extern bran::uniform_int_distribution<UINT8>        uint8_uniform;

#endif // COMMON_H
