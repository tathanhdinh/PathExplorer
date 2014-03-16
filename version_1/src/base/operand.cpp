#include "operand.h"
#include "../util/stuffs.h"

/*================================================================================================*/

operand::operand(ADDRINT mem_addr)
{
  this->value = this->exact_value = mem_addr;
  this->name = this->exact_name = addrint_to_hexstring(mem_addr);
}


operand::operand(REG reg)
{
  this->exact_value = reg;
  this->exact_name = REG_StringShort(reg);

  this->value = REG_FullRegName(reg);
  this->name = REG_StringShort(boost::get<REG>(this->value));
}
