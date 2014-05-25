#include "execution_dfa.h"
#include "../util/stuffs.h"

#include <boost/graph/graphviz.hpp>

typedef ptr_cond_direct_inss_t                                dfa_vertex;
typedef addrint_value_maps_t                                  dfa_edge;
typedef boost::adjacency_list<boost::listS, boost::vecS,
                              boost::bidirectionalS,
                              dfa_vertex, dfa_edge>           dfa_graph_t;
typedef boost::graph_traits<dfa_graph_t>::vertex_descriptor   dfa_vertex_desc;
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
auto execution_dfa::add_exec_path (ptr_exec_path_t exec_path) -> void
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
  auto prev_state = initial_state; auto mismatch = false;
  std::for_each(exec_path->condition.begin(), exec_path->condition.end(),
                [&](decltype(exec_path->condition)::const_reference sub_cond)
  {
    // trick: if mismatch is true then it will be never re-assigned to false
    auto current_state = mismatch ?
          boost::add_vertex(ptr_cond_direct_inss_t(), internal_dfa) :
          get_next_state(prev_state, std::get<0>(sub_cond));
    mismatch = (current_state == boost::graph_traits<dfa_graph_t>::null_vertex());

    if (mismatch)
    {
      internal_dfa[prev_state] = std::get<1>(sub_cond);
      boost::add_edge(prev_state, current_state, std::get<0>(sub_cond), internal_dfa);
    }

    prev_state = current_state;
  });

  return;
}


/**
 * @brief execution_dfa::save_to_file
 */
auto execution_dfa::save_to_file(const std::string& filename) -> void
{

  return;
}
