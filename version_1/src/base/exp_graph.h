#ifndef EXP_GRAPH_H
#define EXP_GRAPH_H

#include "instruction.h"

class exp_graph
{
public:
  exp_graph();
  void add_node(const ptr_instruction_t& ins);
  void add_edge(const ptr_instruction_t& ins_a, const ptr_instruction_t& ins_b);
  void save_to_file(std::string filename);
};

typedef pept::shared_ptr<exp_graph> ptr_exp_graph_t;

#endif // EXP_GRAPH_H
