#include <pin.H>

#include <algorithm>
#include <iterator>

#include <boost/predef.h>
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

#include "../base/branch.h"
#include "../base/operand.h"
#include "../base/instruction.h"
#include "../base/cond_direct_instruction.h"
#include "../util/stuffs.h"

/*================================================================================================*/

#if BOOST_OS_LINUX
extern ADDRINT                                  logged_syscall_index;
extern ADDRINT                                  logged_syscall_args[6];
#endif

extern bool                                     in_tainting;

extern df_diagram                               dta_graph;
extern df_vertex_desc_set                       dta_outer_vertices;

extern std::map<ADDRINT, ptr_instruction_t>     ins_at_addr;
extern std::map<UINT32, ptr_instruction_t>      ins_at_order;

extern UINT32                                   current_exec_order;

extern std::map<UINT32, ptr_branch_t>           order_input_dep_ptr_branch_map;
extern std::map<UINT32, ptr_branch_t>           order_input_indep_ptr_branch_map;
extern std::map<UINT32, ptr_branch_t>           order_tainted_ptr_branch_map;
extern std::vector<ptr_branch_t>                total_input_dep_ptr_branches;
extern ptr_cond_direct_instructions_t           examined_input_dep_cfis;

typedef std::vector<ptr_checkpoint_t>           ptr_checkpoints_t;
extern std::map<UINT32, ptr_checkpoints_t>      exepoint_checkpoints_map;

extern std::vector<ptr_checkpoint_t>            saved_checkpoints;
extern ptr_checkpoint_t                         master_ptr_checkpoint;

extern ptr_cond_direct_instruction_t            exploring_cfi;
extern ptr_branch_t                             exploring_ptr_branch;

extern UINT32                                   received_msg_num;
extern ADDRINT                                  received_msg_addr;
extern UINT32                                   received_msg_size;

extern UINT32                                   max_trace_size;

namespace logging = boost::log;
namespace sinks   = boost::log::sinks;
namespace sources = boost::log::sources;
typedef sinks::text_file_backend text_backend;
typedef sinks::synchronous_sink<text_backend>   sink_file_backend;
typedef logging::trivial::severity_level        log_level;
extern sources::severity_logger<log_level>      log_instance;
extern boost::shared_ptr<sink_file_backend>     log_sink;

namespace tainting
{

/*================================================================================================*/

static std::map<df_vertex_desc, df_vertex_desc> prec_vertex_desc;
static std::set<df_edge_desc> visited_edges;

/*================================================================================================*/

/**
 * @brief The df_bfs_visitor class discovering all dependent edges from a vertex.
 */
class df_bfs_visitor : public boost::default_bfs_visitor
{
public:
  template <typename Edge, typename Graph>
  void tree_edge(Edge e, const Graph& g)
  {
    prec_vertex_desc[boost::target(e, g)] = boost::source(e, g);
    visited_edges.insert(e);
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

inline static std::vector<UINT32> backward_trace(df_vertex_desc root_vertex,
                                                 df_vertex_desc last_vertex)
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
      BOOST_LOG_SEV(log_instance, boost::log::trivial::fatal)
        << "edge not found in backward trace construction";
      PIN_ExitApplication(1);
    }

    current_vertex = backward_vertex;
  }

  return backward_trace;
}


/**
 * @brief for each executed instruction in this tainting phase, determine the set of input memory
 * addresses that affect to the instruction.
 */
