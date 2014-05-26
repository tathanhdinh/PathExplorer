#include "execution_dfa.h"
#include "../util/stuffs.h"

#include <numeric>
#include <boost/graph/graphviz.hpp>

typedef ptr_cond_direct_inss_t                                dfa_vertex;
typedef addrint_value_maps_t                                  dfa_edge;
typedef boost::adjacency_list<boost::listS, boost::vecS,
                              boost::bidirectionalS,
                              dfa_vertex, dfa_edge>           dfa_graph_t;
typedef boost::graph_traits<dfa_graph_t>::vertex_descriptor   dfa_vertex_desc;
typedef std::vector<dfa_vertex_desc>                          dfa_vertex_descs;
typedef boost::graph_traits<dfa_graph_t>::edge_descriptor     dfa_edge_desc;
typedef boost::graph_traits<dfa_graph_t>::vertex_iterator     dfa_vertex_iter;
typedef boost::graph_traits<dfa_graph_t>::edge_iterator       dfa_edge_iter;

typedef std::shared_ptr<dfa_graph_t>                          ptr_dfa_graph_t;


/*================================================================================================*/

static dfa_graph_t      internal_dfa;
static dfa_vertex_desc  initial_state;


static ptr_exec_dfa_t   single_dfa_instance;

/*================================================================================================*/


auto execution_dfa::instance() -> ptr_exec_dfa_t
{
  if (!single_dfa_instance)
  {
    single_dfa_instance = std::make_shared<execution_dfa>(construction_key());
    internal_dfa.clear(); /*initial_state = boost::graph_traits<dfa_graph_t>::null_vertex();*/
    initial_state = boost::add_vertex(ptr_cond_direct_inss_t(), internal_dfa);
  }

  return single_dfa_instance;
}


/**
 * @brief execution_dfa::add_exec_path
 */
static auto add_exec_path (ptr_exec_path_t exec_path) -> void
{
  auto get_next_state = [](dfa_vertex_desc current_state,
      const addrint_value_maps_t& transition_cond) -> dfa_vertex_desc
  {
    if (current_state == boost::graph_traits<dfa_graph_t>::null_vertex())
      return boost::graph_traits<dfa_graph_t>::null_vertex();
    else
    {
      boost::graph_traits<dfa_graph_t>::out_edge_iterator trans_iter, last_trans_iter;
      std::tie(trans_iter, last_trans_iter) = boost::out_edges(current_state, internal_dfa);

      auto predicate = [&transition_cond](dfa_edge_desc edge) -> bool
      {
        return two_vmaps_are_isomorphic(transition_cond, internal_dfa[edge]);
      };

      auto found_trans_iter = std::find_if(trans_iter, last_trans_iter, predicate);
      return (found_trans_iter == last_trans_iter) ?
            boost::graph_traits<dfa_graph_t>::null_vertex() :
            boost::target(*found_trans_iter, internal_dfa);
    }
  };


  // add the execution path into the DFA
//  tfm::format(std::cerr, "======\nadd a new execution path\n");
  auto prev_state = initial_state; auto mismatch = false;
  std::for_each(exec_path->condition.begin(), exec_path->condition.end(),
                [&](decltype(exec_path->condition)::const_reference sub_cond)
  {
    // trick: once mismatch is assigned to true then it will be never re-assigned to false
    dfa_vertex_desc current_state;
    if (!mismatch)
    {
      current_state = get_next_state(prev_state, std::get<0>(sub_cond));
      mismatch = (current_state == boost::graph_traits<dfa_graph_t>::null_vertex());
    }

    if (mismatch)
    {
//      internal_dfa[prev_state] = std::get<1>(sub_cond);
//      internal_dfa[prev_state].insert(internal_dfa[prev_state].end(),
//                                      std::get<1>(sub_cond).begin(), std::get<1>(sub_cond).end());
      std::for_each(std::get<1>(sub_cond).begin(), std::get<1>(sub_cond).end(),
                    [&](ptr_cond_direct_inss_t::const_reference cfi)
      {
        if (std::find(internal_dfa[prev_state].begin(),
                      internal_dfa[prev_state].end(), cfi) == internal_dfa[prev_state].end())
          internal_dfa[prev_state].push_back(cfi);
      });

      current_state = boost::add_vertex(ptr_cond_direct_inss_t(), internal_dfa);
      boost::add_edge(prev_state, current_state, std::get<0>(sub_cond), internal_dfa);
    }

    prev_state = current_state;
  });

  return;
}


/**
 * @brief execution_dfa::add_exec_paths
 */
