#include "cond_direct_instruction.h"

/**
 * @brief constructor from a raw INS
 */
cond_direct_instruction::cond_direct_instruction(const INS& ins) : instruction(ins)
{
  this->is_resolved = false; this->is_bypassed = false; this->is_explored = false;

  this->input_dep_addrs.clear(); this->affecting_checkpoint_addrs_pairs.clear();
  this->first_input_projections.clear(); this->second_input_projections.clear();

  this->used_rollback_num = 0; this->is_singular = false;
}


/**
 * @brief copy constructor from an instruction
 */
cond_direct_instruction::cond_direct_instruction(instruction& ins) : instruction(ins)
{
  this->is_resolved = false; this->is_bypassed = false; this->is_explored = false;

  this->input_dep_addrs.clear(); this->affecting_checkpoint_addrs_pairs.clear();
  this->first_input_projections.clear(); this->second_input_projections.clear();

  this->used_rollback_num = 0; this->is_singular = false;
}
