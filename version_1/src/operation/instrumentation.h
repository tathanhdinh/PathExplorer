#ifndef INSTRUMENTATION_FUNCTIONS_H
#define INSTRUMENTATION_FUNCTIONS_H

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
#endif // defined(_WIN32) || defined(_WIN64)
#include <pin.H>

namespace instrumentation
{
extern auto instruction_executing   (INS ins, VOID* data)                       -> VOID;

extern auto routine_calling         (RTN rtn, VOID* data)                       -> VOID;

extern auto image_loading           (IMG loaded_img, VOID *data)                -> VOID;

extern auto process_creating        (CHILD_PROCESS created_process, VOID* data) -> BOOL;

extern auto initialize              ()                                          -> void;
} // end of instrumentation namespace

#endif
