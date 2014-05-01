#include "execution_path.h"

#include "../util/stuffs.h"
#include <functional>
#include <algorithm>

typedef std::function<conditions_t ()> lazy_func_cond_t;

/**
 * @brief higher_join_if_needed
 */
lazy_func_cond_t higher_join_if_needed(lazy_func_cond_t in_cond)
{
  auto new_cond = [&]() -> conditions_t
  {
    conditions_t real_in_cond = in_cond();
    return real_in_cond;
  };
  return new_cond;
}


/**
 * @brief higher_fix
 */
lazy_func_cond_t higher_fix(std::function<decltype(higher_join_if_needed)> join_func)
{
  // explicit Y combinator: Y f = f (Y f)
//  return join_func(fix(join_func));

  // implicit Y combinator: Y f = f (\x -> (Y f) x)
  return std::bind(join_func, std::bind(&higher_fix, join_func))();
}


/**
 * @brief join_if_needed
 */
conditions_t join_maps_in_condition(conditions_t in_cond)
{
  return in_cond;
}


/**
 * @brief fix
 */
conditions_t y_fix(std::function<decltype(join_maps_in_condition)> join_func)
{
  // explicit Y combinator: Y f = f (Y f)
//  return join_func(fix(join_func));

  // implicit Y combinator: Y f = f (\x -> (Y f) x)
  return std::bind(join_func, std::bind(&y_fix, join_func))();
}


/**
 * @brief verifying if two maps a and b are of the same type, i.e. the sets of addresses of a and
 * of b are the same
 */
auto are_of_the_same_type (const addrint_value_map_t& map_a,
                           const addrint_value_map_t& map_b) -> bool
{
  return ((map_a.size() == map_b.size()) &&
          std::all_of(map_a.begin(), map_a.end(),
                      [&](addrint_value_map_t::value_type map_a_elem) -> bool
                      {
                        // verify if every element of a is also element of b
                        return (map_b.find(map_a_elem.first) != map_b.end());
                      }));
};


/**
 * @brief verify if two maps a and b are isomorphic, i.e. there is an isomorphism (function) f
 * making
 *                                    map_a
 *                             A --------------> V
 *                             |                 |
 *                           f |                 | f = 1
 *                             V                 V
 *                             B --------------> V
 *                                    map_b
 * commutative
 */
auto are_isomorphic (const addrint_value_map_t& map_a, const addrint_value_map_t& map_b) -> bool
{
  addrint_value_map_t::const_iterator map_b_iter = map_b.begin();

  return ((map_a.size() == map_b.size()) &&
          std::all_of(map_a.begin(), map_a.end(),
                      [&](addrint_value_map_t::const_reference map_a_elem) -> bool
          {
            return (map_a_elem.second == (map_b_iter++)->second);
          }));
}


/**
 * @brief verify if hom(A,V) and hom(B,V) are isomorphic, i.e. there is an isomorphism (functor) F
 * making
 *                                    map_a
 *                             A --------------> V
 *                             |                 |
 *                           F |                 | F = 1
 *                             V                 V
 *                             B --------------> V
 *                                    map_b
 * commutative
 */
auto are_isomorphic (const addrint_value_maps_t& maps_a, const addrint_value_maps_t& maps_b) -> bool
{
  return ((maps_a.size() == maps_b.size()) &&
          std::all_of(maps_a.begin(), maps_a.end(),
                      [&](addrint_value_maps_t::const_reference maps_a_elem) -> bool
  {
    return std::any_of(maps_b.begin(), maps_b.end(),
                       [&](addrint_value_maps_t::const_reference maps_b_elem) -> bool
    {
      return are_isomorphic(maps_a_elem, maps_b_elem);
    });
  }));
}


/**
 * @brief calculate stabilized condition which can be considered as a cartesian product
 *                             A x ... x B x ...
 */
