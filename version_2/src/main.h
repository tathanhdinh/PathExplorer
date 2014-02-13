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

#include "analysis/instruction.h"
#include "analysis/cbranch.h"
#include "engine/checkpoint.h"
#include "engine/fast_execution.h"

#include <boost/cstdint.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>

using namespace analysis;
using namespace engine;

typedef boost::unordered_set<UINT32> exeorders_t;
typedef boost::unordered_set<ADDRINT> addresses_t;
typedef boost::unordered_set<ptr_insoperand_t> ptr_insoperands_t;

extern bool debug_enabled;

extern ADDRINT received_message_address;
extern INT32 received_message_length;
extern UINT32 current_execorder;
extern UINT32 exectrace_max_length;
extern UINT32 total_reexec_times;
extern UINT32 max_local_reexec_number;
extern UINT32 min_bridge_length;

extern boost::unordered_map<ADDRINT, ptr_instruction_t> instruction_at_address;
extern boost::unordered_map<UINT32, ptr_instruction_t> instruction_at_execorder;
extern boost::unordered_map<UINT32, ptr_cbranch_t> cbranch_at_execorder;
extern boost::unordered_map<UINT32, ptr_checkpoint_t> checkpoint_at_execorder;
extern boost::unordered_map<UINT32, exeorders_t> checkpoint_execorders_of_cbranch_at_execorder;
extern boost::unordered_map<UINT32, addresses_t> inputaddrs_affecting_cbranch_at_execorder;
extern boost::unordered_map<UINT32, ptr_insoperands_t> outerface_at_execorder;
extern boost::unordered_map<ADDRINT, UINT8> original_msgstate_at_address;
extern boost::unordered_map<ADDRINT, UINT8> original_memstate_at_address;
extern boost::unordered_map<ADDRINT, UINT8> current_memstate_at_address;
extern boost::unordered_map<UINT32, UINT32> target_execorder_of_bridge_at_execorder;

#endif // MAIN_H
