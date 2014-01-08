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

#include "operand.h"
#include <pin.H>
#include <string>
#include <boost/unordered_set.hpp>
#include <boost/shared_ptr.hpp>

namespace analysis 
{
  
typedef enum 
{
  MEMORY_READ = 0,
  MEMORY_WRITE = 1
} memory_access_t;

class instruction
{
public:
  ADDRINT     address;
  std::string dissasembled_name;
  std::string contained_library;
  std::string contained_function;
  
  bool        is_syscall;
  bool        is_vdso;
  bool        is_memread;
  bool        is_memwrite;
  bool        is_cbranch;
  bool        is_indirectBrOrCall;
  
  boost::unordered_set<ptr_insoperand_t> source_operands;
  boost::unordered_set<ptr_insoperand_t> target_operands;
//   boost::unordered_set<instruction_operand, operand_hash> source_operands;
//   boost::unordered_set<instruction_operand, operand_hash> target_operands;
  
public:
  instruction(const INS& current_instruction);
  instruction(const instruction& other_instruction);
  void update_memory_access_info(ADDRINT access_address, UINT8 access_length, 
                                 memory_access_t access_type);
};

typedef boost::shared_ptr<instruction> ptr_instruction_t;

} // end of dataflow_analysis namespace

#endif // INSTRUCTION_H
