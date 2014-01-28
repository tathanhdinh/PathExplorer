#ifndef INSTRUMENTATION_FUNCTIONS_H
#define INSTRUMENTATION_FUNCTIONS_H

#include <pin.H>

VOID ins_instrumenter(INS ins, VOID *data);

VOID image_load_instrumenter(IMG loaded_img, VOID *data);

BOOL process_create_instrumenter(CHILD_PROCESS created_process, VOID* data);

#endif
