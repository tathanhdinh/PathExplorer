#ifndef OPERAND_H
#define OPERAND_H

#include "../parsing_helper.h"
#include <pin.H>

#include <memory>
#include <string>
#include <list>

#include <boost/variant.hpp>
#include <boost/graph/adjacency_list.hpp>

class operand
{
public:
  std::string name;
  boost::variant<ADDRINT, REG>  value;

  std::string exact_name;
  boost::variant<ADDRINT, REG>  exact_value;
  
public:
  operand(ADDRINT mem_addr);
  operand(REG reg);
};

typedef std::shared_ptr<operand>                            ptr_operand_t;
typedef std::set<ptr_operand_t>                             ptr_operand_set_t;
typedef ptr_operand_t                                       df_vertex;
typedef UINT32                                              df_edge;
typedef boost::adjacency_list<boost::listS, boost::vecS,
                              boost::bidirectionalS,
                              df_vertex, df_edge>           df_diagram;

typedef boost::graph_traits<df_diagram>::vertex_descriptor  df_vertex_desc;
typedef boost::graph_traits<df_diagram>::edge_descriptor    df_edge_desc;
typedef boost::graph_traits<df_diagram>::vertex_iterator    df_vertex_iter;
typedef boost::graph_traits<df_diagram>::edge_iterator      df_edge_iter;

typedef std::list<df_edge_desc>                             df_edge_desc_list;
typedef std::list<df_vertex_desc>                           df_vertex_desc_list;
typedef boost::unordered_map<df_vertex_desc,
                             df_edge_desc_list>             df_vertex_edges_dependency;
typedef boost::unordered_map<df_edge_desc,
                             df_vertex_desc_list>           df_edge_vertices_dependency;

typedef std::set<df_vertex_desc>                            df_vertex_desc_set;

#endif // OPERAND_H
