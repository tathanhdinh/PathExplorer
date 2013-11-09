#include <pin.H>

#include <iostream>
#include <fstream>
#include <map>
#include <set>

#include <boost/format.hpp>
#include <boost/graph/breadth_first_search.hpp>

#include "stuffs.h"
#include "instruction.h"
#include "checkpoint.h"
#include "branch.h"
#include "variable.h"

/*====================================================================================================================*/

extern bool                                     in_tainting;

extern std::map<ADDRINT, instruction>           addr_ins_static_map;
extern std::map<UINT32, instruction>            order_ins_dynamic_map;

extern UINT32                                   total_rollback_times;

extern vdep_graph                               dta_graph;
extern map_ins_io                               dta_inss_io;

extern std::vector<ADDRINT>                     explored_trace;

extern std::vector<ptr_branch>                  input_dep_ptr_branches;
extern std::vector<ptr_branch>                  input_indep_ptr_branches;
extern std::vector<ptr_branch>                  tainted_ptr_branches;

extern ptr_branch                               exploring_ptr_branch;

extern UINT32                                   input_dep_branch_num;

extern std::vector<ptr_checkpoint>              saved_ptr_checkpoints;
extern ptr_checkpoint                           master_ptr_checkpoint;

extern std::map< UINT32,
                 std::vector<ptr_checkpoint> >  exepoint_checkpoints_map;

extern ADDRINT                                  received_msg_addr;
extern UINT32                                   received_msg_size;

extern KNOB<BOOL>                               print_debug_text;
extern KNOB<UINT32>                             max_trace_length;

/*====================================================================================================================*/

static std::map<vdep_vertex_desc,
                vdep_vertex_desc>   prec_vertex_desc;

/*====================================================================================================================*/

class dep_bfs_visitor : public boost::default_bfs_visitor
{
public:
  template <typename Edge, typename Graph>
  void tree_edge(Edge e, const Graph& g)
  {
    prec_vertex_desc[boost::target(e, g)] = boost::source(e, g);
  }
};

/*====================================================================================================================*/

inline void omit_branch(ptr_branch& omitted_ptr_branch)
{
  omitted_ptr_branch->is_resolved = true;
  omitted_ptr_branch->is_bypassed = false;
  omitted_ptr_branch->is_just_resolved = false;

  return;
}

/*====================================================================================================================*/

std::vector<UINT32> backward_trace(vdep_vertex_desc root_vertex, vdep_vertex_desc last_vertex)
{
  std::vector<UINT32> backward_trace;

  vdep_vertex_desc current_vertex = last_vertex;
  vdep_vertex_desc backward_vertex;

  vdep_edge_desc current_edge;
  bool edge_exist;

  while (current_vertex != root_vertex)
  {
    backward_vertex = prec_vertex_desc[current_vertex];

    boost::tie(current_edge, edge_exist) = boost::edge(backward_vertex, current_vertex, dta_graph);
    if (edge_exist)
    {
      backward_trace.push_back(dta_graph[current_edge].second);
    }
    else
    {
      std::cout << "Critical error: edge not found in backward trace construction.\n";
      PIN_ExitApplication(0);
    }

    current_vertex = backward_vertex;
  }

  return backward_trace;
}

/*====================================================================================================================*/

