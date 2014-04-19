#include "execution_path.h"

#include <functional>

/**
 * @brief calculate join (least upper bound or cartesian product) of two cfi conditions
 */
addrint_value_maps_t join_conditions(const addrint_value_maps_t& cond_a,
                                     const addrint_value_maps_t& cond_b)
{
  addrint_value_maps_t joined_cond;

  std::for_each(cond_a.begin(), cond_a.end(), [&](addrint_value_maps_t::value_type a_map)
  {
    std::for_each(cond_b.begin(), cond_b.end(), [&](addrint_value_maps_t::value_type b_map)
    {
      addrint_value_map_t joined_map = a_map;
      bool is_conflict = false;

      std::for_each(b_map.begin(), b_map.end(),
                    [&](addrint_value_maps_t::value_type::value_type b_point)
      {
        if (!is_conflict)
        {
          // verify if the source of b_point exists in the a_map
          if (a_map.find(b_point.first) != a_map.end())
          {
            // exists, then verify if there is a conflict between corresponding targets
            // in a_map and b_map
            if (a_map[b_point.first] != b_map[b_point.first])
            {
              is_conflict = true;
            }
          }
          else
          {
            // does not exist, then add b_point into the joined map
            joined_map.insert(b_point);
          }
        }
      });

      if (!is_conflict) joined_cond.push_back(joined_map);
    });
  });

  return joined_cond;
}


/**
 * @brief join_if_needed
 */
condition_t join_if_needed(condition_t in_cond)
{
  condition_t out_cond;
  return out_cond;
}


condition_t fix(std::function<decltype(join_if_needed)> join_func)
{
//  return join_func(fix(join_func));
  return std::bind(join_func, std::bind(&fix, join_func), std::placeholders::_1);
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
 * @brief reconstruct path condition
 */
void execution_path::calculate_condition()
{

  typedef decltype(this->content) ins_at_order_t;
//  ins_at_order_t::mapped_type prev_ins;
  std::for_each(this->content.begin(), this->content.end(), [&](ins_at_order_t::value_type order_ins)
  {

  });

  return;
}
