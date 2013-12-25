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

#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include "instruction_operand.h"
#include <pin.H>
#include <string>
#include <boost/unordered_set.hpp>

namespace dataflow_analysis 
{

class instruction
{
public:
  ADDRINT     address;
  
  std::string dissasembled_name;
  
  boost::unordered_set<instruction_operand, operand_hash> source_operands;
  boost::unordered_set<instruction_operand, operand_hash> target_operands;
  
public:
  instruction(const INS& current_instruction);
  void update_memory(ADDRINT access_address, UINT8 access_length, bool read_or_written);
};


} // end of dataflow_analysis namespace

#endif // INSTRUCTION_H
