#include <pin.H>

#include <iostream>
#include <fstream>
#include <map>
#include <set>

#include <boost/predef.h>
#include <boost/format.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>

#include "stuffs.h"
#include "instruction.h"
#include "checkpoint.h"
#include "branch.h"

/*================================================================================================*/

extern ADDRINT logged_syscall_index;
extern ADDRINT logged_syscall_args[6];

extern bool                                     in_tainting;

extern std::map<ADDRINT, ptr_instruction_t>     ins_at_addr;
extern std::map<UINT32, ptr_instruction_t>      ins_at_order;

extern UINT32                                   total_rollback_times;

extern df_diagram                               dta_graph;

extern std::vector<ADDRINT>                     explored_trace;
extern UINT32                                   current_execution_order;

extern std::map<UINT32, ptr_branch_t>             order_input_dep_ptr_branch_map;
extern std::map<UINT32, ptr_branch_t>             order_input_indep_ptr_branch_map;
extern std::map<UINT32, ptr_branch_t>             order_tainted_ptr_branch_map;

extern ptr_branch_t                               exploring_ptr_branch;

extern std::vector<ptr_branch_t>                  total_input_dep_ptr_branches;

extern std::vector<ptr_checkpoint_t>              saved_ptr_checkpoints;
extern ptr_checkpoint_t                           master_ptr_checkpoint;

extern std::map< UINT32,
                 std::vector<ptr_checkpoint_t> >  exepoint_checkpoints_map;

extern UINT32																		received_msg_num;
extern ADDRINT                                  received_msg_addr;
extern UINT32                                   received_msg_size;
extern ADDRINT																	received_msg_struct_addr;

extern KNOB<BOOL>                               print_debug_text;
extern KNOB<UINT32>                             max_trace_length;

extern UINT32                                   max_trace_size;

namespace logging = boost::log;
namespace sinks   = boost::log::sinks;
namespace sources = boost::log::sources;
typedef sinks::text_file_backend text_backend;
typedef sinks::synchronous_sink<text_backend>   sink_file_backend;
typedef logging::trivial::severity_level        log_level;

extern sources::severity_logger<log_level>      log_instance;
extern boost::shared_ptr<sink_file_backend>     log_sink;

/*================================================================================================*/

static std::map<df_vertex_desc, df_vertex_desc> prec_vertex_desc;

/*================================================================================================*/

class dep_bfs_visitor : public boost::default_bfs_visitor
{
public:
  template <typename Edge, typename Graph>
  void tree_edge(Edge e, const Graph& g)
  {
    prec_vertex_desc[boost::target(e, g)] = boost::source(e, g);
  }
};

/*================================================================================================*/

inline void mark_resolved(ptr_branch_t& omitted_ptr_branch)
{
  omitted_ptr_branch->is_resolved = true;
  omitted_ptr_branch->is_bypassed = false;
  omitted_ptr_branch->is_just_resolved = false;

  return;
}

/*================================================================================================*/

std::vector<UINT32> backward_trace(df_vertex_desc root_vertex, df_vertex_desc last_vertex)
{
  std::vector<UINT32> backward_trace;

  df_vertex_desc current_vertex = last_vertex;
  df_vertex_desc backward_vertex;

  df_edge_desc current_edge;
  bool edge_exist;

  while (current_vertex != root_vertex)
  {
    backward_vertex = prec_vertex_desc[current_vertex];

    boost::tie(current_edge, edge_exist) = boost::edge(backward_vertex, 
                                                       current_vertex, dta_graph);
    if (edge_exist)
    {
//      backward_trace.push_back(dta_graph[current_edge].second);
      backward_trace.push_back(dta_graph[current_edge]);
    }
    else
    {
      //BOOST_LOG_TRIVIAL(fatal) << "Edge not found in backward trace construction.";
      BOOST_LOG_SEV(log_instance, boost::log::trivial::fatal) 
        << "edge not found in backward trace construction";
      PIN_ExitApplication(0);
    }

    current_vertex = backward_vertex;
  }

  return backward_trace;
}

/*================================================================================================*/