inline static void determine_cfi_input_dependency()
{
  df_vertex_iter                    vertex_iter;
  df_vertex_iter                    last_vertex_iter;
  df_bfs_visitor                    df_visitor;
  std::set<df_edge_desc>::iterator  visited_edge_iter;

  ADDRINT mem_addr;

  UINT32 visited_edge_exec_order;
  ptr_cond_direct_instruction_t visited_cfi;

  // get the set of vertices in the tainting graph
  boost::tie(vertex_iter, last_vertex_iter) = boost::vertices(dta_graph);
  // for each vertice of the tainting graph
  for (; vertex_iter != last_vertex_iter; ++vertex_iter)
  {
    // if it represents some memory address
    if (dta_graph[*vertex_iter]->value.type() == typeid(ADDRINT))
    {
      // and this memory address belongs to the input
      mem_addr = boost::get<ADDRINT>(dta_graph[*vertex_iter]->value);
      if ((received_msg_addr <= mem_addr) && (mem_addr < received_msg_addr + received_msg_size))
      {
        // take a BFS from this vertice
        prec_vertex_desc.clear(); visited_edges.clear();
        boost::breadth_first_search(dta_graph, *vertex_iter, boost::visitor(df_visitor));

        // for each visited edge
        for (visited_edge_iter = visited_edges.begin();
             visited_edge_iter != visited_edges.end(); ++visited_edge_iter)
        {
          // the value of the edge is the execution order of the corresponding instruction
          visited_edge_exec_order = dta_graph[*visited_edge_iter];
          // consider the instruction which is a CFI
          if (ins_at_order[visited_edge_exec_order]->is_cond_direct_cf)
          {
            // then the instruction depends on the value of the memory address
            visited_cfi = boost::static_pointer_cast<cond_direct_instruction>(
                  ins_at_order[visited_edge_exec_order]);
            if (!exploring_cfi ||
                (exploring_cfi && (visited_cfi->exec_order > exploring_cfi->exec_order)))
            {
              visited_cfi->input_dep_addrs.insert(mem_addr);
            }
          }
        }
      }
    }
  }

  return;
}


/**
 * @brief determine_checkpoints_for_cfi
 */
inline static void determine_checkpoints_for_cfi(ptr_cond_direct_instruction_t cfi)
{
  addrint_set dep_addrs = cfi->input_dep_addrs;
  addrint_set new_dep_addrs;
  addrint_set intersected_addrs;
  checkpoint_with_modified_addrs checkpoint_with_input_addrs;

      ptr_checkpoints_t::iterator chkpnt_iter;
  for (chkpnt_iter = saved_checkpoints.begin();
       chkpnt_iter != saved_checkpoints.end(); ++chkpnt_iter)
  {
    // find the intersection between the input addresses of the checkpoint and the affecting input
    // addresses of the CFI
    intersected_addrs.clear();
    std::set_intersection((*chkpnt_iter)->input_dep_addrs.begin(),
                          (*chkpnt_iter)->input_dep_addrs.end(),
                          dep_addrs.begin(), dep_addrs.end(),
                          std::inserter(intersected_addrs, intersected_addrs.begin()));
    // verify if the intersection is not empty
    if (!intersected_addrs.empty())
    {
      // not empty, then the checkpoint and the intersected addrs make a pair, namely when we need
      // to change the decision of the CFI then we should rollback to the checkpoint and modify some
      // value at the address of the intersected addrs
      checkpoint_with_input_addrs = std::make_pair(*chkpnt_iter, intersected_addrs);
      cfi->checkpoints.push_back(checkpoint_with_input_addrs);

      // the addrs in the intersected set are subtracted from the original dep_addrs
      new_dep_addrs.clear();
      std::set_difference(dep_addrs.begin(), dep_addrs.end(),
                          intersected_addrs.begin(), intersected_addrs.end(),
                          std::inserter(new_dep_addrs, new_dep_addrs.begin()));
      // if the rest is empty then we have finished
      if (new_dep_addrs.empty()) break;
      else dep_addrs = new_dep_addrs;
    }
  }
  return;
}

/**
 * @brief save new tainted CFI in this tainting phase
 */
