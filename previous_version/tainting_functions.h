#ifndef TAINTING_FUNCTIONS_H
#define TAINTING_FUNCTIONS_H

#include <pin.H>

extern VOID tainting_general_instruction_analyzer (ADDRINT ins_addr);

// VOID tainting_mem_to_st_analyzer(ADDRINT ins_addr, ADDRINT mem_read_addr, UINT32 mem_read_size);

// VOID tainting_st_to_mem_analyzer(ADDRINT ins_addr, ADDRINT mem_written_addr, UINT32 mem_written_size);

#endif
