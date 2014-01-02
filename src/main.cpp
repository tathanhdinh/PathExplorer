/*
 * Copyright (C) 2014  Ta Thanh Dinh <thanhdinh.ta@inria.fr>
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

#include <pin.H>
#include "analysis/instruction.h"
#include "analysis/dataflow.h"

using namespace analysis;

boost::unordered_map<ADDRINT, ptr_instruction_t> address_instruction_map;
boost::unordered_map<UINT32, ptr_instruction_t> execution_order_instruction_map;
boost::unordered_map<UINT32, ADDRINT> address_of_instruction_executed_at;
boost::unordered_map<ADDRINT, UINT8>  original_value_at_address;

int main(int argc, char* argv[])
{
  
  return 0;
}