#include "exploring_graph.h"
#include "stuffs.h"

#include <vector>
#include <list>
#include <iostream>
#include <fstream>
#include <bitset>
#include <algorithm>

#include <boost/integer_traits.hpp>
#include <boost/cstdint.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/dynamic_bitset.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/format.hpp>

typedef boost::tuple<ADDRINT, std::string>                    exp_vertex;
typedef boost::tuple<next_exe_type, UINT32, UINT32, UINT32>   exp_edge;
typedef boost::adjacency_list<boost::listS, boost::vecS, 
                              boost::bidirectionalS, 
                              exp_vertex, exp_edge>           exp_graph;
                              
typedef boost::graph_traits<exp_graph>::vertex_descriptor     exp_vertex_desc;
typedef boost::graph_traits<exp_graph>::edge_descriptor       exp_edge_desc;
typedef boost::graph_traits<exp_graph>::vertex_iterator       exp_vertex_iter;
typedef boost::graph_traits<exp_graph>::edge_iterator         exp_edge_iter;


/*================================================================================================*/

extern std::map<UINT32, ptr_branch>   order_input_dep_ptr_branch_map;
extern std::map<UINT32, ptr_branch>   order_input_indep_ptr_branch_map;
extern std::map<UINT32, ptr_branch>   order_tainted_ptr_branch_map;

extern std::vector<ptr_checkpoint>    saved_ptr_checkpoints;
extern bool                           in_tainting;

extern std::map<ADDRINT, instruction> addr_ins_static_map;
extern std::bitset<30>                path_code;

/*================================================================================================*/

static exp_graph internal_exp_graph;

/*================================================================================================*/

exploring_graph::exploring_graph()
{
  internal_exp_graph.clear();
}

/*------------------------------------------------------------------------------------------------*/

static inline exp_vertex make_vertex(ADDRINT node_addr, UINT32 node_br_order)
{
  std::size_t first_input_dep_br_order = 0;
  
  if (!saved_ptr_checkpoints.empty() && !in_tainting) 
  {
    std::map<UINT32, ptr_branch>::iterator branch_iter;
    for (branch_iter = order_tainted_ptr_branch_map.begin(); 
        branch_iter != order_tainted_ptr_branch_map.end(); ++branch_iter) 
    {
      if (branch_iter->first < saved_ptr_checkpoints[0]->trace.size()) 
      {
        ++first_input_dep_br_order;
      }
    }
  }
  
  boost::dynamic_bitset<unsigned char> node_code(node_br_order);
  for (UINT32 bit_idx = 0; bit_idx < node_br_order; ++bit_idx) 
  {
    node_code[bit_idx] = path_code[bit_idx + first_input_dep_br_order];
  }
  std::string node_code_str; boost::to_string(node_code, node_code_str);
  return boost::make_tuple(node_addr, node_code_str);
}

/*================================================================================================*/

std::size_t exploring_graph::add_vertex(ADDRINT node_addr, UINT32 node_br_order)
{
  exp_vertex curr_vertex = make_vertex(node_addr, node_br_order);
  
  exp_vertex_desc added_node;
  exp_vertex_iter vertex_iter;
  exp_vertex_iter last_vertex_iter;
  boost::tie(vertex_iter, last_vertex_iter) = boost::vertices(internal_exp_graph);
  for (; vertex_iter != last_vertex_iter; ++vertex_iter)
  {
    if ((boost::get<0>(internal_exp_graph[*vertex_iter]) == boost::get<0>(curr_vertex)) && 
        (boost::get<1>(internal_exp_graph[*vertex_iter]) == boost::get<1>(curr_vertex)))
    {
      added_node = *vertex_iter;
      break;
    }
  }
  if (vertex_iter == last_vertex_iter) 
  {
    added_node = boost::add_vertex(curr_vertex, internal_exp_graph);
  }
  return added_node;
}

/*================================================================================================*/

static inline exp_vertex normalize_vertex(std::size_t offset, exp_vertex_desc vertex_desc) 
{
  std::string node_code_str = boost::get<1>(internal_exp_graph[vertex_desc]);
  boost::dynamic_bitset<unsigned char> node_code(node_code_str);
  boost::dynamic_bitset<unsigned char> node_new_code(node_code_str.size() - 
                                        std::min(node_code_str.size(), offset));
  std::size_t bit_idx;
  for (bit_idx = 0; bit_idx < node_new_code.size(); ++bit_idx) 
  {
    node_new_code[bit_idx] = node_code[bit_idx + offset];
  }
  
//     boost::dynamic_bitset<unsigned char> 
//       node_new_code(node_code_str, 0, 
//                     node_code_str.size() - std::min(node_code_str.size(), first_input_dep_br_order + 1));      
  std::string node_new_code_str; boost::to_string(node_new_code, node_new_code_str);
  ADDRINT node_addr = boost::get<0>(internal_exp_graph[vertex_desc]);
  return boost::make_tuple(node_addr, node_new_code_str);
}

