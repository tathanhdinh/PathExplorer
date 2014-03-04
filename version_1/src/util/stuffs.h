#ifndef STUFFS_H
#define STUFFS_H

#include "../operation/common.h"

void        save_static_trace     (const std::string& filename);

void        save_explored_trace   (const std::string& filename);

void        save_tainting_graph   (df_diagram& dta_graph, const std::string& filename);

std::string addrint_to_hexstring  (ADDRINT input);

#endif // STUFFS_H
