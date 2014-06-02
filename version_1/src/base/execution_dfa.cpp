#include "execution_dfa.h"
#include "../util/stuffs.h"

#include <numeric>
#include <boost/graph/graphviz.hpp>

typedef ptr_cond_direct_inss_t                                dfa_vertex;
typedef std::vector<dfa_vertex>                               dfa_vertices;
typedef addrint_value_maps_t                                  dfa_edge;
typedef std::vector<dfa_edge>                                 dfa_edges;
typedef boost::adjacency_list<boost::listS, boost::vecS,
                              boost::bidirectionalS,
                              dfa_vertex, dfa_edge>           dfa_graph_t;
typedef boost::graph_traits<dfa_graph_t>::vertex_descriptor   dfa_vertex_desc;
typedef std::vector<dfa_vertex_desc>                          dfa_vertex_descs;
typedef boost::graph_traits<dfa_graph_t>::edge_descriptor     dfa_edge_desc;
typedef boost::graph_traits<dfa_graph_t>::vertex_iterator     dfa_vertex_iter;
typedef boost::graph_traits<dfa_graph_t>::edge_iterator       dfa_edge_iter;
typedef boost::graph_traits<dfa_graph_t>::out_edge_iterator   dfa_out_edge_iter;
typedef boost::graph_traits<dfa_graph_t>::in_edge_iterator    dfa_in_edge_iter;

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
    internal_dfa.clear();
    initial_state = boost::add_vertex(ptr_cond_direct_inss_t(), internal_dfa);
  }

  return single_dfa_instance;
}


/**
 * @brief execution_dfa::add_exec_paths
 */
auto execution_dfa::add_exec_paths (const ptr_exec_paths_t& exec_paths) -> void
{
  auto add_exec_path = [](ptr_exec_path_t exec_path) -> void
  {
    auto get_next_state = [](dfa_vertex_desc current_state,
        const addrint_value_maps_t& transition_cond) -> dfa_vertex_desc
    {
      if (current_state == boost::graph_traits<dfa_graph_t>::null_vertex())
        return boost::graph_traits<dfa_graph_t>::null_vertex();
      else
      {
        boost::graph_traits<dfa_graph_t>::out_edge_iterator first_trans_iter, last_trans_iter;
        std::tie(first_trans_iter, last_trans_iter) = boost::out_edges(current_state, internal_dfa);

        auto iso_predicate = [&transition_cond](dfa_edge_desc edge) -> bool
        {
          return two_vmaps_are_isomorphic(transition_cond, internal_dfa[edge]);
        };

        auto found_trans_iter = std::find_if(first_trans_iter, last_trans_iter, iso_predicate);
        return (found_trans_iter == last_trans_iter) ?
              boost::graph_traits<dfa_graph_t>::null_vertex() :
              boost::target(*found_trans_iter, internal_dfa);
      }
    };


    // add the execution path into the DFA
  //  tfm::format(std::cerr, "======\nadd a new execution path\n");
    auto prev_state = initial_state; auto mismatch = false;
    std::for_each(std::begin(exec_path->condition), std::end(exec_path->condition),
                  [&](decltype(exec_path->condition)::const_reference sub_cond)
    {
      // trick: once mismatch is assigned to true then it will be never re-assigned to false
//      dfa_vertex_desc current_state;
      auto current_state = boost::graph_traits<dfa_graph_t>::null_vertex();
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
        std::for_each(std::begin(std::get<1>(sub_cond)), std::end(std::get<1>(sub_cond)),
                      [&](ptr_cond_direct_inss_t::const_reference cfi)
        {
          if (std::find(std::begin(internal_dfa[prev_state]), std::end(internal_dfa[prev_state]),
                        cfi) == std::end(internal_dfa[prev_state]))
            internal_dfa[prev_state].push_back(cfi);
        });

        current_state = boost::add_vertex(ptr_cond_direct_inss_t(), internal_dfa);
        boost::add_edge(prev_state, current_state, std::get<0>(sub_cond), internal_dfa);
      }

      prev_state = current_state;
    });

    return;
  };

  std::for_each(std::begin(exec_paths), std::end(exec_paths),
                [&](decltype(explored_exec_paths)::const_reference exec_path)
  {
    add_exec_path(exec_path);
  });
  return;
}


/**
 * @brief merge_state_contents
 */
static auto merge_state_contents (const dfa_vertex& content_a,
                                  const dfa_vertex& content_b) -> dfa_vertex
{
  auto merged_content = content_a;
  std::for_each(std::begin(content_b), std::end(content_b),
                [&merged_content](dfa_vertex::const_reference sub_content)
  {
    if (std::find(std::begin(merged_content),
                  std::end(merged_content), sub_content) == std::end(merged_content))
      merged_content.push_back(sub_content);
  });
  return merged_content;
}


/**
 * @brief find_state_by_content
 */
static auto find_state_by_content (const dfa_graph_t& graph,
                                   const dfa_vertex& content) -> dfa_vertex_desc
{
  auto first_state_iter = dfa_vertex_iter(); auto last_state_iter = dfa_vertex_iter();
  std::tie(first_state_iter, last_state_iter) = boost::vertices(graph);

  auto state_iter = std::find_if(first_state_iter, last_state_iter, [&](dfa_vertex_desc state)
  {
    return (graph[state] == content);
  });

  if (state_iter == last_state_iter) return boost::graph_traits<dfa_graph_t>::null_vertex();
  else return *state_iter;
}


/**
 * @brief erase_state_by_content
 */
static auto erase_state_by_content (dfa_graph_t& graph, const dfa_vertex& content) -> void
{
//  auto first_state_iter = dfa_vertex_iter(); auto last_state_iter = dfa_vertex_iter();
//  std::tie(first_state_iter, last_state_iter) = boost::vertices(internal_dfa);

//  auto state = find_state_by_content(first_state_iter, last_state_iter, content);
//  if (state != boost::graph_traits<dfa_graph_t>::null_vertex())
//  {
//    boost::clear_vertex(state, internal_dfa); boost::remove_vertex(state, internal_dfa);
//  }
  auto state = find_state_by_content(graph, content);
  if (state != boost::graph_traits<dfa_graph_t>::null_vertex())
  {
    boost::clear_vertex(state, graph); boost::remove_vertex(state, graph);
  }

  return;
}


/**
 * @brief copy_transitions_from_state_to_state
 */
