#include "explorer_graph.h"
#include "cond_direct_instruction.h"
#include "../common.h"
#include "../util/stuffs.h"

#include <boost/graph/graphviz.hpp>
#include <boost/graph/filtered_graph.hpp>
#include <boost/graph/copy.hpp>
#include <algorithm>
#include <tuple>

typedef ADDRINT                                               exp_vertex;
typedef std::pair<std::vector<bool>, addrint_value_maps_t>    exp_edge;
typedef boost::adjacency_list<boost::listS, boost::vecS,
                              boost::bidirectionalS,
                              exp_vertex, exp_edge>           exp_graph_t;
typedef boost::graph_traits<exp_graph_t>::vertex_descriptor   exp_vertex_desc;
typedef boost::graph_traits<exp_graph_t>::edge_descriptor     exp_edge_desc;
typedef boost::graph_traits<exp_graph_t>::vertex_iterator     exp_vertex_iter;
typedef boost::graph_traits<exp_graph_t>::edge_iterator       exp_edge_iter;

typedef ptr_instruction_t                                     exp_tree_vertex_t;
typedef path_code_t                                           exp_tree_edge_t;
typedef boost::adjacency_list<boost::listS, boost::vecS,
                              boost::bidirectionalS,
                              exp_tree_vertex_t,
                              exp_tree_edge_t>                exp_tree_t;
typedef boost::graph_traits<exp_tree_t>::vertex_descriptor    exp_tree_vertex_desc;
typedef boost::graph_traits<exp_tree_t>::edge_descriptor      exp_tree_edge_desc;
typedef boost::graph_traits<exp_tree_t>::vertex_iterator      exp_tree_vertex_iter;
typedef boost::graph_traits<exp_tree_t>::edge_iterator        exp_tree_edge_iter;

typedef std::pair<exp_tree_edge_t, exp_tree_vertex_t>         exp_tree_with_root_t;


/*================================================================================================*/

static exp_graph_t            internal_exp_graph;
static exp_graph_t            internal_exp_graph_simple;
static ptr_explorer_graph_t single_graph_instance;
static addrint_value_map_t  default_addrs_values;

static exp_tree_t             internal_exp_tree;
static exp_tree_t             internal_exp_cfi_tree;

/*================================================================================================*/
/**
 * @brief private constructor of the explorer graph
 */
//explorer_graph::explorer_graph(private_construct_key)
//{
//  internal_exp_graph.clear(); internal_exp_graph_simple.clear(); internal_exp_tree.clear();
//}


/**
 * @brief return the single instanced of the graph
 */
auto explorer_graph::instance() -> ptr_explorer_graph_t
{
//  if (!single_graph_instance) single_graph_instance.reset(new explorer_graph());
  if (!single_graph_instance)
  {
    single_graph_instance = std::make_shared<explorer_graph>(private_construct_key());
    internal_exp_graph.clear(); internal_exp_graph_simple.clear(); internal_exp_tree.clear();
  }
  return single_graph_instance;
}


/**
 * @brief add a vertex into the explorer graph
 */
auto explorer_graph::add_vertex(ADDRINT ins_addr) -> void
{
  boost::add_vertex(ins_addr, internal_exp_graph);
  boost::add_vertex(ins_addr, internal_exp_graph_simple);

  return;
}


/**
 * @brief add a vertex into the explorer tree
 */
auto explorer_graph::add_vertex(ptr_instruction_t ins) -> void
{
  boost::add_vertex(ins, internal_exp_tree);
  return;
}


/**
 * @brief verify if two path codes are equal
 */
//static inline bool path_codes_are_equal(const path_code_t& x, const path_code_t& y)
//{
//  if (x.size() == y.size())
//  {
//    path_code_t::const_iterator x_iter, y_iter;
//    std::tie(x_iter, y_iter) = std::mismatch(x.begin(), x.end(), y.begin());
//    if ((x_iter == x.end()) && (y_iter == y.end())) return true;
//  }
//  return false;
//}


/**
 * @brief add an edge into the explorer graph
 */