auto stabilize (const conditions_t& input_cond) -> conditions_t
{
  // lambda verifying if two maps a and b have an intersection
  auto have_intersection = [&](const addrint_value_map_t& map_a,
                               const addrint_value_map_t& map_b) -> bool
  {
    tfm::format(std::cerr, "finding intersection %d %d \n", map_a.size(), map_b.size());

    // verify if there is some element of a is also an element of b
    return std::any_of(map_a.begin(), map_a.end(),
                       [&](addrint_value_map_t::value_type map_a_elem) -> bool
    {
      return (map_b.find(map_a_elem.first) != map_b.end());
    });
  };

  // lambda calculating join (least upper bound or cartesian product) of two maps
  auto join_maps = [&](const addrint_value_maps_t& cond_a,
                       const addrint_value_maps_t& cond_b) -> addrint_value_maps_t
  {
    addrint_value_maps_t joined_cond;
    addrint_value_map_t joined_map;

    std::for_each(cond_a.begin(), cond_a.end(), [&](addrint_value_maps_t::const_reference a_map)
    {
      std::for_each(cond_b.begin(), cond_b.end(), [&](addrint_value_maps_t::const_reference b_map)
      {
        joined_map = a_map;
        if (std::all_of(b_map.begin(), b_map.end(),
                        [&](addrint_value_maps_t::value_type::const_reference b_point) -> bool
        {
          // verify if the source of b_point exists in the a_map
          if (a_map.find(b_point.first) == a_map.end())
          {
            // does not exist, then add b_point into the joined map
            joined_map.insert(b_point);
            return true;
          }
          else
          {
            // exists, then verify if there is a conflict between a_map and b_map
            return (a_map.at(b_point.first) == b_map.at(b_point.first));
          }
        }))
        {
          // if there is no conflict then add the joined map into the condition
          joined_cond.push_back(joined_map);
        }
      });
    });

    // using move semantics may be not quite effective because of return value optimization
    return std::move(joined_cond);
  };

  // lambda calculating join of two list of cfi
  auto join_cfis = [&](const ptr_cond_direct_inss_t& cfis_a,
                       const ptr_cond_direct_inss_t& cfis_b) -> ptr_cond_direct_inss_t
  {
    auto joined_list = cfis_a;
    std::for_each(cfis_b.begin(), cfis_b.end(), [&](ptr_cond_direct_inss_t::const_reference cfi_b)
    {
      // verify if a element of cfis_b exists also in cfis_a
      if (std::find(cfis_a.begin(), cfis_a.end(), cfi_b) != cfis_a.end())
      {
        // does not exist, then add it
        joined_list.push_back(cfi_b);
      }
    });

    // using move semantics may be not quite effective because of return value optimization
    return std::move(joined_list);
  };

  // lambda erasing some sub-condition of given type from a path condition
  auto erase_from = [&](const addrint_value_map_t cond_type, conditions_t path_cond) -> void
  {
    for (auto cond_elem = path_cond.begin(); cond_elem != path_cond.end(); ++cond_elem)
    {
      if (are_of_the_same_type(cond_type, *(cond_elem->first.begin())))
        path_cond.erase(cond_elem); break;
    }
    return;
  };


  // the following loop makes the condition converge on a stabilized state: it modifies the
  // condition by merging intersected sub-conditions until no such intersection is found
  conditions_t examined_cond = input_cond;
  bool intersection_exists;

  tfm::format(std::cerr, "find fix-point\n");

  do
  {
    intersection_exists = false;

    tfm::format(std::cerr, "examined condition size %d\n", examined_cond.size());

    // for each pair of sub-conditions
    for (auto cond_elem_a = examined_cond.begin(); cond_elem_a != examined_cond.end();
         ++cond_elem_a)
    {
      tfm::format(std::cerr, "sub-condition a %d\n", cond_elem_a->first.size());
      for (auto cond_elem_b = std::next(cond_elem_a); cond_elem_b != examined_cond.end();
           ++cond_elem_b)
      {
        // verify if they have intersection
        if (have_intersection(*(cond_elem_a->first.begin()), *(cond_elem_b->first.begin())))
        {
          // yes
          intersection_exists = true;

          // then join their maps
          auto joined_maps = join_maps(cond_elem_a->first, cond_elem_b->first);
          // and join their cfi
          auto joined_cfis = join_cfis(cond_elem_a->second, cond_elem_b->second);

          // temporarily save the intersected sub-conditions;
          auto map_a = *(cond_elem_a->first.begin()); auto map_b = *(cond_elem_b->first.begin());

          // erase sub-condition a and b from the path condition, note the side-effect: the input
          // condition examining_cond will be modified
          erase_from(map_a, examined_cond); erase_from(map_b, examined_cond);

          // add joined condition into the path condition
          examined_cond.push_back(std::make_pair(joined_maps, joined_cfis));

          // because the curr_cond has been modified, both iterators cond_elem_a and cond_elem_b
          // have been made invalid, breakout to restart the verification
          break;
        }
      }
      if (intersection_exists) break;
    }
  }
  while (intersection_exists);

  return examined_cond;
}


