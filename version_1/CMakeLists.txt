cmake_minimum_required(VERSION 2.8)

project(PathExplorer_V1)

include(cotire.cmake)
include(FindPin.cmake)
include(UsePin.cmake)

# add_subdirectory(src)
include_directories(/home/monads/.local/include)
link_directories(/home/monads/.local/lib)

set(PINTOOL_LINK_LIBS boost_log pthread)

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
  src/operation/instrumentation.cpp
  src/operation/instrumentation.h
  src/operation/common.h
  src/util/stuffs.cc
  src/util/stuffs.h)
cotire(path_explorer)

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