inline static void save_tainted_cfis()
{
  ptr_cond_direct_instruction_t tainted_cfi;

  std::map<UINT32, ptr_instruction_t>::iterator tainted_ins_iter;
  // iterate over executed instructions in this tainting phase
  for (tainted_ins_iter = ins_at_order.begin();
       tainted_ins_iter != ins_at_order.end(); ++tainted_ins_iter)
  {
    // consider only the instruction that is not behind the exploring CFI
    if (!exploring_cfi ||
        (exploring_cfi && (tainted_ins_iter->first > exploring_cfi->exec_order)))
    {
      if (tainted_ins_iter->second->is_cond_direct_cf)
      {
        tainted_cfi = boost::static_pointer_cast<cond_direct_instruction>(tainted_ins_iter->second);
        // and depends on the input
        if (!tainted_cfi->input_dep_addrs.empty())
        {
          // then save it
          examined_input_dep_cfis.push_back(tainted_cfi);
        }
      }
    }
  }
  return;
}

/*================================================================================================*/



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
        ptr_checkpoint_iter = saved_checkpoints.begin();
        for (; ptr_checkpoint_iter != saved_checkpoints.end(); ++ptr_checkpoint_iter)
        {
          nearest_checkpoint_found = false;
          // *addr_iter is found in (*ptr_checkpoint_iter)->dep_mems
          if (std::find((*ptr_checkpoint_iter)->input_dep_addrs.begin(),
                        (*ptr_checkpoint_iter)->input_dep_addrs.end(), *addr_iter)
              != (*ptr_checkpoint_iter)->input_dep_addrs.end())
          {
            nearest_checkpoint_found = true;
            current_ptr_branch->nearest_checkpoints[*ptr_checkpoint_iter].insert(*addr_iter);
            break;
          }
        }

        // find the ideal checkpoint by finding reversely the checkpoint list
        if (nearest_checkpoint_found)
        {
          ptr_checkpoint_reverse_iter = saved_checkpoints.rbegin();
          for (; ptr_checkpoint_reverse_iter != saved_checkpoints.rend();
               ++ptr_checkpoint_reverse_iter)
          {
            if ((*ptr_checkpoint_reverse_iter)->exec_order < current_ptr_branch->execution_order)
            {
              if (std::find((*ptr_checkpoint_reverse_iter)->input_dep_addrs.begin(),
                            (*ptr_checkpoint_reverse_iter)->input_dep_addrs.end(), *addr_iter)
                  != (*ptr_checkpoint_reverse_iter)->input_dep_addrs.end())
              {
                current_ptr_branch->econ_execution_length[*ptr_checkpoint_iter] =
                (*ptr_checkpoint_reverse_iter)->exec_order - (*ptr_checkpoint_iter)->exec_order;
//                 std::cout << current_ptr_branch->econ_execution_length[*ptr_checkpoint_iter] << std::endl;
                break;
              }
            }
          }

        }
      }

      if (current_ptr_branch->nearest_checkpoints.size() != 0)
      {
        BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
          << boost::format("the branch at %d:%d (%s: %s) has %d nearest checkpoints.")
              % current_ptr_branch->execution_order
              % current_ptr_branch->br_taken
              % addrint_to_hexstring(current_ptr_branch->addr)
              % ins_at_order[current_ptr_branch->execution_order]->disassembled_name
              % current_ptr_branch->nearest_checkpoints.size();

        current_ptr_branch->checkpoint = current_ptr_branch->nearest_checkpoints.rbegin()->first;
      }
      else
      {
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
  BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
    << boost::format("stop exploring, %d instructions analyzed; start detecting checkpoints")
        % current_exec_order;

//   journal_tainting_graph("tainting_graph.dot");
//   PIN_ExitApplication(0);

  determine_cfi_input_dependency();
  compute_branch_min_checkpoint();

  BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
    << boost::format("stop tainting, %d checkpoints and %d/%d branches detected; start rollbacking")
        % saved_checkpoints.size()
        % order_input_dep_ptr_branch_map.size()
        % order_tainted_ptr_branch_map.size();

//   journal_tainting_log();

  in_tainting = false;
  PIN_RemoveInstrumentation();

  if (exploring_ptr_branch)
  {
    journal_explored_trace("last_explored_trace.log");

    rollback_and_restore(saved_checkpoints[0],
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
      rollback_and_restore(saved_checkpoints[0],
                           first_ptr_branch->inputs[first_ptr_branch->br_taken][0].get());
    }
    else
    {
      BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
          << "there is no branch needed to resolve";
      PIN_ExitApplication(0);
    }
  }

  return;
}

