#ifndef TAINTING_PHASE_H
#define TAINTING_PHASE_H

#include "../parsing_helper.h"
#include <pin.H>

namespace tainting
{
extern auto initialize                ()                                            -> void;

extern auto kernel_mapped_instruction (ADDRINT ins_addr, THREADID thread_id)        -> VOID;

extern auto generic_instruction       (ADDRINT ins_addr, const CONTEXT* p_ctxt,
                                       THREADID thread_id)                          -> VOID;

extern auto mem_read_instruction      (ADDRINT ins_addr, ADDRINT mem_read_addr,
                                       UINT32 mem_read_size, const CONTEXT* p_ctxt,
                                       THREADID thread_id)                          -> VOID;

extern auto mem_write_instruction     (ADDRINT ins_addr, ADDRINT mem_written_addr,
                                       UINT32 mem_written_size, THREADID thread_id) -> VOID;

extern auto graphical_propagation     (ADDRINT ins_addr, THREADID thread_id)        -> VOID;
} // end of tainting namespace
#endif
