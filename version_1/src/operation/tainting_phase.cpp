#include <algorithm>

#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/lookup_edge.hpp>

#include "common.h"
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
static inline void determine_cfi_input_dependency()
{
  df_vertex_iter vertex_iter;
  df_vertex_iter last_vertex_iter;
  df_bfs_visitor df_visitor;
  std::vector<df_edge_desc>::iterator visited_edge_iter;

  ADDRINT mem_addr;

  UINT32 visited_edge_exec_order;
  ptr_cond_direct_ins_t visited_cfi;

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
        visited_edges.clear();
        boost::breadth_first_search(dta_graph, *vertex_iter, boost::visitor(df_visitor));

        // for each visited edge
        for (visited_edge_iter = visited_edges.begin();
             visited_edge_iter != visited_edges.end(); ++visited_edge_iter)
        {
          // the value of the edge is the execution order of the corresponding instruction
          visited_edge_exec_order = dta_graph[*visited_edge_iter];
          // consider only the instruction that is beyond the exploring CFI
          if (!exploring_cfi ||
              (exploring_cfi && (visited_edge_exec_order > exploring_cfi->exec_order)))
          {
            // and is some CFI
            if (ins_at_order[visited_edge_exec_order]->is_cond_direct_cf)
            {
              // then this CFI depends on the value of the memory address
              visited_cfi = pept::static_pointer_cast<cond_direct_instruction>(
                    ins_at_order[visited_edge_exec_order]);
              visited_cfi->input_dep_addrs.insert(mem_addr);
//#if !defined(NDEBUG)
//              tfm::format(log_file, "the cfi at %d depends on the address %s\n",
//                          visited_cfi->exec_order, addrint_to_hexstring(mem_addr));
//#endif
            }
          }
        }
      }
    }
  }

  return;
}


/**
 * @brief for each CFI, determine pairs of <checkpoint, affecting input addresses> so that a
 * rollback from the checkpoint with the modification on the affecting input addresses may change
 * the CFI's decision.
 */
static inline void set_checkpoints_for_cfi(const ptr_cond_direct_ins_t& cfi)
{
  addrint_set_t dep_addrs = cfi->input_dep_addrs;
  addrint_set_t input_dep_addrs, new_dep_addrs, intersected_addrs;
  checkpoint_with_modified_addrs checkpoint_with_input_addrs;

  ptr_checkpoints_t::iterator chkpnt_iter = saved_checkpoints.begin();
  for (; chkpnt_iter != saved_checkpoints.end(); ++chkpnt_iter)
  {
    // consider only checkpoints before the CFI
    if ((*chkpnt_iter)->exec_order <= cfi->exec_order)
    {
      // find the input addresses of the checkpoint
      input_dep_addrs.clear();
      addrint_value_map_t::iterator addr_iter = (*chkpnt_iter)->input_dep_original_values.begin();
      for (; addr_iter != (*chkpnt_iter)->input_dep_original_values.end(); ++addr_iter)
      {
        input_dep_addrs.insert(addr_iter->first);
      }

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
        checkpoint_with_input_addrs = std::make_pair(*chkpnt_iter, intersected_addrs);
        cfi->checkpoints.push_back(checkpoint_with_input_addrs);
//#if !defined(NDEBUG)
//        tfm::format(log_file, "the cfi at %d has a checkpoint at %d\n", cfi->exec_order,
//                    (*chkpnt_iter)->exec_order);
//#endif

        // the addrs in the intersected set are subtracted from the original dep_addrs
        new_dep_addrs.clear();
        std::set_difference(dep_addrs.begin(), dep_addrs.end(),
                            intersected_addrs.begin(), intersected_addrs.end(),
                            std::inserter(new_dep_addrs, new_dep_addrs.begin()));
        // if the rest is empty then we have finished
        if (new_dep_addrs.empty()) break;
        // but if it is not empty then we continue to the next checkpoint
        else dep_addrs = new_dep_addrs;
      }
    }
  }

  return;
}


/**
 * @brief save new tainted CFIs in this tainting phase
 */