inline void compute_branch_mem_dependency()
{
  vdep_vertex_iter vertex_iter;
  vdep_vertex_iter last_vertex_iter;

  vdep_edge_desc edge_desc;
  bool edge_exist;

  dep_bfs_visitor dep_vis;

  std::set<vdep_edge_desc>::iterator edge_iter;
  std::vector<ptr_branch>::iterator  ptr_branch_iter;

  std::map<vdep_vertex_desc, vdep_vertex_desc>::iterator prec_vertex_iter;

  ADDRINT current_addr;

  boost::tie(vertex_iter, last_vertex_iter) = boost::vertices(dta_graph);
  for (; vertex_iter != last_vertex_iter; ++vertex_iter)
  {
    if (dta_graph[*vertex_iter].type == MEM_VAR)
    {
      std::map<vdep_vertex_desc, vdep_vertex_desc>().swap(prec_vertex_desc);

      boost::breadth_first_search(dta_graph, *vertex_iter, boost::visitor(dep_vis));

      for (prec_vertex_iter = prec_vertex_desc.begin();
              prec_vertex_iter != prec_vertex_desc.end(); ++prec_vertex_iter)
      {
        boost::tie(edge_desc, edge_exist) = boost::edge(prec_vertex_iter->second, 
                                                        prec_vertex_iter->first, dta_graph);
        if (edge_exist)
        {
          for (ptr_branch_iter = tainted_ptr_branches.begin();
                  ptr_branch_iter != tainted_ptr_branches.end(); ++ptr_branch_iter)
          {
            if (dta_graph[edge_desc].second == (*ptr_branch_iter)->trace.size())
            {
              current_addr = dta_graph[*vertex_iter].mem;

              if ((received_msg_addr <= current_addr) && (current_addr < received_msg_addr + received_msg_size))
              {
                (*ptr_branch_iter)->dep_input_addrs.insert(current_addr);
              }
              else
              {
                (*ptr_branch_iter)->dep_other_addrs.insert(dta_graph[*vertex_iter].mem);
              }

              (*ptr_branch_iter)->dep_backward_traces[current_addr] = backward_trace(*vertex_iter, 
                                                                                     prec_vertex_iter->second);
            }
          }
        }
        else
        {
          std::cerr << "Critical error: backward edge not found in BFS.\n";
          PIN_ExitApplication(0);
        }
      }
    }
  }

  ptr_branch_iter = tainted_ptr_branches.begin();
  for (; ptr_branch_iter != tainted_ptr_branches.end(); ++ptr_branch_iter)
  {
    if (!(*ptr_branch_iter)->dep_input_addrs.empty())
    {
      input_dep_ptr_branches.push_back(*ptr_branch_iter);
    }
    else
    {
      input_indep_ptr_branches.push_back(*ptr_branch_iter);
    }
  }

    return;
}

/*====================================================================================================================*/

inline void compute_branch_min_checkpoint()
{
  std::vector<ptr_branch>::iterator     ptr_branch_iter;
  std::vector<ptr_checkpoint>::iterator ptr_checkpoint_iter;
  std::set<ADDRINT>::iterator           addr_iter;

  ptr_checkpoint nearest_ptr_checkpoint;

  std::set<ADDRINT> intersec_mems;

  ptr_branch_iter = tainted_ptr_branches.begin();
  for (; ptr_branch_iter != tainted_ptr_branches.end(); ++ptr_branch_iter)
  {
    if ((*ptr_branch_iter)->dep_input_addrs.empty())
    {
        (*ptr_branch_iter)->checkpoint.reset();
    }
    else // compute nearest checkpoint for *ptr_branch_iter
    {
      // for each *addr_iter in (*ptr_branch_iter)->dep_input_addrs, find the earliest checkpoint that uses it
      addr_iter = (*ptr_branch_iter)->dep_input_addrs.begin();
      for (; addr_iter != (*ptr_branch_iter)->dep_input_addrs.end(); ++addr_iter) 
      {
        ptr_checkpoint_iter = saved_ptr_checkpoints.begin();
        for (; ptr_checkpoint_iter != saved_ptr_checkpoints.end(); ++ptr_checkpoint_iter) 
        {
          // *addr_iter is found in (*ptr_checkpoint_iter)->dep_mems
          if (std::find((*ptr_checkpoint_iter)->dep_mems.begin(), (*ptr_checkpoint_iter)->dep_mems.end(), *addr_iter) 
              != (*ptr_checkpoint_iter)->dep_mems.end()) 
          {
            (*ptr_branch_iter)->nearest_checkpoints[*ptr_checkpoint_iter].insert(*addr_iter);
            break;
          }
        }
      }
      
      if ((*ptr_branch_iter)->nearest_checkpoints.size() != 0) 
      {
        (*ptr_branch_iter)->checkpoint = (*ptr_branch_iter)->nearest_checkpoints.rbegin()->first;
      }
      else 
      {
        std::cerr << "Critical error: nearest checkpoint cannot found!\n";
        PIN_ExitApplication(0);
      }
    }
  }

  return;
}

