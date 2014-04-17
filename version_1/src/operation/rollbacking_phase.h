#ifndef ROLLBACKING_PHASE_H
#define ROLLBACKING_PHASE_H

#include "../parsing_helper.h"
#include <pin.H>

namespace rollbacking
{
extern auto initialize                (UINT32 trace_length_limit)             -> void;

extern auto generic_instruction       (ADDRINT ins_addr, THREADID thread_id)  -> VOID;

extern auto mem_write_instruction     (ADDRINT ins_addr, ADDRINT mem_addr,
                                       UINT32 mem_length, THREADID thread_id) -> VOID;

extern auto control_flow_instruction  (ADDRINT ins_addr, THREADID thread_id)  -> VOID;
}

#endif // ROLLBACKING_FUNCTIONS_H
