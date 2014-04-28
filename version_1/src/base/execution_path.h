#ifndef EXECUTION_PATH_H
#define EXECUTION_PATH_H

#include "../parsing_helper.h"
#include "cond_direct_instruction.h"

#include <functional>

typedef std::pair<addrint_value_maps_t, ptr_cond_direct_inss_t> condition_t;
typedef std::vector<condition_t> conditions_t;
//typedef std::function<conditions_t(int)> lazy_conditions_t;

class execution_path
{
public:
  order_ins_map_t   content;
  path_code_t       code;
  conditions_t      condition;
//  lazy_conditions_t lazy_condition;

  execution_path(const order_ins_map_t& current_path, const path_code_t& current_path_code);
//  void calculate_conditions();
  conditions_t lazy_condition(int n);
};

typedef std::shared_ptr<execution_path> ptr_execution_path_t;
typedef std::vector<ptr_execution_path_t> ptr_execution_paths_t;

#endif // EXECUTION_PATH_H
