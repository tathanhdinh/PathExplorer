cmake_minimum_required(VERSION 2.8)

project(PathExplorer)

include(FindPin.cmake)
include(UsePin.cmake)

# add_subdirectory(src)
include_directories(/home/monads/.local/include)
link_directories(/home/monads/.local/lib)

set(PINTOOL_LINK_LIBS boost_log pthread)

ADD_PINTOOL(path_explorer   
  branch.cpp 
  variable.cpp
  path_explorer.cpp 
  operand.cpp 
  instruction.cpp 
  checkpoint.cpp 
  resolving_functions.cc 
  logging_functions.cc 
  tainting_functions.cc 
  analysis_functions.cc 
  instrumentation_functions.cc 
  stuffs.cc)

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