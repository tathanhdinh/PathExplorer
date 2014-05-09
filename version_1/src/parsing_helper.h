#ifndef PARSING_HELPER_H
#define PARSING_HELPER_H

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

#if _MSC_VER == 1600
#include <utility>
#define decltype(...) \
  std::identity<decltype(__VA_ARGS__)>::type
#endif

#endif // PARSING_HELPER_H
