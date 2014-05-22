set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/CMake/")
#set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake/")
include(cotire)

project(PathExplorer_V1)

cmake_minimum_required(VERSION 2.8)

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


else() #============================================================================================

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

set(PIN_DEFS "/D TARGET_IA32 /D HOST_IA32 /D TARGET_WINDOWS /D USING_XED /D_SECURE_SCL=0 /D_ITERATOR_DEBUG_LEVEL=0 /Zc:auto")
set(LINKER_DEFS "/D _WINDLL /D _MBCS /GS- /MT /EHsc /Ox /O2 /Ob2 /nologo")

set(FUNC_DEFS "")
if((DEFINED DISABLE_FSA) AND (DISABLE_FSA STREQUAL "Yes"))
set(FUNC_DEFS "/D DISABLE_FSA")
message(STATUS "FSA reconstruction is disabled.")
else()
message(STATUS "FSA reconstruction is enabled by default.")
endif()

set(PIN_LIBRARY_DIRS
  ${PIN_ROOT_DIR}/extras/xed2-${PIN_CPU_ARCH_LONG}/lib
  ${PIN_ROOT_DIR}/${PIN_CPU_ARCH_LONG}/lib
  ${PIN_ROOT_DIR}/${PIN_CPU_ARCH_LONG}/lib-ext
)

set(PIN_LIBS pin pinvm libxed ntdll-32 dbghelp)

set(OTHER_LIBS libcpmt libcmt)

if(NOT DEFINED BOOST_ROOT_DIR)
  message(FATAL_ERROR "\nBoost not found!\n"
    "Please set the environment variable BOOST_ROOT_DIR to the base directory"
    " of the Boost library.\n")
endif(NOT DEFINED BOOST_ROOT_DIR)
message(STATUS "BOOST_ROOT_DIR: ${BOOST_ROOT_DIR}")


set(INCLUDE_DIRS
  ${PIN_INCLUDE_DIRS}
  ${BOOST_ROOT_DIR}
)

#set(SOURCES
#  src/path_explorer.cpp
#  src/base/operand.cpp
#  src/base/operand.h
#  src/base/instruction.cpp
#  src/base/instruction.h
#  src/base/cond_direct_instruction.cpp
#  src/base/cond_direct_instruction.h
#  src/base/checkpoint.cpp
#  src/base/checkpoint.h
#  src/operation/rollbacking_phase.cpp
#  src/operation/rollbacking_phase.h
#  src/operation/tainting_phase.cpp
#  src/operation/tainting_phase.h
#  src/operation/capturing_phase.cpp
#  src/operation/capturing_phase.h
#  src/operation/instrumentation.cpp
#  src/operation/instrumentation.h
#  src/operation/common.h
#  src/util/stuffs.cc
#  src/util/stuffs.h
#  src/util/tinyformat.h
#)

file(GLOB_RECURSE SOURCES src "*.h" "*.cpp")

include_directories(${INCLUDE_DIRS})
link_directories(${PIN_LIBRARY_DIRS})

add_definitions(${PIN_DEFS})
add_definitions(${LINKER_DEFS})
add_definitions(${FUNC_DEFS})

#remove(CMAKE_CXX_FLAGS)
set(CMAKE_CXX_FLAGS "${FUNC_DEFS} ${OTHER_DEFS} ${PIN_DEFS}")
set(CMAKE_SHARED_LINKER_FLAGS "/EXPORT:main /ENTRY:Ptrace_DllMainCRTStartup@12 /NODEFAULTLIB")

add_library(pathexplorer SHARED ${SOURCES})
target_link_libraries(pathexplorer ${PIN_LIBS} ${OTHER_LIBS})

#set(HEADER_IGNORE_PATH $ENV{PROGRAMFILES})
#set_target_properties(pathexplorer PROPERTIES
#  COTIRE_PREFIX_HEADER_IGNORE_PATH "${PIN_ROOT_DIR}"
#)
set_target_properties(pathexplorer PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "pch.h")
cotire(pathexplorer)

#add_executable(pseudope ${SOURCES})
#target_link_libraries(pseudope ${PIN_LIBS} ${OTHER_LIBS})

endif()
