#ifndef ROLLBACKING_FUNCTIONS_H
#define ROLLBACKING_FUNCTIONS_H

#include <pin.H>

namespace rollbacking
{

void prepare();

VOID generic_instruction(ADDRINT ins_addr);
VOID mem_write_instruction(ADDRINT ins_addr, ADDRINT mem_addr, UINT32 mem_length);
VOID control_flow_instruction(ADDRINT ins_addr);

};

#endif // ROLLBACKING_FUNCTIONS_H
