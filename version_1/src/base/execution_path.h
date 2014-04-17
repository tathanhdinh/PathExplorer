#ifndef EXECUTION_PATH_H
#define EXECUTION_PATH_H

#include "../parsing_helper.h"

#include "cond_direct_instruction.h"

class execution_path
{
public:
  order_ins_map_t       content;
  path_code_t           code;
  addrint_value_maps_t  condition;

  execution_path(const order_ins_map_t& current_path, const path_code_t& current_path_code);
  void calculate_condition();
};

typedef std::shared_ptr<execution_path> ptr_execution_path_t;
typedef std::vector<ptr_execution_path_t> ptr_execution_paths_t;

#endif // EXECUTION_PATH_H
