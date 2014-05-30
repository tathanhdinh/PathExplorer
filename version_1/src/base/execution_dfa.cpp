#include "execution_dfa.h"
#include "../util/stuffs.h"

#include <numeric>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/copy.hpp>

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
static auto find_state_by_content (dfa_vertex_iter first_state_iter,
                                   dfa_vertex_iter last_state_iter,
                                   const dfa_vertex& content) -> dfa_vertex_desc
{
  auto state_iter =
      std::find_if(first_state_iter, last_state_iter, [&content](dfa_vertex_desc state)
  {
    return (internal_dfa[state] == content);
  });

  if (state_iter == last_state_iter)
    return boost::graph_traits<dfa_graph_t>::null_vertex();
  else return *state_iter;
};


/**
 * @brief erase_state_by_content
 */
static auto erase_state_by_content (const dfa_vertex& content) -> void
{
  auto first_state_iter = dfa_vertex_iter(); auto last_state_iter = dfa_vertex_iter();
  std::tie(first_state_iter, last_state_iter) = boost::vertices(internal_dfa);

  auto state = find_state_by_content(first_state_iter, last_state_iter, content);
  if (state != boost::graph_traits<dfa_graph_t>::null_vertex())
  {
    boost::clear_vertex(state, internal_dfa); boost::remove_vertex(state, internal_dfa);
  }
  return;
}


/**
 * @brief copy_transitions_from_state_to_state
 */
static auto copy_transitions_from_state_to_state (dfa_vertex_desc state_a,
                                                  dfa_vertex_desc state_b) -> void
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
    boost::add_edge(state_b, boost::target(trans, internal_dfa), internal_dfa[trans],
                    internal_dfa);
  });

  return;
};


/**
 * @brief erase_duplicated_transitions
 */
