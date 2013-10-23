#ifndef VARIABLE_H
#define VARIABLE_H

#include <pin.H>
// #include <z3++.h>

#include <set>
#include <map>
#include <vector>
#include <list>
#include <string>
#include <iostream>
#include <utility>

#include <boost/shared_ptr.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/functional/hash.hpp>
#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>

#include "instruction.h"

class variable;
class variable_hash;

typedef boost::unordered_set<variable, variable_hash>       var_set;

typedef variable                                            vdep_vertex;
typedef std::pair<ADDRINT, UINT32>                          vdep_edge;

typedef boost::adjacency_list<boost::listS, boost::vecS, 
                              boost::bidirectionalS, 
                              vdep_vertex, vdep_edge>       vdep_graph;

// typedef boost::adjacency_list<boost::listS, boost::listS, 
//                               boost::bidirectionalS, 
//                               vdep_vertex, vdep_edge>       vdep_graph;
                                                         
typedef boost::graph_traits<vdep_graph>::vertex_descriptor  vdep_vertex_desc;
typedef boost::graph_traits<vdep_graph>::edge_descriptor    vdep_edge_desc;
typedef boost::graph_traits<vdep_graph>::vertex_iterator    vdep_vertex_iter;
typedef boost::graph_traits<vdep_graph>::edge_iterator      vdep_edge_iter;
typedef boost::graph_traits<vdep_graph>::out_edge_iterator  vdep_out_edge_iter;
typedef boost::graph_traits<vdep_graph>::in_edge_iterator   vdep_in_edge_iter;

// typedef std::vector<vdep_edge_desc>                         vdep_path
typedef std::list<vdep_edge_desc>                           vdep_edge_desc_list;
typedef std::list<vdep_vertex_desc>                         vdep_vertex_desc_list;
typedef boost::unordered_map<vdep_vertex_desc, 
                             vdep_edge_desc_list>           vdep_vertex_edges_dependency;
typedef boost::unordered_map<vdep_edge_desc, 
                             vdep_vertex_desc_list>         vdep_edge_vertices_dependency;

typedef enum 
{
  MEM_VAR = 0,
  REG_VAR = 1, 
  IMM_VAR = 2
} var_type;

class variable
{
public:
  variable();
  variable(ADDRINT new_mem);
  variable(REG new_reg);
  variable(UINT32 new_imm);
  variable(const variable& var);
  variable& operator=(const variable& var);
  
  std::string name;
  var_type    type;
  
  REG         reg;
  
  ADDRINT     mem;
//   UINT32      mem_size;
  
  UINT32      imm;
};

inline bool operator==(const variable& var_a, const variable& var_b)
{
 return ((var_a.name == var_b.name) && (var_a.type == var_b.type));
}

class variable_hash
{
public:
  std::size_t operator()(variable const& var) const
  {
    boost::hash<std::string> str_hash;
    return str_hash(var.name);
  }
};

/*====================================================================================================================*/

extern std::map< ADDRINT, 
                 instruction > addr_ins_static_map;

extern ADDRINT received_msg_addr;
extern UINT32  received_msg_size;

/*====================================================================================================================*/

class vertex_label_writer
{
public:
  vertex_label_writer(vdep_graph& g) : graph(g) 
  {
  }
  
  template <typename Vertex>
  void operator()(std::ostream& out, Vertex v) 
  {
    vdep_vertex vertex_var(graph[v]);
    std::string vertex_label;
        
    if ((vertex_var.type == MEM_VAR) && 
        (received_msg_addr <= vertex_var.mem) && 
        (vertex_var.mem < received_msg_addr + received_msg_size)) 
    {
      vertex_label = "[color=blue, style=filled, label=\"";
    }
    else 
    {
      vertex_label = "[color=black, label=\"";
    }
    vertex_label += vertex_var.name;
    vertex_label += "\"]";
    
//     std::cerr << vertex_label << "\n";
    out << vertex_label;
//     out << "[label=\"" << vertex_var.name << "\"]";
  }
  
private:
  vdep_graph graph;
};

/*====================================================================================================================*/

class edge_label_writer
{
public:
  edge_label_writer(vdep_graph& g) : graph(g)
  {
  }
  
  template <typename Edge>
  void operator()(std::ostream& out, Edge edge) 
  {
    vdep_edge ve = graph[edge];
    
    out << "[label=\"(" << decstr(ve.second) << ") " << addr_ins_static_map[ve.first].disass << "\"]";
  }
  
private:
  vdep_graph graph;
};

/*====================================================================================================================*/

// void linear_traversing(vdep_vertex_desc const& in_root_vertex, 
//                        vdep_graph const& in_graph, 
//                        vdep_edge_desc_list& out_path);
// 
// void mem_branch_dependency(vdep_graph const& in_graph, 
//                            std::list< std::pair<vdep_vertex_desc, 
//                                                 vdep_edge_desc_list> >& out_ve_dependencies);
// 
// void branch_mem_dependency(vdep_graph const& in_graph, 
//                            std::list< std::pair<vdep_edge_desc, 
//                                                 vdep_vertex_desc_list> >& out_ev_dependencies);

#endif // VARIABLE_H
