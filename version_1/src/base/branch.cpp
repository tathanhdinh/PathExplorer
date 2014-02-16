#include "branch.h"

#include <algorithm>
#include <set>

/*================================================================================================*/

extern UINT32                         current_execution_order;
extern std::vector<ptr_checkpoint_t>  saved_ptr_checkpoints;

/*================================================================================================*/

branch::branch(ADDRINT ins_addr, bool br_taken)
{
  this->addr              = ins_addr;
  this->execution_order   = current_execution_order;
  this->br_taken          = br_taken;

  this->is_resolved       = false;
  this->is_just_resolved  = false;
  this->is_bypassed       = false;
  this->is_explored       = false;
}

/*================================================================================================*/

bool branch::operator==(const branch& other)
{
  return (
          (this->addr = other.addr) && 
          (this->execution_order == other.execution_order) &&
          (this->br_taken == other.br_taken)
         );
}
