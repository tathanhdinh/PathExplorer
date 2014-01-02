/*
 * <one line to give the program's name and a brief idea of what it does.>
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

#ifndef MAIN_H
#define MAIN_H

#include <pin.H>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
#include "analysis/instruction.h"
#include "analysis/conditional_branch.h"
#include "engine/checkpoint.h"

using namespace analysis;
using namespace engine;

extern ADDRINT received_message_address;
extern INT32 received_message_length;
extern UINT32 current_execution_order;
extern UINT32 execution_trace_max_length;

extern boost::unordered_map<ADDRINT, ptr_instruction_t> instruction_at_address;
extern boost::unordered_map<UINT32, ptr_instruction_t> instruction_at_exeorder;
extern boost::unordered_map<UINT32, ptr_conditional_branch_t> branch_at_exeorder;
extern boost::unordered_map<UINT32, ADDRINT> address_of_instruction_at_exeorder;
extern boost::unordered_map<UINT32, ptr_checkpoint_t> checkpoint_at_exeorder;
extern boost::unordered_map<ADDRINT, UINT8> original_value_at_address;

#endif // MAIN_H