auto execution_dfa::add_exec_paths (ptr_exec_paths_t exec_paths) -> void
{
  std::for_each(exec_paths.begin(),exec_paths.end(),
                [](decltype(explored_exec_paths)::const_reference exec_path)
  {
    add_exec_path(exec_path);
  });
  return;
}


/**
 * @brief execution_dfa::optimization
 */
auto execution_dfa::optimization() -> void
{
  // merge equivalent states into a single state
  auto merge_equivalent_states = [](dfa_vertex_descs equiv_states) -> void
  {
    auto merge_two_cfis = [](const ptr_cond_direct_inss_t& cfis_a,
        const ptr_cond_direct_inss_t& cfis_b) -> ptr_cond_direct_inss_t
    {
      ptr_cond_direct_inss_t merged_cfis = cfis_a;
      std::for_each(cfis_b.begin(), cfis_b.end(),
                    [&merged_cfis](ptr_cond_direct_inss_t::const_reference cfi_b)
      {
        if (std::find(merged_cfis.begin(),
                      merged_cfis.end(), cfi_b) == merged_cfis.end()) merged_cfis.push_back(cfi_b);
      });
      return merged_cfis;
    };

    auto merge_two_cfis_states = [&merge_two_cfis](
        dfa_vertex_desc state_a, dfa_vertex_desc state_b) -> ptr_cond_direct_inss_t
    {
      return merge_two_cfis(internal_dfa[state_a], internal_dfa[state_b]);
    };

//    ptr_cond_direct_inss_t new_state_prop;
//    new_state_prop = std::accumulate(equiv_states.begin(), equiv_states.end(), new_state_prop, merge_two_cfis_states);

    return;
  };

  return;
}


/**
 * @brief execution_dfa::save_to_file
 */
auto execution_dfa::save_to_file(const std::string& filename) -> void
{
  auto write_dfa_transition = [](std::ostream& label, dfa_edge_desc trans) -> void
  {
    auto trans_cond = internal_dfa[trans];
    if (trans_cond.size() <= 2)
    {
      tfm::format(label, "[label=\"{ ");
      std::for_each(trans_cond.begin(), trans_cond.end(),
                    [&label](decltype(trans_cond)::const_reference trans_elem)
      {
        std::for_each(trans_elem.begin(), trans_elem.end(),
                      [&](addrint_value_map_t::const_reference addr_val)
        {
          tfm::format(label, "%d ", std::get<1>(addr_val));
        });
      });
      tfm::format(label, "%s", "}\"]");
    }
    else
    {
//      tfm::format(std::cerr, "condition size: %d\n", trans_cond.size());
      tfm::format(label, "[label=\"!{ ");

      auto value_exists = [&trans_cond](UINT8 value) -> bool
      {
        return std::any_of(trans_cond.begin(), trans_cond.end(),
                           [&value](addrint_value_maps_t::const_reference trans_elem)
        {
          return std::any_of(trans_elem.begin(), trans_elem.end(),
                             [&](addrint_value_map_t::const_reference addr_val)
          {
            return (std::get<1>(addr_val) == value);
          });
        });
      };

      for (auto val = 0; val <= std::numeric_limits<UINT8>::max(); ++val)
      {
        if (!value_exists(val)) tfm::format(label, "%d ", val);
      }
      tfm::format(label, "}\"]");
    }

    return;
  };

  auto write_dfa_state = [](std::ostream& label, dfa_vertex_desc state) -> void
  {
    auto state_val = internal_dfa[state];
    if (state_val.size() > 0)
    {
      tfm::format(label, "[label=\"");
      std::for_each(state_val.begin(),
                    state_val.end(), [&](decltype(state_val)::const_reference cfi)
      {
        tfm::format(label, "%s: %s\n", addrint_to_hexstring(cfi->address), cfi->disassembled_name);
//        if (cfi != state_val.back()) tfm::format(label, "\n");
      });
      tfm::format(label, "\"]");
//      tfm::format(label, "[label=\"<%s: %s>\"]", addrint_to_hexstring(state_val.front()->address),
//                  state_val.front()->disassembled_name);
    }
    else tfm::format(label, "[label=\"unknown\"]");
    return;
  };

  std::ofstream dfa_file(("dfa_" + filename).c_str(), std::ofstream::out | std::ofstream::trunc);
  boost::write_graphviz(dfa_file, internal_dfa,
                        std::bind(write_dfa_state, std::placeholders::_1, std::placeholders::_2),
                        std::bind(write_dfa_transition, std::placeholders::_1, std::placeholders::_2));
  dfa_file.close();
  return;
}