static auto erase_duplicated_transitions (dfa_vertex_desc state) -> void
{
  auto same_transition = [](dfa_edge_desc trans_x, dfa_edge_desc trans_y) -> bool
  {
    return ((boost::target(trans_x, internal_dfa) == boost::target(trans_y, internal_dfa)) &&
        two_vmaps_are_isomorphic(internal_dfa[trans_x], internal_dfa[trans_y]));
  };

  boost::graph_traits<dfa_graph_t>::out_edge_iterator first_trans_iter, last_trans_iter;
  boost::graph_traits<dfa_graph_t>::out_edge_iterator trans_iter, next_trans_iter;
  auto duplicated_trans_exists = true;
  while (duplicated_trans_exists)
  {
    duplicated_trans_exists = false;
    std::tie(first_trans_iter, last_trans_iter) = boost::out_edges(state, internal_dfa);
    trans_iter = first_trans_iter;
    for (next_trans_iter = trans_iter;
         trans_iter != last_trans_iter; trans_iter = next_trans_iter)
    {
      ++next_trans_iter;
      if (std::find_if(next_trans_iter, last_trans_iter,
                       std::bind(same_transition, *trans_iter, std::placeholders::_1))
          != last_trans_iter)
      {
        duplicated_trans_exists = true; break;
      }
    }
    if (duplicated_trans_exists) boost::remove_edge(*trans_iter, internal_dfa);
  };

  return;
};


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
        erase_state_by_content(content);
      });

      return;
    };

    auto find_states_by_contents = [](const dfa_vertices& contents) -> dfa_vertex_descs
    {
      // get the descriptor of the new state in the new DFA
      auto first_state_iter = dfa_vertex_iter(); auto last_state_iter = dfa_vertex_iter();
      std::tie(first_state_iter, last_state_iter) = boost::vertices(internal_dfa);

      auto states = dfa_vertex_descs();
      std::for_each(std::begin(contents), std::end(contents), [&](const dfa_vertex& content)
      {
        auto new_state = find_state_by_content(first_state_iter, last_state_iter, content);
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
        std::accumulate(std::begin(equiv_states), std::end(equiv_states), ptr_cond_direct_inss_t(),
                        /*merge_state_labels*/accum_op);
    auto new_representing_state = boost::add_vertex(new_representing_state_content, internal_dfa);

    // add the new state into the set of representing states
    representing_state_contents.push_back(new_representing_state_content);

    // copy transitions
    std::for_each(std::begin(equiv_states), std::end(equiv_states), [&](dfa_vertex_desc state)
    {
      copy_transitions_from_state_to_state(state, new_representing_state);
    });

    // erase duplicated transitions from the new state
    erase_duplicated_transitions(new_representing_state);

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

      tfm::format(std::cerr, "derived states: %d\n", derived_states.size());
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
      tfm::format(std::cerr, "initial states: %d\n", init_states.size());

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
          tfm::format(std::cerr, "equivalent state found\n");
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

  auto construct_approx_table = [](approx_table_t& approximation_table) -> void
  {
    typedef std::function<bool(dfa_vertex_desc, dfa_vertex_desc)> approx_calculation_t;
    // verify if the state b can be approximated by a, namely b "less than or equal" a
    approx_calculation_t is_approximable = [&is_approximable](
        dfa_vertex_desc state_a, dfa_vertex_desc state_b) -> bool
    {
      if (internal_dfa[state_b].empty() ||
          (internal_dfa[state_a] == internal_dfa[state_b])) return true;
      else
      {
        if (internal_dfa[state_a].empty()) return false;
        else
        {
          // now both a and b are not empty
          boost::graph_traits<dfa_graph_t>::out_edge_iterator first_trans_a_iter, last_trans_a_iter;
          std::tie(first_trans_a_iter, last_trans_a_iter) = boost::out_edges(state_a, internal_dfa);

          boost::graph_traits<dfa_graph_t>::out_edge_iterator first_trans_b_iter, last_trans_b_iter;
          std::tie(first_trans_b_iter, last_trans_b_iter) = boost::out_edges(state_b, internal_dfa);

          return std::all_of(first_trans_a_iter, last_trans_a_iter, [&](dfa_edge_desc trans_a)
          {
            auto iso_predicate = [&trans_a](dfa_edge_desc trans) -> bool
            {
              return two_vmaps_are_isomorphic(internal_dfa[trans_a], internal_dfa[trans]);
            };

            auto trans_b_iter = std::find_if(first_trans_b_iter, last_trans_b_iter, iso_predicate);
            if (trans_b_iter != last_trans_b_iter)
              return is_approximable(boost::target(trans_a, internal_dfa),
                                     boost::target(*trans_b_iter, internal_dfa));
            else
            {
              auto approx_predicate = [&trans_a](dfa_edge_desc trans) -> bool
              {
                return a_vmaps_is_included_in_b(internal_dfa[trans_a], internal_dfa[trans]);
              };

              auto trans_b_iter =
                  std::find_if(first_trans_b_iter, last_trans_b_iter, approx_predicate);
              if (trans_b_iter != last_trans_b_iter)
                return internal_dfa[boost::target(*trans_b_iter, internal_dfa)].empty();
              else return false;
            }
          });
        }
      }
    };

    auto first_vertex_iter = dfa_vertex_iter();
    auto last_vertex_iter = dfa_vertex_iter();
    std::tie(first_vertex_iter, last_vertex_iter) = boost::vertices(internal_dfa);
    std::for_each(first_vertex_iter, last_vertex_iter, [&](dfa_vertex_desc state_a)
    {
      std::for_each(first_vertex_iter, last_vertex_iter, [&](dfa_vertex_desc state_b)
      {
        if (is_approximable(state_a, state_b))
        {
          approximation_table[std::make_pair(state_a, state_b)] = more_than;
          approximation_table[std::make_pair(state_b, state_a)] = less_than;
        }
        else if (is_approximable(state_b, state_a))
        {
          approximation_table[std::make_pair(state_b, state_a)] = more_than;
          approximation_table[std::make_pair(state_a, state_b)] = less_than;
        }
        else
        {
          approximation_table[std::make_pair(state_a, state_b)] = uncomparable;
          approximation_table[std::make_pair(state_b, state_a)] = uncomparable;
        }
      });
    });
  }; // end of construct_approx_table lambda

  auto construct_approx_dag = [](
      const approx_table_t& approx_relation, dfa_graph_t& approx_graph) -> void
  {
    // construct a direction connected table
    auto construct_direct_connected_table = [](const approx_table_t& approx_relation,
        std::map<state_pair_t, bool> is_direct_connected) -> void
    {
      auto first_vertex_iter = dfa_vertex_iter(); auto last_vertex_iter = dfa_vertex_iter();
      std::tie(first_vertex_iter, last_vertex_iter) = boost::vertices(internal_dfa);

      std::for_each(first_vertex_iter, last_vertex_iter, [&](dfa_vertex_desc state_a)
      {
        std::for_each(first_vertex_iter, last_vertex_iter, [&](dfa_vertex_desc state_b)
        {
          is_direct_connected[std::make_pair(state_a, state_b)] =
              (state_a != state_b) &&
              (approx_relation.at(std::make_pair(state_a, state_b)) == more_than) &&
              std::none_of(first_vertex_iter, last_vertex_iter, [&](dfa_vertex_desc state_c)
          {
            return (approx_relation.at(std::make_pair(state_a, state_c)) &&
                approx_relation.at(std::make_pair(state_c, state_b)));
          });
        });
      });

      return;
    };

    // construct the approximation DAG
    auto add_states_and_approx_relations = [](
        const std::map<state_pair_t, bool>& is_direct_connected, dfa_graph_t& approx_graph) -> void
    {
      auto first_vertex_iter = dfa_vertex_iter(); auto last_vertex_iter = dfa_vertex_iter();
      std::tie(first_vertex_iter, last_vertex_iter) = boost::vertices(internal_dfa);

      // add states
      std::for_each(first_vertex_iter, last_vertex_iter, [&](dfa_vertex_desc state)
      {
        boost::add_vertex(internal_dfa[state], approx_graph);
      });
      // add edges
      auto first_dag_vertex_iter = dfa_vertex_iter(); auto last_dag_vertex_iter = dfa_vertex_iter();
      std::tie(first_dag_vertex_iter, last_dag_vertex_iter) = boost::vertices(approx_graph);
      std::for_each(first_vertex_iter, last_vertex_iter, [&](dfa_vertex_desc state_a)
      {
        std::for_each(first_vertex_iter, last_vertex_iter, [&](dfa_vertex_desc state_b)
        {
          if (is_direct_connected.at(std::make_pair(state_a, state_b)))
          {
            auto state_a_in_dag = find_state_by_content(first_dag_vertex_iter,
                                                        last_vertex_iter, internal_dfa[state_a]);
            auto state_b_in_dag = find_state_by_content(first_dag_vertex_iter,
                                                        last_vertex_iter, internal_dfa[state_b]);
            boost::add_edge(state_a_in_dag, state_b_in_dag, dfa_edge(), approx_graph);
          }
        });
      });

      return;
    };

    auto erase_isolated_states = [](dfa_graph_t& approx_graph) -> void
    {
      auto first_dag_vertex_iter = dfa_vertex_iter(); auto last_dag_vertex_iter = dfa_vertex_iter();
      auto isolated_vertex_exists = true;

      while (isolated_vertex_exists)
      {
        isolated_vertex_exists = false;
        std::tie(first_dag_vertex_iter, last_dag_vertex_iter) = boost::vertices(approx_graph);

        auto isolated_predicate = [&](dfa_vertex_desc state) -> bool
        {
          return (boost::in_degree(state, approx_graph) == 0) &&
              (boost::out_degree(state, approx_graph) == 0);
        };

        auto isolated_state_iter = std::find_if(first_dag_vertex_iter,
                                                last_dag_vertex_iter, isolated_predicate);
        if (isolated_state_iter != last_dag_vertex_iter)
        {
          isolated_vertex_exists = true;
          boost::clear_vertex(*isolated_state_iter, approx_graph);
          boost::remove_vertex(*isolated_state_iter, approx_graph);
        }
      }
      return;
    };

    auto is_direct_connected = std::map<state_pair_t, bool>();
    construct_direct_connected_table(approx_relation, is_direct_connected);
    add_states_and_approx_relations(is_direct_connected, approx_graph);
    erase_isolated_states(approx_graph);

    return;
  }; // end of construct_approx_dag lambda

  auto save_approx_dag = [](std::string& filename, const dfa_graph_t& approx_graph) -> void
  {
    auto write_dag_state = [&approx_graph](std::ostream& label, dfa_vertex_desc state) -> void
    {
      auto content = approx_graph[state];
      if (content.size() > 0)
      {
        tfm::format(label, "[label=\"");
        std::for_each(content.begin(), content.end(), [&](decltype(content)::const_reference cfi)
        {
          tfm::format(label, "%s: %s\n", addrint_to_hexstring(cfi->address), cfi->disassembled_name);
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

    auto first_dfa_vertex_iter = dfa_vertex_iter(); auto last_dfa_vertex_iter = dfa_vertex_iter();
    std::tie(first_dfa_vertex_iter, last_dfa_vertex_iter) = boost::vertices(internal_dfa);

    // find root
    auto root_predicate = [&approx_graph](dfa_vertex_desc state) -> bool
    {
      return (boost::in_degree(state, approx_graph) == 0);
    };
    auto root_vertex_iter =
        std::find_if(first_dag_vertex_iter, last_dag_vertex_iter, root_predicate);
    if (root_vertex_iter == last_dag_vertex_iter)
      return std::make_pair(boost::graph_traits<dfa_graph_t>::null_vertex(),
                            boost::graph_traits<dfa_graph_t>::null_vertex());
    else
    {
      auto root_state = *root_vertex_iter;
      if (boost::out_degree(root_state, approx_graph) == 0)
        return std::make_pair(find_state_by_content(first_dfa_vertex_iter,
                                                    last_dfa_vertex_iter, approx_graph[root_state]),
                              boost::graph_traits<dfa_graph_t>::null_vertex());
      else
      {
        auto first_out_edge_iter = dfa_out_edge_iter();
        std::tie(first_out_edge_iter, std::ignore) = boost::out_edges(root_state, approx_graph);
        auto approx_root_state = boost::target(*first_out_edge_iter, approx_graph);

        return std::make_pair(find_state_by_content(first_dfa_vertex_iter, last_dfa_vertex_iter,
                                                    approx_graph[root_state]),
                              find_state_by_content(first_dfa_vertex_iter, last_dfa_vertex_iter,
                                                    approx_graph[approx_root_state]));
      }
    }
  };

  typedef std::function<dfa_vertex_desc(dfa_vertex_desc, dfa_vertex_desc)> merge_states_t;
  merge_states_t merge_approximated_states = [&merge_approximated_states](
      dfa_vertex_desc state_a, dfa_vertex_desc state_b) -> dfa_vertex_desc
  {
    // get the transition from a to b
    auto loopback_trans = internal_dfa[std::get<0>(boost::edge(state_a, state_b, internal_dfa))];

    auto merged_content = merge_state_contents(internal_dfa[state_a], internal_dfa[state_b]);
    auto merged_state = boost::add_vertex(merged_content, internal_dfa);

    copy_transitions_from_state_to_state(state_a, merged_state);
    copy_transitions_from_state_to_state(state_b, merged_state);
    boost::add_edge(merged_state, merged_state, loopback_trans, internal_dfa);

    if (!internal_dfa[state_b].empty())
    {
      auto first_a_trans_iter = dfa_out_edge_iter(); auto last_a_trans_iter = dfa_out_edge_iter();
      std::tie(first_a_trans_iter, last_a_trans_iter) = boost::out_edges(state_a, internal_dfa);

      auto first_b_trans_iter = dfa_out_edge_iter(); auto last_b_trans_iter = dfa_out_edge_iter();
      std::tie(first_b_trans_iter, last_b_trans_iter) = boost::out_edges(state_b, internal_dfa);

      std::for_each(first_a_trans_iter, last_a_trans_iter, [&](dfa_edge_desc trans_a)
      {
        auto approx_predicate = [&trans_a](dfa_edge_desc trans) -> bool
        {
          return a_vmaps_is_included_in_b(internal_dfa[trans_a], internal_dfa[trans]);
        };

        auto corr_b_trans_iter = std::find_if(first_b_trans_iter,
                                              last_b_trans_iter, approx_predicate);

        auto next_state_a = boost::target(trans_a, internal_dfa);
        auto next_state_b = boost::target(*corr_b_trans_iter, internal_dfa);

        auto next_merged_state = merge_approximated_states(next_state_a, next_state_b);
        boost::add_edge(merged_state, next_merged_state, internal_dfa[trans_a], internal_dfa);
      });
    }

    return merged_state;
  }; // end of merge_approximated_states lambda

  auto erase_states_from_root_content = [](const dfa_vertex& content) -> void
  {
    typedef std::function<dfa_vertices(dfa_vertex_desc)> get_contents_from_root_t;
    get_contents_from_root_t get_contents_from_root = [&get_contents_from_root](
        dfa_vertex_desc state) -> dfa_vertices
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

    auto first_vertex_iter = dfa_vertex_iter(); auto last_vertex_iter = dfa_vertex_iter();
    std::tie(first_vertex_iter, last_vertex_iter) = boost::vertices(internal_dfa);

    auto contents = get_contents_from_root(find_state_by_content(first_vertex_iter,
                                                                 last_vertex_iter, content));
    std::for_each(std::begin(contents), std::end(contents), [&](const dfa_vertex& content)
    {
      erase_state_by_content(content);
    });

    return;
  }; // end of erase_states_from_root lambda

  dfa_graph_t     approx_dag;
  approx_table_t  approx_table;

  construct_approx_table(approx_table);
  construct_approx_dag(approx_table, approx_dag);
  save_approx_dag("dag_" + process_id_str, approx_dag);

  auto state_a = dfa_vertex_desc(); auto state_b = dfa_vertex_desc();
  std::tie(state_a, state_b) = select_approximable_states(approx_dag);
  if ((state_a != boost::graph_traits<dfa_graph_t>::null_vertex()) &&
      (state_b != boost::graph_traits<dfa_graph_t>::null_vertex()))
  {
    merge_approximated_states(state_a, state_b);

    auto content_a = internal_dfa[state_a]; auto content_b = internal_dfa[state_b];
    erase_states_from_root_content(content_a);
    erase_states_from_root_content(content_b);
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
