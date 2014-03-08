#include "explorer_graph.h"
#include "cond_direct_instruction.h"
#include "../operation/common.h"
#include "../util/stuffs.h"

#include <boost/graph/graphviz.hpp>
#include <algorithm>

typedef ADDRINT                                             exp_vertex;
typedef std::pair<std::vector<bool>, addrint_value_maps_t>  exp_edge;
typedef boost::adjacency_list<boost::listS, boost::vecS,
                              boost::bidirectionalS,
                              exp_vertex, exp_edge>         exp_graph;

typedef boost::graph_traits<exp_graph>::vertex_descriptor   exp_vertex_desc;
typedef boost::graph_traits<exp_graph>::edge_descriptor     exp_edge_desc;
typedef boost::graph_traits<exp_graph>::vertex_iterator     exp_vertex_iter;
typedef boost::graph_traits<exp_graph>::edge_iterator       exp_edge_iter;

/*================================================================================================*/

static exp_graph            internal_exp_graph;
static ptr_explorer_graph_t single_graph_instance;
static addrint_value_map_t  default_addrs_values;

/*================================================================================================*/
/**
 * @brief private constructor of the explorer graph
 */
explorer_graph::explorer_graph()
{
  internal_exp_graph.clear();
}


/**
 * @brief return the single instanced of the graph
 */
ptr_explorer_graph_t explorer_graph::instance()
{
  if (!single_graph_instance) single_graph_instance.reset(new explorer_graph());
  return single_graph_instance;
}


/**
 * @brief add a vertex into the graph
 */
void explorer_graph::add_vertex(ADDRINT ins_addr)
{
//  exp_vertex_iter vertex_iter, last_vertex_iter;
//  boost::tie(vertex_iter, last_vertex_iter) = boost::vertices(internal_exp_graph);

//  // verify if the instruction exists in the graph
//  for (; vertex_iter != last_vertex_iter; ++vertex_iter)
//  {
//    if (internal_exp_graph[*vertex_iter] == ins) break;
//  }
//  if (vertex_iter == last_vertex_iter) boost::add_vertex(ins, internal_exp_graph);

  boost::add_vertex(ins_addr, internal_exp_graph);

  return;
}


/**
 * @brief verify if two path codes are equal
 */
static inline bool path_codes_are_equal(path_code_t& x, path_code_t& y)
{
  bool equality = false;
  path_code_t::iterator path_iter, last_path_iter;
  if (x.size() == y.size())
  {
    boost::tie(path_iter, last_path_iter) = std::mismatch(x.begin(), x.end(), y.begin());
    if (path_iter == last_path_iter) equality = true;
  }
  return equality;
}


/**
 * @brief add an edge into the graph
 */
void explorer_graph::add_edge(ADDRINT ins_a_addr, ADDRINT ins_b_addr, path_code_t& edge_path_code,
                              const addrint_value_map_t& edge_addrs_values)
{
  exp_vertex_desc ins_a_desc, ins_b_desc;
  exp_vertex_iter vertex_iter, last_vertex_iter;

  // look for descriptors of instruction a and b
  boost::tie(vertex_iter, last_vertex_iter) = boost::vertices(internal_exp_graph);
  for (; vertex_iter != last_vertex_iter; ++vertex_iter)
  {
    if (internal_exp_graph[*vertex_iter] == ins_a_addr) ins_a_desc = *vertex_iter;
    if (internal_exp_graph[*vertex_iter] == ins_b_addr) ins_b_desc = *vertex_iter;
  }

  boost::graph_traits<exp_graph>::out_edge_iterator out_edge_iter, last_out_edge_iter;
  boost::tie(out_edge_iter, last_out_edge_iter) = boost::out_edges(ins_a_desc, internal_exp_graph);
  // verify if there exist edges from a to b
  for (; out_edge_iter != last_out_edge_iter; ++out_edge_iter)
  {
    // exist, then verify if the path code of this edge is also the input path code
    if (boost::target(*out_edge_iter, internal_exp_graph) == ins_b_desc)
    {
      // yes, then add the selected values and addresses into the list of this edge
      if (path_codes_are_equal(edge_path_code, internal_exp_graph[*out_edge_iter].first))
      {
        internal_exp_graph[*out_edge_iter].second.push_back(edge_addrs_values); break;
      }
    }
  }
  // there exists no such edge with the path code, then add it as a new edge
  if (out_edge_iter == last_out_edge_iter)
  {
    addrint_value_maps_t edge_input_values; edge_input_values.push_back(edge_addrs_values);
    boost::add_edge(ins_a_desc, ins_b_desc, std::make_pair(edge_path_code, edge_input_values),
                    internal_exp_graph);
  }

  return;
}


class exp_vertex_label_writer
{
public:
  template<typename Vertex>
  void operator()(std::ostream& label, Vertex vertex)
  {
    exp_vertex vertex_ins_addr = internal_exp_graph[vertex];
    tfm::format(label, "[label=\"<%s: %s>\"]", addrint_to_hexstring(vertex_ins_addr),
                ins_at_addr[vertex_ins_addr]->disassembled_name);
    return;
  }
};


class exp_edge_label_writer
{
public:
  template<typename Edge>
  void operator()(std::ostream& label, Edge edge)
  {
    exp_edge current_edge = internal_exp_graph[edge];
    tfm::format(label, "[label=\"%d times\"]", current_edge.second.size());
    return;
  }
};


static inline void prune_graph()
{
  boost::graph_traits<exp_graph>::out_edge_iterator out_e_iter, last_out_e_iter;
  boost::graph_traits<exp_graph>::in_edge_iterator in_e_iter, last_in_e_iter;

  exp_vertex_iter v_iter, next_v_iter, last_v_iter;
  boost::tie(v_iter, last_v_iter) = boost::vertices(internal_exp_graph);
  for (next_v_iter = v_iter; v_iter != last_v_iter; v_iter = next_v_iter)
  {
    ++next_v_iter;
    // verify if the vertex is isolated (no in/out edges)
    boost::tie(out_e_iter, last_out_e_iter) = boost::out_edges(*v_iter, internal_exp_graph);
    boost::tie(in_e_iter, last_in_e_iter) = boost::in_edges(*v_iter, internal_exp_graph);
    if ((out_e_iter == last_out_e_iter) && (in_e_iter == last_in_e_iter))
      boost::remove_vertex(*v_iter, internal_exp_graph);
  }
}


void explorer_graph::save_to_file(std::string filename)
{
  std::ofstream output(filename.c_str(), std::ofstream::out | std::ofstream::trunc);
  boost::write_graphviz(output, internal_exp_graph, exp_vertex_label_writer(),
                        exp_edge_label_writer());
  return;
}


