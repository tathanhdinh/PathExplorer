#ifndef INSTRUMENTATION_FUNCTIONS_H
#define INSTRUMENTATION_FUNCTIONS_H

#include <pin.H>

typedef enum
{
  capturing_state   = 0,
  tainting_state    = 1,
  rollbacking_state = 2
} running_state;

extern VOID ins_instrumenter(INS ins, VOID *data);

extern VOID image_load_instrumenter(IMG loaded_img, VOID *data);

extern BOOL process_create_instrumenter(CHILD_PROCESS created_process, VOID* data);

#endif
