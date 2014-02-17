#include <pin.H>

#include <iostream>
#include <fstream>
#include <map>
#include <set>

#include <boost/predef.h>
#include <boost/format.hpp>
#include <boost/graph/breadth_first_search.hpp>


#include "../util/stuffs.h"
#include "../base/instruction.h"
#include "../base/checkpoint.h"
#include "../base/branch.h"

/*================================================================================================*/


extern UINT32                                     total_rollback_times;

extern df_diagram                                 dta_graph;

extern KNOB<BOOL>                                 print_debug_text;
extern KNOB<UINT32>                               max_trace_length;


/*================================================================================================*/