/*================================================================================================*/

VOID syscall_instruction(ADDRINT ins_addr)
{
  prepare_new_rollbacking_phase();
  return;
}

/*================================================================================================*/

VOID general_instruction(ADDRINT ins_addr)
{
  ptr_cond_direct_instruction_t current_cfi, duplicated_cfi;

  if ((current_exec_order < max_trace_size) && !ins_at_addr[ins_addr]->is_mapped_from_kernel)
  {
    current_exec_order++;
    if (ins_at_addr[ins_addr]->is_cond_direct_cf)
    {
      // duplicate a CFI
      current_cfi = boost::static_pointer_cast<cond_direct_instruction>(ins_at_addr[ins_addr]);
      duplicated_cfi.reset(new cond_direct_instruction(*current_cfi));
      duplicated_cfi->exec_order = current_exec_order;
      ins_at_order[current_exec_order] = duplicated_cfi;
    }
    else
    {
      // duplicate an instruction
      ins_at_order[current_exec_order].reset(new instruction(*ins_at_addr[ins_addr]));
    }

    BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
        << boost::format("%-3d %-15s %-50s %-25s %-25s")
           % current_exec_order % addrint_to_hexstring(ins_addr)
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

VOID mem_read_instruction(ADDRINT ins_addr,
                                           ADDRINT mem_read_addr, UINT32 mem_read_size,
                                           CONTEXT* p_ctxt)
{
  // a new checkpoint found
  if (std::max(mem_read_addr, received_msg_addr) <
      std::min(mem_read_addr + mem_read_size, received_msg_addr + received_msg_size))
  {
    ptr_checkpoint_t new_ptr_checkpoint(new checkpoint(p_ctxt, mem_read_addr, mem_read_size));
    saved_checkpoints.push_back(new_ptr_checkpoint);

    BOOST_LOG_SEV(log_instance, boost::log::trivial::info)
      << boost::format("checkpoint detected at %d (%s: %s) because memory is read (%s: %d)")
         % new_ptr_checkpoint->exec_order
         % remove_leading_zeros(StringFromAddrint(ins_addr))
         % ins_at_addr[ins_addr]->disassembled_name
         % addrint_to_hexstring(mem_read_addr) % mem_read_size;
  }

  ptr_operand_t mem_operand;
  for (UINT32 idx = 0; idx < mem_read_size; ++idx)
  {
    mem_operand.reset(new operand(mem_read_addr + idx));
    ins_at_order[current_exec_order]->src_operands.insert(mem_operand);
  }

  return;
}

/*================================================================================================*/

VOID mem_write_instruction(ADDRINT ins_addr, ADDRINT mem_written_addr, UINT32 mem_written_size)
{
  if (!saved_checkpoints.empty())
  {
    saved_checkpoints[0]->mem_write_tracking(mem_written_addr, mem_written_size);
  }

  exepoint_checkpoints_map[current_exec_order] = saved_checkpoints;

  ptr_operand_t mem_operand;
  for (UINT32 idx = 0; idx < mem_written_size; ++idx)
  {
    mem_operand.reset(new operand(mem_written_addr + idx));
    ins_at_order[current_exec_order]->dst_operands.insert(mem_operand);
  }

  return;
}

/*================================================================================================*/

VOID cond_branch_instruction(ADDRINT ins_addr, bool br_taken)
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

  order_tainted_ptr_branch_map[current_exec_order] = new_ptr_branch;

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

/*================================================================================================*/

inline std::set<df_vertex_desc> source_variables(UINT32 idx)
{
  df_vertex_desc_set src_vertex_descs;
  df_vertex_desc_set::iterator outer_vertex_iter;
  df_vertex_desc new_vertex_desc;
  std::set<ptr_operand_t>::iterator src_operand_iter;
  for (src_operand_iter = ins_at_order[idx]->src_operands.begin();
       src_operand_iter != ins_at_order[idx]->src_operands.end(); ++src_operand_iter)
  {
    // verify if the current source operand is
    for (outer_vertex_iter = dta_outer_vertices.begin();
         outer_vertex_iter != dta_outer_vertices.end(); ++outer_vertex_iter)
    {
      // found in the outer interface
      if (((*src_operand_iter)->value.type() == dta_graph[*outer_vertex_iter]->value.type()) &&
          ((*src_operand_iter)->name == dta_graph[*outer_vertex_iter]->name))
      {
        src_vertex_descs.insert(*outer_vertex_iter);
        break;
      }
    }

    // not found
    if (outer_vertex_iter == dta_outer_vertices.end())
    {
      new_vertex_desc = boost::add_vertex(*src_operand_iter, dta_graph);
      dta_outer_vertices.insert(new_vertex_desc);
      src_vertex_descs.insert(new_vertex_desc);
    }
  }

  return src_vertex_descs;
}

/*================================================================================================*/

inline std::set<df_vertex_desc> destination_variables(UINT32 idx)
{
  std::set<df_vertex_desc> dst_vertex_descs;
  df_vertex_desc new_vertex_desc;
  df_vertex_desc_set::iterator outer_vertex_iter;
  df_vertex_desc_set::iterator next_vertex_iter;
  std::set<ptr_operand_t>::iterator dst_operand_iter;

  for (dst_operand_iter = ins_at_order[idx]->dst_operands.begin();
       dst_operand_iter != ins_at_order[idx]->dst_operands.end(); ++dst_operand_iter)
  {
    // verify if the current target operand is
    outer_vertex_iter = dta_outer_vertices.begin();
    for (next_vertex_iter = outer_vertex_iter;
         outer_vertex_iter != dta_outer_vertices.end(); outer_vertex_iter = next_vertex_iter)
    {
      ++next_vertex_iter;

      // found in the outer interface
      if (((*dst_operand_iter)->value.type() == dta_graph[*outer_vertex_iter]->value.type())
          && ((*dst_operand_iter)->name == dta_graph[*outer_vertex_iter]->name))
      {
        // then insert the current target operand into the graph
        new_vertex_desc = boost::add_vertex(*dst_operand_iter, dta_graph);

        // and modify the outer interface by replacing the old vertex with the new vertex
        dta_outer_vertices.erase(outer_vertex_iter);
        dta_outer_vertices.insert(new_vertex_desc);

        dst_vertex_descs.insert(new_vertex_desc);
        break;
      }
    }

    // not found
    if (outer_vertex_iter == dta_outer_vertices.end())
    {
      // then insert the current target operand into the graph
      new_vertex_desc = boost::add_vertex(*dst_operand_iter, dta_graph);

      // and modify the outer interface by insert the new vertex
      dta_outer_vertices.insert(new_vertex_desc);

      dst_vertex_descs.insert(new_vertex_desc);
    }
  }

  return dst_vertex_descs;
}

/*================================================================================================*/

VOID graphical_propagation(ADDRINT ins_addr)
{
  std::set<df_vertex_desc> src_vertex_descs = source_variables(current_exec_order);
  std::set<df_vertex_desc> dst_vertex_descs = destination_variables(current_exec_order);

  std::set<df_vertex_desc>::iterator src_vertex_desc_iter;
  std::set<df_vertex_desc>::iterator dst_vertex_desc_iter;

  // insert the edges between each pair (source, destination) into the tainting graph
  for (src_vertex_desc_iter = src_vertex_descs.begin();
       src_vertex_desc_iter != src_vertex_descs.end(); ++src_vertex_desc_iter) 
  {
    for (dst_vertex_desc_iter = dst_vertex_descs.begin();
         dst_vertex_desc_iter != dst_vertex_descs.end(); ++dst_vertex_desc_iter) 
    {
      boost::add_edge(*src_vertex_desc_iter, *dst_vertex_desc_iter, current_exec_order, dta_graph);
    }
  }

  return;
}

} // end of tainting namespace
