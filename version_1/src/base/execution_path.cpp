#include "execution_path.h"

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
 * @brief verifying if two maps a and b are of the same type (i.e. the sets of addresses of a and
 * of b are the same)
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
 * @brief verify if two maps a and b are isomorphic (i.e. there is an isomorphism (function) f
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
 * @brief verify if hom(A,V) and hom(B,V) are isomorphic (i.e. there is an isomorphism (functor) F
 * making
 *                                    map_a
 *                             A --------------> V
 *                             |                 |
 *                           F |                 | F = 1
 *                             V                 V
 *                             B --------------> V
 *                                    map_b
 * commutative)
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
 * @brief calculate stabilized condition
 */
auto stabilize_condition (const conditions_t& prev_cond) -> conditions_t
{
  // lambda verifying if two maps a and b have an intersection
  auto have_intersection = [&](const addrint_value_map_t& map_a,
                               const addrint_value_map_t& map_b) -> bool
  {
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

  // the condition is a cartesian product A x ... x B x ...
  conditions_t new_cond = prev_cond;

  // verify if there exist some intersected elements
  auto cond_elem_a = prev_cond.begin();
  for (; cond_elem_a != prev_cond.end(); ++cond_elem_a)
  {
    auto cond_elem_b = std::next(cond_elem_a);
    for (; cond_elem_b != prev_cond.end(); ++cond_elem_b)
    {
      // exist
      if (have_intersection(*(cond_elem_a->first.begin()), *(cond_elem_b->first.begin())))
      {
        // then join their maps
        auto joined_map = join_maps(cond_elem_a->first, cond_elem_b->first);

        // and join their cfis
        auto joined_cfis = join_cfis(cond_elem_a->second, cond_elem_b->second);

        // erase element a from the condition
        auto map_a = *(cond_elem_a->first.begin());
        for (auto cond_elem = new_cond.begin(); cond_elem != new_cond.end(); ++cond_elem)
        {
          if (are_of_the_same_type(map_a, *(cond_elem->first.begin())))
          {
            new_cond.erase(cond_elem); break;
          }
        }

        // erase element b from the condition
        auto map_b = *(cond_elem_b->first.begin());
        for (auto cond_elem = new_cond.begin(); cond_elem != new_cond.end(); ++cond_elem)
        {
          if (are_of_the_same_type(map_b, *(cond_elem->first.begin())))
          {
            new_cond.erase(cond_elem); break;
          }
        }

        // push the joined element into the condition
        new_cond.push_back(std::make_pair(joined_map, joined_cfis)); break;
      }
    }

    // that mean some intersected elements have been detected
    if (cond_elem_b != prev_cond.end()) break;
  }

  // tail recursion
  if (cond_elem_a == prev_cond.end())
  {
    // using move semantics may be not quite effective because of return value optimization
    return std::move(new_cond);
  }
  else
  {
    // using move semantics may be not quite effective because of return value optimization
    return std::move(std::bind(&stabilize_condition, new_cond)());
  }
}


auto generalize_condition(const conditions_t& prev_condition) -> lazy_conditions_t
{
  return [&](int n)
  {
    std::for_each(prev_condition.rbegin(), prev_condition.rend(),
                  [&](conditions_t::const_reference cond)
    {

    });
  };
}


/**
 * @brief constructor
 */
execution_path::execution_path(const order_ins_map_t& current_path,
                               const path_code_t& current_path_code)
{
  this->content = current_path;
  this->code = current_path_code;
}


/**
 * @brief reconstruct path condition as a cartesian product A x ... x B x ...
 */
void execution_path::calculate_conditions()
{
  decltype(this->condition) raw_condition;

  typedef decltype(this->code) code_t;
  code_t::size_type current_code_order = 0;

  typedef decltype(this->content) content_t;
  std::for_each(this->content.begin(), this->content.end(), [&](content_t::const_reference order_ins)
  {
    // verify if the current instruction is a cfi
    if (order_ins.second->is_cond_direct_cf)
    {
      // yes, then downcast it as a CFI
      auto current_cfi = std::static_pointer_cast<cond_direct_instruction>(order_ins.second);
      // verify if this CFI is resolved
      if (current_cfi->is_resolved)
      {
        // look into the path code to know which condition should be added
        if (!this->code[current_code_order])
          raw_condition.push_back(std::make_pair(current_cfi->first_input_projections,
                                                 ptr_cond_direct_inss_t(1, current_cfi)));
        else raw_condition.push_back(std::make_pair(current_cfi->second_input_projections,
                                                    ptr_cond_direct_inss_t(1, current_cfi)));
      }
      current_code_order++;
    }
  });

  this->condition = stabilize_condition(raw_condition);

  return;
}
