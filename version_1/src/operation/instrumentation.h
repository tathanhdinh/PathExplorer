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
#endif
#include <pin.H>

extern auto ins_instrumenter            (INS ins, VOID *data)                       -> VOID;

extern auto image_load_instrumenter     (IMG loaded_img, VOID *data)                -> VOID;

extern auto process_create_instrumenter (CHILD_PROCESS created_process, VOID* data) -> BOOL;

#endif
