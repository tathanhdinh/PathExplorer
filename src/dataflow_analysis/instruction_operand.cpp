/*
 * Copyright (C) 2013  Ta Thanh Dinh <thanhdinh.ta@inria.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "instruction_operand.h"
#include "../utils/utils.h"

#include <boost/lexical_cast.hpp>

/*================================================================================================*/

instruction_operand::instruction_operand(ADDRINT memory_operand)
{
  this->value = memory_operand;
  this->name = utils::remove_leading_zeros(StringFromAddrint(memory_operand));
}

/*================================================================================================*/


instruction_operand::instruction_operand(REG register_operand)
{
  this->value = REG_FullRegName(register_operand);
  this->name = REG_StringShort(boost::get<REG>(this->value));
}

/*================================================================================================*/

instruction_operand::instruction_operand(UINT32 immediate_operand)
{
  this->value = immediate_operand;
  this->name = boost::lexical_cast<std::string>(immediate_operand);
}

