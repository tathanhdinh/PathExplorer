#ifndef LOGGING_FUNCTIONS
#define LOGGING_FUNCTIONS

#include <pin.H>

VOID logging_syscall_instruction_analyzer(ADDRINT ins_addr);

VOID logging_general_instruction_analyzer(ADDRINT ins_addr);

VOID logging_mem_read_instruction_analyzer(ADDRINT ins_addr, ADDRINT mem_read_addr, UINT32 mem_read_size, CONTEXT* p_ctxt);

VOID logging_mem_write_instruction_analyzer(ADDRINT ins_addr, ADDRINT mem_written_addr, UINT32 mem_written_size);

VOID logging_cond_br_analyzer(ADDRINT ins_addr, bool br_taken);

VOID logging_before_recv_functions_analyzer(ADDRINT msg_addr);

VOID logging_after_recv_functions_analyzer(UINT32 msg_length);

#endif
