#include "branch.h"

#include <algorithm>

#include "variable.h"

extern vdep_graph                   dta_graph;
extern std::vector<ADDRINT>         explored_trace;
extern std::vector<ptr_checkpoint>  saved_ptr_checkpoints;

/*====================================================================================================================*/

inline void dfs_traversal(vdep_vertex_desc const& start_vertex, 
                          std::vector< std::vector<vdep_edge_desc> >& traversed_edges_list)
{
  vdep_out_edge_iter edge_iter;
  vdep_out_edge_iter last_edge_iter;
  vdep_out_edge_iter min_edge_iter;
  
  UINT32 min_order;
  bool found_new_edge;
  
  std::vector<vdep_edge_desc> traversed_edges;
  vdep_vertex_desc current_vertex;
  
  vdep_out_edge_iter edge_start_iter;
  vdep_out_edge_iter last_edge_start_iter;
  boost::tie(edge_start_iter, last_edge_start_iter) = boost::out_edges(start_vertex, dta_graph);
  
  // start traversing with out edges of start_vertex (each of them will give a different path)
  for (; edge_start_iter != last_edge_start_iter; ++edge_start_iter) 
  {
    std::vector<vdep_edge_desc>().swap(traversed_edges);
    traversed_edges.push_back(*edge_start_iter);
    
    min_order = dta_graph[*edge_start_iter].second;
    current_vertex = boost::target(*edge_start_iter, dta_graph);
    
    do
    {
      found_new_edge = false;
      
      // find an out edge of the minimum order (but still greater than one of the current traversed edge) 
      boost::tie(edge_iter, last_edge_iter) = boost::out_edges(current_vertex, dta_graph);
      for (; edge_iter != last_edge_iter; ++edge_iter) 
      {
        if (dta_graph[*edge_iter].second > min_order) 
        {
          if (!found_new_edge) 
          {
            min_edge_iter = edge_iter;
            found_new_edge = true;
          }
          else 
          {
            if (dta_graph[*min_edge_iter].second > dta_graph[*edge_iter].second) 
            {
              min_edge_iter = edge_iter;
            }
          }
        }
      }
      
      if (found_new_edge) 
      {
        min_order = dta_graph[*min_edge_iter].second;
        traversed_edges.push_back(*min_edge_iter);
        current_vertex = boost::target(*min_edge_iter, dta_graph);
      }
    }
    while (found_new_edge);
    
    traversed_edges_list.push_back(traversed_edges);
  }
  
  return;
}

/*====================================================================================================================*/

inline void dependency_computation(std::set<ADDRINT>& dep_input_addrs, std::set<ADDRINT>& dep_out_addrs)
{
  vdep_vertex_iter vertex_iter;
  vdep_vertex_iter last_vertex_iter;
  std::vector< std::vector<vdep_edge_desc> > traversed_edges_list;
  
  boost::tie(vertex_iter, last_vertex_iter) = boost::vertices(dta_graph);
  for (; vertex_iter != last_vertex_iter; ++vertex_iter)
  {
    if (dta_graph[*vertex_iter].type == MEM_VAR) // a memory address found
    {
      std::vector< std::vector<vdep_edge_desc> >().swap(traversed_edges_list);
      dfs_traversal(*vertex_iter, traversed_edges_list); // then take dfs traversal from it
      
      std::vector< std::vector<vdep_edge_desc> >::iterator traversed_edges_iter = traversed_edges_list.begin();
      for (; traversed_edges_iter != traversed_edges_list.end(); ++traversed_edges_iter) 
      {
        if (dta_graph[traversed_edges_iter->back()].second == explored_trace.size()) 
        {
          if (
              (received_msg_addr <= dta_graph[*vertex_iter].mem) &&
              (received_msg_addr + received_msg_size > dta_graph[*vertex_iter].mem)
             ) // and it locates in the message
          {
            dep_input_addrs.insert(dta_graph[*vertex_iter].mem);
          }
          else 
          {
            dep_out_addrs.insert(dta_graph[*vertex_iter].mem);
          }
        }
      }
    }
  }
  
  return;
}

/*====================================================================================================================*/

branch::branch(ADDRINT ins_addr, bool br_taken)
{
  this->addr              = ins_addr;
  this->trace             = explored_trace;
  this->br_taken          = br_taken;
  this->is_resolved       = false;
  this->is_just_resolved  = false;
  this->is_bypassed       = false;
  this->is_explored       = false;

  // calculate the dependency memories
  dependency_computation(this->dep_input_addrs, this->dep_out_addrs);
  
  // calculate the minimal checkpoint
  bool minimal_checkpoint_found = false;
  
  if (!this->dep_input_addrs.empty())
  {
    std::set<ADDRINT> intersec_mems;
    
    std::vector<ptr_checkpoint>::iterator ptr_chkpt_iter = saved_ptr_checkpoints.begin();
    for (; ptr_chkpt_iter != saved_ptr_checkpoints.end(); ++ptr_chkpt_iter)
    {
      std::set<ADDRINT>::iterator addr_iter = (*ptr_chkpt_iter)->dep_mems.begin();
      for (; addr_iter != (*ptr_chkpt_iter)->dep_mems.end(); ++addr_iter) 
      {
        if (
            std::find(this->dep_input_addrs.begin(), this->dep_input_addrs.end(), *addr_iter) != this->dep_input_addrs.end()
           ) 
        {
          minimal_checkpoint_found = true;
          break;
        }
      }

      if (minimal_checkpoint_found) 
      {
        this->chkpnt = *ptr_chkpt_iter;
        break;
      }
    }
    
//     if (!minimal_checkpoint_found) 
//     {
//       std::cerr << "Critical error: minimal checkpoint cannot found!\n";
//       PIN_ExitApplication(0);
//     }
  }  
}

/*====================================================================================================================*/

branch::branch(const branch& other)
{
  this->addr              = other.addr;
  this->trace             = other.trace;
  this->br_taken          = other.br_taken;
  this->dep_input_addrs          = other.dep_input_addrs;
  this->chkpnt            = other.chkpnt;
  this->inputs            = other.inputs;
  
  this->is_resolved       = other.is_resolved;
  this->is_just_resolved  = other.is_just_resolved;
  this->is_bypassed       = other.is_bypassed;
  this->is_explored       = other.is_explored;
}

/*====================================================================================================================*/

branch& branch::operator=(const branch& other)
{
  this->addr              = other.addr;
  this->trace             = other.trace;
  this->br_taken          = other.br_taken;
  this->dep_input_addrs          = other.dep_input_addrs;
  this->chkpnt            = other.chkpnt;
  this->inputs            = other.inputs;
  
  this->is_resolved       = other.is_resolved;
  this->is_just_resolved  = other.is_just_resolved;
  this->is_bypassed       = other.is_bypassed;
  this->is_explored       = other.is_explored;
  
  return *this;
}

/*====================================================================================================================*/

bool branch::operator==(const branch& other)
{
  return ((this->addr = other.addr) && (this->trace.size() == other.trace.size()) && (this->br_taken == other.br_taken)); 
}
