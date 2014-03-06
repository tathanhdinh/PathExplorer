#ifndef PCH_H
#define PCH_H

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

#if __cplusplus <= 199711L
#include <boost/shared_ptr.hpp>
#else
#include <memory>
#endif

#include <boost/variant.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/lookup_edge.hpp>

#endif // PCH_H
