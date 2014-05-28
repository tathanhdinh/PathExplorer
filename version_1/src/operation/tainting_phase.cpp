#include <algorithm>

#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/lookup_edge.hpp>

#include "../common.h"
#include "rollbacking_phase.h"
#include "../util/stuffs.h"

namespace tainting
{

static std::vector<df_edge_desc>  visited_edges;
static df_diagram                 dta_graph;
static df_vertex_desc_set         dta_outer_vertices;
static UINT32                     rollbacking_trace_length;

#if !defined(NDEBUG)
static ptr_cond_direct_inss_t     newly_detected_input_dep_cfis;
static ptr_cond_direct_inss_t     newly_detected_cfis;
#endif


/**
 * @brief The df_bfs_visitor class discovering all dependent edges from a vertex.
 */
class df_bfs_visitor : public boost::default_bfs_visitor
{
public:
  template <typename Edge, typename Graph>
  void tree_edge(Edge e, const Graph& g)
  {
    visited_edges.push_back(e);
  }
};


/**
 * @brief for each executed instruction in this tainting phase, determine the set of input memory
 * addresses that affect to the instruction.
 */
static auto determine_cfi_input_dependency() -> void
{
//  df_vertex_iter vertex_iter, last_vertex_iter;
  df_vertex_iter last_vertex_iter, first_vertex_iter;
  df_bfs_visitor df_visitor;

  // for each vertice of the tainting graph
//  decltype(last_vertex_iter) first_vertex_iter;
  std::tie(first_vertex_iter, last_vertex_iter) = boost::vertices(dta_graph);
  std::for_each(first_vertex_iter, last_vertex_iter,
                [&df_visitor](decltype(*first_vertex_iter) vertex_desc)
  {
    // if it represents some memory address
    if (dta_graph[vertex_desc]->value.type() == typeid(ADDRINT))
    {
      // and this memory address belongs to the input
      auto mem_addr = boost::get<ADDRINT>(dta_graph[vertex_desc]->value);
      if ((received_msg_addr <= mem_addr) && (mem_addr < received_msg_addr + received_msg_size))
      {
        // take a BFS from this vertice
        visited_edges.clear();
        boost::breadth_first_search(dta_graph, vertex_desc, boost::visitor(df_visitor));

        // for each visited edge
        std::for_each(visited_edges.begin(), visited_edges.end(),
                      [&mem_addr](df_edge_desc visited_edge_desc)
        {
          // the value of the edge is the execution order of the corresponding instruction
          auto visited_edge_exec_order = dta_graph[visited_edge_desc];
          // consider only the instruction that is beyond the exploring CFI
          if (!exploring_cfi ||
              (exploring_cfi && (visited_edge_exec_order > exploring_cfi->exec_order)))
          {
            // and is some CFI
            if (ins_at_order[visited_edge_exec_order]->is_cond_direct_cf)
            {
              // then this CFI depends on the value of the memory address
              auto visited_cfi = std::static_pointer_cast<cond_direct_instruction>(
                    ins_at_order[visited_edge_exec_order]);
              visited_cfi->input_dep_addrs.insert(mem_addr);
//#if !defined(NDEBUG)
//              tfm::format(std::cerr, "the cfi at %d depends on the address %s\n",
//                          visited_cfi->exec_order, addrint_to_hexstring(mem_addr));
//#endif
            }
          }
        });
      }
    }
  });

  return;
}


/**
 * @brief for each CFI, determine pairs of <checkpoint, affecting input addresses> so that a
 * rollback from the checkpoint with the modification on the affecting input addresses may change
 * the CFI's decision.
 */
static auto set_checkpoints_for_cfi(/*const */ptr_cond_direct_ins_t/*&*/ cfi) -> void
{
  auto dep_addrs = cfi->input_dep_addrs;
  decltype(dep_addrs) input_dep_addrs, new_dep_addrs, intersected_addrs;
  checkpoint_addrs_pair_t checkpoint_with_input_addrs;

  std::all_of(std::begin(saved_checkpoints), std::end(saved_checkpoints),
              [&](decltype(saved_checkpoints)::value_type chkpnt) -> bool
  {
    // consider only checkpoints before the CFI
    if (chkpnt->exec_order <= cfi->exec_order)
    {
      // find the input addresses of the checkpoint
      input_dep_addrs.clear();
      std::for_each(std::begin(chkpnt->input_dep_original_values),
                    std::end(chkpnt->input_dep_original_values),
                    [&input_dep_addrs](decltype(chkpnt->input_dep_original_values)::const_reference addr_val)
      {
        input_dep_addrs.insert(std::get<0>(addr_val));
      });
//      auto addr_iter = chkpnt->input_dep_original_values.begin();
//      for (; addr_iter != chkpnt->input_dep_original_values.end(); ++addr_iter)
//      {
//        input_dep_addrs.insert(addr_iter->first);
//      }

      // find the intersection between the input addresses of the checkpoint and the affecting input
      // addresses of the CFI
      intersected_addrs.clear();
      std::set_intersection(input_dep_addrs.begin(), input_dep_addrs.end(),
                            dep_addrs.begin(), dep_addrs.end(),
                            std::inserter(intersected_addrs, intersected_addrs.begin()));
      // verify if the intersection is not empty
      if (!intersected_addrs.empty())
      {
        // not empty, then the checkpoint and the intersected addrs make a pair, namely when we need
        // to change the decision of the CFI then we should rollback to the checkpoint and modify some
        // value at the address of the intersected addrs
        checkpoint_with_input_addrs = std::make_pair(chkpnt, intersected_addrs);
        cfi->affecting_checkpoint_addrs_pairs.push_back(checkpoint_with_input_addrs);

        // the addrs in the intersected set are subtracted from the original dep_addrs
        new_dep_addrs.clear();
        std::set_difference(dep_addrs.begin(), dep_addrs.end(),
                            intersected_addrs.begin(), intersected_addrs.end(),
                            std::inserter(new_dep_addrs, new_dep_addrs.begin()));
        // if the rest is empty then we have finished
        if (new_dep_addrs.empty()) return false;
        // but if it is not empty then we continue to the next checkpoint
        else dep_addrs = new_dep_addrs;
      }
    }
    return true;
  });

//  for (auto chkpnt_iter = saved_checkpoints.begin(); chkpnt_iter != saved_checkpoints.end();
//       ++chkpnt_iter)
//  {
//    // consider only checkpoints before the CFI
//    if ((*chkpnt_iter)->exec_order <= cfi->exec_order)
//    {
//      // find the input addresses of the checkpoint
//      input_dep_addrs.clear();
//      auto addr_iter = (*chkpnt_iter)->input_dep_original_values.begin();
//      for (; addr_iter != (*chkpnt_iter)->input_dep_original_values.end(); ++addr_iter)
//      {
//        input_dep_addrs.insert(addr_iter->first);
//      }

//      // find the intersection between the input addresses of the checkpoint and the affecting input
//      // addresses of the CFI
//      intersected_addrs.clear();
//      std::set_intersection(input_dep_addrs.begin(), input_dep_addrs.end(),
//                            dep_addrs.begin(), dep_addrs.end(),
//                            std::inserter(intersected_addrs, intersected_addrs.begin()));
//      // verify if the intersection is not empty
//      if (!intersected_addrs.empty())
//      {
//        // not empty, then the checkpoint and the intersected addrs make a pair, namely when we need
//        // to change the decision of the CFI then we should rollback to the checkpoint and modify some
//        // value at the address of the intersected addrs
//        checkpoint_with_input_addrs = std::make_pair(*chkpnt_iter, intersected_addrs);
//        cfi->affecting_checkpoint_addrs_pairs.push_back(checkpoint_with_input_addrs);
////#if !defined(NDEBUG)
////        tfm::format(std::cerr, "the cfi at %d has a checkpoint at %d\n", cfi->exec_order,
////                    (*chkpnt_iter)->exec_order);
////#endif

//        // the addrs in the intersected set are subtracted from the original dep_addrs
//        new_dep_addrs.clear();
//        std::set_difference(dep_addrs.begin(), dep_addrs.end(),
//                            intersected_addrs.begin(), intersected_addrs.end(),
//                            std::inserter(new_dep_addrs, new_dep_addrs.begin()));
//        // if the rest is empty then we have finished
//        if (new_dep_addrs.empty()) break;
//        // but if it is not empty then we continue to the next checkpoint
//        else dep_addrs = new_dep_addrs;
//      }
//    }
//  }

  return;
}


/**
 * @brief save new tainted CFIs in this tainting phase
 */
static auto save_detected_cfis () -> void
{
  if (ins_at_order.size() > 1)
  {
    auto last_order_ins = *ins_at_order.rbegin();
    std::for_each(ins_at_order.begin(), ins_at_order.end(),
                  [&](decltype(ins_at_order)::const_reference order_ins)
    {
      // consider only the instruction that is not behind the exploring CFI
      if ((!exploring_cfi || (exploring_cfi && (std::get<0>(order_ins) > exploring_cfi->exec_order))) &&
          (std::get<0>(order_ins) < std::get<0>(last_order_ins)))
      {
        // verify if the instruction is a CFI
        if (std::get<1>(order_ins)->is_cond_direct_cf)
        {
          // then recast to get its type
          auto new_cfi = std::static_pointer_cast<cond_direct_instruction>(order_ins.second);
          // and if the recasted CFI depends on the input
          if (!new_cfi->input_dep_addrs.empty())
          {
            // then copy a fresh input for it
            new_cfi->fresh_input.reset(new UINT8[received_msg_size], std::default_delete<UINT8[]>());
            std::copy(fresh_input.get(),
                      fresh_input.get() + received_msg_size, new_cfi->fresh_input.get());

            // set its checkpoints and save it
            set_checkpoints_for_cfi(new_cfi); detected_input_dep_cfis.push_back(new_cfi);
#if !defined(NDEBUG)
            newly_detected_input_dep_cfis.push_back(new_cfi);
#endif
          }
#if !defined(NDEBUG)
          newly_detected_cfis.push_back(new_cfi);
#endif
        }
      }
    });
  }

  return;
}


/**
 * @brief update the explored instructions into the explorer graph
 */
static inline auto calculate_path_code () -> void
{
  if (ins_at_order.size() > 1)
  {
    // the root path code is one of the exploring CFI: if the current instruction is the
    // exploring CFI then "1" should be appended into the path code (because "0" has been
    // appended in the previous tainting phase)
    if (exploring_cfi) current_path_code.push_back(true);

//    typedef decltype(ins_at_order) ins_at_order_t;
    decltype(ins_at_order)::mapped_type prev_ins;
    std::for_each(ins_at_order.begin(), ins_at_order.end(),
                  [&](decltype(ins_at_order)::const_reference order_ins)
    {
      if (!exploring_cfi || (exploring_cfi && (/*order_ins.first*/std::get<0>(order_ins) > exploring_cfi->exec_order)))
      {
#if !defined(DISABLE_FSA)
        explored_fsa->add_vertex(/*order_ins.second*/std::get<1>(order_ins));
        if (prev_ins)
        {
          explored_fsa->add_edge(prev_ins->address, order_ins.second->address, current_path_code);
          explored_fsa->add_edge(prev_ins, order_ins.second, current_path_code);
        }
        else
        {
          if (exploring_cfi)
          {
            explored_fsa->add_edge(exploring_cfi->address, order_ins.second->address,
                                   current_path_code);
            explored_fsa->add_edge(std::dynamic_pointer_cast<instruction>(exploring_cfi),
                                   order_ins.second, current_path_code);
          }
        }
#endif

        // update the path code
        if (/*order_ins.second*/std::get<1>(order_ins)->is_cond_direct_cf)
        {
          auto current_cfi = std::static_pointer_cast<cond_direct_instruction>(order_ins.second);
          if (!current_cfi->input_dep_addrs.empty())
          {
            current_cfi->path_code = current_path_code; current_path_code.push_back(false);
          }
        }
        prev_ins = /*order_ins.second*/std::get<1>(order_ins);
      }
    });
  }

  return;
}


/**
 * @brief analyze_executed_instructions
 */
static inline auto analyze_executed_instructions () -> void
{
  if (!exploring_cfi)
  {
    save_tainting_graph(dta_graph, process_id_str + "_path_explorer_tainting_graph.dot");
  }
  determine_cfi_input_dependency(); save_detected_cfis();
  calculate_path_code();

//  current_exec_path = std::make_shared<execution_path>(ins_at_order, current_path_code);

  return;
}


/**
 * @brief calculate a new limit trace length for the next rollbacking phase, this limit is the
 * execution order of the last input dependent CFI in the tainting phase.
 */
static auto calculate_rollbacking_trace_length() -> void
{
  rollbacking_trace_length = 0;
//  auto ins_iter = ins_at_order.rbegin();
  ptr_cond_direct_ins_t last_cfi;

  // reverse iterate in the list of executed instructions
//  for (++ins_iter; ins_iter != ins_at_order.rend(); ++ins_iter)
//  {
//    // verify if the instruction is a CFI
//    if (ins_iter->second->is_cond_direct_cf)
//    {
//      // and this CFI depends on the input
//      last_cfi = std::static_pointer_cast<cond_direct_instruction>(ins_iter->second);
//      if (!last_cfi->input_dep_addrs.empty()) break;
//    }
//  }

  std::any_of(std::next(ins_at_order.rbegin()), ins_at_order.rend(),
              [&](decltype(ins_at_order)::const_reference order_ins) -> bool
  {
    // verify if the instruction is a CFI
    if (order_ins.second->is_cond_direct_cf)
    {
      // and this CFI depends on the input
      last_cfi = std::static_pointer_cast<cond_direct_instruction>(/*ins_ord.second*/std::get<1>(order_ins));
      if (last_cfi->input_dep_addrs.empty())
      {
        last_cfi.reset(); return false;
      }
      else return true;
    }
    else return false;
  });

  if (last_cfi && (!exploring_cfi || (last_cfi->exec_order > exploring_cfi->exec_order)))
    rollbacking_trace_length = std::min(last_cfi->exec_order + 1, max_trace_size);
  else rollbacking_trace_length = 0;

  return;
}


/**
 * @brief prepare_new_rollbacking_phase
 */
static auto prepare_new_rollbacking_phase() -> void
{
  if (saved_checkpoints.empty())
  {
#if !defined(NDEBUG)
    log_file << "no checkpoint saved, stop exploring\n";
#endif
    PIN_ExitApplication(0);
  }
  else
  {
#if !defined(NDEBUG)
    tfm::format(log_file, "stop tainting, %d instructions executed; start analyzing\n",
                current_exec_order);
//    log_file.flush();
#endif

    analyze_executed_instructions();

    // initalize the next rollbacking phase
    current_running_phase = rollbacking_phase; calculate_rollbacking_trace_length();
    rollbacking::initialize(rollbacking_trace_length);

#if !defined(NDEBUG)
    tfm::format(log_file, "stop analyzing, %d checkpoints, %d/%d branches detected; start rollbacking with limit trace %d\n",
                saved_checkpoints.size(), newly_detected_input_dep_cfis.size(),
                newly_detected_cfis.size(), rollbacking_trace_length);
//    log_file.flush();
#endif

    // and rollback to the first checkpoint (tainting->rollbacking transition)
     PIN_RemoveInstrumentation();
     rollback_with_current_input(saved_checkpoints[0], current_exec_order);
  }

  return;
}


/**
 * @brief analysis functions applied for syscall instructions
 */
auto kernel_mapped_instruction (ADDRINT ins_addr, THREADID thread_id) -> VOID
{
//  tfm::format(std::cerr, "kernel mapped instruction %d <%s: %s> %s %s\n", current_exec_order,
//              addrint_to_hexstring(ins_addr), ins_at_addr[ins_addr]->disassembled_name,
//              ins_at_addr[ins_addr]->contained_image, ins_at_addr[ins_addr]->contained_function);
  // the tainting phase always finishes when a kernel mapped instruction is met
  if (thread_id == traced_thread_id) prepare_new_rollbacking_phase();
  return;
}


/**
 * @brief analysis function applied for all instructions
 */
auto generic_instruction (ADDRINT ins_addr, const CONTEXT* p_ctxt, THREADID thread_id) -> VOID
{
  if (thread_id == traced_thread_id)
  {
//    tfm::format(std::cerr, "%d <%s: %s>\n", current_exec_order + 1, addrint_to_hexstring(ins_addr),
//                ins_at_addr[ins_addr]->disassembled_name);

    // verify if the execution order exceeds the limit trace length and the executed
    // instruction is always in user-space
    if ((current_exec_order < max_trace_size))
    {
      // does not exceed
      current_exec_order++;
      if (exploring_cfi && (exploring_cfi->exec_order == current_exec_order) &&
          (exploring_cfi->address != ins_addr))
      {
#if !defined(NDEBUG)
        tfm::format(log_file, "fatal: exploring the CFI <%s: %s> at %d but meet <%s: %s>\n",
                    addrint_to_hexstring(exploring_cfi->address), exploring_cfi->disassembled_name,
                    exploring_cfi->exec_order, addrint_to_hexstring(ins_addr),
                    ins_at_addr[ins_addr]->disassembled_name);
#endif
        PIN_ExitApplication(1);
      }
      else
      {
        if (ins_at_addr[ins_addr]->is_cond_direct_cf)
        {
          // duplicate a CFI (the default copy constructor is used)
          auto current_cfi =
              std::static_pointer_cast<cond_direct_instruction>(ins_at_addr[ins_addr]);
//          duplicated_cfi.reset(new cond_direct_instruction(*current_cfi));
          auto duplicated_cfi = std::make_shared<cond_direct_instruction>(*current_cfi);
          duplicated_cfi->exec_order = current_exec_order;
          ins_at_order[current_exec_order] = duplicated_cfi;
        }
        else
        {
          // duplicate an instruction (the default copy constructor is used)
//          ins_at_order[current_exec_order].reset(new instruction(*ins_at_addr[ins_addr]));
          ins_at_order[current_exec_order] = std::make_shared<instruction>(*ins_at_addr[ins_addr]);
        }

#if !defined(NDEBUG)
        tfm::format(log_file, "%-4d %-15s %-50s ", current_exec_order,
                    addrint_to_hexstring(ins_addr), ins_at_addr[ins_addr]->disassembled_name);
        std::for_each(ins_at_addr[ins_addr]->src_operands.begin(),
                      ins_at_addr[ins_addr]->src_operands.end(), [&](ptr_operand_t opr)
        {
          if ((opr->value.type() == typeid(REG)) &&
              !REG_is_fr_or_x87(boost::get<REG>(opr->exact_value)))
          {
            tfm::format(log_file, "(%s: %s)", opr->name,
                        addrint_to_hexstring(PIN_GetContextReg(p_ctxt, boost::get<REG>(opr->value))));
          }
        });

        tfm::format(log_file, " %-25s %-25s\n", ins_at_addr[ins_addr]->contained_image,
                    ins_at_addr[ins_addr]->contained_function);
#endif
      }
    }
    else
    {
      // the execution order exceed the maximal trace size, the instruction is mapped from the
      // kernel or located in some message receiving function
      prepare_new_rollbacking_phase();
    }
  }
  return;
}


/**
 * @brief in the tainting phase, the memory read analysis is used to:
 *  save a checkpoint each time the instruction read some memory addresses in the input buffer, and
 *  update source operands of the instruction as read memory addresses.
 */
auto mem_read_instruction (ADDRINT ins_addr, ADDRINT mem_read_addr, UINT32 mem_read_size,
                           const CONTEXT* p_ctxt, THREADID thread_id) -> VOID
{
  if (thread_id == traced_thread_id)
  {
    // verify if the instruction read some addresses in the input buffer
    if (std::max(mem_read_addr, received_msg_addr) <
        std::min(mem_read_addr + mem_read_size, received_msg_addr + received_msg_size))
    {
      // yes, then save a checkpoint
      ptr_checkpoint_t new_ptr_checkpoint(new checkpoint(current_exec_order,
                                                         p_ctxt, mem_read_addr, mem_read_size));
      saved_checkpoints.push_back(new_ptr_checkpoint);

#if !defined(NDEBUG)
      tfm::format(log_file, "checkpoint detected at %d because memory is read ",
                  new_ptr_checkpoint->exec_order);
      for (auto mem_idx = 0; mem_idx < mem_read_size; ++mem_idx)
        tfm::format(log_file, "(%s: %d)", addrint_to_hexstring(mem_read_addr + mem_idx),
                    *(reinterpret_cast<UINT8*>(mem_read_addr + mem_idx)));
      log_file << "\n";
#endif
    }

    // update source operands
    for (auto addr_offset = 0; addr_offset < mem_read_size; ++addr_offset)
    {
      ins_at_order[current_exec_order]->src_operands.insert(std::make_shared<operand>(
                                                              mem_read_addr + addr_offset));
    }
  }

  return;
}


/**
 * @brief in the tainting phase, the memory write analysis is used to:
 *  save orginal values of overwritten memory addresses, and
 *  update destination operands of the instruction as written memory addresses.
 */
auto mem_write_instruction(ADDRINT ins_addr, ADDRINT mem_written_addr, UINT32 mem_written_size,
                           THREADID thread_id) -> VOID
{
  if (thread_id == traced_thread_id)
  {
#if !defined(ENABLE_FAST_ROLLBACK)
    // the first saved checkpoint tracks memory write operations so that we can always rollback to
    if (!saved_checkpoints.empty())
    {
      saved_checkpoints[0]->mem_write_tracking(mem_written_addr, mem_written_size);
    }
#else
    ptr_checkpoints_t::iterator chkpnt_iter = saved_checkpoints.begin();
    for (; chkpnt_iter != saved_checkpoints.end(); ++chkpnt_iter)
    {
      (*chkpnt_iter)->mem_write_tracking(mem_written_addr, mem_written_size);
    }
#endif

    // update destination operands
    for (auto addr_offset = 0; addr_offset < mem_written_size; ++addr_offset)
    {
      ins_at_order[current_exec_order]->dst_operands.insert(std::make_shared<operand>(
                                                              mem_written_addr + addr_offset));
    }
  }

  return;
}


/**
 * @brief source_variables
 * @param idx
 * @return
 */
static inline auto source_variables(UINT32 ins_exec_order) -> std::set<df_vertex_desc>
{
  df_vertex_desc_set src_vertex_descs;

  std::for_each(ins_at_order[ins_exec_order]->src_operands.begin(),
                ins_at_order[ins_exec_order]->src_operands.end(), [&](ptr_operand_t opr)
  {
    // verify if the current source operand is
    auto outer_vertex_iter = dta_outer_vertices.begin();
    for (; outer_vertex_iter != dta_outer_vertices.end(); ++outer_vertex_iter)
    {
      // found in the outer interface
      if ((opr->value.type() == dta_graph[*outer_vertex_iter]->value.type()) &&
          (opr->name == dta_graph[*outer_vertex_iter]->name))
      {
        src_vertex_descs.insert(*outer_vertex_iter);
        break;
      }
    }

    // not found
    if (outer_vertex_iter == dta_outer_vertices.end())
    {
      auto new_vertex_desc = boost::add_vertex(opr, dta_graph);
      dta_outer_vertices.insert(new_vertex_desc); src_vertex_descs.insert(new_vertex_desc);
    }
  });

  return src_vertex_descs;
}


/**
 * @brief destination_variables
 * @param idx
 * @return
 */
static inline auto destination_variables(UINT32 idx) -> std::set<df_vertex_desc>
{
  std::set<df_vertex_desc> dst_vertex_descs;
  std::for_each(ins_at_order[idx]->dst_operands.begin(), ins_at_order[idx]->dst_operands.end(),
                [&](ptr_operand_t dst_operand)
  {
    // verify if the current target operand is
    auto outer_vertex_iter = dta_outer_vertices.begin();
    for (auto next_vertex_iter = outer_vertex_iter; outer_vertex_iter != dta_outer_vertices.end();
         outer_vertex_iter = next_vertex_iter)
    {
      ++next_vertex_iter;

      // found in the outer interface
      if ((dst_operand->value.type() == dta_graph[*outer_vertex_iter]->value.type())
          && (dst_operand->name == dta_graph[*outer_vertex_iter]->name))
      {
        // then insert the current target operand into the graph
        auto new_vertex_desc = boost::add_vertex(dst_operand, dta_graph);

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
      auto new_vertex_desc = boost::add_vertex(dst_operand, dta_graph);

      // and modify the outer interface by insert the new vertex
      dta_outer_vertices.insert(new_vertex_desc);

      dst_vertex_descs.insert(new_vertex_desc);
    }
  });

  return dst_vertex_descs;
}


/**
 * @brief graphical_propagation
 * @param ins_addr
 * @return
 */
auto graphical_propagation (ADDRINT ins_addr, THREADID thread_id) -> VOID
{
  if (thread_id == traced_thread_id)
  {
    auto src_vertex_descs = source_variables(current_exec_order);
    auto dst_vertex_descs = destination_variables(current_exec_order);

    std::for_each(src_vertex_descs.begin(), src_vertex_descs.end(),
                  [&](df_vertex_desc src_desc)
    {
      std::for_each(dst_vertex_descs.begin(), dst_vertex_descs.end(),
                    [&](df_vertex_desc dst_desc)
      {
        boost::add_edge(src_desc, dst_desc, current_exec_order, dta_graph);
      });
    });
  }

//  tfm::format(std::cerr, "graphical propagation %d <%s: %s>\n", current_exec_order,
//              addrint_to_hexstring(ins_addr), ins_at_addr[ins_addr]->disassembled_name);

  return;
}


/**
 * @brief initialize_tainting_phase
 */
void initialize()
{
  dta_graph.clear(); dta_outer_vertices.clear(); saved_checkpoints.clear(); ins_at_order.clear();

#if !defined(NDEBUG)
  newly_detected_input_dep_cfis.clear(); newly_detected_cfis.clear();
#endif

#if !defined(DISABLE_FSA)
//  path_code_at_order.clear();
  if (exploring_cfi) current_path_code = exploring_cfi->path_code;
#endif

  return;
}

} // end of tainting namespace
