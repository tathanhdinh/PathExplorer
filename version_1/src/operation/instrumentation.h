#ifndef INSTRUMENTATION_FUNCTIONS_H
#define INSTRUMENTATION_FUNCTIONS_H

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

extern VOID ins_instrumenter              (INS ins, VOID *data);

extern VOID image_load_instrumenter       (IMG loaded_img, VOID *data);

extern BOOL process_create_instrumenter   (CHILD_PROCESS created_process, VOID* data);

#endif