inline void compute_branch_mem_dependency()
{
  df_vertex_iter vertex_iter;
  df_vertex_iter last_vertex_iter;

  df_edge_desc edge_desc;
  bool edge_exist;

  dep_bfs_visitor dep_vis;

  std::map<UINT32, ptr_branch_t>::iterator order_ptr_branch_iter;
  ptr_branch_t current_ptr_branch;

  std::map<df_vertex_desc, df_vertex_desc>::iterator prec_vertex_iter;

  ADDRINT current_addr;

  boost::tie(vertex_iter, last_vertex_iter) = boost::vertices(dta_graph);
  for (; vertex_iter != last_vertex_iter; ++vertex_iter)
  {
//    if (dta_graph[*vertex_iter].type == MEM_VAR)
    if (dta_graph[*vertex_iter]->value.type() == typeid(ADDRINT))
    {
      prec_vertex_desc.clear();

      boost::breadth_first_search(dta_graph, *vertex_iter, boost::visitor(dep_vis));

      for (prec_vertex_iter = prec_vertex_desc.begin();
              prec_vertex_iter != prec_vertex_desc.end(); ++prec_vertex_iter)
      {
        boost::tie(edge_desc, edge_exist) = boost::edge(prec_vertex_iter->second, 
                                                        prec_vertex_iter->first, 
                                                        dta_graph);
        if (edge_exist)
        {
          order_ptr_branch_iter = order_tainted_ptr_branch_map.begin();
          for (; order_ptr_branch_iter != order_tainted_ptr_branch_map.end(); 
               ++order_ptr_branch_iter) 
          {
            current_ptr_branch = order_ptr_branch_iter->second;
//            if (dta_graph[edge_desc].second == current_ptr_branch->trace.size())
            if (dta_graph[edge_desc] == current_ptr_branch->execution_order)
            {
//              current_addr = dta_graph[*vertex_iter].mem;
              current_addr = boost::get<ADDRINT>(dta_graph[*vertex_iter]->value);
              
              if ((received_msg_addr <= current_addr) && 
                  (current_addr < received_msg_addr + received_msg_size)) 
              {
                current_ptr_branch->dep_input_addrs.insert(current_addr);
              }
              else 
              {
                current_ptr_branch->dep_other_addrs.insert(current_addr);
              }
              
              current_ptr_branch->dep_backward_traces[current_addr] 
                = backward_trace(*vertex_iter, prec_vertex_iter->second);
            }
          }
        }
        else
        {
          //BOOST_LOG_TRIVIAL(fatal) << "Backward edge not found in BFS.";
          BOOST_LOG_SEV(log_instance, boost::log::trivial::fatal)
              << "backward edge not found in BFS";
          PIN_ExitApplication(0);
        }
      }
    }
  }

  order_ptr_branch_iter = order_tainted_ptr_branch_map.begin();
  for (; order_ptr_branch_iter != order_tainted_ptr_branch_map.end(); 
       ++order_ptr_branch_iter) 
  {
    current_ptr_branch = order_ptr_branch_iter->second;
    if (!current_ptr_branch->dep_input_addrs.empty()) 
    {
      order_input_dep_ptr_branch_map[current_ptr_branch->execution_order]
        = current_ptr_branch;
      
      if (exploring_ptr_branch) 
      {
        if (current_ptr_branch->execution_order > exploring_ptr_branch->execution_order)
        {
          total_input_dep_ptr_branches.push_back(current_ptr_branch);
        }
      }
      else 
      {
        total_input_dep_ptr_branches.push_back(current_ptr_branch);
      }
    }
    else 
    {
      order_input_indep_ptr_branch_map[current_ptr_branch->execution_order]
        = current_ptr_branch;
    }
  }

  return;
}

/*================================================================================================*/

