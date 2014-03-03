#ifndef ROLLBACKING_PHASE_H
#define ROLLBACKING_PHASE_H

// these definitions are not necessary (since they are defined already in the CMakeLists),
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

namespace rollbacking
{
extern void initialize                  (UINT32 trace_length_limit);

extern VOID generic_instruction         (ADDRINT ins_addr);

extern VOID mem_write_instruction       (ADDRINT ins_addr, ADDRINT mem_addr, UINT32 mem_length);

extern VOID control_flow_instruction    (ADDRINT ins_addr);
};

#endif // ROLLBACKING_FUNCTIONS_H
