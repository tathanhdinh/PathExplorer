#include "cond_direct_instruction.h"

/*================================================================================================*/

extern KNOB<UINT32> max_local_rollback;

/*================================================================================================*/

cond_direct_instruction::cond_direct_instruction(const INS& ins) : instruction(ins)
{
  this->is_resolved = false;
  this->is_bypassed = false;
  this->is_explored = false;

  this->input_dep_addrs.clear();
  this->inputs.clear();
  this->checkpoints.clear();  

  this->used_rollback_num = 0;
  this->max_rollback_num  = max_local_rollback.Value();
}

/*================================================================================================*/

cond_direct_instruction::cond_direct_instruction(instruction& ins) : instruction(ins)
{
  this->is_resolved = false;
  this->is_bypassed = false;
  this->is_explored = false;

  this->input_dep_addrs.clear();
  this->inputs.clear();
  this->checkpoints.clear();  

  this->used_rollback_num = 0;
  this->max_rollback_num  = max_local_rollback.Value();
}
