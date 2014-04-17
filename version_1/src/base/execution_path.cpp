#include "execution_path.h"


/**
 * @brief calculate join (least upper bound or cartesian product) of two cfi conditions
 */
addrint_value_maps_t join_conditions(addrint_value_maps_t cond_a, addrint_value_maps_t cond_b)
{
  addrint_value_maps_t joined_cond;

  return joined_cond;
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
