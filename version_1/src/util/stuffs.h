#ifndef STUFFS_H
#define STUFFS_H

#include "../common.h"

auto addrint_to_hexstring       (ADDRINT input)                -> std::string;

auto path_code_to_string        (const path_code_t& path_code) -> std::string;

auto two_maps_are_identical     (const addrint_value_map_t& map_a,
                                 const addrint_value_map_t& map_b) -> bool;

auto is_input_dep_cfi           (ptr_instruction_t tested_ins) -> bool;

auto is_resolved_cfi            (ptr_instruction_t tested_ins) -> bool;

auto look_for_saved_instance    (const ptr_cond_direct_ins_t cfi,
                                 const path_code_t path_code) -> ptr_cond_direct_ins_t;

auto save_static_trace          (const std::string& filename)  -> void;

auto save_explored_trace        (const std::string& filename)  -> void;

auto save_tainting_graph        (df_diagram& dta_graph,
                                 const std::string& filename)  -> void;

auto save_cfi_inputs            (const std::string& filename)  -> void;

auto save_path_condition        (const conditions_t& cond, const std::string& filename)  -> void;

auto show_exploring_progress    () -> void;

auto show_cfi_logged_inputs     () -> void;

#endif // STUFFS_H
