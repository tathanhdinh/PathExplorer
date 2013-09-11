#ifndef LOGGING_FUNCTIONS
#define LOGGING_FUNCTIONS

#include <pin.H>

VOID logging_ins_count_analyzer(ADDRINT ins_addr);

VOID logging_mem_to_st_analyzer(ADDRINT ins_addr, ADDRINT mem_read_addr, UINT32 mem_read_size, CONTEXT* p_ctxt);

VOID logging_st_to_mem_analyzer(ADDRINT ins_addr, ADDRINT mem_addr, UINT32 mem_size);

VOID logging_cond_br_analyzer(ADDRINT ins_addr, bool br_taken);

#endif