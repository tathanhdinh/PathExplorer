#ifndef STUFFS_H
#define STUFFS_H

#include "../operation/common.h"

auto save_static_trace          (const std::string& filename) -> void;

auto save_explored_trace        (const std::string& filename) -> void;

auto save_tainting_graph        (df_diagram& dta_graph, const std::string& filename) -> void;

auto addrint_to_hexstring       (ADDRINT input) -> std::string;

auto rollback_bound_is_reached  () -> bool;

#if !defined(DISABLE_FSA)
auto path_code_to_string        (const path_code_t& path_code) -> std::string;
#endif

auto show_exploring_progress    () -> void;

#endif // STUFFS_H