inline static void compute_branch_min_checkpoint()
{
  std::vector<ptr_checkpoint_t>::iterator   ptr_checkpoint_iter;
  std::vector<ptr_checkpoint_t>::reverse_iterator ptr_checkpoint_reverse_iter;
  std::set<ADDRINT>::iterator             addr_iter;
  std::map<UINT32, ptr_branch_t>::iterator  order_ptr_branch_iter;

  ptr_branch_t      current_ptr_branch;
  ptr_checkpoint_t  nearest_ptr_checkpoint;
  
  bool nearest_checkpoint_found;
  std::set<ADDRINT> intersec_mems;

  order_ptr_branch_iter = order_tainted_ptr_branch_map.begin();
  for (; order_ptr_branch_iter != order_tainted_ptr_branch_map.end(); ++order_ptr_branch_iter) 
  {
    current_ptr_branch = order_ptr_branch_iter->second;
    if (current_ptr_branch->dep_input_addrs.empty()) 
    {
      current_ptr_branch->checkpoint.reset();
    }
    else // compute the nearest checkpoint for current_ptr_branch
    {
      // for each *addr_iter in current_ptr_branch->dep_input_addrs, 
      // find the earliest checkpoint that uses it
      addr_iter = current_ptr_branch->dep_input_addrs.begin();
      for (; addr_iter != current_ptr_branch->dep_input_addrs.end(); ++addr_iter) 
      {
        ptr_checkpoint_iter = saved_ptr_checkpoints.begin();
        for (; ptr_checkpoint_iter != saved_ptr_checkpoints.end(); ++ptr_checkpoint_iter) 
        {
          nearest_checkpoint_found = false;
          // *addr_iter is found in (*ptr_checkpoint_iter)->dep_mems
          if (std::find((*ptr_checkpoint_iter)->dep_mems.begin(), 
                        (*ptr_checkpoint_iter)->dep_mems.end(), *addr_iter) 
              != (*ptr_checkpoint_iter)->dep_mems.end()) 
          {
            nearest_checkpoint_found = true;
            current_ptr_branch->nearest_checkpoints[*ptr_checkpoint_iter].insert(*addr_iter);
            break;
          }
        }
        
        // find the ideal checkpoint by finding reversely the checkpoint list 
        if (nearest_checkpoint_found) 
        {
          ptr_checkpoint_reverse_iter = saved_ptr_checkpoints.rbegin();
          for (; ptr_checkpoint_reverse_iter != saved_ptr_checkpoints.rend(); 
               ++ptr_checkpoint_reverse_iter) 
          {
            if ((*ptr_checkpoint_reverse_iter)->execution_order < current_ptr_branch->execution_order)
            {
              if (std::find((*ptr_checkpoint_reverse_iter)->dep_mems.begin(), 
                            (*ptr_checkpoint_reverse_iter)->dep_mems.end(), *addr_iter) 
                  != (*ptr_checkpoint_reverse_iter)->dep_mems.end()) 
              {
                current_ptr_branch->econ_execution_length[*ptr_checkpoint_iter] = 
                (*ptr_checkpoint_reverse_iter)->execution_order - (*ptr_checkpoint_iter)->execution_order;
//                 std::cout << current_ptr_branch->econ_execution_length[*ptr_checkpoint_iter] << std::endl;
                break;
              }
            }
          }
          
        }
      }
      
      if (current_ptr_branch->nearest_checkpoints.size() != 0) 
      {
        //BOOST_LOG_TRIVIAL(info) 
        BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
          << boost::format("the branch at %d:%d (%s: %s) has %d nearest checkpoints.") 
              % current_ptr_branch->execution_order
              % current_ptr_branch->br_taken
              % remove_leading_zeros(StringFromAddrint(current_ptr_branch->addr))
              % ins_at_order[current_ptr_branch->execution_order]->disassembled_name
              % current_ptr_branch->nearest_checkpoints.size();
              
        current_ptr_branch->checkpoint = current_ptr_branch->nearest_checkpoints.rbegin()->first;
      }
      else 
      {
        //BOOST_LOG_TRIVIAL(fatal)
        BOOST_LOG_SEV(log_instance, boost::log::trivial::fatal)
          << boost::format("cannot found any nearest checkpoint for the branch at %d.!") 
              % current_ptr_branch->execution_order;
              
        PIN_ExitApplication(0);
      }
    }
  }
   
  return;
}

/*================================================================================================*/

inline void prepare_new_rollbacking_phase()
{
  //BOOST_LOG_TRIVIAL(info) 
  BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
    << boost::format("stop exploring, %d instructions analyzed; start detecting checkpoints")
        % current_execution_order;
  
//   journal_tainting_graph("tainting_graph.dot");
//   PIN_ExitApplication(0);
  
  compute_branch_mem_dependency();
  compute_branch_min_checkpoint();
    
  //BOOST_LOG_TRIVIAL(info) 
  BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
    << boost::format("stop detecting, %d checkpoints and %d/%d branches detected; start rollbacking")
        % saved_ptr_checkpoints.size() 
        % order_input_dep_ptr_branch_map.size() 
        % order_tainted_ptr_branch_map.size();

//   journal_tainting_log();
    
  in_tainting = false;
  PIN_RemoveInstrumentation();
  
  if (exploring_ptr_branch)
  {
    journal_explored_trace("last_explored_trace.log");

    rollback_and_restore(saved_ptr_checkpoints[0],
                         exploring_ptr_branch->inputs[!exploring_ptr_branch->br_taken][0].get());
  }
  else
  {
    journal_tainting_graph("tainting_graph.dot");
    journal_explored_trace("first_explored_trace.log");
//     journal_static_trace("static_trace");
    
    // the first rollbacking phase
    if (!order_input_dep_ptr_branch_map.empty())
    { 
      ptr_branch_t first_ptr_branch = order_input_dep_ptr_branch_map.begin()->second;
      rollback_and_restore(saved_ptr_checkpoints[0],
                           first_ptr_branch->inputs[first_ptr_branch->br_taken][0].get());
    }
    else
    {
      //BOOST_LOG_TRIVIAL(info) << "There is no branch needed to resolve.";
      BOOST_LOG_SEV(log_instance, boost::log::trivial::info) 
        << "there is no branch needed to resolve";
      PIN_ExitApplication(0);
    }
  }

  return;
}

