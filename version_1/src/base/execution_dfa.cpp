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
  tfm::format(std::cerr, "======\nadd a new execution path\n");
  auto prev_state = initial_state; auto mismatch = false;
  std::for_each(exec_path->condition.begin(), exec_path->condition.end(),
                [&](decltype(exec_path->condition)::const_reference sub_cond)
  {
    // trick: once mismatch is assigned to true then it will be never re-assigned to false
//    auto current_state = mismatch ?
//          boost::add_vertex(ptr_cond_direct_inss_t(), internal_dfa) :
//          get_next_state(prev_state, std::get<0>(sub_cond));

//    mismatch = mismatch ?
//          mismatch : (current_state == boost::graph_traits<dfa_graph_t>::null_vertex());

//    tfm::format(std::cerr, "mismatch: %b\n", mismatch);

    dfa_vertex_desc current_state;
    if (!mismatch)
    {
      current_state = get_next_state(prev_state, std::get<0>(sub_cond));
      mismatch = (current_state == boost::graph_traits<dfa_graph_t>::null_vertex());
    }

    if (mismatch)
    {
      internal_dfa[prev_state] = std::get<1>(sub_cond);
//      tfm::format(std::cerr, "before adding transition\n");
      current_state = boost::add_vertex(ptr_cond_direct_inss_t(), internal_dfa);
      boost::add_edge(prev_state, current_state, std::get<0>(sub_cond), internal_dfa);
//      tfm::format(std::cerr, "after adding transition\n");
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
  return;
}


/**
 * @brief write_dfa_transition
 */
//static auto write_dfa_transition(std::ostream& label, dfa_edge_desc trans) -> void
//{
//  auto trans_cond = internal_dfa[trans];
//  if (trans_cond.size() <= 2)
//  {
//    tfm::format(label, "{ ");
//    std::for_each(trans_cond.begin(), trans_cond.end(),
//                  [&label](decltype(trans_cond)::const_reference trans_elem)
//    {
//      std::for_each(trans_elem.begin(), trans_elem.end(),
//                    [&](addrint_value_map_t::const_reference addr_val)
//      {
//        tfm::format(label, "%d ", std::get<1>(addr_val));
//      });
//    });
//    tfm::format(label, "%s", "}");
//  }
//  else
//  {
//    tfm::format(label, "not {");
//    std::for_each(trans_cond.begin(), trans_cond.end(),
//                  [&label](decltype(trans_cond)::const_reference trans_elem)
//    {
//      for (auto val = 0; val <= std::numeric_limits<UINT8>::max(); ++val)
//      {
//        if (std::find_if_not(trans_elem.begin(), trans_elem.end(),
//                             [&val](addrint_value_map_t::const_reference addr_val)
//        {
//          return (std::get<1>(addr_val) == val);
//        }) == trans_elem.end())
//        {
//          tfm::format(label, "%d ", val);
//        }
//      }
//    });
//    tfm::format(label, "}");
//  }

//  return;
//}


/**
 * @brief write_dfa_state
 */
//static auto write_dfa_state(std::ostream& label, dfa_vertex_desc state) -> void
//{
//  auto state_val = internal_dfa[state];
//  tfm::format(label, "%s:%s", addrint_to_hexstring(state_val.front()->address),
//              state_val.front()->disassembled_name);
//  return;
//}

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
      tfm::format(label, "{ ");
      std::for_each(trans_cond.begin(), trans_cond.end(),
                    [&label](decltype(trans_cond)::const_reference trans_elem)
      {
        std::for_each(trans_elem.begin(), trans_elem.end(),
                      [&](addrint_value_map_t::const_reference addr_val)
        {
          tfm::format(label, "%d ", std::get<1>(addr_val));
        });
      });
      tfm::format(label, "%s", "}");
    }
    else
    {
//      tfm::format(std::cerr, "condition size: %d\n", trans_cond.size());
      tfm::format(label, "not {");

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
        if (value_exists(val)) tfm::format(label, "%d ", val);
      }

//      std::for_each(trans_cond.begin(), trans_cond.end(),
//                    [&label](decltype(trans_cond)::const_reference trans_elem)
//      {
//        for (auto val = 0; val <= std::numeric_limits<UINT8>::max(); ++val)
//        {
//          if (std::find_if(trans_elem.begin(), trans_elem.end(),
//                           [&val](addrint_value_map_t::const_reference addr_val)
//          {
//            return (std::get<1>(addr_val) == val);
//          }) == trans_elem.end())
//          {
//            tfm::format(label, "%d ", val);
//          }
//        }
//      });
      tfm::format(label, "}");
    }

    return;
  };

  auto write_dfa_state = [](std::ostream& label, dfa_vertex_desc state) -> void
  {
    auto state_val = internal_dfa[state];
    if (state_val.size() > 0)
      tfm::format(label, "%s:%s", addrint_to_hexstring(state_val.front()->address),
                  state_val.front()->disassembled_name);
    else
      tfm::format(label, "%c", 193);
    return;
  };

  std::ofstream dfa_file(("dfa_" + filename).c_str(), std::ofstream::out | std::ofstream::trunc);
  boost::write_graphviz(dfa_file, internal_dfa,
                        std::bind(write_dfa_state, std::placeholders::_1, std::placeholders::_2),
                        std::bind(write_dfa_transition, std::placeholders::_1, std::placeholders::_2));
  dfa_file.close();
  return;
}
