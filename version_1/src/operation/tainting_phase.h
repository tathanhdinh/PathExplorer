#ifndef TAINTING_PHASE_H
#define TAINTING_PHASE_H

// these definitions are not necessary (since are defined already in the CMakeLists),
// they are added just to make qt-creator parsing headers
#if defined(_WIN32) || defined(_WIN64)
#ifndef TARGET_IA32
#define TARGET_IA32
#endif
#ifndef HOST_IA32
#define HOST_IA32
#endif
#ifndef TARGET_WINDOWS
#define TARGET_WINDOWS
#endif
#ifndef USING_XED
#define USING_XED
#endif
#endif
#include <pin.H>

namespace tainting
{
extern void initialize                  ();

extern VOID kernel_mapped_instruction   (ADDRINT ins_addr, THREADID thread_id);

extern VOID generic_instruction         (ADDRINT ins_addr, THREADID thread_id);

extern VOID mem_read_instruction        (ADDRINT ins_addr, ADDRINT mem_read_addr,
                                         UINT32 mem_read_size, CONTEXT* p_ctxt, THREADID thread_id);

extern VOID mem_write_instruction       (ADDRINT ins_addr, ADDRINT mem_written_addr,
                                         UINT32 mem_written_size, THREADID thread_id);

extern VOID graphical_propagation       (ADDRINT ins_addr, THREADID thread_id);
} // end of tainting namespace
#endif
