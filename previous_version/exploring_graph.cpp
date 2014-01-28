#include "exploring_graph.h"
#include "stuffs.h"

#include <vector>
#include <list>
#include <iostream>
#include <fstream>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/format.hpp>

typedef ADDRINT                                               exp_vertex;
typedef boost::tuple<next_exe_type, UINT32, UINT32, UINT32>   exp_edge;
typedef boost::adjacency_list<boost::listS, boost::vecS, 
                              boost::bidirectionalS, 
                              exp_vertex, exp_edge>           exp_graph;
                              
typedef boost::graph_traits<exp_graph>::vertex_descriptor     exp_vertex_desc;
typedef boost::graph_traits<exp_graph>::edge_descriptor       exp_edge_desc;
typedef boost::graph_traits<exp_graph>::vertex_iterator       exp_vertex_iter;
typedef boost::graph_traits<exp_graph>::edge_iterator         exp_edge_iter;


/*================================================================================================*/

extern std::map<ADDRINT, instruction> addr_ins_static_map;
static exp_graph internal_exp_graph;

/*================================================================================================*/

exploring_graph::exploring_graph()
{
  internal_exp_graph.clear();
}

void exploring_graph::add_node(ADDRINT node_addr)
{
  exp_vertex_iter vertex_iter;
  exp_vertex_iter last_vertex_iter;
  boost::tie(vertex_iter, last_vertex_iter) = boost::vertices(internal_exp_graph);
  for (; vertex_iter != last_vertex_iter; ++vertex_iter)
  {
    if (internal_exp_graph[*vertex_iter] == node_addr) 
    {
      break;
    }
  }
  if (vertex_iter == last_vertex_iter) 
  {
    boost::add_vertex(node_addr, internal_exp_graph);
  }
  return;
}

void exploring_graph::add_edge(ADDRINT source_addr, ADDRINT target_addr, next_exe_type direction, 
                               UINT32 nb_bits, UINT32 rb_length, UINT32 nb_rb)
{
  exp_vertex_desc source_desc = 0;
  exp_vertex_desc target_desc = 0;
  
  exp_vertex_iter vertex_iter;
  exp_vertex_iter last_vertex_iter;
  boost::tie(vertex_iter, last_vertex_iter) = boost::vertices(internal_exp_graph);
  for (; vertex_iter != last_vertex_iter; ++vertex_iter) 
  {
    if (internal_exp_graph[*vertex_iter] == source_addr) 
    {
      source_desc = *vertex_iter;
    }
    if (internal_exp_graph[*vertex_iter] == target_addr) 
    {
      target_desc = *vertex_iter;
    }
    
    if ((source_desc != 0) && (target_desc != 0)) 
    {
      break;
    }
  }
  
  exp_edge_desc edge_desc;
  bool edge_existed;
  boost::tie(edge_desc, edge_existed) = boost::edge(source_desc, target_desc, internal_exp_graph);
  if (!edge_existed) 
  {
    boost::add_edge(source_desc, target_desc, 
                    boost::make_tuple(direction, nb_bits, rb_length, nb_rb), internal_exp_graph);
  }
//   std::cout << nb_bits << "\n";
  return;
}

/*================================================================================================*/

class exp_vertex_label_writer
{
public:
  exp_vertex_label_writer(exp_graph& g) : graph(g) 
  {
  }
  
  template <typename Vertex>
  void operator()(std::ostream& out, Vertex v) 
  {
    exp_vertex vertex_addr = graph[v];
    out << boost::format("[label=\"<%s,%s>\"]") 
            % remove_leading_zeros(StringFromAddrint(vertex_addr)) 
            % addr_ins_static_map[vertex_addr].disass;
//     std::string vertex_label;
//     vertex_label = "<" + remove_leading_zeros(StringFromAddrint(vertex_addr)) + ", " + 
//                    addr_ins_static_map[vertex_addr].disass + ">";
      
//     exp_vertex vertex_var(graph[v]);
//     std::string vertex_label;
    
    
        
//     if ((vertex_var.type == MEM_VAR) && (received_msg_addr <= vertex_var.mem) && 
//         (vertex_var.mem < received_msg_addr + received_msg_size)) 
//     {
//       vertex_label = "[color=blue, style=filled, label=\"";
//     }
//     else 
//     {
//       vertex_label = "[color=black, label=\"";
//     }
//     vertex_label += vertex_var.name;
//     vertex_label += "\"]";
    
//     out << vertex_label;
  }
  
private:
  exp_graph graph;
};

class exp_edge_label_writer
{
public:
  exp_edge_label_writer(exp_graph& g) : graph(g)
  {
  }
  
  template <typename Edge>
  void operator()(std::ostream& out, Edge edge) 
  {
//     exp_edge ve = graph[edge];
    
//     out << "[label=\"(" << decstr(ve.second) << ") " << addr_ins_static_map[ve.first].disass << "\"]";
    
    next_exe_type direction; UINT32 nb_bits; UINT32 rb_length; UINT32 nb_rb;
    boost::tie(direction, nb_bits, rb_length, nb_rb) = graph[edge];
    if (direction == ROLLBACK) 
    {
      out << boost::format("[color=red, label=\"<%d,%d,%d,%d>\"]") 
              % direction % nb_bits % rb_length % nb_rb;
    }
    else 
    {
      out << boost::format("[label=\"<%d,%d,%d,%d>\"]") 
              % direction % nb_bits % rb_length % nb_rb;
    }
    
  }
  
private:
  exp_graph graph;
};

void exploring_graph::print_to_file(const std::string& filename)
{
  std::ofstream output_file(filename.c_str(), std::ofstream::out | std::ofstream::trunc);
  boost::write_graphviz(output_file, internal_exp_graph, 
                        exp_vertex_label_writer(internal_exp_graph), 
                        exp_edge_label_writer(internal_exp_graph));
  return;
}




