cmake_minimum_required(VERSION 2.8)

project(PathExplorer_V1)

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")

include(FindPin.cmake)
include(UsePin.cmake)

# add_subdirectory(src)
#include_directories(/home/monads/.local/include)
#link_directories(/home/monads/.local/lib)

#set(PINTOOL_LINK_LIBS boost_log pthread)
#add_definitions("-std=c++0x")

ADD_PINTOOL(path_explorer
  src/path_explorer.cpp
  src/base/operand.cpp
  src/base/operand.h
  src/base/instruction.cpp
  src/base/instruction.h
  src/base/cond_direct_instruction.cpp
  src/base/cond_direct_instruction.h
  src/base/checkpoint.cpp
  src/base/checkpoint.h
  src/operation/rollbacking_phase.cpp
  src/operation/rollbacking_phase.h
  src/operation/tainting_phase.cpp
  src/operation/tainting_phase.h
  src/operation/capturing_phase.cpp
  src/operation/capturing_phase.h
  src/operation/instrumentation.cpp
  src/operation/instrumentation.h
  src/operation/common.h
  src/util/stuffs.cc
  src/util/stuffs.h
  src/util/tinyformat.h)

# add_executable(path_explorer.so instruction.cpp path_explorer.cpp)

# install(TARGETS pintaint RUNTIME DESTINATION bin)

# ADD_PINTOOL(PathExplorer2
#   src/analysis/instruction_operand.cpp
#   src/analysis/instruction.cpp
#   src/analysis/conditional_branch.cpp
#   src/analysis/dataflow.cpp
#   src/engine/checkpoint.cpp
#   src/engine/fast_execution.cpp
#   src/instrumentation/trace_analyzer.cpp
#   src/instrumentation/trace_resolver.cpp
#   src/utilities/utils.cpp
#   src/main.cpp)

else()

set(CMAKE_BUILD_TYPE "None")

if(NOT PIN_ROOT_DIR)
  message(FATAL_ERROR
    "\nPin not found!\n"
    "Please set the environment variable PIN_ROOT_DIR to the base directory"
    " of the pin library.\n"
  )
endif(NOT PIN_ROOT_DIR)

message(STATUS "PIN_ROOT_DIR: ${PIN_ROOT_DIR}")

set(PIN_CPU_ARCH "ia32")
set(PIN_CPU_ARCH_LONG "ia32")

set(PIN_INCLUDE_DIRS
  ${PIN_ROOT_DIR}/extras/xed2-ia32/include
  ${PIN_ROOT_DIR}/source/include/pin
  ${PIN_ROOT_DIR}/source/include/pin/gen
  ${PIN_ROOT_DIR}/extras/components/include
)

set(PIN_DEFS "/D TARGET_IA32 /D HOST_IA32 /D TARGET_WINDOWS /D USING_XED /D_SECURE_SCL=0 /D_ITERATOR_DEBUG_LEVEL=0")

set(OTHER_DEFS "/D _WINDLL /D _MBCS /MT /EHsc /nologo")

set(PIN_LIBRARY_DIRS
  ${PIN_ROOT_DIR}/extras/xed2-${PIN_CPU_ARCH_LONG}/lib
  ${PIN_ROOT_DIR}/${PIN_CPU_ARCH_LONG}/lib
  ${PIN_ROOT_DIR}/${PIN_CPU_ARCH_LONG}/lib-ext
)

set(PIN_LIBS pin pinvm libxed ntdll-32)

set(OTHER_LIBS libcpmt libcmt)

set(BOOST_INCLUDE_DIRS
  "C:/Users/Ta Thanh Dinh/Documents/boost_1_55_0-msvc-10.0-32"
)

set(INCLUDE_DIRS
  ${PIN_INCLUDE_DIRS}
  ${BOOST_INCLUDE_DIRS}
)

set(SOURCES
  src/path_explorer.cpp
  src/base/operand.cpp
  src/base/operand.h
  src/base/instruction.cpp
  src/base/instruction.h
  src/base/cond_direct_instruction.cpp
  src/base/cond_direct_instruction.h
  src/base/checkpoint.cpp
  src/base/checkpoint.h
  src/operation/rollbacking_phase.cpp
  src/operation/rollbacking_phase.h
  src/operation/tainting_phase.cpp
  src/operation/tainting_phase.h
  src/operation/capturing_phase.cpp
  src/operation/capturing_phase.h
  src/operation/instrumentation.cpp
  src/operation/instrumentation.h
  src/operation/common.h
  src/util/stuffs.cc
  src/util/stuffs.h
  src/util/tinyformat.h
)

include_directories(${INCLUDE_DIRS})
link_directories(${PIN_LIBRARY_DIRS})

add_definitions(${PIN_DEFS})
add_definitions(${OTHER_DEFS})

#remove(CMAKE_CXX_FLAGS)
set(CMAKE_CXX_FLAGS "${OTHER_DEFS} ${PIN_DEFS}")
set(CMAKE_SHARED_LINKER_FLAGS "/EXPORT:main /ENTRY:Ptrace_DllMainCRTStartup@12 /NODEFAULTLIB")

add_library(pathexplorer SHARED ${SOURCES})

target_link_libraries(pathexplorer ${PIN_LIBS} ${OTHER_LIBS})

endif()
