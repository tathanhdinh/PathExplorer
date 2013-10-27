#include "branch.h"

#include <algorithm>
#include <set>

#include <boost/graph/breadth_first_search.hpp>

#include "variable.h"

/*====================================================================================================================*/

static std::set<vdep_edge_desc> dep_edges;

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

extern vdep_graph                   dta_graph;
extern std::vector<ADDRINT>         explored_trace;
extern std::vector<ptr_checkpoint>  saved_ptr_checkpoints;

/*====================================================================================================================*/

void dependent_mem_addrs(std::set<ADDRINT>& dep_mem_addrs) 
{
  vdep_vertex_iter vertex_iter;
  vdep_vertex_iter last_vertex_iter;
  
  dep_bfs_visitor dep_vis;
  
  boost::tie(vertex_iter, last_vertex_iter) = boost::vertices(dta_graph);
  for (; vertex_iter != last_vertex_iter; ++vertex_iter) 
  {
    if (dta_graph[*vertex_iter].type == MEM_VAR) 
    {
      boost::breadth_first_search(dta_graph, *vertex_iter, boost::visitor(dep_vis));
      
      std::set<vdep_edge_desc>::iterator edge_desc_iter = dep_edges.begin();
      for (; edge_desc_iter != dep_edges.end(); ++edge_desc_iter) 
      {
        if (dta_graph[*edge_desc_iter].second == explored_trace.size()) 
        {
          dep_mem_addrs.insert(dta_graph[*vertex_iter].mem);
        }
      }
      
      std::set<vdep_edge_desc>().swap(dep_edges);
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
  std::set<ADDRINT> dep_mem_addrs;
  dependent_mem_addrs(dep_mem_addrs);
  for (std::set<ADDRINT>::iterator addr_iter = dep_mem_addrs.begin(); addr_iter != dep_mem_addrs.end(); ++addr_iter) 
  {
    if (
        (received_msg_addr <= *addr_iter) && (*addr_iter < received_msg_addr + received_msg_size)
       ) 
    {
      this->dep_input_addrs.insert(*addr_iter);
    }
    else 
    {
      this->dep_other_addrs.insert(*addr_iter);
    }
  }
  
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
    if (!minimal_checkpoint_found) 
    {
      std::cerr << "Critical error: minimal checkpoint cannot found!\n";
      PIN_ExitApplication(0);
    }
  }
}

/*====================================================================================================================*/

branch::branch(const branch& other)
{
  this->addr              = other.addr;
  this->trace             = other.trace;
  this->br_taken          = other.br_taken;
  this->dep_input_addrs   = other.dep_input_addrs;
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
  this->dep_input_addrs   = other.dep_input_addrs;
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
  return (
          (this->addr = other.addr) && (this->trace.size() == other.trace.size()) && (this->br_taken == other.br_taken)
         ); 
}
