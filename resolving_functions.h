#ifndef RESOLVING_FUNCTIONS_H
#define RESOLVING_FUNCTIONS_H

#include <pin.H>

VOID resolving_ins_count_analyzer   (ADDRINT ins_addr);

VOID resolving_mem_to_st_analyzer   (ADDRINT ins_addr, ADDRINT mem_addr, UINT32 mem_size);

VOID resolving_st_to_mem_analyzer   (ADDRINT ins_addr, ADDRINT mem_addr, UINT32 mem_size);

VOID resolving_cond_branch_analyzer (ADDRINT ins_addr, bool br_taken);

#endif