void exploring_graph::add_edge(ADDRINT source_addr, UINT32 source_br_order, 
                               ADDRINT target_addr, UINT32 target_br_order,
                               next_exe_type direction, UINT32 nb_bits, UINT32 rb_length, UINT32 nb_rb)
{
  exp_vertex source_vertex = make_vertex(source_addr, source_br_order);
  exp_vertex target_vertex = make_vertex(target_addr, target_br_order);
  exp_vertex curr_vertex;
  
  exp_vertex_desc source_desc = boost::integer_traits<std::size_t>::const_max;
  exp_vertex_desc target_desc = source_desc;
  
  exp_vertex_iter vertex_iter;
  exp_vertex_iter last_vertex_iter;
  boost::tie(vertex_iter, last_vertex_iter) = boost::vertices(internal_exp_graph);
  for (; vertex_iter != last_vertex_iter; ++vertex_iter) 
  {
    curr_vertex = internal_exp_graph[*vertex_iter];
    
    if (source_desc == boost::integer_traits<std::size_t>::const_max) 
    {
      if ((boost::get<0>(curr_vertex) == boost::get<0>(source_vertex)) && 
          (boost::get<1>(curr_vertex) == boost::get<1>(source_vertex)))
      {
        source_desc = *vertex_iter;
      }
    }
    
    if (target_desc == boost::integer_traits<std::size_t>::const_max) 
    {
      if ((boost::get<0>(curr_vertex) == boost::get<0>(target_vertex)) && 
          (boost::get<1>(curr_vertex) == boost::get<1>(target_vertex))) 
      {
        target_desc = *vertex_iter;
      }
    }
    
    if ((source_desc != boost::integer_traits<std::size_t>::const_max) && 
        (target_desc != boost::integer_traits<std::size_t>::const_max)) 
    {
      break;
    }
  }
  
  if ((source_desc != boost::integer_traits<std::size_t>::const_max) && 
      (target_desc != boost::integer_traits<std::size_t>::const_max)) 
  {
    exp_edge_desc edge_desc;
    bool edge_existed;
    boost::tie(edge_desc, edge_existed) = boost::edge(source_desc, target_desc, internal_exp_graph);
    if (!edge_existed) 
    {
      boost::add_edge(source_desc, target_desc, 
                      boost::make_tuple(direction, nb_bits, rb_length, nb_rb), internal_exp_graph);
    }
  }
  else 
  {
//     std::string path_code_str; 
//     boost::to_string(path_code, path_code_str);
    
    if (source_desc == boost::integer_traits<std::size_t>::const_max) 
    {
      std::cout << "source not found\n";
    }
    
    if (target_desc == boost::integer_traits<std::size_t>::const_max) 
    {
      std::cout << "target not found\n";
    }
    
    std::cout << "edge not found\n";
//     PIN_ExitApplication(0);
  }
  
  std::cout << "path code " << path_code.to_string() << "\n";
  std::cout << remove_leading_zeros(StringFromAddrint(source_addr)) 
              << "  " << boost::get<1>(source_vertex) 
              << " " << source_br_order << "\n";
    std::cout << remove_leading_zeros(StringFromAddrint(target_addr)) 
              << "  " << boost::get<1>(target_vertex) 
              << "  " << target_br_order << "\n";
  
//   std::cout << nb_bits << "\n";
  return;
}

/*================================================================================================*/