/*================================================================================================*/

VOID logging_syscall_instruction_analyzer(ADDRINT ins_addr)
{
  prepare_new_rollbacking_phase();
  return;
}

/*================================================================================================*/

VOID logging_general_instruction_analyzer(ADDRINT ins_addr)
{
  if ((current_execution_order < max_trace_size) &&
      !ins_at_addr[ins_addr]->is_mapped_from_kernel)
  {
//    explored_trace.push_back(ins_addr);
//    order_ins_dynamic_map[explored_trace.size()] = addr_ins_static_map[ins_addr];

    current_execution_order++;
    ins_at_order[current_execution_order] = ins_at_addr[ins_addr];

    BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
        << boost::format("%-3d %-15s %-50s %-25s %-25s")
           % current_execution_order
           % addrint_to_hexstring(ins_addr)
           % ins_at_addr[ins_addr]->disassembled_name
           % ins_at_addr[ins_addr]->contained_image
           % ins_at_addr[ins_addr]->contained_function;
  }
  else // trace length limit reached
  {
    prepare_new_rollbacking_phase();
  }

  return;
}

/*================================================================================================*/
// memmory read
VOID logging_mem_read_instruction_analyzer(ADDRINT ins_addr, 
                                           ADDRINT mem_read_addr, UINT32 mem_read_size, 
                                           CONTEXT* p_ctxt)
{
  // a new checkpoint found
  if (std::max(mem_read_addr, received_msg_addr) <
      std::min(mem_read_addr + mem_read_size, received_msg_addr + received_msg_size))
  {
    ptr_checkpoint_t new_ptr_checkpoint(new checkpoint(ins_addr, p_ctxt,
                                                       mem_read_addr, mem_read_size));
    saved_ptr_checkpoints.push_back(new_ptr_checkpoint);
    
    //BOOST_LOG_TRIVIAL(trace) 
    BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
      << boost::format("checkpoint detected at %d (%s: %s) because memory is read (%s: %d)")
         % new_ptr_checkpoint->execution_order
         % remove_leading_zeros(StringFromAddrint(ins_addr))
         % ins_at_addr[ins_addr]->disassembled_name
         % remove_leading_zeros(StringFromAddrint(mem_read_addr)) % mem_read_size;
  }

  ptr_operand_t mem_operand;
  for (UINT32 idx = 0; idx < mem_read_size; ++idx)
  {
    mem_operand.reset(new operand(mem_read_addr + idx));
    ins_at_order[current_execution_order]->src_operands.insert(mem_operand);
  }

  return;
}

/*================================================================================================*/
// memmory read 2
// VOID logging_mem_read2_instruction_analyzer(ADDRINT ins_addr, 
//                                             ADDRINT mem_read_addr, UINT32 mem_read_size, 
//                                             CONTEXT* p_ctxt)
// {
//   logging_mem_read_instruction_analyzer(ins_addr, mem_read_addr, mem_read_size, p_ctxt);
//   return;
// }

/*================================================================================================*/
// memory written
VOID logging_mem_write_instruction_analyzer(ADDRINT ins_addr, 
                                            ADDRINT mem_written_addr, UINT32 mem_written_size)
{
  if (!saved_ptr_checkpoints.empty()) 
  {
    saved_ptr_checkpoints[0]->mem_written_logging(ins_addr, 
                                                  mem_written_addr, mem_written_size);
  }

  exepoint_checkpoints_map[current_execution_order] = saved_ptr_checkpoints;

  ptr_operand_t mem_operand;
  for (UINT32 idx = 0; idx < mem_written_size; ++idx)
  {
    mem_operand.reset(new operand(mem_written_addr + idx));
    ins_at_order[current_execution_order]->dst_operands.insert(mem_operand);
  }

  return;
}

/*================================================================================================*/

