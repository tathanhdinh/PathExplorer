#ifndef STUFFS_H
#define STUFFS_H

#include "../common.h"

auto is_input_dep_cfi           (ptr_instruction_t tested_ins) -> bool;

auto is_resolved_cfi            (ptr_instruction_t tested_ins) -> bool;

auto save_static_trace          (const std::string& filename)  -> void;

auto save_explored_trace        (const std::string& filename)  -> void;

auto save_tainting_graph        (df_diagram& dta_graph,
                                 const std::string& filename)  -> void;

auto save_cfi_inputs            (const std::string& filename)  -> void;

auto addrint_to_hexstring       (ADDRINT input)                -> std::string;

#if !defined(DISABLE_FSA)
auto path_code_to_string        (const path_code_t& path_code) -> std::string;
#endif

auto show_exploring_progress    () -> void;

auto show_cfi_logged_inputs     () -> void;

#endif // STUFFS_H
