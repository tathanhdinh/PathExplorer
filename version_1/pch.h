#ifndef PCH_H
#define PCH_H

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

#include <map>
#include <set>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <limits>
#include <ctime>
#include <cstdlib>
#include <memory>
#include <functional>
#include <locale>

#include <boost/variant.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/filtered_graph.hpp>
#include <boost/graph/lookup_edge.hpp>
#include <boost/type_traits/function_traits.hpp>
#include <boost/algorithm/string/find.hpp>

#endif // PCH_H
