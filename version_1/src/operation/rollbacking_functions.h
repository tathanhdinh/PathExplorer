#ifndef ROLLBACKING_FUNCTIONS_H
#define ROLLBACKING_FUNCTIONS_H

#include <pin.H>

namespace rollbacking
{

VOID generic_instruction(ADDRINT ins_addr);
VOID mem_write_instruction(ADDRINT ins_addr, ADDRINT mem_addr, UINT32 mem_size);

};

#endif // ROLLBACKING_FUNCTIONS_H