/**
 * @brief verify if the condition is recursive
 */
auto is_recursive(const conditions_t& stabilized_cond) -> bool
{
  return ((stabilized_cond.size() >= 2) &&
          (are_isomorphic(stabilized_cond.crbegin()->first,
                          std::next(stabilized_cond.crbegin())->first)));
}


/**
 * @brief calculate the non-recursive order of the condition
 */
auto order(const conditions_t& stabilized_cond) -> int
{
  conditions_t::const_reverse_iterator examining_cond_iter = stabilized_cond.crbegin();
  std::all_of(stabilized_cond.crbegin(), stabilized_cond.crend(),
              [&](conditions_t::const_reference cond_elem) -> bool
  {
    return ((stabilized_cond.crend() - examining_cond_iter >= 2) &&
            are_isomorphic(examining_cond_iter->first, (++examining_cond_iter)->first));
  });

  return stabilized_cond.crend() - examining_cond_iter;
}


/**
 * @brief reconstruct path condition as a cartesian product A x ... x B x ...
 */
static auto calculate_from(const order_ins_map_t& current_path,
                           const path_code_t& current_path_code) -> conditions_t
{
  conditions_t raw_condition;
  std::size_t current_code_order = 0;

  tfm::format(std::cerr, "current path size %d with code size %d\n", current_path.size(),
              current_path_code.size());
  std::for_each(current_path.begin(), current_path.end(),
                [&](order_ins_map_t::const_reference order_ins)
  {
    // verify if the current instruction is a cfi
    if (order_ins.second->is_cond_direct_cf)
    {
      // yes, then downcast it as a CFI
      auto current_cfi = std::static_pointer_cast<cond_direct_instruction>(order_ins.second);
      // verify if this CFI is resolved
      if (current_cfi->is_resolved)
      {
        tfm::format(std::cerr, "added cfi %d %d\n", current_cfi->first_input_projections.size(),
                    current_cfi->second_input_projections.size());

        // look into the path code to know which condition should be added
        if (!current_path_code[current_code_order])
          raw_condition.push_back(std::make_pair(current_cfi->first_input_projections,
                                                 ptr_cond_direct_inss_t(1, current_cfi)));
        else raw_condition.push_back(std::make_pair(current_cfi->second_input_projections,
                                                    ptr_cond_direct_inss_t(1, current_cfi)));
      }
      current_code_order++;
    }
  });

  tfm::format(std::cerr, "stabilizing condition\n");
  return stabilize(raw_condition);
}


/**
 * @brief constructor
 */
execution_path::execution_path(const order_ins_map_t& current_path,
                               const path_code_t& current_path_code)
{
  this->content = current_path;
  this->code = current_path_code;
  this->condition = calculate_from(current_path, current_path_code);
  this->condition_is_recursive = is_recursive(this->condition);
  this->condition_order = order(this->condition);
}


/**
 * @brief predict a lazy condition
 */
auto execution_path::lazy_condition(int n) -> conditions_t
{
  conditions_t lazy_cond(this->condition.begin(),
                         std::next(this->condition.begin(), std::min(n, this->condition_order)));
  if (this->condition_is_recursive)
  {
    while (this->condition_order < n--) lazy_cond.push_back(this->condition.back());
  }

  return std::move(lazy_cond);
}


#if !defined(NDEBUG)
/**
 * @brief show_path_condition
 */
auto show_path_condition(const ptr_execution_paths_t& exp_paths) -> void
{
  auto show_cond = [&](ptr_execution_paths_t::const_reference exp_path, int i) -> void
  {
    tfm::format(std::cout, "| ");
    std::for_each(exp_path->condition.at(i).first.begin()->begin(),
                  exp_path->condition.at(i).first.begin()->end(),
                  [&](addrint_value_map_t::const_reference cond_elem)
    {
      tfm::format(std::cout, "%s ", addrint_to_hexstring(cond_elem.first));
    });
    return;
  };

  std::for_each(exp_paths.begin(), exp_paths.end(),
                [&](ptr_execution_paths_t::const_reference exp_path)
  {
    auto i = 0;
    for (; i < exp_path->condition_order; ++i) show_cond(exp_path, i);
    if (exp_path->condition_is_recursive) tfm::format(std::cout, "|*\n");
    else tfm::format(std::cout, "|\n");
  });
  return;
}
#endif
