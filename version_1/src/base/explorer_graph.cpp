#include "explorer_graph.h"
#include "cond_direct_instruction.h"
#include "../operation/common.h"

#include <algorithm>

typedef ptr_instruction_t                                   exp_vertex;
typedef std::pair<std::vector<bool>, addrint_value_maps_t>  exp_edge;
typedef boost::adjacency_list<boost::listS, boost::vecS,
                              boost::bidirectionalS,
                              exp_vertex, exp_edge>         exp_graph;

typedef boost::graph_traits<exp_graph>::vertex_descriptor   exp_vertex_desc;
typedef boost::graph_traits<exp_graph>::edge_descriptor     exp_edge_desc;
typedef boost::graph_traits<exp_graph>::vertex_iterator     exp_vertex_iter;

/*================================================================================================*/

static exp_graph            internal_exp_graph;
static ptr_explorer_graph_t single_graph_instance;

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
void explorer_graph::add_vertex(ptr_instruction_t& ins)
{
  exp_vertex_iter vertex_iter, last_vertex_iter;
  boost::tie(vertex_iter, last_vertex_iter) = boost::vertices(internal_exp_graph);

  // verify if the instruction exists in the graph
  for (; vertex_iter != last_vertex_iter; ++vertex_iter)
  {
    if (internal_exp_graph[*vertex_iter] == ins) break;
  }
  if (vertex_iter == last_vertex_iter) boost::add_vertex(ins, internal_exp_graph);

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
void explorer_graph::add_edge(ptr_instruction_t& ins_a, ptr_instruction_t& ins_b,
                              path_code_t& edge_path_code, addrint_value_map_t& edge_addrs_values)
{
  exp_vertex_desc ins_a_desc, ins_b_desc;
  exp_vertex_iter vertex_iter, last_vertex_iter;

  // look for descriptors of instruction a and b
  boost::tie(vertex_iter, last_vertex_iter) = boost::vertices(internal_exp_graph);
  for (; vertex_iter != last_vertex_iter; ++vertex_iter)
  {
    if (internal_exp_graph[*vertex_iter] == ins_a) ins_a_desc = *vertex_iter;
    if (internal_exp_graph[*vertex_iter] == ins_b) ins_b_desc = *vertex_iter;
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
    exp_vertex = internal_exp_graph[vertex];
    tfm::format(label, "[label=\"<%s: %s>\"]", addrint_to_hexstring(exp_vertex->address),
                exp_vertex->disassembled_name);
    return;
  }
};


class exp_edge_label_writer
{
public:
  template<typename Edge>
  void operator()(std::ostream& label, Edge edge)
  {
    exp_edge = internal_exp_graph[edge];
    tfm::format(label, "[label=\"<follow times: %d>\"]", exp_edge.second.size());
    return;
  }
};


