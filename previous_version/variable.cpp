#include "variable.h"

#include <algorithm>
#include <sstream>
#include <cstdlib>
#include <iostream>
#include <exception>

#include "stuffs.h"

/*================================================================================================*/

variable::variable()
{
}

/*================================================================================================*/

variable::variable(ADDRINT new_mem) : mem(new_mem)
{
  this->name = remove_leading_zeros(StringFromAddrint(new_mem));
  this->type = MEM_VAR;
}

/*================================================================================================*/

variable::variable(REG new_reg) : reg(new_reg)
{
  REG full_reg = REG_FullRegName(reg);
  this->name = REG_StringShort(full_reg);
//   this->name = REG_StringShort(reg_name);
  this->type = REG_VAR;
}

/*================================================================================================*/

//variable::variable(UINT32 new_imm) : imm(new_imm)
//{
//  this->name = hexstr(imm);
//  this->type = IMM_VAR;
//}

/*================================================================================================*/

variable::variable(const variable& var)
{
  this->name = var.name;
  this->type = var.type;
  this->mem  = var.mem;
  this->reg  = var.reg;
}

/*================================================================================================*/

variable& variable::operator=(const variable& var)
{
  this->name = var.name;
  this->type = var.type;
  this->mem  = var.mem;
  this->reg  = var.reg;
  return *this;
}
