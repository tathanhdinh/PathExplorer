#ifndef VARIABLE_H
#define VARIABLE_H

#include <pin.H>

#include <set>
#include <map>
#include <vector>
#include <list>
#include <string>
#include <iostream>
#include <utility>

#include <boost/format.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/functional/hash.hpp>
#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>

#include "instruction.h"

/*================================================================================================*/

typedef enum 
{
  MEM_VAR = 0,
  REG_VAR = 1, 
  IMM_VAR = 2,
} var_type;

class variable
{
public:
  variable();
  variable(ADDRINT new_mem);
  variable(REG new_reg);
  //variable(UINT32 new_imm);
  variable(const variable& var);
  
  variable& operator=(const variable& var);
  
  std::string name;
  var_type    type;
  
  REG         reg;
  ADDRINT     mem;
  UINT32      imm;
};

inline bool operator==(const variable& var_a, const variable& var_b)
{
  return ((var_a.type == var_b.type) && (var_a.name == var_b.name));
}

/*================================================================================================*/

class variable_hash
{
public:
  std::size_t operator()(variable const& var) const
  {
    boost::hash<std::string> str_hash;
    return str_hash(var.name);
  }
};

/*================================================================================================*/

typedef boost::unordered_set<variable, variable_hash>       var_set;

//typedef variable                                            dataflow_vertex;
//typedef std::pair<ADDRINT, UINT32>                          dataflow_edge;
typedef ptr_operand_t                                       df_vertex;
typedef UINT32                                              df_edge;
typedef boost::adjacency_list<boost::listS, boost::vecS, 
                              boost::bidirectionalS, 
                              df_vertex, df_edge>           df_diagram;
                                                         
typedef boost::graph_traits<df_diagram>::vertex_descriptor  df_vertex_desc;
typedef boost::graph_traits<df_diagram>::edge_descriptor    df_edge_desc;
typedef boost::graph_traits<df_diagram>::vertex_iterator    df_vertex_iter;
typedef boost::graph_traits<df_diagram>::edge_iterator      df_edge_iter;
//typedef boost::graph_traits<df_diagram>::out_edge_iterator  vdep_out_edge_iter;
//typedef boost::graph_traits<df_diagram>::in_edge_iterator   vdep_in_edge_iter;

typedef std::list<df_edge_desc>                             df_edge_desc_list;
typedef std::list<df_vertex_desc>                           df_vertex_desc_list;
typedef boost::unordered_map<df_vertex_desc,
                             df_edge_desc_list>             df_vertex_edges_dependency;
typedef boost::unordered_map<df_edge_desc,
                             df_vertex_desc_list>           df_edge_vertices_dependency;
                             
//typedef boost::unordered_set<vdep_vertex_desc>                  vdep_vertex_desc_set;
typedef std::set<df_vertex_desc>                            df_vertex_desc_set;

/*================================================================================================*/

extern std::map<ADDRINT, ptr_instruction_t> addr_ins_static_map;
extern std::map<UINT32, ptr_instruction_t>  order_ins_dynamic_map;

extern ADDRINT                        received_msg_addr;
extern UINT32                         received_msg_size;

/*================================================================================================*/

class vertex_label_writer
{
public:
  vertex_label_writer(df_diagram& g) : graph(g)
  {
  }
  
  template <typename Vertex>
  void operator()(std::ostream& out, Vertex v) 
  {
    df_vertex current_vertex = graph[v];
    if ((current_vertex->value.type() == typeid(ADDRINT)) &&
        (received_msg_addr <= boost::get<ADDRINT>(current_vertex->value)) &&
        (boost::get<ADDRINT>(current_vertex->value) < received_msg_addr + received_msg_size))
    {
      out << boost::format("[color=blue,style=filled,label=\"%s\"]") % current_vertex->name;
    }
    else
    {
      out << boost::format("[color=black,label=\"%s\"]") % current_vertex->name;
    }

//    dataflow_vertex vertex_var(graph[v]);
//    std::string vertex_label;

        
//    if ((vertex_var.type == MEM_VAR) && (received_msg_addr <= vertex_var.mem) &&
//        (vertex_var.mem < received_msg_addr + received_msg_size))
//    {
//      vertex_label = "[color=blue, style=filled, label=\"";
//    }
//    else
//    {
//      vertex_label = "[color=black, label=\"";
//    }
//    vertex_label += vertex_var.name;
//    vertex_label += "\"]";
    
//    out << vertex_label;
  }
  
private:
  df_diagram graph;
};

/*================================================================================================*/

class edge_label_writer
{
public:
  edge_label_writer(df_diagram& g) : graph(g)
  {
  }
  
  template <typename Edge>
  void operator()(std::ostream& out, Edge edge) 
  {
    df_edge current_edge = graph[edge];
    out << boost::format("[label=\"%s: %s\"]")
           % current_edge % order_ins_dynamic_map[current_edge]->disassembled_name;
    
//    out << "[label=\"(" << decstr(ve.second) << ") " << addr_ins_static_map[ve.first].disassembled_name << "\"]";
  }
  
private:
  df_diagram graph;
};

#endif // VARIABLE_H
