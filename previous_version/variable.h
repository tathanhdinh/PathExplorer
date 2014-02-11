#ifndef VARIABLE_H
#define VARIABLE_H

#include <pin.H>

#include <map>
#include <list>
#include <string>
#include <utility>

#include <boost/format.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/functional/hash.hpp>
#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>

#include "instruction.h"

/*================================================================================================*/

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

#endif // VARIABLE_H
