#ifndef INSTRUMENTATION_FUNCTIONS_H
#define INSTRUMENTATION_FUNCTIONS_H

#include "../parsing_helper.h"
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
