#ifndef EXECUTION_PATH_H
#define EXECUTION_PATH_H

// these definitions are not necessary (defined already in the CMakeLists),
// they are added just to help qt-creator parsing headers
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

#include "cond_direct_instruction.h"

class execution_path
{
public:
  addrint_value_maps_t condition;

  execution_path();
};

#endif // EXECUTION_PATH_H
