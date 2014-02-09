#include "branch.h"

#include <algorithm>
#include <set>

#include <boost/graph/breadth_first_search.hpp>

//#include "instruction.h"
#include "variable.h"

/*================================================================================================*/

// static std::set<vdep_edge_desc> dep_edges;
//
// class dep_bfs_visitor : public boost::default_bfs_visitor
// {
// public:
//   template < typename Edge, typename Graph >
//   void examine_edge(Edge e, const Graph& g)
//   {
//     dep_edges.insert(e);
//   }
// };

/*====================================================================================================================*/

extern df_diagram                   dta_graph;
extern std::vector<ADDRINT>         explored_trace;
extern std::vector<ptr_checkpoint_t>  saved_ptr_checkpoints;

/*====================================================================================================================*/

// void dependent_mem_addrs(std::set<ADDRINT>& dep_mem_addrs)
// {
//   vdep_vertex_iter vertex_iter;
//   vdep_vertex_iter last_vertex_iter;
//
//   dep_bfs_visitor dep_vis;
//
//   boost::tie(vertex_iter, last_vertex_iter) = boost::vertices(dta_graph);
//   for (; vertex_iter != last_vertex_iter; ++vertex_iter)
//   {
//     if (dta_graph[*vertex_iter].type == MEM_VAR)
//     {
//       boost::breadth_first_search(dta_graph, *vertex_iter, boost::visitor(dep_vis));
//
//       std::set<vdep_edge_desc>::iterator edge_desc_iter = dep_edges.begin();
//       for (; edge_desc_iter != dep_edges.end(); ++edge_desc_iter)
//       {
//         if (dta_graph[*edge_desc_iter].second == explored_trace.size())
//         {
//           dep_mem_addrs.insert(dta_graph[*vertex_iter].mem);
//         }
//       }
//
//       std::set<vdep_edge_desc>().swap(dep_edges);
//     }
//   }
//
//   return;
// }

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
}

/*====================================================================================================================*/

branch::branch(const branch& other)
{
  this->addr              = other.addr;
  this->trace             = other.trace;
  this->br_taken          = other.br_taken;

  this->dep_input_addrs   = other.dep_input_addrs;
  this->dep_other_addrs   = other.dep_other_addrs;
  
  this->dep_backward_traces = other.dep_backward_traces;
  
  this->nearest_checkpoints = other.nearest_checkpoints;
  this->econ_execution_length = other.econ_execution_length;

  this->checkpoint        = other.checkpoint;
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
  this->dep_other_addrs   = other.dep_other_addrs;
  
  this->dep_backward_traces = other.dep_backward_traces;
  
  this->nearest_checkpoints = other.nearest_checkpoints;
  this->econ_execution_length = other.econ_execution_length;

  this->checkpoint        = other.checkpoint;
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
          (this->addr = other.addr) && 
          (this->trace.size() == other.trace.size()) && 
          (this->br_taken == other.br_taken)
         );
}
