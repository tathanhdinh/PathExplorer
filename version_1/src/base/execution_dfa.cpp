#include "execution_dfa.h"
#include "../util/stuffs.h"

#include <numeric>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/copy.hpp>

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
  auto merge_equivalent_states = [](dfa_vertex_descs equiv_states) -> dfa_vertex_desc
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

    auto operation = [&merge_two_cfis](
        ptr_cond_direct_inss_t cfis_a, dfa_vertex_desc state_b) -> ptr_cond_direct_inss_t
    {
      return merge_two_cfis(cfis_a, internal_dfa[state_b]);
    };

    auto copy_transitions = [](dfa_vertex_desc state_a, dfa_vertex_desc state_b) -> void
    {
      // copy in transitions
      boost::graph_traits<dfa_graph_t>::in_edge_iterator in_edge_iter_a, in_edge_last_iter_a;
      std::tie(in_edge_iter_a, in_edge_last_iter_a) = boost::in_edges(state_a, internal_dfa);
      std::for_each(in_edge_iter_a, in_edge_last_iter_a, [&state_b](dfa_edge_desc trans)
      {
        boost::add_edge(boost::source(trans, internal_dfa),
                        state_b, internal_dfa[trans], internal_dfa);
      });

      // copy out transitions
      boost::graph_traits<dfa_graph_t>::out_edge_iterator out_edge_iter_a, out_edge_last_iter_a;
      std::tie(out_edge_iter_a, out_edge_last_iter_a) = boost::out_edges(state_a, internal_dfa);
      std::for_each(out_edge_iter_a, out_edge_last_iter_a, [&state_b](dfa_edge_desc trans)
      {
        boost::add_edge(state_b,
                        boost::target(trans, internal_dfa), internal_dfa[trans], internal_dfa);
      });

      return;
    };

    auto erase_states = [](dfa_vertex_descs& states) -> void
    {
      std::vector<ptr_cond_direct_inss_t> state_contents;
      std::for_each(states.begin(), states.end(), [&state_contents](dfa_vertex_desc state)
      {
        state_contents.push_back(internal_dfa[state]);
      });

      std::for_each(state_contents.begin(), state_contents.end(),
                    [](ptr_cond_direct_inss_t content)
      {
        dfa_vertex_iter state_iter, last_state_iter;
        std::tie(state_iter, last_state_iter) = boost::vertices(internal_dfa);
        std::any_of(state_iter, last_state_iter, [&](dfa_vertex_desc state) -> bool
        {
          if (internal_dfa[state] == content)
          {
            boost::clear_vertex(state, internal_dfa); boost::remove_vertex(state, internal_dfa);
            return true;
          }
          else return false;
        });
      });

      return;
    };

    // add a new state reprenting the class of equivalent states
    auto new_state_prop = std::accumulate(equiv_states.begin(),
                                          equiv_states.end(), ptr_cond_direct_inss_t(), operation);

    auto new_state = boost::add_vertex(new_state_prop, internal_dfa);
    auto new_state_content = internal_dfa[new_state];
    std::for_each(equiv_states.begin(), equiv_states.end(), [&](dfa_vertex_desc state)
    {
      copy_transitions(state, new_state);
    });

    // erase equivalent states
    erase_states(equiv_states);

    // get the descriptor of the new state in the new DFA
    dfa_vertex_iter first_state_iter, last_state_iter;
    std::tie(first_state_iter, last_state_iter) = boost::vertices(internal_dfa);

    auto new_state_iter = std::find_if(first_state_iter, last_state_iter,
                                       [&new_state_content](dfa_vertex_desc state)
    {
      return (internal_dfa[state] == new_state_content);
    });

    if (new_state_iter != last_state_iter) return *new_state_iter;
    else return boost::graph_traits<dfa_graph_t>::null_vertex();
  };

  auto two_states_are_equivalent = [](dfa_vertex_desc state_a, dfa_vertex_desc state_b) -> bool
  {
    boost::graph_traits<dfa_graph_t>::out_edge_iterator first_a_trans_iter, last_a_trans_iter;
    std::tie(first_a_trans_iter, last_a_trans_iter) = boost::out_edges(state_a, internal_dfa);

    boost::graph_traits<dfa_graph_t>::out_edge_iterator first_b_trans_iter, last_b_trans_iter;
    std::tie(first_a_trans_iter, last_a_trans_iter) = boost::out_edges(state_b, internal_dfa);

    // verifying if any transition of a is also one of b: because transitions of a and of b are
    // complete so this verification is sufficient to verify a and b are equivalent
    return std::all_of(first_a_trans_iter, last_a_trans_iter, [&](dfa_edge_desc trans_a)
    {
      return std::any_of(first_b_trans_iter, last_b_trans_iter, [&](dfa_edge_desc trans_b)
      {
        return ((internal_dfa[boost::target(trans_a, internal_dfa)] ==
            internal_dfa[boost::target(trans_b, internal_dfa)]) &&
            two_vmaps_are_isomorphic(internal_dfa[trans_a], internal_dfa[trans_b]));
      });
    });
  };

  auto find_equivalent_states =
      [&two_states_are_equivalent](const dfa_vertex_descs& init_states) -> dfa_vertex_descs
  {
    dfa_vertex_descs result_states;

    dfa_vertex_iter first_state_iter, last_state_iter;
    std::tie(first_state_iter, last_state_iter) = boost::vertices(internal_dfa);

    if (init_states.size() == 0)
    {
      std::for_each(first_state_iter, last_state_iter, [&result_states](dfa_vertex_desc state)
      {
        if (internal_dfa[state].size() == 0) result_states.push_back(state);
      });
    }
    else
    {
      // verify if all transitions from the state have target as some equivalent states
      auto all_trans_to_equiv = [&](dfa_vertex_desc state) -> bool
      {
        boost::graph_traits<dfa_graph_t>::out_edge_iterator first_trans_iter, last_trans_iter;
        std::tie(first_trans_iter, last_trans_iter) = boost::out_edges(state, internal_dfa);

        return std::all_of(first_trans_iter, last_trans_iter, [&](dfa_edge_desc trans)
        {
          return (std::find(init_states.begin(), init_states.end(),
                            boost::target(trans, internal_dfa)) != init_states.end());
        });
      };

      auto state_a = boost::graph_traits<dfa_graph_t>::null_vertex();
      auto state_b = boost::graph_traits<dfa_graph_t>::null_vertex();
      std::any_of(first_state_iter, last_state_iter, [&](dfa_vertex_desc state_1)
      {
        if (all_trans_to_equiv(state_1) &&
            std::find(init_states.begin(), init_states.end(), state_1) == init_states.end())
        {
          return std::any_of(first_state_iter, last_state_iter, [&](dfa_vertex_desc state_2)
          {
            if (all_trans_to_equiv(state_2) &&
                std::find(init_states.begin(), init_states.end(), state_2) == init_states.end())
            {
              if (internal_dfa[state_1] != internal_dfa[state_2] &&
                  two_states_are_equivalent(state_1, state_2))
              {
                state_a = state_1; state_b = state_2;
                return true;
              }
              else return false;
            }
            else return false;
          });
        }
        else return false;
      });

      result_states.push_back(state_a);
      if (state_a != boost::graph_traits<dfa_graph_t>::null_vertex())
      {
        std::for_each(first_state_iter, last_state_iter, [&](dfa_vertex_desc state)
        {
          if ((internal_dfa[state_a] != internal_dfa[state]) &&
              two_states_are_equivalent(state_a, state))
          {
            result_states.push_back(state);
          }
        });
      }
    }

    return result_states;
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
