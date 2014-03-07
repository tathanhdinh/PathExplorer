#ifndef EXPLORER_GRAPH_H
#define EXPLORER_GRAPH_H

#include "cond_direct_instruction.h"

class explorer_graph;
typedef pept::shared_ptr<explorer_graph>  ptr_explorer_graph_t;
typedef std::vector<bool>                 path_code_t;

class explorer_graph
{
public:
  static ptr_explorer_graph_t instance(); // allow only a single instance of explorer graph
  void add_vertex(ptr_instruction_t& ins);
  void add_edge(ptr_instruction_t& ins_a, ptr_instruction_t& ins_b, path_code_t& edge_path_code,
                addrint_value_map_t& edge_addr_value);
  void save_to_file(std::string filename);

private:
  explorer_graph();
};


#endif // EXPLORER_GRAPH_H