VOID logging_cond_br_analyzer(ADDRINT ins_addr, bool br_taken)
{
  ptr_branch_t new_ptr_branch(new branch(ins_addr, br_taken));

  // save the first input
  store_input(new_ptr_branch, br_taken);

  // verify if the branch is a new tainted branch
  if (exploring_ptr_branch && 
      (new_ptr_branch->execution_order <= exploring_ptr_branch->execution_order))
  {
    // mark it as resolved
    mark_resolved(new_ptr_branch); 
  }
  
  order_tainted_ptr_branch_map[current_execution_order] = new_ptr_branch;
  /*std::cout << "first value of the stored buffer at branch "
    << new_ptr_branch->trace.size() << new_ptr_branch->inputs[br_taken][0].get()[0] << "\n";*/

  return;
}

/*================================================================================================*/
#if BOOST_OS_WINDOWS
static bool function_has_been_called = false;
VOID logging_before_recv_functions_analyzer(ADDRINT msg_addr)
{
  received_msg_addr = msg_addr;
  function_has_been_called = true;

  BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
    << "recv or recvfrom called, received msg buffer: "
    << remove_leading_zeros(StringFromAddrint(received_msg_addr));
  log_sink->flush();
  return;
}

VOID logging_after_recv_functions_analyzer(UINT32 msg_length)
{
  if (function_has_been_called)
  {
    if (msg_length > 0)
    {
      received_msg_num++; received_msg_size = msg_length;

      BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
        << "recv or recvfrom returned, received msg size: " << received_msg_size;
      log_sink->flush();
    }

    function_has_been_called = false;
  }
  return;
}

namespace WINDOWS
{
#include <WinSock2.h>
#include <Windows.h>
};
VOID logging_before_wsarecv_functions_analyzer(ADDRINT msg_struct_adddr)
{
  received_msg_struct_addr = msg_struct_adddr;
  received_msg_addr = reinterpret_cast<ADDRINT>(
        (reinterpret_cast<WINDOWS::LPWSABUF>(received_msg_struct_addr))->buf);
  function_has_been_called = true;

  BOOST_LOG_SEV(log_instance, boost::log::trivial::info) 
    << "WSARecv or WSARecvFrom called, received msg buffer: " 
    << remove_leading_zeros(StringFromAddrint(received_msg_addr));
  log_sink->flush();

  return;
}

VOID logging_after_wsarecv_funtions_analyzer()
{
  if (function_has_been_called)
  {
    received_msg_size = (reinterpret_cast<WINDOWS::LPWSABUF>(received_msg_struct_addr))->len;
    if (received_msg_size > 0)
    {
      ++received_msg_num;

      BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
        << "WSARecv or WSARecvFrom returned, received msg size: " << received_msg_size;

      BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
        << boost::format("the first message saved at %s with size %d bytes")
            % remove_leading_zeros(StringFromAddrint(received_msg_addr)) % received_msg_size
        << "\n-------------------------------------------------------------------------------------------------\n"
        << boost::format("start tainting the first time with trace size %d") 
            % max_trace_size;
      //log_sink->flush();

      PIN_RemoveInstrumentation();
    }
    function_has_been_called = false;
  }
  
  return;
}
#elif BOOST_OS_LINUX
VOID syscall_entry_analyzer(THREADID thread_id,
                            CONTEXT* p_ctxt, SYSCALL_STANDARD syscall_std, VOID *data)
{
  if (received_msg_num == 0)
  {
    logged_syscall_index = PIN_GetSyscallNumber(p_ctxt, syscall_std);
    if (logged_syscall_index == syscall_recvfrom)
    {
      for (UINT8 arg_id = 0; arg_id < 6; ++arg_id)
      {
        logged_syscall_args[arg_id] = PIN_GetSyscallArgument(p_ctxt, syscall_std, arg_id);
      }
    }
  }

  return;
}

/*================================================================================================*/

VOID syscall_exit_analyzer(THREADID thread_id,
                           CONTEXT* p_ctxt, SYSCALL_STANDARD syscall_std, VOID *data)
{
  if (received_msg_num == 0)
  {
    if (logged_syscall_index == syscall_recvfrom)
    {
      ADDRINT ret_val = PIN_GetSyscallReturn(p_ctxt, syscall_std);
      if (ret_val > 0)
      {
        received_msg_num++;
        received_msg_addr = logged_syscall_args[1];
        received_msg_size = ret_val;

        BOOST_LOG_TRIVIAL(info)
          << boost::format("the first message saved at %s with size %d bytes\n%s\n%s %d")
             % addrint_to_hexstring(received_msg_addr) % received_msg_size
             % "-----------------------------------------------------------------------------------"
             % "start tainting the first time with trace size" % max_trace_size;

        PIN_RemoveInstrumentation();
      }
    }
  }

  return;
}
#endif
