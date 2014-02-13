#include "cond_direct_instruction.h"

cond_direct_instruction::cond_direct_instruction(const INS& ins) : instruction(ins)
{
  this->is_resolved = false;
  this->is_bypassed = false;
  this->is_explored = false;

  this->input_dep_addrs.clear();
  this->inputs.clear();
  this->checkpoints.clear();  
}

cond_direct_instruction::cond_direct_instruction(instruction& ins) : instruction(ins)
{
  this->is_resolved = false;
  this->is_bypassed = false;
  this->is_explored = false;

  this->input_dep_addrs.clear();
  this->inputs.clear();
  this->checkpoints.clear();
}
