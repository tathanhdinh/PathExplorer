#ifndef OPERAND_H
#define OPERAND_H

// these definitions are not necessary (defined already in the CMakeLists), add them just to make
// qt-creator parsing headers properly
#if defined(_WIN32) || defined(_WIN64)
#ifndef TARGET_IA32
#define TARGET_IA32
#endif
#ifndef HOST_IA32
#define HOST_IA32
#endif
#ifndef TARGET_WINDOWS
#define TARGET_WINDOWS
#endif
#ifndef USING_XED
#define USING_XED
#endif
#endif
#include <pin.H>

#include <string>
#include <list>

#if __cplusplus <= 199711L
#include <boost/shared_ptr.hpp>
#define pept boost
#else
#include <memory>
#define pept std
#endif

#include <boost/variant.hpp>
#include <boost/graph/adjacency_list.hpp>

class operand
{
public:
  std::string name;
  boost::variant<ADDRINT, REG>  value;
  
public:
  operand(ADDRINT mem_addr);
  operand(REG reg);
};

typedef pept::shared_ptr<operand>                           ptr_operand_t;
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
