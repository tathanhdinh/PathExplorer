#ifndef EXPLORING_GRAPH_H
#define EXPLORING_GRAPH_H

#include <pin.H>
#include <boost/shared_ptr.hpp>
#include <string>

typedef enum 
{
  NEXT, NOT_TAKEN = 0,
  TAKEN = 1,
  ROLLBACK = 2
} next_exe_type;

class exploring_graph
{
public:
  exploring_graph();
  void add_node(ADDRINT node_addr);
  void add_edge(ADDRINT source_addr, ADDRINT target_addr, next_exe_type direction,
                UINT32 nb_bits, UINT32 rb_length, UINT32 nb_rb);
  void print_to_file(const std::string& filename);
};

typedef boost::shared_ptr<exploring_graph> ptr_exploring_graph;

#endif // EXPLORING_GRAPH_H
