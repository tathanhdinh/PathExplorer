#include <pin.H>

#include <iostream>
#include <fstream>
#include <map>
#include <set>

#include <boost/format.hpp>
#include <boost/log/trivial.hpp>
#include <boost/graph/breadth_first_search.hpp>

#include "stuffs.h"
#include "instruction.h"
#include "checkpoint.h"
#include "branch.h"
#include "variable.h"

/*================================================================================================*/

extern bool                                     in_tainting;

extern std::map<ADDRINT, instruction>           addr_ins_static_map;
extern std::map<UINT32, instruction>            order_ins_dynamic_map;

extern UINT32                                   total_rollback_times;

extern vdep_graph                               dta_graph;
extern map_ins_io                               dta_inss_io;

extern std::vector<ADDRINT>                     explored_trace;

extern std::map<UINT32, ptr_branch>             order_input_dep_ptr_branch_map;
extern std::map<UINT32, ptr_branch>             order_input_indep_ptr_branch_map;
extern std::map<UINT32, ptr_branch>             order_tainted_ptr_branch_map;

extern ptr_branch                               exploring_ptr_branch;

extern std::vector<ptr_branch>                  total_input_dep_ptr_branches;

// extern UINT32                                   input_dep_branch_num;

extern std::vector<ptr_checkpoint>              saved_ptr_checkpoints;
extern ptr_checkpoint                           master_ptr_checkpoint;

extern std::map< UINT32,
                 std::vector<ptr_checkpoint> >  exepoint_checkpoints_map;

extern ADDRINT                                  received_msg_addr;
extern UINT32                                   received_msg_size;

extern KNOB<BOOL>                               print_debug_text;
extern KNOB<UINT32>                             max_trace_length;

extern UINT32                                   max_trace_size;

/*================================================================================================*/

static std::map<vdep_vertex_desc,
                vdep_vertex_desc>               prec_vertex_desc;

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

inline void mark_resolved(ptr_branch& omitted_ptr_branch)
{
  omitted_ptr_branch->is_resolved = true;
  omitted_ptr_branch->is_bypassed = false;
  omitted_ptr_branch->is_just_resolved = false;

  return;
}

/*================================================================================================*/

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

    boost::tie(current_edge, edge_exist) = boost::edge(backward_vertex, 
                                                       current_vertex, dta_graph);
    if (edge_exist)
    {
      backward_trace.push_back(dta_graph[current_edge].second);
    }
    else
    {
      BOOST_LOG_TRIVIAL(fatal) << "Edge not found in backward trace construction.";
      PIN_ExitApplication(0);
    }

    current_vertex = backward_vertex;
  }

  return backward_trace;
}

/*================================================================================================*/

inline void compute_branch_mem_dependency()
{
  vdep_vertex_iter vertex_iter;
  vdep_vertex_iter last_vertex_iter;

  vdep_edge_desc edge_desc;
  bool edge_exist;

  dep_bfs_visitor dep_vis;

  std::map<UINT32, ptr_branch>::iterator order_ptr_branch_iter;
  ptr_branch current_ptr_branch;

  std::map<vdep_vertex_desc, vdep_vertex_desc>::iterator prec_vertex_iter;

  ADDRINT current_addr;

  boost::tie(vertex_iter, last_vertex_iter) = boost::vertices(dta_graph);
  for (; vertex_iter != last_vertex_iter; ++vertex_iter)
  {
    if (dta_graph[*vertex_iter].type == MEM_VAR)
    {
//       std::map<vdep_vertex_desc, vdep_vertex_desc>().swap(prec_vertex_desc);
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
            if (dta_graph[edge_desc].second == current_ptr_branch->trace.size()) 
            {
              current_addr = dta_graph[*vertex_iter].mem;
              
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
          BOOST_LOG_TRIVIAL(fatal) << "Backward edge not found in BFS.";
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
      order_input_dep_ptr_branch_map[current_ptr_branch->trace.size()] 
        = current_ptr_branch;
      
      if (exploring_ptr_branch) 
      {
        if (current_ptr_branch->trace.size() > exploring_ptr_branch->trace.size()) 
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
      order_input_indep_ptr_branch_map[current_ptr_branch->trace.size()] 
        = current_ptr_branch;
    }
  }

  return;
}

/*================================================================================================*/

inline void compute_branch_min_checkpoint()
{
  std::vector<ptr_checkpoint>::iterator   ptr_checkpoint_iter;
  std::vector<ptr_checkpoint>::reverse_iterator ptr_checkpoint_reverse_iter;
  std::set<ADDRINT>::iterator             addr_iter;
  std::map<UINT32, ptr_branch>::iterator  order_ptr_branch_iter;

  ptr_branch      current_ptr_branch;
  ptr_checkpoint  nearest_ptr_checkpoint;
  
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
        
        // find the ideal checkpoint
        if (nearest_checkpoint_found) 
        {
          ptr_checkpoint_reverse_iter = saved_ptr_checkpoints.rbegin();
          for (; ptr_checkpoint_reverse_iter != saved_ptr_checkpoints.rend(); 
               ++ptr_checkpoint_reverse_iter) 
          {
            
          }
          
        }
      }
      
      if (current_ptr_branch->nearest_checkpoints.size() != 0) 
      {
        BOOST_LOG_TRIVIAL(info) 
          << boost::format("The branch at %d:%d (%s: %s) has %d nearest checkpoints.") 
              % current_ptr_branch->trace.size()
              % current_ptr_branch->br_taken
              % remove_leading_zeros(StringFromAddrint(current_ptr_branch->addr))
              % order_ins_dynamic_map[current_ptr_branch->trace.size()].disass
              % current_ptr_branch->nearest_checkpoints.size();
              
        current_ptr_branch->checkpoint = current_ptr_branch->nearest_checkpoints.rbegin()->first;
      }
      else 
      {
        BOOST_LOG_TRIVIAL(fatal) 
          << boost::format("Cannot found any nearest checkpoint for the branch at %d.!") 
              % current_ptr_branch->trace.size();
              
        PIN_ExitApplication(0);
      }
    }
  }
   
  return;
}

