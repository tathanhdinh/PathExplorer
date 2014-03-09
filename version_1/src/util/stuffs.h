#ifndef STUFFS_H
#define STUFFS_H

#include "../operation/common.h"

void        save_static_trace     (const std::string& filename);

void        save_explored_trace   (const std::string& filename);

void        save_tainting_graph   (df_diagram& dta_graph, const std::string& filename);

std::string addrint_to_hexstring  (ADDRINT input);

#if !defined(DISABLE_FSA)
std::string path_code_to_string   (const path_code_t& path_code);
#endif

#endif // STUFFS_H