static auto copy_transitions_from_state_to_state (dfa_graph_t& graph,
                                                  dfa_vertex_desc state_a,
                                                  dfa_vertex_desc state_b) -> void
{
  // copy in transitions
  auto in_edge_first_iter_a = dfa_in_edge_iter(); auto in_edge_last_iter_a = dfa_in_edge_iter();
  std::tie(in_edge_first_iter_a, in_edge_last_iter_a) = boost::in_edges(state_a, graph);

//  tfm::format(std::cerr, "copy %d in transitions\n",
//              std::distance(in_edge_first_iter_a, in_edge_last_iter_a));
  std::for_each(in_edge_first_iter_a, in_edge_last_iter_a, [&](dfa_edge_desc trans)
  {
    boost::add_edge(boost::source(trans, graph), state_b, graph[trans], graph);
  });

  // copy out transitions
  auto out_edge_first_iter_a = dfa_out_edge_iter(); auto out_edge_last_iter_a = dfa_out_edge_iter();
  std::tie(out_edge_first_iter_a, out_edge_last_iter_a) = boost::out_edges(state_a, graph);

//  tfm::format(std::cerr, "copy %d out transitions\n",
//              std::distance(out_edge_first_iter_a, out_edge_last_iter_a));
  std::for_each(out_edge_first_iter_a, out_edge_last_iter_a, [&](dfa_edge_desc trans)
  {
    boost::add_edge(state_b, boost::target(trans, graph), graph[trans], graph);
  });

  return;
}


/**
 * @brief erase_duplicated_transitions
 */
static auto erase_duplicated_transitions (dfa_graph_t& graph, dfa_vertex_desc state) -> void
{
  auto same_transition = [&graph](dfa_edge_desc trans_x, dfa_edge_desc trans_y) -> bool
  {
    return ((boost::target(trans_x, graph) == boost::target(trans_y, graph)) &&
        two_vmaps_are_isomorphic(graph[trans_x], graph[trans_y]));
  };

  auto first_trans_iter = dfa_out_edge_iter(); auto last_trans_iter = dfa_out_edge_iter();
  auto trans_iter = dfa_out_edge_iter(); auto next_trans_iter = dfa_out_edge_iter();
  auto duplicated_trans_exists = true;
  while (duplicated_trans_exists)
  {
    duplicated_trans_exists = false;
    std::tie(first_trans_iter, last_trans_iter) = boost::out_edges(state, graph);
    trans_iter = first_trans_iter;
    for (next_trans_iter = trans_iter; trans_iter != last_trans_iter; trans_iter = next_trans_iter)
    {
      ++next_trans_iter;
      if (std::find_if(next_trans_iter, last_trans_iter,
                       std::bind(same_transition, *trans_iter, std::placeholders::_1))
          != last_trans_iter)
      {
        duplicated_trans_exists = true; break;
      }
    }
    if (duplicated_trans_exists) boost::remove_edge(*trans_iter, graph);
  };

  return;
}


/**
 * @brief execution_dfa::optimization
 */