auto explorer_graph::add_edge(ADDRINT ins_a_addr, ADDRINT ins_b_addr,
                              const path_code_t& edge_path_code,
                              const addrint_value_map_t& edge_addrs_values) -> void
{
//  tfm::format(std::cerr, "add edge <%s -> %s>\n", addrint_to_hexstring(ins_a_addr),
//              addrint_to_hexstring(ins_b_addr));
  exp_vertex_desc ins_a_desc, ins_b_desc;
  exp_vertex_iter vertex_iter, last_vertex_iter;

  // look for descriptors of instruction a and b
  std::tie(vertex_iter, last_vertex_iter) = boost::vertices(internal_exp_graph);
  for (; vertex_iter != last_vertex_iter; ++vertex_iter)
  {
    if (internal_exp_graph[*vertex_iter] == ins_a_addr) ins_a_desc = *vertex_iter;
    if (internal_exp_graph[*vertex_iter] == ins_b_addr) ins_b_desc = *vertex_iter;
  }

  boost::graph_traits<exp_graph_t>::out_edge_iterator out_edge_iter, last_out_edge_iter;
  std::tie(out_edge_iter, last_out_edge_iter) = boost::out_edges(ins_a_desc, internal_exp_graph);

  // iterate over out edges from a
  for (; out_edge_iter != last_out_edge_iter; ++out_edge_iter)
  {
    // verify if there exist an edge to b
    if (boost::target(*out_edge_iter, internal_exp_graph) == ins_b_desc)
    {
      // lambda to verify if two path codes are equal
      auto path_codes_are_equal = [](const path_code_t& x, const path_code_t& y) -> bool
      {
        if (x.size() == y.size())
        {
          path_code_t::const_iterator x_iter, y_iter;
          std::tie(x_iter, y_iter) = std::mismatch(x.begin(), x.end(), y.begin());
          if ((x_iter == x.end()) && (y_iter == y.end())) return true;
        }
        return false;
      };
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
    if ((current_running_phase == rollbacking_phase) && !edge_path_code.empty() &&
        !edge_path_code.back())
    {
#if !defined(NDEBUG)
      tfm::format(std::cerr, "fatal: edge <%s->%s> %d\n", addrint_to_hexstring(ins_a_addr),
                  addrint_to_hexstring(ins_b_addr), edge_path_code.size());
#endif
      PIN_ExitApplication(1);
    }
    else
    {
      addrint_value_maps_t edge_input_values;
      if (!edge_input_values.empty()) edge_input_values.push_back(edge_addrs_values);
      boost::add_edge(ins_a_desc, ins_b_desc, std::make_pair(edge_path_code, edge_input_values),
                      internal_exp_graph);
    }
  }

  // add a edge with empty label into the simple graph

  // look for descriptors of instruction a and b
  std::tie(vertex_iter, last_vertex_iter) = boost::vertices(internal_exp_graph_simple);
  for (; vertex_iter != last_vertex_iter; ++vertex_iter)
  {
    if (internal_exp_graph_simple[*vertex_iter] == ins_a_addr) ins_a_desc = *vertex_iter;
    if (internal_exp_graph_simple[*vertex_iter] == ins_b_addr) ins_b_desc = *vertex_iter;
  }

  // verify if the edge from a to b exists
  bool edge_exists;
  std::tie(std::ignore, edge_exists) = boost::edge(ins_a_desc, ins_b_desc,
                                                   internal_exp_graph_simple);
  // no, then add a new edge with empty label
  if (!edge_exists) boost::add_edge(ins_a_desc, ins_b_desc,
                                    std::make_pair(std::vector<bool>(), addrint_value_maps_t()),
                                    internal_exp_graph_simple);
  return;
}


/**
 * @brief add an edge into the explorer tree
 */
auto explorer_graph::add_edge(ptr_instruction_t ins_a, ptr_instruction_t ins_b,
                              const path_code_t& edge_path_code) -> void
{
  exp_tree_vertex_desc vertex_a_desc, vertex_b_desc;
  exp_tree_vertex_iter first_vertex_tree_iter, last_vertex_tree_iter;

  std::tie(first_vertex_tree_iter, last_vertex_tree_iter) = boost::vertices(internal_exp_tree);
  std::for_each(first_vertex_tree_iter, last_vertex_tree_iter, [&](exp_tree_vertex_desc vertex_desc)
  {
    if (internal_exp_tree[vertex_desc] == ins_a) vertex_a_desc = vertex_desc;
    if (internal_exp_tree[vertex_desc] == ins_b) vertex_b_desc = vertex_desc;
  });
  boost::add_edge(vertex_a_desc, vertex_b_desc, edge_path_code, internal_exp_tree);

  return;
}


static auto nearest_cfi_vertex (exp_tree_vertex_desc start_vertex) -> exp_tree_vertex_desc
{
//  exp_tree_vertex_desc result;

  // verify if the start vertex is a cfi
  if (internal_exp_tree[start_vertex]->is_cond_direct_cf &&
      !std::static_pointer_cast<cond_direct_instruction>(
        internal_exp_tree[start_vertex])->input_dep_addrs.empty()) return start_vertex;
  else
  {
    // not a cfi, then its neighbor number is 0 or 1
//    auto start_vertex_desc = look_for_vertex(start_vertex, internal_exp_tree);
    if (boost::out_degree(start_vertex, internal_exp_tree) == 1)
      return nearest_cfi_vertex(boost::target(*(boost::out_edges(start_vertex,
                                                                 internal_exp_tree).first),
                                              internal_exp_tree));
  }

  return boost::graph_traits<exp_tree_t>::null_vertex();
}


/**
 * @brief extract CFI tree from the explorer tree
 */
auto explorer_graph::extract_cfi_tree () -> void
{
  auto look_for_vertex = [&](exp_tree_vertex_t vertex, exp_tree_t tree) -> exp_tree_vertex_desc
  {
    exp_tree_vertex_desc result_vertex_desc;

    exp_tree_vertex_iter first_vertex_iter, last_vertex_iter;
    std::tie(first_vertex_iter, last_vertex_iter) = boost::vertices(tree);
    std::any_of(first_vertex_iter, last_vertex_iter, [&](exp_tree_vertex_desc vertex_desc) -> bool
    {
      if (tree[vertex_desc] == vertex)
      {
        result_vertex_desc = vertex_desc; return true;
      }
      else return false;
    });

    return result_vertex_desc;
  };

  auto next_cfi_following_path =
      [&](exp_tree_vertex_desc current_cfi_desc, const path_code_t& path) -> exp_tree_vertex_desc
  {
    auto result = boost::graph_traits<exp_tree_t>::null_vertex();
    if (current_cfi_desc != boost::graph_traits<exp_tree_t>::null_vertex())
    {
//      auto current_cfi_desc = look_for_vertex(current_cfi, internal_exp_tree);
      std::any_of(boost::out_edges(current_cfi_desc, internal_exp_tree).first,
                  boost::out_edges(current_cfi_desc, internal_exp_tree).second,
                  [&](exp_tree_edge_desc out_edge) -> bool
      {
        if (internal_exp_tree[out_edge] == path)
        {
          result = nearest_cfi_vertex(boost::target(out_edge, internal_exp_tree));
          return true;
        }
        else return false;
      });
    }

    return result;
  };

  // add all vertices which are of type CFI in the explorer tree into the explorer cfi tree
  std::for_each(boost::vertices(internal_exp_tree).first, boost::vertices(internal_exp_tree).second,
                [&](exp_tree_vertex_desc vertex_desc)
  {
    if (internal_exp_tree[vertex_desc]->is_cond_direct_cf &&
        !std::static_pointer_cast<cond_direct_instruction>(
          internal_exp_tree[vertex_desc])->input_dep_addrs.empty())
    {
      boost::add_vertex(internal_exp_tree[vertex_desc], internal_exp_cfi_tree);
    }
  });


  std::for_each(boost::vertices(internal_exp_tree).first, boost::vertices(internal_exp_tree).second,
                [&](exp_tree_vertex_desc vertex_desc)
  {
    if (internal_exp_tree[vertex_desc]->is_cond_direct_cf &&
        !std::static_pointer_cast<cond_direct_instruction>(
          internal_exp_tree[vertex_desc])->input_dep_addrs.empty())
    {
      std::for_each(boost::out_edges(vertex_desc, internal_exp_tree).first,
                    boost::out_edges(vertex_desc, internal_exp_tree).second,
                    [&](exp_tree_edge_desc out_edge)
      {
        auto nearest_cfi_desc = next_cfi_following_path(vertex_desc, internal_exp_tree[out_edge]);
        if (nearest_cfi_desc != boost::graph_traits<exp_tree_t>::null_vertex())
          boost::add_edge(look_for_vertex(internal_exp_tree[vertex_desc], internal_exp_cfi_tree),
                          look_for_vertex(internal_exp_tree[nearest_cfi_desc], internal_exp_cfi_tree),
                          internal_exp_tree[out_edge], internal_exp_cfi_tree);
      });
    }
  });

  return;
}


extern ptr_cond_direct_inss_t detected_input_dep_cfis;
class exp_vertex_label_writer
{
public:
  exp_vertex_label_writer(exp_graph_t& input_graph) : internal_graph(input_graph) {}

//  template<typename Vertex>
  void operator()(std::ostream& label, /*Vertex*/exp_vertex_desc vertex)
  {
    auto vertex_ins_addr = internal_graph[vertex];
    // verify if the instruction at this address is a input dependent CFI
    auto cfi_iter = detected_input_dep_cfis.begin();
    for (; cfi_iter != detected_input_dep_cfis.end(); ++cfi_iter)
    {
      if ((*cfi_iter)->address == vertex_ins_addr) break;
    }
    if (cfi_iter != detected_input_dep_cfis.end())
    {
      tfm::format(label, "[label=\"<%s: %s>\",style=filled,fillcolor=slateblue]",
                  addrint_to_hexstring(vertex_ins_addr),
                  ins_at_addr[vertex_ins_addr]->disassembled_name);
    }
    else
    {
      tfm::format(label, "[label=\"<%s: %s>\"]", addrint_to_hexstring(vertex_ins_addr),
                  ins_at_addr[vertex_ins_addr]->disassembled_name);
    }

    return;
  }

private:
  exp_graph_t internal_graph;
};


template <typename exp_t>
class generic_exp_vertex_label_writer
{
public:
  generic_exp_vertex_label_writer(const exp_t& input_graph) : internal_graph(input_graph) {};

  void operator()(std::ostream& label,
                  typename boost::graph_traits<exp_t>::vertex_descriptor vertex_desc)
  {
    auto current_vertex = internal_graph[vertex_desc];
//    auto vertex_is_cfi = false;
//    std::for_each(detected_input_dep_cfis.begin(), detected_input_dep_cfis.end(),
//                  [&](ptr_cond_direct_ins_t cfi)
//    {
//      if (cfi->address == current_vertex->address) vertex_is_cfi = true;
//    });


//    if (vertex_is_cfi) tfm::format(label, "[label=\"<%s: %s>\",style=filled,fillcolor=slateblue]",
//                                   addrint_to_hexstring(current_vertex->address),
//                                   current_vertex->disassembled_name);
//    else tfm::format(label, "[label=\"<%s: %s>\"]", addrint_to_hexstring(current_vertex->address),
//                     current_vertex->disassembled_name);
    if (is_input_dep_cfi(current_vertex))
    {
      if (is_resolved_cfi(current_vertex))
        tfm::format(label, "[label=\"<%s: %s>\",style=filled,fillcolor=coral]",
                    addrint_to_hexstring(current_vertex->address),
                    current_vertex->disassembled_name);
      else
        tfm::format(label, "[label=\"<%s: %s>\",style=filled,fillcolor=slateblue]",
                    addrint_to_hexstring(current_vertex->address),
                    current_vertex->disassembled_name);
    }
    else
    {
      tfm::format(label, "[label=\"<%s: %s>\"]", addrint_to_hexstring(current_vertex->address),
                  current_vertex->disassembled_name);
    }
    return;
  }

private:
  exp_t internal_graph;
};


class exp_edge_label_writer
{
public:
  exp_edge_label_writer(exp_graph_t& input_graph) : internal_graph(input_graph) {}

//  template<typename Edge>
  void operator()(std::ostream& label, /*Edge*/exp_edge_desc edge)
  {
    auto current_edge = internal_graph[edge];
    tfm::format(label, "[label=\"%s\"]", /*path_code_to_string(current_edge.first)*/"");
    return;
  }

private:
  exp_graph_t internal_graph;
};


template <typename exp_t>
class generic_exp_edge_label_writer
{
public:
  generic_exp_edge_label_writer(const exp_t& input_graph) : internal_graph(input_graph) {};

  void operator()(std::ostream& label,
                  typename boost::graph_traits<exp_t>::edge_descriptor edge_desc)
  {
//    auto current_edge = internal_graph[edge_desc];
//    tfm::format(label, "[label=\"%s\"]", /*path_code_to_string(current_edge)*/"");

    auto target_vertex_desc = boost::target(edge_desc, internal_graph);
    if (is_input_dep_cfi(internal_graph[target_vertex_desc]) &&
        !std::static_pointer_cast<cond_direct_instruction>(
          internal_graph[target_vertex_desc])->path_code.empty())
    {
      tfm::format(label, "[label=\"%d\"]",
                  std::static_pointer_cast<cond_direct_instruction>(
                    internal_graph[target_vertex_desc])->path_code.back());
    }
    else
      tfm::format(label, "[label=\"\"]");

    return;
  }

private:
  exp_t internal_graph;
};


class exp_vertex_isolated_prunner
{
public:
  exp_vertex_isolated_prunner() : internal_graph(internal_exp_graph) {}

  bool operator()(exp_vertex_desc vertex) const
  {
    boost::graph_traits<exp_graph_t>::out_edge_iterator out_e_iter, last_out_e_iter;
    boost::graph_traits<exp_graph_t>::in_edge_iterator in_e_iter, last_in_e_iter;

    // verify if the vertex is isolated (no in/out edges)
    std::tie(out_e_iter, last_out_e_iter) = boost::out_edges(vertex, internal_graph);
    std::tie(in_e_iter, last_in_e_iter) = boost::in_edges(vertex, internal_graph);

    // the isolated one (i.e. the corresponing instruction is never executed) will be prunned
    if ((out_e_iter == last_out_e_iter) && (in_e_iter == last_in_e_iter)) return false;

    return true;
  }

private:
  exp_graph_t internal_graph;
};


///**
// * @brief prune isolated vertices
// */
//static inline void prune_graph()
//{
//  boost::graph_traits<exp_graph>::out_edge_iterator out_e_iter, last_out_e_iter;
//  boost::graph_traits<exp_graph>::in_edge_iterator in_e_iter, last_in_e_iter;

//  exp_vertex_iter v_iter, next_v_iter, last_v_iter;
//  boost::tie(v_iter, last_v_iter) = boost::vertices(internal_exp_graph);
//  for (next_v_iter = v_iter; v_iter != last_v_iter; v_iter = next_v_iter)
//  {
//    ++next_v_iter;

//    // verify if the vertex is isolated (no in/out edges)
//    boost::tie(out_e_iter, last_out_e_iter) = boost::out_edges(*v_iter, internal_exp_graph);
//    boost::tie(in_e_iter, last_in_e_iter) = boost::in_edges(*v_iter, internal_exp_graph);
//    if ((out_e_iter == last_out_e_iter) && (in_e_iter == last_in_e_iter))
//    {
//      // the isolated one (i.e. the corresponing instruction is never executed) will be prunned
//      boost::remove_vertex(*v_iter, internal_exp_graph);
//    }
//  }
//  return;
//}


/**
 * @brief save the graph to a dot file
 *
 */
auto explorer_graph::save_to_file (std::string filename) -> void
{
//  exp_vertex_isolated_prunner vertex_prunner;
  boost::filtered_graph<exp_graph_t, boost::keep_all, exp_vertex_isolated_prunner>
      prunned_graph(internal_exp_graph, boost::keep_all(), exp_vertex_isolated_prunner());
  std::ofstream output(filename.c_str(), std::ofstream::out | std::ofstream::trunc);
  boost::write_graphviz(output, prunned_graph,
                        exp_vertex_label_writer(internal_exp_graph),
                        exp_edge_label_writer(internal_exp_graph));

  boost::filtered_graph<exp_graph_t, boost::keep_all, exp_vertex_isolated_prunner>
      prunned_graph_simple(internal_exp_graph_simple, boost::keep_all(),
                           exp_vertex_isolated_prunner());
  std::ofstream output_simple(("simple_" + filename).c_str(),
                              std::ofstream::out | std::ofstream::trunc);
  boost::write_graphviz(output_simple, prunned_graph_simple,
                        exp_vertex_label_writer(internal_exp_graph_simple),
                        exp_edge_label_writer(internal_exp_graph_simple));

  std::ofstream output_tree(("tree_" + filename).c_str(),
                            std::ofstream::out | std::ofstream::trunc);
  boost::write_graphviz(output_tree, internal_exp_tree,
                        generic_exp_vertex_label_writer<exp_tree_t>(internal_exp_tree),
                        generic_exp_edge_label_writer<exp_tree_t>(internal_exp_tree));

  std::ofstream output_cfi_tree(("cfi_tree_" + filename).c_str(),
                                std::ofstream::out | std::ofstream::trunc);
  boost::write_graphviz(output_cfi_tree, internal_exp_cfi_tree,
                        generic_exp_vertex_label_writer<exp_tree_t>(internal_exp_cfi_tree),
                        generic_exp_edge_label_writer<exp_tree_t>(internal_exp_cfi_tree));
  return;
}
