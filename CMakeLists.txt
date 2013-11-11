cmake_minimum_required(VERSION 2.8)

project(PathExplorer)

include(FindPin.cmake)
include(UsePin.cmake)

set(PINTOOL_LINK_LIBS boost_log pthread)

ADD_PINTOOL(path_explorer branch.cpp 
  variable.cpp
  path_explorer.cpp
#   partial_trace.cpp 
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
