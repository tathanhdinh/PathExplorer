#ifndef COND_DIRECT_INSTRUCTION_H
#define COND_DIRECT_INSTRUCTION_H

#include "instruction.h"
#include "checkpoint.h"

#include <vector>

typedef std::set<ADDRINT>                           addrint_set_t;
typedef std::map<ADDRINT, UINT8>                    addrint_value_map_t;
typedef std::vector<addrint_value_map_t>            vector_of_addrint_value_map_t;
typedef std::pair<ptr_checkpoint_t, addrint_set_t>  checkpoint_with_modified_addrs;

class cond_direct_instruction : public instruction
{
public:
  bool is_resolved;
  bool is_bypassed;
  bool is_explored;

  addrint_set_t                                   input_dep_addrs;
  std::map<bool, vector_of_addrint_value_map_t>   inputs;
  std::vector<checkpoint_with_modified_addrs>     checkpoints;

  UINT32 used_rollback_num;
  UINT32 max_rollback_num;

  UINT32 exec_order;

public:
  cond_direct_instruction(const INS& ins);
  cond_direct_instruction(instruction& ins);
};

typedef boost::shared_ptr<cond_direct_instruction>  ptr_cond_direct_instruction_t;
typedef std::vector<ptr_cond_direct_instruction_t>  ptr_cond_direct_instructions_t;

#endif // COND_DIRECT_INSTRUCTION_H
