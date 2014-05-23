#ifndef EXECUTION_DFA_H
#define EXECUTION_DFA_H

#include "execution_path.h"

class execution_dfa;
typedef std::shared_ptr<execution_dfa> ptr_exec_dfa_t;

class execution_dfa
{
private:
  // private key used in the constructor to prevent the free construction
  struct construction_key {};

public:
  execution_dfa         (const construction_key& init_key) {};

  static auto instance  ()                          -> ptr_exec_dfa_t;

  auto add_exec_path    (ptr_exec_path_t exec_path) -> void;
  auto optimization     ()                          -> void;
  auto save_to_file     ()                          -> void;
};

#endif // EXECUTION_DFA_H
