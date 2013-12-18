/*
 * Copyright (C) 2013  Ta Thanh Dinh <email>
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

#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <pin.H>

#include "instruction_operand.h"

#include <boost/utility/string_ref.hpp>
#include <boost/unordered_set.hpp>

class instruction
{
public:
  ADDRINT           address;
  boost::string_ref dissasembled_name;
  
  boost::unordered_set<instruction_operand> source_operands;
  boost::unordered_set<instruction_operand> target_operands;
  
public:
  instruction(const INS& current_instruction);
};

#endif // INSTRUCTION_H
