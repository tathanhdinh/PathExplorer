#ifndef ROLLBACKING_PHASE_H
#define ROLLBACKING_PHASE_H

#include <pin.H>

namespace rollbacking
{
extern void initialize_rollbacking_phase(UINT32 trace_length_limit);
extern VOID generic_instruction(ADDRINT ins_addr);
extern VOID mem_write_instruction(ADDRINT ins_addr, ADDRINT mem_addr, UINT32 mem_length);
extern VOID control_flow_instruction(ADDRINT ins_addr);
};

#endif // ROLLBACKING_FUNCTIONS_H