static inline void save_detected_cfis()
{
  if (ins_at_order.size() > 1)
  {
#if defined(ENABLE_FSA)
    // the root path code is one of the exploring CFI: if the current instruction is the
    // exploring CFI then "1" should be appended into the path code (because "0" has been
    // appended in the previous tainting phase)
    if (exploring_cfi) current_path_code.push_back(true);
#endif

    ptr_cond_direct_ins_t new_cfi;
    std::map<UINT32, ptr_instruction_t>::iterator ins_iter = ins_at_order.begin();
#if defined(ENABLE_FSA)
    std::map<UINT32, ptr_instruction_t>::iterator prev_ins_iter = ins_iter;
#endif

    for (++ins_iter; ins_iter != ins_at_order.end(); ++ins_iter)
    {
      // consider only the instruction that is not behind the exploring CFI
      if (!exploring_cfi || (exploring_cfi && (ins_iter->first > exploring_cfi->exec_order)))
      {
#if defined(ENABLE_FSA)
        explored_fsa->add_edge((prev_ins_iter->second)->address, (ins_iter->second)->address,
                               current_path_code);
#endif

        if (ins_iter->second->is_cond_direct_cf)
        {
          new_cfi = pept::static_pointer_cast<cond_direct_instruction>(ins_iter->second);
          // and depends on the input
          if (!new_cfi->input_dep_addrs.empty())
          {
            // then set its checkpoints and save it
            set_checkpoints_for_cfi(new_cfi); detected_input_dep_cfis.push_back(new_cfi);
#if !defined(NDEBUG)
            newly_detected_input_dep_cfis.push_back(new_cfi);
#endif
          }
#if !defined(NDEBUG)
          newly_detected_cfis.push_back(new_cfi);
#endif

#if defined(ENABLE_FSA)
          // update the path code of the CFI
          new_cfi->path_code = current_path_code; current_path_code.push_back(false);
#endif
        }
      }
#if defined(ENABLE_FSA)
      prev_ins_iter = ins_iter;
#endif
//      else
//      {
//        break;
//      }
    }
  }

  return;
}


/**
 * @brief analyze_executed_instructions
 */
static inline void analyze_executed_instructions()
{
  if (!exploring_cfi)
  {
    save_tainting_graph(dta_graph, "tainting_graph.dot");
  }
  determine_cfi_input_dependency(); save_detected_cfis();
  return;
}


/**
 * @brief calculate a new limit trace length for the next rollbacking phase, this limit is the
 * execution order of the last input dependent CFI in the tainting phase.
 */
static inline void calculate_rollbacking_trace_length()
{
  rollbacking_trace_length = 0;
  order_ins_map_t::reverse_iterator ins_iter = ins_at_order.rbegin();
  ptr_cond_direct_ins_t last_cfi;

  // reverse iterate in the list of executed instructions
  for (++ins_iter; ins_iter != ins_at_order.rend(); ++ins_iter)
  {
    // verify if the instruction is a CFI
    if (ins_iter->second->is_cond_direct_cf)
    {
      // and this CFI depends on the input
      last_cfi = pept::static_pointer_cast<cond_direct_instruction>(ins_iter->second);
      if (!last_cfi->input_dep_addrs.empty()) break;
//      {
//        rollbacking_trace_length = last_cfi->exec_order;
//        break;
//      }
    }
  }

  if (last_cfi && (!exploring_cfi || (last_cfi->exec_order > exploring_cfi->exec_order)))
    rollbacking_trace_length = std::min(last_cfi->exec_order + 1, max_trace_size);
  else rollbacking_trace_length = 0;

  return;
}


/**
 * @brief prepare_new_rollbacking_phase
 */
inline void prepare_new_rollbacking_phase()
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
    log_file.flush();
#endif

    analyze_executed_instructions();

    // initalize the next rollbacking phase
    current_running_phase = rollbacking_phase; calculate_rollbacking_trace_length();
    rollbacking::initialize(rollbacking_trace_length);

#if !defined(NDEBUG)
    tfm::format(log_file, "stop analyzing, %d checkpoints, %d/%d branches detected; start rollbacking with limit trace %d\n",
                saved_checkpoints.size(), newly_detected_input_dep_cfis.size(),
                newly_detected_cfis.size(), rollbacking_trace_length);
    log_file.flush();
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
VOID kernel_mapped_instruction(ADDRINT ins_addr, THREADID thread_id)
{
  // the tainting phase always finishes when a kernel mapped instruction is met
  if (thread_id == traced_thread_id) prepare_new_rollbacking_phase();
  return;
}


/**
 * @brief analysis function applied for all instructions
 */
