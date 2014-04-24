#include "execution_path.h"

#include <functional>

/**
 * @brief calculate join (least upper bound or cartesian product) of two cfi conditions
 */
addrint_value_maps_t join_maps(const addrint_value_maps_t& cond_a,
                               const addrint_value_maps_t& cond_b)
{
  addrint_value_maps_t joined_cond;
  addrint_value_map_t joined_map;

  std::for_each(cond_a.begin(), cond_a.end(), [&](addrint_value_maps_t::value_type a_map)
  {
    std::for_each(cond_b.begin(), cond_b.end(), [&](addrint_value_maps_t::value_type b_map)
    {
      joined_map = a_map;
      if (std::all_of(b_map.begin(), b_map.end(),
                      [&](addrint_value_maps_t::value_type::value_type& b_point) -> bool
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
                          return (a_map[b_point.first] == b_map[b_point.first]);
                        }
                      }))
      {
        // if there is no conflict then add the joined map into the condition
        joined_cond.push_back(joined_map);
      }
    });
  });

  return std::move(joined_cond);
}


typedef std::function<condition_t ()> lazy_func_cond_t;

/**
 * @brief higher_join_if_needed
 */
lazy_func_cond_t higher_join_if_needed(lazy_func_cond_t in_cond)
{
  auto new_cond = [&]() -> condition_t
  {
    condition_t real_in_cond = in_cond();
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
condition_t join_maps_in_condition(condition_t in_cond)
{
  return in_cond;
}


/**
 * @brief fix
 */
condition_t y_fix(std::function<decltype(join_maps_in_condition)> join_func)
{
  // explicit Y combinator: Y f = f (Y f)
//  return join_func(fix(join_func));

  // implicit Y combinator: Y f = f (\x -> (Y f) x)
  return std::bind(join_func, std::bind(&y_fix, join_func))();
}


/**
 * @brief calculate stabilized condition
 */
auto stablizing(const condition_t& prev_cond) -> condition_t
{
  // verify if two maps a and b have an intersection
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

  // verify if two maps a and b have the same type
  auto have_the_same_type = [&](const addrint_value_map_t& map_a,
                                const addrint_value_map_t& map_b) -> bool
  {
    return (// verify if the map a and be have the same size,
            (map_a.size() == map_b.size()) &&
            // yes, now verify if every element of a is also an element of b
            std::all_of(map_a.begin(), map_a.end(),
                        [&](addrint_value_map_t::value_type map_a_elem) -> bool
            {
              return (map_b.find(map_a_elem.first) != map_b.end());
            }));
  };

  condition_t new_cond = prev_cond;

  // the condition is a cartesian product A x ... x B x ...
  // verify if there exist some intersected elements
  auto cond_elem_a = prev_cond.begin();
  for (; cond_elem_a != prev_cond.end(); ++cond_elem_a)
  {
    auto cond_elem_b = std::next(cond_elem_a);
    for (; cond_elem_b != prev_cond.end(); ++cond_elem_b)
    {
      // exist
      if (have_intersection(*(cond_elem_a->begin()), *(cond_elem_b->begin())))
      {
        // then join them into a single element
        auto joined_map = join_maps(*cond_elem_a, *cond_elem_b);

        // erase element a from the condition
        auto map_a = *(cond_elem_a->begin());
        for (auto cond_elem = new_cond.begin(); cond_elem != new_cond.end(); ++cond_elem)
        {
          if (have_the_same_type(map_a, *(cond_elem->begin())))
          {
            new_cond.erase(cond_elem); break;
          }
        }

        // erase element b from the condition
        auto map_b = *(cond_elem_b->begin());
        for (auto cond_elem = new_cond.begin(); cond_elem != new_cond.end(); ++cond_elem)
        {
          if (have_the_same_type(map_b, *(cond_elem->begin())))
          {
            new_cond.erase(cond_elem); break;
          }
        }

        // push the joined element into the condition
        new_cond.push_back(joined_map); break;
      }
    }

    // that mean some intersected elements have been detected
    if (cond_elem_b != prev_cond.end()) break;
  }

  // tail recursion
  if (cond_elem_a == prev_cond.end())
  {
    return std::move(new_cond);
  }
  else
  {
    return std::move(std::bind(&stablizing, new_cond)());
  }
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
void execution_path::calculate_condition()
{
  decltype(this->condition) raw_condition;

  typedef decltype(this->code) code_t;
  code_t::size_type current_code_order = 0;

  typedef decltype(this->content) content_t;
  std::for_each(this->content.begin(), this->content.end(), [&](content_t::value_type order_ins)
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
          raw_condition.push_back(current_cfi->first_input_projections);
        else raw_condition.push_back(current_cfi->second_input_projections);
      }
      current_code_order++;
    }
  });

  this->condition = stablizing(raw_condition);

  return;
}