void exploring_graph::normalize_orders_of_nodes(std::set<std::size_t>& added_nodes)
{
  std::cout << "normalize_orders_of_nodes\n";
  std::size_t first_input_dep_br_order = 0;
  std::map<UINT32, ptr_branch>::iterator branch_iter;
  for (branch_iter = order_tainted_ptr_branch_map.begin(); 
       branch_iter != order_tainted_ptr_branch_map.end(); ++branch_iter) 
  {
    if (branch_iter->first < saved_ptr_checkpoints[0]->trace.size()) 
    {
      ++first_input_dep_br_order;
    }
  
//     if (order_input_dep_ptr_branch_map.find(branch_iter->first) != 
//         order_input_dep_ptr_branch_map.end()) 
//     {
//       break;
//     }
//     else 
//     {
//       ++first_input_dep_br_order;
//     }
  }
  
  std::set<std::size_t>::iterator added_node_iter;
  for (added_node_iter = added_nodes.begin(); added_node_iter != added_nodes.end(); 
       ++added_node_iter) 
  {
//     std::string node_code_str = boost::get<1>(internal_exp_graph[*added_node_iter]);
//     boost::dynamic_bitset<unsigned char> node_code(node_code_str);
//     boost::dynamic_bitset<unsigned char> node_new_code(node_code_str.size() - 
//                                           std::min(node_code_str.size(), first_input_dep_br_order));
//     std::size_t bit_idx;
//     for (bit_idx = 0; bit_idx < node_new_code.size(); ++bit_idx) 
//     {
//       node_new_code[bit_idx] = node_code[bit_idx + first_input_dep_br_order];
//     }
//     
// //     boost::dynamic_bitset<unsigned char> 
// //       node_new_code(node_code_str, 0, 
// //                     node_code_str.size() - std::min(node_code_str.size(), first_input_dep_br_order + 1));      
//     std::string node_new_code_str; boost::to_string(node_new_code, node_new_code_str);
//     ADDRINT node_addr = boost::get<0>(internal_exp_graph[*added_node_iter]);
//     
//     internal_exp_graph[*added_node_iter] = boost::make_tuple(node_addr, node_new_code_str);
    internal_exp_graph[*added_node_iter] = normalize_vertex(first_input_dep_br_order, *added_node_iter);
//     std::cout << remove_leading_zeros(StringFromAddrint(node_addr)) 
//               << " before: " << node_code_str << " after: " << node_new_code_str << "\n";
    
  }
  
//   std::set<UINT32> input_dep_br_orders;
//   UINT32 br_order = 0;
//   
//   for (branch_iter = order_tainted_ptr_branch_map.begin(); 
//        branch_iter != order_tainted_ptr_branch_map.end(); ++branch_iter) 
//   {
//     if (order_input_dep_ptr_branch_map.find(branch_iter->first) != 
//         order_input_dep_ptr_branch_map.end()) 
//     {
//       input_dep_br_orders.insert(br_order);
//       break;
//     }
//     ++br_order;
//   }
//   
//   std::set<std::size_t>::iterator added_node_iter;
//   for (added_node_iter = added_nodes.begin(); added_node_iter != added_nodes.end(); 
//        ++added_node_iter) 
//   {
//     boost::dynamic_bitset<unsigned char> node_code = 
//       boost::dynamic_bitset<unsigned char>(boost::get<1>(internal_exp_graph[*added_node_iter]));
// 
//     std::set<UINT32> node_code_br_orders(boost::counting_iterator<UINT32>(0), 
//                                          boost::counting_iterator<UINT32>(node_code.size()));
//     std::set<UINT32> node_code_input_dep_br_orders;
//     std::set_intersection(node_code_br_orders.begin(), node_code_br_orders.end(), 
//                           input_dep_br_orders.begin(), input_dep_br_orders.end(), 
//                           std::inserter(node_code_input_dep_br_orders, node_code_input_dep_br_orders.begin()));
//     
//     boost::dynamic_bitset<unsigned char> node_new_code(node_code_input_dep_br_orders.size());
//     std::set<UINT32>::iterator br_order_iter;
//     UINT32 bit_idx = 0;
//     for (br_order_iter = node_code_input_dep_br_orders.begin(); 
//          br_order_iter != node_code_input_dep_br_orders.end(); ++br_order_iter) 
//     {
//       node_new_code[bit_idx] = node_code[*br_order_iter];
//       ++bit_idx;
//     }
//     
//     ADDRINT node_addr = boost::get<0>(internal_exp_graph[*added_node_iter]);
//     std::string node_new_code_str; boost::to_string(node_new_code, node_new_code_str);
//     internal_exp_graph[*added_node_iter] = boost::make_tuple(node_addr, node_new_code_str);
//   }
  
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
    exp_vertex curr_vertex = graph[v];
    out << boost::format("[label=\"<%s@%s,%s>\"]") 
            % remove_leading_zeros(StringFromAddrint(boost::get<0>(curr_vertex))) 
            % boost::get<1>(curr_vertex)
            % addr_ins_static_map[boost::get<0>(curr_vertex)].disass;
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




