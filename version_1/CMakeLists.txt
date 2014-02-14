cmake_minimum_required(VERSION 2.8)

project(PathExplorer_V1)

include(FindPin.cmake)
include(UsePin.cmake)

# add_subdirectory(src)
include_directories(/home/monads/.local/include)
link_directories(/home/monads/.local/lib)

set(PINTOOL_LINK_LIBS boost_log pthread)

ADD_PINTOOL(path_explorer   
  src/branch.cpp 
  src/path_explorer.cpp 
  src/operand.cpp 
  src/instruction.cpp 
#  src/cond_direct_instruction.cpp
  src/checkpoint.cpp 
  src/resolving_functions.cc 
  src/logging_functions.cc 
  src/tainting_functions.cc 
  src/instrumentation_functions.cc 
  src/stuffs.cc)

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
