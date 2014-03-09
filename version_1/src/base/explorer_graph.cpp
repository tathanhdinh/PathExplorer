#include "explorer_graph.h"
#include "cond_direct_instruction.h"
#include "../operation/common.h"
#include "../util/stuffs.h"

#include <boost/graph/graphviz.hpp>
#include <boost/graph/filtered_graph.hpp>
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

//  tfm::format(std::cerr, "add vertex <%s: %s>\n", addrint_to_hexstring(ins_addr),
//              ins_at_addr[ins_addr]->disassembled_name);
  boost::add_vertex(ins_addr, internal_exp_graph);

  return;
}


/**
 * @brief verify if two path codes are equal
 */
static inline bool path_codes_are_equal(const path_code_t& x, const path_code_t& y)
{
  bool equality = false;
  path_code_t::const_iterator x_iter, y_iter;
  if (x.size() == y.size())
  {
    boost::tie(x_iter, y_iter) = std::mismatch(x.begin(), x.end(), y.begin());
    if ((x_iter == x.end()) && (y_iter == y.end())) equality = true;
  }
  return equality;
}


/**
 * @brief add an edge into the graph
 */
void explorer_graph::add_edge(ADDRINT ins_a_addr, ADDRINT ins_b_addr,
                              const path_code_t& edge_path_code,
                              const addrint_value_map_t& edge_addrs_values)
{
//  tfm::format(std::cerr, "add edge <%s -> %s>\n", addrint_to_hexstring(ins_a_addr),
//              addrint_to_hexstring(ins_b_addr));
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
  // iterate over out edges from a
  for (; out_edge_iter != last_out_edge_iter; ++out_edge_iter)
  {
    // verify if there exist an edge to b
    if (boost::target(*out_edge_iter, internal_exp_graph) == ins_b_desc)
    {
      // exist, then verify if the path code of this edge is also the input path code
      if (path_codes_are_equal(edge_path_code, internal_exp_graph[*out_edge_iter].first))
      {
        // yes, then add the selected values and addresses into the list of this edge
        if (!edge_addrs_values.empty())
          internal_exp_graph[*out_edge_iter].second.push_back(edge_addrs_values);
        break;
      }
    }
  }
  // there exists no such edge with the path code, then add it as a new edge
  if (out_edge_iter == last_out_edge_iter)
  {
    if (current_running_phase == rollbacking_phase)
    {
      bool last_code = edge_path_code.back();
      if (!last_code)
      {
        tfm::format(std::cerr, "fatal edge <%s->%s> %d\n", addrint_to_hexstring(ins_a_addr),
                    addrint_to_hexstring(ins_b_addr), edge_path_code.size());
        PIN_ExitApplication(1);
      }
    }

    addrint_value_maps_t edge_input_values;
    if (!edge_input_values.empty()) edge_input_values.push_back(edge_addrs_values);
    boost::add_edge(ins_a_desc, ins_b_desc, std::make_pair(edge_path_code, edge_input_values),
                    internal_exp_graph);
  }
//  std::cerr << "out add edge\n";
  return;
}


class exp_vertex_label_writer
{
public:
//  template<typename Vertex>
  void operator()(std::ostream& label, /*Vertex*/exp_vertex_desc vertex)
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
//  template<typename Edge>
  void operator()(std::ostream& label, /*Edge*/exp_edge_desc edge)
  {
    exp_edge current_edge = internal_exp_graph[edge];
    tfm::format(label, "[label=\"%s\"]", path_code_to_string(current_edge.first));
    return;
  }
};


class exp_vertex_isolated_prunner
{
public:
  bool operator()(exp_vertex_desc vertex) const
  {
    bool is_kept = true;
    boost::graph_traits<exp_graph>::out_edge_iterator out_e_iter, last_out_e_iter;
    boost::graph_traits<exp_graph>::in_edge_iterator in_e_iter, last_in_e_iter;

    // verify if the vertex is isolated (no in/out edges)
    boost::tie(out_e_iter, last_out_e_iter) = boost::out_edges(vertex, internal_exp_graph);
    boost::tie(in_e_iter, last_in_e_iter) = boost::in_edges(vertex, internal_exp_graph);

    // the isolated one (i.e. the corresponing instruction is never executed) will be prunned
    if ((out_e_iter == last_out_e_iter) && (in_e_iter == last_in_e_iter)) is_kept = false;

    return is_kept;
  }
};


/**
 * @brief prune isolated vertices
 */
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
    {
      // the isolated one (i.e. the corresponing instruction is never executed) will be prunned
      boost::remove_vertex(*v_iter, internal_exp_graph);
//      tfm::format(std::cerr, "prune <%d %s>\n", *v_iter,
//                  addrint_to_hexstring(internal_exp_graph[*v_iter]));
    }
  }
  return;
}


/**
 * @brief save the graph to a dot file
 *
 */
void explorer_graph::save_to_file(std::string filename)
{
//  prune_graph();
  exp_vertex_isolated_prunner vertex_prunner;
  boost::filtered_graph<exp_graph, boost::keep_all, exp_vertex_isolated_prunner>
      isolated_prunned_graph(internal_exp_graph, boost::keep_all(), vertex_prunner);

  std::ofstream output(filename.c_str(), std::ofstream::out | std::ofstream::trunc);
  boost::write_graphviz(output, /*internal_exp_graph*/isolated_prunned_graph, exp_vertex_label_writer(),
                        exp_edge_label_writer());
  return;
}


