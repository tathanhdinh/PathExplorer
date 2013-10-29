#include <pin.H>

#include <iostream>
#include <fstream>

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

static std::set<vdep_edge_desc> dep_edges;

/*====================================================================================================================*/

class dep_bfs_visitor : public boost::default_bfs_visitor
{
public:
  template < typename Edge, typename Graph > 
  void examine_edge(Edge e, const Graph& g) 
  {
    dep_edges.insert(e);
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

inline void compute_branch_mem_dependency() 
{
  vdep_vertex_iter vertex_iter;
  vdep_vertex_iter last_vertex_iter;
  
  dep_bfs_visitor dep_vis;
  
  std::set<vdep_edge_desc>::iterator edge_desc_iter;
  std::vector<ptr_branch>::iterator  ptr_branch_iter;
  
  boost::tie(vertex_iter, last_vertex_iter) = boost::vertices(dta_graph);
  for (; vertex_iter != last_vertex_iter; ++vertex_iter) 
  {
    if (dta_graph[*vertex_iter].type == MEM_VAR) 
    {
      boost::breadth_first_search(dta_graph, *vertex_iter, boost::visitor(dep_vis));
      
      for (edge_desc_iter = dep_edges.begin(); edge_desc_iter != dep_edges.end(); ++edge_desc_iter) 
      {
        for (ptr_branch_iter = tainted_ptr_branches.begin(); 
             ptr_branch_iter != tainted_ptr_branches.end(); ++ptr_branch_iter) 
        {
          if (dta_graph[*edge_desc_iter].second == (*ptr_branch_iter)->trace.size()) 
          {
            if (
                (received_msg_addr <= dta_graph[*vertex_iter].mem) && 
                (dta_graph[*vertex_iter].mem < received_msg_addr + received_msg_size)
               ) 
            {
              (*ptr_branch_iter)->dep_input_addrs.insert(dta_graph[*vertex_iter].mem);
            }
            else 
            {
              (*ptr_branch_iter)->dep_other_addrs.insert(dta_graph[*vertex_iter].mem);
            }
          }
        }
      }
      
      std::set<vdep_edge_desc>().swap(dep_edges);
    }
  }
  
  return;
}

/*====================================================================================================================*/

inline void compute_branch_min_checkpoint() 
{
  std::vector<ptr_branch>::iterator ptr_branch_iter;
  std::vector<ptr_checkpoint>::iterator ptr_chkpt_iter;
  std::set<ADDRINT>::iterator addr_iter;
  
  bool minimal_checkpoint_found = false;
  
  std::set<ADDRINT> intersec_mems;
  
  for (ptr_branch_iter = tainted_ptr_branches.begin(); 
       ptr_branch_iter != tainted_ptr_branches.end(); ++ptr_branch_iter) 
  {
    if ((*ptr_branch_iter)->dep_input_addrs.empty()) 
    {
      (*ptr_branch_iter)->chkpnt.reset();
    }
    else 
    {
      
      for (ptr_chkpt_iter = saved_ptr_checkpoints.begin(); 
           ptr_chkpt_iter != saved_ptr_checkpoints.end(); ++ptr_chkpt_iter)
      {
        for (addr_iter = (*ptr_chkpt_iter)->dep_mems.begin(); 
             addr_iter != (*ptr_chkpt_iter)->dep_mems.end(); ++addr_iter) 
        {
          if (
              std::find((*ptr_branch_iter)->dep_input_addrs.begin(), 
                        (*ptr_branch_iter)->dep_input_addrs.end(), *addr_iter) != 
                        (*ptr_branch_iter)->dep_input_addrs.end()
             ) 
          {
            minimal_checkpoint_found = true;
            break;
          }
        }

        if (minimal_checkpoint_found) 
        {
          (*ptr_branch_iter)->chkpnt = *ptr_chkpt_iter;
          break;
        }
      }
      
      if (!minimal_checkpoint_found) 
      {
        std::cerr << "Critical error: minimal checkpoint cannot found!\n";
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

VOID logging_ins_syscall(ADDRINT ins_addr)
{
  compute_branch_mem_dependency();
  compute_branch_min_checkpoint();
  
  journal_tainting_log();
  
  prepare_new_rollbacking_phase();
  return;
}

/*====================================================================================================================*/

// VOID logging_ins_count_analyzer(ADDRINT ins_addr)
// {  
//   if (explored_trace.size() < max_trace_length.Value())
//   {
//     explored_trace.push_back(ins_addr);
//   }
//   else // trace length limit reached
//   {
//     /* FOR TESTING ONLY */
// //     PIN_ExitApplication(0);
// 
//     compute_branch_mem_dependency();
//     compute_branch_min_checkpoint();
//     prepare_new_rollbacking_phase();
//   }
//   
//   return;
// }

/*====================================================================================================================*/

VOID logging_st_to_st_analyzer(ADDRINT ins_addr, 
                               ADDRINT mem_read_addr, UINT32 mem_read_size, 
                               ADDRINT mem_written_addr, UINT32 mem_written_size) 
{
  if (explored_trace.size() < max_trace_length.Value())
  {
    explored_trace.push_back(ins_addr);
    
    order_ins_dynamic_map[explored_trace.size()] = addr_ins_static_map[ins_addr];
    
    UINT32 idx;
    
    for (idx = 0; idx < mem_read_size; ++idx) 
    {
      order_ins_dynamic_map[explored_trace.size()].src_mems.insert(mem_read_addr + idx);
    }
    
    for (idx = 0; idx < mem_written_size; ++idx) 
    {
      order_ins_dynamic_map[explored_trace.size()].dst_mems.insert(mem_written_addr + idx);
    }
  }
  else // trace length limit reached
  {
    compute_branch_mem_dependency();
    compute_branch_min_checkpoint();
    
    journal_tainting_log();
    
    prepare_new_rollbacking_phase();
  }
  
//   print_debug_reg_to_reg(ins_addr, 
//                          boost::get<0>(dta_inss_io[ins_addr]).first.size(), 
//                          boost::get<0>(dta_inss_io[ins_addr]).second.size());
  return;
}

/*====================================================================================================================*/
// memmory read
VOID logging_mem_to_st_analyzer(ADDRINT ins_addr, ADDRINT mem_read_addr, UINT32 mem_read_size, CONTEXT* p_ctxt) 
{
  if (
      std::max(mem_read_addr, received_msg_addr) < 
      std::min(mem_read_addr + mem_read_size, received_msg_addr + received_msg_size)
     )
  {
    ptr_checkpoint new_ptr_checkpoint(new checkpoint(ins_addr, p_ctxt, explored_trace, mem_read_addr, mem_read_size));
    saved_ptr_checkpoints.push_back(new_ptr_checkpoint);
    
    if (!master_ptr_checkpoint) 
    {
      master_ptr_checkpoint = saved_ptr_checkpoints[0];
    }
   
//     print_debug_new_checkpoint(ins_addr);
  }
  else 
  {
//     print_debug_mem_read(ins_addr, mem_read_addr, mem_read_size);
  }
  
  return;
}

/*====================================================================================================================*/
// memory written
VOID logging_st_to_mem_analyzer(ADDRINT ins_addr, ADDRINT mem_written_addr, UINT32 mem_written_size) 
{  
  if (master_ptr_checkpoint) 
  {
    master_ptr_checkpoint->mem_written_logging(ins_addr, mem_written_addr, mem_written_size);
  }
  
  exepoint_checkpoints_map[explored_trace.size()] = saved_ptr_checkpoints;
  
//   print_debug_mem_written(ins_addr, mem_written_addr, mem_written_size);
  
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
  
  if (!new_ptr_branch->dep_input_addrs.empty()) // input dependent branch
  {
    input_dep_ptr_branches.push_back(new_ptr_branch);    
//     print_debug_dep_branch(ins_addr, new_ptr_branch);
  }
  else // input independent branch
  {
    input_indep_ptr_branches.push_back(new_ptr_branch);
//     print_debug_indep_branch(ins_addr, new_ptr_branch);
  }

  return;
}
