cmake_minimum_required(VERSION 2.8)

project(PathExplorer)

include(FindPin.cmake)
include(UsePin.cmake)

set(PINTOOL_LINK_LIBS boost_log pthread)

ADD_PINTOOL(PathExplorer2
  src/analysis/instruction_operand.cpp
  src/analysis/instruction.cpp
  src/analysis/conditional_branch.cpp
  src/analysis/dataflow.cpp
  src/engine/checkpoint.cpp
  src/engine/fast_execution.cpp
  src/instrumentation/trace_analyzer.cpp
  src/instrumentation/trace_resolver.cpp
  src/utilities/utils.cpp
  src/main.cpp)