auto execution_dfa::optimize () -> void
{
  // merge equivalent states into a single state
  auto merge_equivalent_states = [](const dfa_vertex_descs& equiv_states,
      const dfa_vertex_descs& representing_states) -> dfa_vertex_descs
  {
    auto erase_states = [](const dfa_vertex_descs& states) -> void
    {
      // save contents of equivalent states
      auto state_contents = dfa_vertices();
      std::for_each(std::begin(states), std::end(states), [&state_contents](dfa_vertex_desc state)
      {
        state_contents.push_back(internal_dfa[state]);
      });

      // then use these contents to remove states
      std::for_each(std::begin(state_contents), std::end(state_contents),
                    [](ptr_cond_direct_inss_t content)
      {
        erase_state_by_content(internal_dfa, content);
      });

      return;
    };

    auto find_states_by_contents = [](const dfa_vertices& contents) -> dfa_vertex_descs
    {
      // get the descriptor of the new state in the new DFA

      auto states = dfa_vertex_descs();
      std::for_each(std::begin(contents), std::end(contents), [&](const dfa_vertex& content)
      {
        auto new_state = find_state_by_content(internal_dfa, content);
        if (new_state != boost::graph_traits<dfa_graph_t>::null_vertex())
          states.push_back(new_state);
      });
      return states;
    };
    
    // copy contents of the previous representing states
    auto representing_state_contents = dfa_vertices();
    std::for_each(std::begin(representing_states), std::end(representing_states),
                  [&representing_state_contents](dfa_vertex_desc state)
    {
      representing_state_contents.push_back(internal_dfa[state]);
    });

    // add a new state reprenting the class of equivalent states
    auto accum_op = [](const dfa_vertex& init_content, dfa_vertex_desc state) -> dfa_vertex
    {
      return merge_state_contents(init_content, internal_dfa[state]);
    };
    auto new_representing_state_content =
        std::accumulate(std::begin(equiv_states),
                        std::end(equiv_states), ptr_cond_direct_inss_t(), accum_op);
    auto new_representing_state = boost::add_vertex(new_representing_state_content, internal_dfa);

    // add the new state into the set of representing states
    representing_state_contents.push_back(new_representing_state_content);

    // copy transitions
    std::for_each(std::begin(equiv_states), std::end(equiv_states), [&](dfa_vertex_desc state)
    {
      copy_transitions_from_state_to_state(internal_dfa, state, new_representing_state);
    });

    // erase duplicated transitions from the new state
    erase_duplicated_transitions(internal_dfa, new_representing_state);

    // erase equivalent states
    erase_states(equiv_states);

    return find_states_by_contents(representing_state_contents);
  };

  auto find_equivalent_states = [](const dfa_vertex_descs& init_states) -> dfa_vertex_descs
  {
    auto two_states_are_equivalent = [](dfa_vertex_desc state_a, dfa_vertex_desc state_b) -> bool
    {
//      tfm::format(std::cerr, "verify %s:%s and %s:%s\n",
//                  addrint_to_hexstring(internal_dfa[state_a].front()->address),
//                  internal_dfa[state_a].front()->disassembled_name,
//                  addrint_to_hexstring(internal_dfa[state_b].front()->address),
//                  internal_dfa[state_b].front()->disassembled_name);

      boost::graph_traits<dfa_graph_t>::out_edge_iterator first_a_trans_iter, last_a_trans_iter;
      std::tie(first_a_trans_iter, last_a_trans_iter) = boost::out_edges(state_a, internal_dfa);

      boost::graph_traits<dfa_graph_t>::out_edge_iterator first_b_trans_iter, last_b_trans_iter;
      std::tie(first_b_trans_iter, last_b_trans_iter) = boost::out_edges(state_b, internal_dfa);

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

    auto get_derived_states = [](const dfa_vertex_descs& initial_states) -> dfa_vertex_descs
    {
      // verify if all transitions from the state have target as some equivalent states
      auto all_trans_to_equiv = [](
          dfa_vertex_desc state, const dfa_vertex_descs& init_states) -> bool
      {
//        tfm::format(std::cerr, "verify derived state %s:%s\n",
//                    addrint_to_hexstring(internal_dfa[state].front()->address),
//                    internal_dfa[state].front()->disassembled_name);

        boost::graph_traits<dfa_graph_t>::out_edge_iterator first_trans_iter, last_trans_iter;
        std::tie(first_trans_iter, last_trans_iter) = boost::out_edges(state, internal_dfa);

        return std::all_of(first_trans_iter, last_trans_iter, [&](dfa_edge_desc trans)
        {
          return (std::find(std::begin(init_states), std::end(init_states),
                            boost::target(trans, internal_dfa)) != std::end(init_states));
        });
      };

      auto derived_states = dfa_vertex_descs() ;
      dfa_vertex_iter first_state_iter, last_state_iter;
      std::tie(first_state_iter, last_state_iter) = boost::vertices(internal_dfa);
      std::for_each(first_state_iter, last_state_iter, [&](dfa_vertex_desc state)
      {
//        tfm::format(std::cerr, "%s:%s\n",
//                    addrint_to_hexstring(internal_dfa[state].front()->address),
//                    internal_dfa[state].front()->disassembled_name);

        if ((std::find(std::begin(initial_states),
                       std::end(initial_states), state) == std::end(initial_states)) &&
            (all_trans_to_equiv(state, initial_states)))
          derived_states.push_back(state);
      });

//      tfm::format(std::cerr, "derived states: %d\n", derived_states.size());
      return derived_states;
    };

    auto result_states = dfa_vertex_descs();
    auto first_state_iter = dfa_vertex_iter(); auto last_state_iter = dfa_vertex_iter();
    std::tie(first_state_iter, last_state_iter) = boost::vertices(internal_dfa);

    if (init_states.empty()/*size() == 0*/)
    {
      std::for_each(first_state_iter, last_state_iter, [&result_states](dfa_vertex_desc state)
      {
        if (internal_dfa[state].empty()/*size() == 0*/) result_states.push_back(state);
      });
    }
    else
    {
//      tfm::format(std::cerr, "initial states: %d\n", init_states.size());

      // get derived states: ones with all transitions to initial states
      auto derived_states = get_derived_states(init_states);
      auto s_first_iter = std::begin(derived_states);
      auto s_last_iter = std::end(derived_states);

      auto state_a = boost::graph_traits<dfa_graph_t>::null_vertex();

      for (auto s_iter = s_first_iter; s_iter != s_last_iter; ++s_iter)
      {
        auto predicate = std::bind(two_states_are_equivalent, *s_iter, std::placeholders::_1);

        if (std::find_if(std::next(s_iter), s_last_iter, predicate) != s_last_iter)
        {
//          tfm::format(std::cerr, "equivalent state found\n");
          state_a = *s_iter; break;
        }
      }

      if (state_a != boost::graph_traits<dfa_graph_t>::null_vertex())
      {
        std::for_each(s_first_iter, s_last_iter, [&](dfa_vertex_desc state)
        {
          if (two_states_are_equivalent(state_a, state))
          {
//            tfm::format(std::cerr, "add new\n");
//            tfm::format(std::cerr, "%d", internal_dfa[state].size());
//            tfm::format(std::cerr, "%s:%s = %s:%s\n",
//                        addrint_to_hexstring(internal_dfa[state_a].front()->address),
//                        internal_dfa[state_a].front()->disassembled_name,
//                        addrint_to_hexstring(internal_dfa[state].front()->address),
//                        internal_dfa[state].front()->disassembled_name);
            result_states.push_back(state);
          }
        });
      }
    }

    return result_states;
  };

  auto representing_states = dfa_vertex_descs();
  auto equiv_states = dfa_vertex_descs();

  while (true)
  {
    equiv_states = find_equivalent_states(representing_states);
    if (equiv_states.size() > 1)
      representing_states = merge_equivalent_states(equiv_states, representing_states);
    else break;
  }

//  // loop
//  equiv_states = find_equivalent_states(representing_states);
//  if (equiv_states.size() > 1)
//    representing_states = merge_equivalent_states(equiv_states, representing_states);
  return;
}


static auto get_contents_from_root (dfa_vertex_desc state) -> dfa_vertices
{
  auto contents = dfa_vertices();

  if (internal_dfa[state].size() > 0)
  {
    contents.push_back(internal_dfa[state]);

    auto first_trans_iter = dfa_out_edge_iter(); auto last_trans_iter = dfa_out_edge_iter();
    std::tie(first_trans_iter, last_trans_iter) = boost::out_edges(state, internal_dfa);

    std::for_each(first_trans_iter, last_trans_iter, [&](dfa_edge_desc trans)
    {
      auto sub_contents = get_contents_from_root(boost::target(trans, internal_dfa));
      contents.insert(std::begin(contents), std::begin(sub_contents), std::end(sub_contents));
    });
  }

  return contents;
};


static auto get_states_from_root (dfa_vertex_desc state) -> dfa_vertex_descs
{
  auto states = dfa_vertex_descs();

  if (!internal_dfa[state].empty())
  {
    states.push_back(state);

    auto first_trans_iter = dfa_out_edge_iter(); auto last_trans_iter = dfa_out_edge_iter();
    std::tie(first_trans_iter, last_trans_iter) = boost::out_edges(state, internal_dfa);

    std::for_each(first_trans_iter, last_trans_iter, [&](dfa_edge_desc trans)
    {
      auto sub_states = get_states_from_root(boost::target(trans, internal_dfa));
      std::copy_if(std::begin(sub_states), std::end(sub_states), std::back_inserter(states),
                   [&](dfa_vertex_desc state) -> bool
      {
        return (std::find(std::begin(states), std::end(states), state) == std::end(states));
      });
    });
  }

  return states;
}


//typedef std::function<dfa_vertex_desc(dfa_vertex_desc, dfa_vertex_desc)> merge_states_t;
static auto merge_approximable_states (dfa_vertex_desc state_a,
                                       dfa_vertex_desc state_b) -> dfa_vertex_desc
{
  if (state_a == state_b)
    return state_a;
  else
  {
    // add merged state
    auto merged_content = merge_state_contents(internal_dfa[state_a], internal_dfa[state_b]);
    auto merged_state = boost::add_vertex(merged_content, internal_dfa);

  //  copy_transitions_from_state_to_state(internal_dfa, state_a, merged_state);
  //  copy_transitions_from_state_to_state(internal_dfa, state_b, merged_state);

    // add loopback transition
  //    boost::add_edge(merged_state, merged_state, loopback_trans, internal_dfa);
  //    tfm::format(std::cerr, "loopback transition added\n");

    if ((boost::out_degree(state_a, internal_dfa) > 0) &&
        (boost::out_degree(state_b, internal_dfa) > 0))
    {
      // merge child states of roots a and b
      auto first_a_trans_iter = dfa_out_edge_iter(); auto last_a_trans_iter = dfa_out_edge_iter();
      std::tie(first_a_trans_iter, last_a_trans_iter) = boost::out_edges(state_a, internal_dfa);

      auto first_b_trans_iter = dfa_out_edge_iter(); auto last_b_trans_iter = dfa_out_edge_iter();
      std::tie(first_b_trans_iter, last_b_trans_iter) = boost::out_edges(state_b, internal_dfa);

      tfm::format(std::cerr, "continue with %d child states\n",
                  std::distance(first_a_trans_iter, last_a_trans_iter));
      std::for_each(first_a_trans_iter, last_a_trans_iter, [&](dfa_edge_desc trans_a)
      {
        auto approx_predicate = [&trans_a](dfa_edge_desc trans) -> bool
        {
          return (two_vmaps_are_isomorphic(internal_dfa[trans_a], internal_dfa[trans]) ||
                  a_vmaps_is_included_in_b(internal_dfa[trans_a], internal_dfa[trans]));
        };

        tfm::format(std::cerr, "look in %d corresponding child states\n",
                    std::distance(first_b_trans_iter, last_b_trans_iter));
        auto corr_b_trans_iter = std::find_if(first_b_trans_iter,
                                              last_b_trans_iter, approx_predicate);

        auto next_state_a = boost::target(trans_a, internal_dfa);
        auto next_state_b = boost::target(*corr_b_trans_iter, internal_dfa);

        auto next_merged_state = merge_approximable_states(next_state_a, next_state_b);
        boost::add_edge(merged_state, next_merged_state, internal_dfa[trans_a], internal_dfa);
      });
    }

  //  copy_transitions_from_state_to_state(internal_dfa, state_a, merged_state);
  //  copy_transitions_from_state_to_state(internal_dfa, state_b, merged_state);

    return merged_state;
  }
};


auto static longest_path_length_from (dfa_vertex_desc state, const dfa_graph_t& graph) -> int
{
  if (boost::out_degree(state, graph) == 0)
    return 0;
  else
  {
    auto path_lengths = std::vector<int>();
    auto first_trans_iter = dfa_out_edge_iter(); auto last_trans_iter = dfa_out_edge_iter();
    std::tie(first_trans_iter, last_trans_iter) = boost::out_edges(state, graph);
    std::for_each(first_trans_iter, last_trans_iter, [&](dfa_edge_desc trans)
    {
      path_lengths.push_back(longest_path_length_from(boost::target(trans, graph), graph));
    });

    return (1 + *std::max_element(std::begin(path_lengths), std::end(path_lengths)));
  }
}


typedef std::pair<dfa_vertex_desc, dfa_vertex_desc> state_pair_t;
typedef std::vector<state_pair_t>                   state_pairs_t;
static auto matching_state_pairs_from (dfa_vertex_desc state_a,
                                       dfa_vertex_desc state_b) -> state_pairs_t
{
  auto matching_pairs = state_pairs_t();
  if (!internal_dfa[state_a].empty() && !internal_dfa[state_b].empty())
  {
    matching_pairs.push_back(std::make_pair(state_a, state_b));

    auto first_trans_a_iter = dfa_out_edge_iter(); auto last_trans_a_iter = dfa_out_edge_iter();
    std::tie(first_trans_a_iter, last_trans_a_iter) = boost::out_edges(state_a, internal_dfa);

    auto first_trans_b_iter = dfa_out_edge_iter(); auto last_trans_b_iter = dfa_out_edge_iter();
    std::tie(first_trans_b_iter, last_trans_b_iter) = boost::out_edges(state_b, internal_dfa);

    std::for_each(first_trans_a_iter, last_trans_a_iter, [&](dfa_edge_desc trans_a)
    {
      auto matching_predicate = [&trans_a](dfa_edge_desc trans) -> bool
      {
        return (two_vmaps_are_isomorphic(internal_dfa[trans_a], internal_dfa[trans]) ||
                a_vmaps_is_included_in_b(internal_dfa[trans_a], internal_dfa[trans]));
      };

      auto trans_b = *std::find_if(first_trans_b_iter, last_trans_b_iter, matching_predicate);
      auto sub_matching_pairs = matching_state_pairs_from(boost::target(trans_a, internal_dfa),
                                                          boost::target(trans_b, internal_dfa));
      std::copy_if(std::begin(sub_matching_pairs), std::end(sub_matching_pairs),
                   std::back_inserter(matching_pairs), [&](state_pair_t sub_pair) -> bool
      {
        return (std::find(std::begin(matching_pairs),
                          std::end(matching_pairs), sub_pair) == std::end(matching_pairs));
      });
    });
  }
  return matching_pairs;
}


typedef std::vector<dfa_vertex_descs>       dfa_vertex_partition_t;
typedef dfa_vertex_partition_t::iterator    dfa_vertex_partition_iter_t;
static auto get_equivalence_classes (const state_pairs_t matching_pairs) -> dfa_vertex_partition_t
{
  auto equiv_classes = dfa_vertex_partition_t();

  auto representing_class = [&equiv_classes](dfa_vertex_desc state) -> dfa_vertex_partition_iter_t
  {
    return std::find_if(std::begin(equiv_classes), std::end(equiv_classes),
                        [&](const dfa_vertex_descs& states)
    {
      return (std::find(std::begin(states), std::end(states), state) != std::end(states));
    });
  };

  std::for_each(std::begin(matching_pairs), std::end(matching_pairs),
                [&](const state_pair_t& state_pair)
  {
    auto state_a = std::get<0>(state_pair); auto state_b = std::get<1>(state_pair);

    auto class_a_iter = representing_class(state_a); auto class_a = *class_a_iter;
    auto class_b_iter = representing_class(state_b); auto class_b = *class_b_iter;

    if ((class_a_iter != std::end(equiv_classes)) && (class_b_iter != std::end(equiv_classes)))
    {
      if (class_a_iter != class_b_iter)
      {
        // merge two equivalence classes: c = a + b
        auto class_c = class_a;
        class_c.insert(std::end(class_c), std::begin(class_b), std::end(class_b));

        // remove a and b
        equiv_classes.erase(std::find(std::begin(equiv_classes), std::end(equiv_classes), class_a));
        equiv_classes.erase(std::find(std::begin(equiv_classes), std::end(equiv_classes), class_b));

        // add c = a + b
        equiv_classes.push_back(class_c);
      }
    }
    else
    {
      if ((class_a_iter == std::end(equiv_classes)) && (class_b_iter == std::end(equiv_classes)))
      {
        auto class_ab = dfa_vertex_descs();
        class_ab.push_back(state_a); class_ab.push_back(state_b);

        equiv_classes.push_back(class_ab);
      }
      else
      {
        if (class_a_iter != std::end(equiv_classes)) class_a_iter->push_back(state_b);
        else class_b_iter->push_back(state_a);
      }
    }
  });

  return equiv_classes;
}



/**
 * @brief execution_dfa::approximate
 */
auto execution_dfa::approximate () -> void
{
  enum ordering_t
  {
    uncomparable = 0,
    less_than    = 1,
    more_than    = 2
  };
  typedef std::pair<dfa_vertex_desc, dfa_vertex_desc> state_pair_t;
  typedef std::map<state_pair_t , ordering_t>         approx_table_t;
  typedef std::map<state_pair_t, bool>                direct_connection_rel_t;

  auto construct_approx_table = [](approx_table_t& approximation_table) -> void
  {
    typedef std::function<bool(dfa_vertex_desc, dfa_vertex_desc)> approx_calculation_t;
    // verify if the state b can be approximated by a, namely b "less than or equal" a
    approx_calculation_t is_approximable = [&is_approximable](
        dfa_vertex_desc state_a, dfa_vertex_desc state_b) -> bool
    {
      if (/*internal_dfa[state_b].empty()*/(boost::out_degree(state_b, internal_dfa) == 0) ||
          (internal_dfa[state_a] == internal_dfa[state_b])) return true;
      else
      {
        if (internal_dfa[state_a].empty()) return false;
        else
        {
          // now both a and b are not empty
          auto first_trans_a_iter = dfa_out_edge_iter();
          auto last_trans_a_iter = dfa_out_edge_iter();
          std::tie(first_trans_a_iter, last_trans_a_iter) = boost::out_edges(state_a, internal_dfa);

          auto first_trans_b_iter = dfa_out_edge_iter();
          auto last_trans_b_iter = dfa_out_edge_iter();
          std::tie(first_trans_b_iter, last_trans_b_iter) = boost::out_edges(state_b, internal_dfa);

          /*auto approximable =*/return std::all_of(first_trans_a_iter, last_trans_a_iter, [&](dfa_edge_desc trans_a)
          {
            auto iso_predicate = [&trans_a](dfa_edge_desc trans) -> bool
            {
              return two_vmaps_are_isomorphic(internal_dfa[trans_a], internal_dfa[trans]);
            };

            auto trans_b_iter = std::find_if(first_trans_b_iter, last_trans_b_iter, iso_predicate);
            if (trans_b_iter != last_trans_b_iter)
            {
//              tfm::format(std::cerr, "continue with child\n");
              return is_approximable(boost::target(trans_a, internal_dfa),
                                     boost::target(*trans_b_iter, internal_dfa));
            }
            else
            {
              auto approx_predicate = [&trans_a](dfa_edge_desc trans) -> bool
              {
                return a_vmaps_is_included_in_b(internal_dfa[trans_a], internal_dfa[trans]);
              };

              trans_b_iter = std::find_if(first_trans_b_iter, last_trans_b_iter, approx_predicate);

              return (trans_b_iter != last_trans_b_iter) ?
                    internal_dfa[boost::target(*trans_b_iter, internal_dfa)].empty() : false;

//              if (trans_b_iter != last_trans_b_iter)
//                return internal_dfa[boost::target(*trans_b_iter, internal_dfa)].empty();
//              else return false;
            }
          });

//          if (approximable)
//          {
//            tfm::format(std::cerr, "approximable pair found\n");
//            tfm::format(std::cerr, "state a: \n");
//            std::for_each(std::begin(internal_dfa[state_a]), std::end(internal_dfa[state_a]),
//                          [&](dfa_vertex::const_reference cfi)
//            {
//              tfm::format(std::cerr, "%s: %s\n",
//                          addrint_to_hexstring(cfi->address), cfi->disassembled_name);
//            });
//            tfm::format(std::cerr, "state b: \n");
//            std::for_each(std::begin(internal_dfa[state_b]), std::end(internal_dfa[state_b]),
//                          [&](dfa_vertex::const_reference cfi)
//            {
//              tfm::format(std::cerr, "%s: %s\n",
//                          addrint_to_hexstring(cfi->address), cfi->disassembled_name);
//            });
//            if (internal_dfa[state_a].size() == 1 && internal_dfa[state_b].size() == 2) std::exit(1);
//          }
//          return approximable;
        }
      }
    };

    auto first_vertex_iter = dfa_vertex_iter();
    auto last_vertex_iter = dfa_vertex_iter();
    std::tie(first_vertex_iter, last_vertex_iter) = boost::vertices(internal_dfa);
    for (auto iter_a = first_vertex_iter; iter_a != last_vertex_iter; ++iter_a)
    {
      auto iter_b = iter_a;
      std::for_each(++iter_b, last_vertex_iter, [&](dfa_vertex_desc state_b)
      {
//        tfm::format(std::cerr, "sp");
        if (is_approximable(*iter_a, state_b))
        {
          approximation_table[std::make_pair(*iter_a, state_b)] = more_than;
          approximation_table[std::make_pair(state_b, *iter_a)] = less_than;
        }
        else if (is_approximable(state_b, *iter_a))
        {
          approximation_table[std::make_pair(*iter_a, state_b)] = less_than;
          approximation_table[std::make_pair(state_b, *iter_a)] = more_than;
        }
        else
        {
          approximation_table[std::make_pair(*iter_a, state_b)] = uncomparable;
          approximation_table[std::make_pair(state_b, *iter_a)] = uncomparable;
        }
//        tfm::format(std::cerr, "sss\n");
      });
//      tfm::format(std::cerr, "ddd\n");
    }
//    tfm::format(std::cerr, "counting\n");
    tfm::format(std::cerr, "%d approximable pair(s) founded\n",
                std::count_if(std::begin(approximation_table), std::end(approximation_table),
                              [](approx_table_t::const_reference rel) -> bool
                {
                  return (std::get<1>(rel) != uncomparable);
                }));
  }; // end of construct_approx_table lambda

  auto construct_approx_dag = [](
      const approx_table_t& approx_relation, dfa_graph_t& approx_graph) -> void
  {
    // construct a direction connected table
    auto construct_direct_connected_table = [](
        const approx_table_t& approx_relation) -> direct_connection_rel_t
    {
//      direct_connection_rel_t is_direct_connected;
      auto is_direct_connected = direct_connection_rel_t();

      auto first_vertex_iter = dfa_vertex_iter(); auto last_vertex_iter = dfa_vertex_iter();
      std::tie(first_vertex_iter, last_vertex_iter) = boost::vertices(internal_dfa);

      std::for_each(first_vertex_iter, last_vertex_iter, [&](dfa_vertex_desc state_a)
      {
        std::for_each(first_vertex_iter, last_vertex_iter, [&](dfa_vertex_desc state_b)
        {
          if (state_a != state_b)
          {
            is_direct_connected[std::make_pair(state_a, state_b)] =
                (approx_relation.at(std::make_pair(state_a, state_b)) == more_than) &&
                std::none_of(first_vertex_iter, last_vertex_iter, [&](dfa_vertex_desc state_c)
                {
                  return ((state_c != state_a) && (state_c != state_b) &&
                          (approx_relation.at(std::make_pair(state_a, state_c)) == more_than) &&
                          (approx_relation.at(std::make_pair(state_c, state_b)) == more_than));
                });

//            if (is_direct_connected[std::make_pair(state_a, state_b)])
//            {
//              tfm::format(std::cerr, "direct approximable pair found\n");
//              tfm::format(std::cerr, "state a: \n");
//              std::for_each(std::begin(internal_dfa[state_a]), std::end(internal_dfa[state_a]),
//                            [&](dfa_vertex::const_reference cfi)
//              {
//                tfm::format(std::cerr, "%s: %s\n",
//                            addrint_to_hexstring(cfi->address), cfi->disassembled_name);
//              });
//              tfm::format(std::cerr, "state b: \n");
//              std::for_each(std::begin(internal_dfa[state_b]), std::end(internal_dfa[state_b]),
//                            [&](dfa_vertex::const_reference cfi)
//              {
//                tfm::format(std::cerr, "%s: %s\n",
//                            addrint_to_hexstring(cfi->address), cfi->disassembled_name);
//              });
//            }
          }
        });
      });

      tfm::format(std::cerr, "approximation relation size %d\n",
                  std::count_if(std::begin(is_direct_connected),
                                std::end(is_direct_connected),
                                [&](direct_connection_rel_t::const_reference direct_rel) -> bool
      {
                    return std::get<1>(direct_rel);
                  }));
//      std::exit(1);

      return is_direct_connected;
    };

    // construct the approximation DAG
    auto add_states_and_relations = [](
        const direct_connection_rel_t& is_direct_connected, dfa_graph_t& approx_graph) -> void
    {
      std::for_each(std::begin(is_direct_connected), std::end(is_direct_connected),
                    [&](direct_connection_rel_t::const_reference states_rel)
      {
        auto add_state_from_dfa_to_dag = [&](dfa_vertex_desc dfa_state) -> dfa_vertex_desc
        {
          auto content = internal_dfa[dfa_state];
          auto dag_state = find_state_by_content(approx_graph, content);

          if (dag_state == boost::graph_traits<dfa_graph_t>::null_vertex())
            dag_state = boost::add_vertex(content, approx_graph);

          return dag_state;
        };

        if (std::get<1>(states_rel))
        {
//          tfm::format(std::cerr, "direct approximable pair found\n");
//          tfm::format(std::cerr, "state a: \n");
//          std::for_each(std::begin(internal_dfa[std::get<0>(std::get<0>(states_rel))]),
//              std::end(internal_dfa[std::get<0>(std::get<0>(states_rel))]),
//                        [&](dfa_vertex::const_reference cfi)
//          {
//            tfm::format(std::cerr, "%s: %s\n",
//                        addrint_to_hexstring(cfi->address), cfi->disassembled_name);
//          });
//          tfm::format(std::cerr, "state b: \n");
//          std::for_each(std::begin(internal_dfa[std::get<1>(std::get<0>(states_rel))]),
//              std::end(internal_dfa[std::get<1>(std::get<0>(states_rel))]),
//                        [&](dfa_vertex::const_reference cfi)
//          {
//            tfm::format(std::cerr, "%s: %s\n",
//                        addrint_to_hexstring(cfi->address), cfi->disassembled_name);
//          });

          auto state_a = add_state_from_dfa_to_dag(std::get<0>(std::get<0>(states_rel)));
          auto state_b = add_state_from_dfa_to_dag(std::get<1>(std::get<0>(states_rel)));

          if (!std::get<1>(boost::edge(state_a, state_b, approx_graph)))
            boost::add_edge(state_a, state_b, dfa_edge(), approx_graph);
        }
      });
      return;
    };

    tfm::format(std::cerr, "constructing direct relation table\n");
    auto is_direct_connected = construct_direct_connected_table(approx_relation);

    tfm::format(std::cerr, "adding states and relations in to DAG\n");
    add_states_and_relations(is_direct_connected, approx_graph);

//    tfm::format(std::cerr, "removing isolated states\n");
//    erase_isolated_states(approx_graph);

    return;
  }; // end of construct_approx_dag lambda

  auto save_approx_dag = [](const std::string& filename, const dfa_graph_t& approx_graph) -> void
  {
    auto write_dag_state = [&approx_graph](std::ostream& label, dfa_vertex_desc state) -> void
    {
      auto content = approx_graph[state];
      if (content.size() > 0)
      {
        tfm::format(label, "[label=\"");
        std::for_each(content.begin(), content.end(), [&](decltype(content)::const_reference cfi)
        {
          tfm::format(label, "%s: %s\n",
                      addrint_to_hexstring(cfi->address), cfi->disassembled_name);
        });
        tfm::format(label, "\"]");
      }
      else tfm::format(label, "[label=\"unknown\"]");
      return;
    };

    auto write_dag_rel = [&approx_graph](std::ostream& label, dfa_edge_desc relation) -> void
    {
      tfm::format(label, "[label=\"\"]");
      return;
    };

    auto approx_dag_file = std::ofstream(filename.c_str(),
                                         std::ofstream::out | std::ofstream::trunc);
    boost::write_graphviz(approx_dag_file, approx_graph, write_dag_state, write_dag_rel);
    approx_dag_file.close();

    return;
  }; // end of save_approx_dag lambda

  auto select_approximable_states = [](const dfa_graph_t& approx_graph) -> state_pair_t
  {
    auto first_dag_vertex_iter = dfa_vertex_iter(); auto last_dag_vertex_iter = dfa_vertex_iter();
    std::tie(first_dag_vertex_iter, last_dag_vertex_iter) = boost::vertices(approx_graph);

//    auto first_dfa_vertex_iter = dfa_vertex_iter(); auto last_dfa_vertex_iter = dfa_vertex_iter();
//    std::tie(first_dfa_vertex_iter, last_dfa_vertex_iter) = boost::vertices(internal_dfa);

    auto dag_transition_in_dfa = [&approx_graph](dfa_edge_desc trans) -> bool
    {
      auto dfa_state_a =
          find_state_by_content(internal_dfa, approx_graph[boost::source(trans, approx_graph)]);
      auto dfa_state_b =
          find_state_by_content(internal_dfa, approx_graph[boost::target(trans, approx_graph)]);

      return std::get<1>(boost::edge(dfa_state_a, dfa_state_b, internal_dfa));
    };

    // find roots
    auto root_predicate = [&](dfa_vertex_desc state) -> bool
    {
//      if ((boost::in_degree(state, approx_graph) != 0) ||
//          (boost::out_degree(state, approx_graph) == 0)) return false;
//      else
//      {
//        auto first_dag_out_edge_iter = dfa_out_edge_iter();
//        auto last_dag_out_edge_iter = dfa_out_edge_iter();
//        std::tie(first_dag_out_edge_iter,
//                 last_dag_out_edge_iter) = boost::out_edges(state, approx_graph);

//        return std::any_of(first_dag_out_edge_iter, last_dag_out_edge_iter,
//                           [&](dfa_edge_desc trans) -> bool
//        {
//          return (!approx_graph[boost::target(trans, approx_graph)].empty() &&
//                  dag_transition_in_dfa(trans));
//        });
//      }

      return ((boost::in_degree(state, approx_graph) == 0) &&
              (boost::out_degree(state, approx_graph) != 0));
    };

    auto roots = dfa_vertex_descs();
    std::copy_if(first_dag_vertex_iter,
                 last_dag_vertex_iter, std::back_inserter(roots), root_predicate);
    if (roots.empty())
    {
      return std::make_pair(boost::graph_traits<dfa_graph_t>::null_vertex(),
                            boost::graph_traits<dfa_graph_t>::null_vertex());
    }
    else
    {
      tfm::format(std::cerr, "approximable states found\n");
//      auto first_trans_iter = dfa_out_edge_iter(); auto last_trans_iter = dfa_out_edge_iter();
//      std::tie(first_trans_iter, last_trans_iter) = boost::out_edges(roots.front(), approx_graph);

//      auto selected_dag_trans_iter = std::find_if(first_trans_iter,
//                                                  last_trans_iter, dag_transition_in_dfa);
//      return std::make_pair(
//            find_state_by_content(internal_dfa,
//                                  approx_graph[boost::source(*selected_dag_trans_iter,
//                                                             approx_graph)]),
//            find_state_by_content(internal_dfa,
//                                  approx_graph[boost::target(*selected_dag_trans_iter,
//                                                             approx_graph)]));

      auto selected_root =
          *std::max_element(std::begin(roots), std::end(roots),
                           [&](dfa_vertex_desc state_a, dfa_vertex_desc state_b)
      {
        return (longest_path_length_from(state_a, approx_graph) <
                longest_path_length_from(state_b, approx_graph));
      });

      auto first_trans_iter = dfa_out_edge_iter();
      std::tie(first_trans_iter, std::ignore) = boost::out_edges(selected_root, approx_graph);

      return std::make_pair(find_state_by_content(internal_dfa, approx_graph[selected_root]),
                            find_state_by_content(internal_dfa,
                                                  approx_graph[boost::target(*first_trans_iter,
                                                                             approx_graph)]));
    }
  };

   // end of merge_approximated_states lambda

  auto erase_from_root_content = [](const dfa_vertex& content) -> void
  {
//    auto first_vertex_iter = dfa_vertex_iter(); auto last_vertex_iter = dfa_vertex_iter();
//    std::tie(first_vertex_iter, last_vertex_iter) = boost::vertices(internal_dfa);

    auto contents = get_contents_from_root(find_state_by_content(internal_dfa, content));
    std::for_each(std::begin(contents), std::end(contents), [&](const dfa_vertex& content)
    {
      erase_state_by_content(internal_dfa, content);
    });

    return;
  }; // end of erase_states_from_root lambda

  auto use_state_to_abstract_state = [](dfa_vertex_desc state_a, dfa_vertex_desc state_b) -> void
  {
    auto matching_pairs = matching_state_pairs_from(state_a, state_b);
    auto sub_states_b = get_states_from_root(state_b);

    tfm::format(std::cerr, "matching pairs: %d\n", matching_pairs.size());

//    // copy transitions to children of b
//    std::for_each(std::begin(matching_pairs), std::end(matching_pairs),
//                  [&](decltype(matching_pairs)::const_reference state_pair)
//    {
//      auto first_to_b_trans = dfa_in_edge_iter(); auto last_to_b_trans = dfa_in_edge_iter();
//      std::tie(first_to_b_trans, last_to_b_trans) = boost::in_edges(state_b, internal_dfa);

//      std::for_each(first_to_b_trans, last_to_b_trans, [&](dfa_edge_desc trans)
//      {
//        boost::add_edge(boost::source(trans, internal_dfa), state_a,
//                        internal_dfa[trans], internal_dfa);
//      });
//    });


//    auto incident_predicate = [&sub_states_b](dfa_edge_desc trans) -> bool
//    {
//      return ((std::find(std::begin(sub_states_b), std::end(sub_states_b),
//                         boost::source(trans, internal_dfa)) != std::end(sub_states_b)) ||
//              (std::find(std::begin(sub_states_b), std::end(sub_states_b),
//                         boost::target(trans, internal_dfa)) != std::end(sub_states_b)));
//    };
//    boost::remove_edge_if(incident_predicate, internal_dfa);

    // add loopback transition
//    auto loopback_trans = internal_dfa[std::get<0>(boost::edge(state_a, state_b, internal_dfa))];
//    boost::add_edge(state_a, state_a, loopback_trans, internal_dfa);

    // erase transitions from children of b
    tfm::format(std::cerr, "sub-states of b: %d\n", sub_states_b.size());

    auto from_incident_predicate = [&sub_states_b](dfa_edge_desc trans) -> bool
    {
      return ((std::find(std::begin(sub_states_b), std::end(sub_states_b),
                         boost::source(trans, internal_dfa)) != std::end(sub_states_b)) /*||
              (std::find(std::begin(sub_states_b), std::end(sub_states_b),
                         boost::target(trans, internal_dfa)) != std::end(sub_states_b))*/);
    };
    boost::remove_edge_if(from_incident_predicate, internal_dfa);

//     copy transtions to children of b to a
    std::for_each(std::begin(matching_pairs), std::end(matching_pairs),
                  [&](decltype(matching_pairs)::const_reference state_pair)
    {
      auto first_to_b_trans = dfa_in_edge_iter(); auto last_to_b_trans = dfa_in_edge_iter();
      std::tie(first_to_b_trans, last_to_b_trans) = boost::in_edges(state_b, internal_dfa);

      std::for_each(first_to_b_trans, last_to_b_trans, [&](dfa_edge_desc trans)
      {
        boost::add_edge(boost::source(trans, internal_dfa), state_a,
                        internal_dfa[trans], internal_dfa);
      });
    });

//     erase transitions to children of b
    auto to_incident_predicate = [&sub_states_b](dfa_edge_desc trans) -> bool
    {
      return (std::find(std::begin(sub_states_b), std::end(sub_states_b),
                        boost::target(trans, internal_dfa)) != std::end(sub_states_b));
    };
    boost::remove_edge_if(to_incident_predicate, internal_dfa);

    // erase isolated states
    auto first_vertex_iter = dfa_vertex_iter(); auto last_vertex_iter = dfa_vertex_iter();
    auto isolated_state_exists = true;

    while (isolated_state_exists)
    {
      isolated_state_exists = false;
      std::tie(first_vertex_iter, last_vertex_iter) = boost::vertices(internal_dfa);

      auto isolated_predicate = [&](dfa_vertex_desc state) -> bool
      {
        return ((boost::in_degree(state, internal_dfa) == 0) &&
                (boost::out_degree(state, internal_dfa) == 0));
      };

      auto isolated_state_iter =
          std::find_if(first_vertex_iter, last_vertex_iter, isolated_predicate);

      if (isolated_state_iter != last_vertex_iter)
      {
        isolated_state_exists = true;
        boost::clear_vertex(*isolated_state_iter, internal_dfa);
        boost::remove_vertex(*isolated_state_iter, internal_dfa);
      }
    }

    return;
  };

  auto state_abstract_state = [&](dfa_vertex_desc state_a, dfa_vertex_desc state_b) -> void
  {
    auto matching_pairs = matching_state_pairs_from(state_a, state_b);
    auto equiv_classes = get_equivalence_classes(matching_pairs);

    auto rep_state_equiv_states = std::map<dfa_vertex_desc, dfa_vertex_descs>();

    auto get_rep_state = [&rep_state_equiv_states](dfa_vertex_desc state) -> dfa_vertex_desc
    {
      auto rep_equiv_iter =
          std::find_if(std::begin(rep_state_equiv_states), std::end(rep_state_equiv_states),
                       [&](std::map<dfa_vertex_desc, dfa_vertex_descs>::const_reference rep_equiv)
      {
        auto equiv_states = std::get<1>(rep_equiv);
        return (std::find(std::begin(equiv_states),
                          std::end(equiv_states), state) != std::end(equiv_states));
      });

      if (rep_equiv_iter == std::end(rep_state_equiv_states))
        return boost::graph_traits<dfa_graph_t>::null_vertex();
      else return std::get<0>(*rep_equiv_iter);
    };

    auto add_out_transitions = [&](dfa_vertex_desc state) -> void
    {
      auto rep_state = get_rep_state(state);
      if (rep_state == boost::graph_traits<dfa_graph_t>::null_vertex()) rep_state = state;

      auto first_out_edge_iter = dfa_out_edge_iter();
      auto last_out_edge_iter = dfa_out_edge_iter();

      std::tie(first_out_edge_iter, last_out_edge_iter) = boost::out_edges(state, internal_dfa);

      std::for_each(first_out_edge_iter, last_out_edge_iter, [&](dfa_edge_desc trans)
      {
        auto next_state = boost::target(trans, internal_dfa);
        auto rep_next_state = get_rep_state(next_state);

        if (rep_next_state != boost::graph_traits<dfa_graph_t>::null_vertex())
        {
          boost::add_edge(rep_state, rep_next_state, internal_dfa[trans], internal_dfa);
        }
      });

      return;
    };

    std::for_each(std::begin(equiv_classes), std::end(equiv_classes),
                  [&](const dfa_vertex_descs equiv_states)
    {
      auto rep_state_content =
          std::accumulate(std::begin(equiv_states), std::end(equiv_states), dfa_vertex(),
                          [&](const dfa_vertex& init_content, dfa_vertex_desc state)
      {
        return merge_state_contents(init_content, internal_dfa[state]);
      });
      auto rep_state = boost::add_vertex(rep_state_content, internal_dfa);
      rep_state_equiv_states[rep_state] = equiv_states;
    });

    auto first_vertex_iter = dfa_vertex_iter(); auto last_vertex_iter = dfa_vertex_iter();
    std::tie(first_vertex_iter, last_vertex_iter) = boost::vertices(internal_dfa);

    std::for_each(first_vertex_iter, last_vertex_iter, [&](dfa_vertex_desc state)
    {
      // not a representing state
      if (rep_state_equiv_states.find(state) != std::end(rep_state_equiv_states))
      {
        add_out_transitions(state);
      }
    });

    auto abstracted_states = dfa_vertex_descs();
    std::for_each(first_vertex_iter, last_vertex_iter, [&](dfa_vertex_desc state)
    {
      if (get_rep_state(state) == boost::graph_traits<dfa_graph_t>::null_vertex())
        abstracted_states.push_back(state);
    });

    auto abstracted_state_contents = dfa_vertices();
    std::for_each(std::begin(abstracted_states), std::end(abstracted_states),
                  [&](dfa_vertex_desc state)
    {
      abstracted_state_contents.push_back(internal_dfa[state]);
    });

    std::for_each(std::begin(abstracted_state_contents), std::end(abstracted_state_contents),
                  [&](const dfa_vertex content)
    {
      erase_state_by_content(internal_dfa, content);
    });

    return;
  };

  dfa_graph_t     approx_dag;
  approx_table_t  approx_table;

  tfm::format(std::cerr, "constructing approximation table\n");
  construct_approx_table(approx_table);

  tfm::format(std::cerr, "constructing approximation DAG\n");
  construct_approx_dag(approx_table, approx_dag);

  tfm::format(std::cerr, "saving approximation DAG\n");
  save_approx_dag("dag_" + process_id_str + ".dot", approx_dag);

  tfm::format(std::cerr, "selecting approximable states\n");
  auto state_a = dfa_vertex_desc(); auto state_b = dfa_vertex_desc();
  std::tie(state_a, state_b) = select_approximable_states(approx_dag);

  if ((state_a != boost::graph_traits<dfa_graph_t>::null_vertex()) &&
      (state_b != boost::graph_traits<dfa_graph_t>::null_vertex()))
  {
    tfm::format(std::cerr, "abstracting states\n");
    state_abstract_state(state_a, state_b);
//    use_state_to_abstract_state(state_a, state_b);

//    merge_approximable_states(state_a, state_b);

//    auto content_a = internal_dfa[state_a]; auto content_b = internal_dfa[state_b];
//    erase_states_from_root_content(content_a);
//    erase_states_from_root_content(content_b);
  }

  return;
}

/**
 * @brief execution_dfa::save_to_file
 */
auto execution_dfa::save_to_file (const std::string& filename) -> void
{
  auto write_dfa_transition = [](std::ostream& label, dfa_edge_desc trans) -> void
  {
    auto trans_cond = internal_dfa[trans];
    if (trans_cond.size() <= 2)
    {
      tfm::format(label, "[label=\"{ ");
      std::for_each(std::begin(trans_cond), std::end(trans_cond),
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
        return std::any_of(std::begin(trans_cond), std::end(trans_cond),
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

  auto dfa_file = std::ofstream(("dfa_" + filename).c_str(),
                                std::ofstream::out | std::ofstream::trunc);
  boost::write_graphviz(dfa_file, internal_dfa,
                        std::bind(write_dfa_state, std::placeholders::_1, std::placeholders::_2),
                        std::bind(write_dfa_transition, std::placeholders::_1, std::placeholders::_2));
  dfa_file.close();
  return;
}