VOID generic_instruction(ADDRINT ins_addr, THREADID thread_id)
{
  ptr_cond_direct_ins_t current_cfi, duplicated_cfi;

  if (thread_id == traced_thread_id)
  {
    // verify if the execution order exceeds the limit trace length and the executed
    // instruction is always in user-space
    if ((current_exec_order < max_trace_size) && !ins_at_addr[ins_addr]->is_mapped_from_kernel)
    {
      // does not exceed
      current_exec_order++;
      if (ins_at_addr[ins_addr]->is_cond_direct_cf)
      {
        // duplicate a CFI (the default copy constructor is used)
        current_cfi = pept::static_pointer_cast<cond_direct_instruction>(ins_at_addr[ins_addr]);
        duplicated_cfi.reset(new cond_direct_instruction(*current_cfi));
        duplicated_cfi->exec_order = current_exec_order;
        ins_at_order[current_exec_order] = duplicated_cfi;

//#if defined(ENABLE_FSA)
//        // add an empty edge from the previous to the current instruction
//        if ((exploring_cfi && (current_exec_order > exploring_cfi->exec_order)) ||
//            (!exploring_cfi && (current_exec_order > 1)))
//          explored_fsa->add_edge(ins_at_order[current_exec_order - 1]->address,
//                                 ins_at_order[current_exec_order]->address, current_path_code);

//        duplicated_cfi->path_code = current_path_code;

        // the root path code is one of the exploring CFI: if the current instruction is the
        // exploring CFI then "1" should be appended into the path code (because "0" has been
        // appended in the previous tainting phase)
//        if (exploring_cfi && (current_exec_order == exploring_cfi->exec_order))
//          current_path_code.push_back(true);
//        else current_path_code.push_back(false);
//#endif
      }
      else
      {
        // duplicate an instruction (the default copy constructor is used)
        ins_at_order[current_exec_order].reset(new instruction(*ins_at_addr[ins_addr]));
      }

#if !defined(NDEBUG)
      tfm::format(log_file, "%-3d %-15s %-50s %-25s %-25s\n", current_exec_order,
                  addrint_to_hexstring(ins_addr), ins_at_addr[ins_addr]->disassembled_name,
                  ins_at_addr[ins_addr]->contained_image, ins_at_addr[ins_addr]->contained_function);
      //    log_file.flush();
#endif
    }
    else
    {
      // exceed
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
VOID mem_read_instruction(ADDRINT ins_addr, ADDRINT mem_read_addr, UINT32 mem_read_size,
                          CONTEXT* p_ctxt, THREADID thread_id)
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
      tfm::format(log_file, "checkpoint detected at %d (%s: %s) because memory is read (%s: %d)\n",
                  new_ptr_checkpoint->exec_order, addrint_to_hexstring(ins_addr),
                  ins_at_addr[ins_addr]->disassembled_name, addrint_to_hexstring(mem_read_addr),
                  *(reinterpret_cast<UINT8*>(mem_read_addr)));
#endif
    }

    // update source operands
    ptr_operand_t mem_operand;
    for (UINT32 addr_offset = 0; addr_offset < mem_read_size; ++addr_offset)
    {
      mem_operand.reset(new operand(mem_read_addr + addr_offset));
      ins_at_order[current_exec_order]->src_operands.insert(mem_operand);
    }
  }
  return;
}


/**
 * @brief in the tainting phase, the memory write analysis is used to:
 *  save orginal values of overwritten memory addresses, and
 *  update destination operands of the instruction as written memory addresses.
 */
VOID mem_write_instruction(ADDRINT ins_addr, ADDRINT mem_written_addr, UINT32 mem_written_size,
                           THREADID thread_id)
{
  if (thread_id == traced_thread_id)
  {
#if !defined(ENABLE_FAST_ROLLBACK)
    // the first saved checkpoint tracks memory write operations so that we can always rollback to it
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
    ptr_operand_t mem_operand;
    for (UINT32 addr_offset = 0; addr_offset < mem_written_size; ++addr_offset)
    {
      mem_operand.reset(new operand(mem_written_addr + addr_offset));
      ins_at_order[current_exec_order]->dst_operands.insert(mem_operand);
    }
  }

  return;
}


/**
 * @brief source_variables
 * @param idx
 * @return
 */
inline std::set<df_vertex_desc> source_variables(UINT32 ins_exec_order)
{
  df_vertex_desc_set src_vertex_descs;
  df_vertex_desc_set::iterator outer_vertex_iter;
  df_vertex_desc new_vertex_desc;
  std::set<ptr_operand_t>::iterator src_operand_iter;
  for (src_operand_iter = ins_at_order[ins_exec_order]->src_operands.begin();
       src_operand_iter != ins_at_order[ins_exec_order]->src_operands.end(); ++src_operand_iter)
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


/**
 * @brief destination_variables
 * @param idx
 * @return
 */
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


/**
 * @brief graphical_propagation
 * @param ins_addr
 * @return
 */
VOID graphical_propagation(ADDRINT ins_addr, THREADID thread_id)
{
  if (thread_id == traced_thread_id)
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
  }

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

#if defined(ENABLE_FSA)
//  path_code_at_order.clear();
  if (exploring_cfi) current_path_code = exploring_cfi->path_code;
#endif

  return;
}

} // end of tainting namespace