/*====================================================================================================================*/

inline void prepare_new_rollbacking_phase()
{
  print_debug_start_rollbacking();

  in_tainting = false;
  PIN_RemoveInstrumentation();

  if (exploring_ptr_branch)
  {
    rollback_with_input_replacement(master_ptr_checkpoint,
                                    exploring_ptr_branch->inputs[!exploring_ptr_branch->br_taken][0].get());
  }
  else
  {
    if (!input_dep_ptr_branches.empty())
    {
      rollback_with_input_replacement(master_ptr_checkpoint,
                                      input_dep_ptr_branches[0]->inputs[input_dep_ptr_branches[0]->br_taken][0].get());
    }
    else
    {
      PIN_ExitApplication(0);
    }
  }

    return;
}

/*====================================================================================================================*/

VOID logging_syscall_instruction_analyzer(ADDRINT ins_addr)
{
  compute_branch_mem_dependency();
  compute_branch_min_checkpoint();

  journal_tainting_log();

  prepare_new_rollbacking_phase();
  return;
}

/*====================================================================================================================*/

VOID logging_general_instruction_analyzer(ADDRINT ins_addr)
{
  if (explored_trace.size() < max_trace_length.Value())
  {
    explored_trace.push_back(ins_addr);
    order_ins_dynamic_map[explored_trace.size()] = addr_ins_static_map[ins_addr];
  }
  else // trace length limit reached
  {
    compute_branch_mem_dependency();
    compute_branch_min_checkpoint();

    journal_tainting_log();

    prepare_new_rollbacking_phase();
  }

  return;
}

/*====================================================================================================================*/
// memmory read
VOID logging_mem_read_instruction_analyzer(ADDRINT ins_addr, 
                                           ADDRINT mem_read_addr, UINT32 mem_read_size, 
                                           CONTEXT* p_ctxt)
{
  // a new checkpoint found
  if (std::max(mem_read_addr, received_msg_addr) <
      std::min(mem_read_addr + mem_read_size, received_msg_addr + received_msg_size))
  {
    ptr_checkpoint new_ptr_checkpoint(new checkpoint(ins_addr, p_ctxt, explored_trace, mem_read_addr, mem_read_size));
    saved_ptr_checkpoints.push_back(new_ptr_checkpoint);

    if (!master_ptr_checkpoint)
    {
      master_ptr_checkpoint = saved_ptr_checkpoints[0];
    }
  }

  for (UINT32 idx = 0; idx < mem_read_size; ++idx)
  {
    order_ins_dynamic_map[explored_trace.size()].src_mems.insert(mem_read_addr + idx);
  }

  return;
}

/*====================================================================================================================*/
// memory written
VOID logging_mem_write_instruction_analyzer(ADDRINT ins_addr, ADDRINT mem_written_addr, UINT32 mem_written_size)
{
  if (master_ptr_checkpoint)
  {
    master_ptr_checkpoint->mem_written_logging(ins_addr, mem_written_addr, mem_written_size);
  }

  exepoint_checkpoints_map[explored_trace.size()] = saved_ptr_checkpoints;

  for (UINT32 idx = 0; idx < mem_written_size; ++idx)
  {
    order_ins_dynamic_map[explored_trace.size()].dst_mems.insert(mem_written_addr + idx);
  }

    return;
}

/*====================================================================================================================*/

VOID logging_cond_br_analyzer(ADDRINT ins_addr, bool br_taken)
{
  ptr_branch new_ptr_branch(new branch(ins_addr, br_taken));

  // save the first input
  store_input(new_ptr_branch, br_taken);

  // verify if the branch is a new tainted branch
  if (exploring_ptr_branch)
  {
    if (new_ptr_branch->trace.size() <= exploring_ptr_branch->trace.size()) // it is not
    {
      omit_branch(new_ptr_branch); // then omit it
    }
    else
    {
      tainted_ptr_branches.push_back(new_ptr_branch);
    }
  }
  else // it is always a new tainted branch in the first tainting phase
  {
    tainted_ptr_branches.push_back(new_ptr_branch);
  }

  return;
}