/*================================================================================================*/

inline void prepare_new_rollbacking_phase()
{
  BOOST_LOG_TRIVIAL(info) 
    << boost::format("\033[33mStop exploring, %d instructions analyzed. Start detecting checkpoints\033[0m") 
        % explored_trace.size();
  
//   journal_tainting_graph("tainting_graph.dot");
//   PIN_ExitApplication(0);
  
  compute_branch_mem_dependency();
  compute_branch_min_checkpoint();
    
  BOOST_LOG_TRIVIAL(info) 
    << boost::format("\033[33mStop detecting, %d checkpoints and %d/%d branches detected. Start rollbacking.\033[0m") 
        % saved_ptr_checkpoints.size() 
        % order_input_dep_ptr_branch_map.size() 
        % order_tainted_ptr_branch_map.size();

//   journal_tainting_log();
    
  in_tainting = false;
  PIN_RemoveInstrumentation();
  
  if (exploring_ptr_branch)
  {
    rollback_with_input_replacement(saved_ptr_checkpoints[0],
                                    exploring_ptr_branch
                                      ->inputs[!exploring_ptr_branch->br_taken][0].get());
  }
  else
  {
    journal_tainting_graph("tainting_graph.dot");
    journal_explored_trace("explored_trace.log");
//     journal_static_trace("static_trace");
    
    // the first rollbacking phase
    if (!order_input_dep_ptr_branch_map.empty())
    { 
      ptr_branch first_ptr_branch = order_input_dep_ptr_branch_map.begin()->second;
      rollback_with_input_replacement(saved_ptr_checkpoints[0], 
                                      first_ptr_branch
                                        ->inputs[first_ptr_branch->br_taken][0].get());
    }
    else
    {
      BOOST_LOG_TRIVIAL(info) << "There is no branch needed to resolve.";
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
  if ((explored_trace.size() < max_trace_size) && 
      (!addr_ins_static_map[ins_addr].contained_image.empty()))
  {
    explored_trace.push_back(ins_addr);
    order_ins_dynamic_map[explored_trace.size()] = addr_ins_static_map[ins_addr];
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
    ptr_checkpoint new_ptr_checkpoint(new checkpoint(ins_addr, p_ctxt, explored_trace, 
                                                     mem_read_addr, mem_read_size));
    saved_ptr_checkpoints.push_back(new_ptr_checkpoint);
    
    BOOST_LOG_TRIVIAL(trace) 
      << boost::format("Checkpoint detected at %d (%s).") 
          % new_ptr_checkpoint->trace.size() % addr_ins_static_map[ins_addr].disass;
  }

  for (UINT32 idx = 0; idx < mem_read_size; ++idx)
  {
    order_ins_dynamic_map[explored_trace.size()].src_mems.insert(mem_read_addr + idx);
  }

  return;
}

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

  exepoint_checkpoints_map[explored_trace.size()] = saved_ptr_checkpoints;

  for (UINT32 idx = 0; idx < mem_written_size; ++idx)
  {
    order_ins_dynamic_map[explored_trace.size()].dst_mems.insert(mem_written_addr + idx);
  }

  return;
}

/*================================================================================================*/

VOID logging_cond_br_analyzer(ADDRINT ins_addr, bool br_taken)
{
  ptr_branch new_ptr_branch(new branch(ins_addr, br_taken));

  // save the first input
  store_input(new_ptr_branch, br_taken);

  // verify if the branch is a new tainted branch
  if (exploring_ptr_branch && 
      (new_ptr_branch->trace.size() <= exploring_ptr_branch->trace.size()))
  {
    // mark it as resolved
    mark_resolved(new_ptr_branch); 
  }
  
  order_tainted_ptr_branch_map[explored_trace.size()] = new_ptr_branch;

  return;
}
