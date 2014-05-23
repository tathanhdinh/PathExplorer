#ifndef EXECUTION_PATH_H
#define EXECUTION_PATH_H

#include "../parsing_helper.h"
#include "cond_direct_instruction.h"

class execution_path;

typedef std::pair<addrint_value_maps_t, ptr_cond_direct_inss_t> condition_t;
typedef std::vector<condition_t>                                conditions_t;
typedef std::vector<ADDRINT>                                    addrints_t;
typedef std::shared_ptr<execution_path>                         ptr_exec_path_t;
typedef std::vector<ptr_exec_path_t>                            ptr_exec_paths_t;

class execution_path
{
public:
  order_ins_map_t   content;
  path_code_t       code;
  conditions_t      condition;
  int               condition_order;
  bool              condition_is_recursive;

  execution_path(const order_ins_map_t& current_path, const path_code_t& current_path_code);
  auto calculate_condition() -> void;
  auto lazy_condition(int n) -> conditions_t;
};

auto calculate_exec_path_conditions (ptr_exec_paths_t& exec_paths) -> void;

#if !defined(NDEBUG)
auto show_path_condition(const ptr_exec_paths_t& exec_paths) -> void;
auto show_path_condition(const ptr_exec_path_t& exec_path) -> void;
#endif

#endif // EXECUTION_PATH_H
