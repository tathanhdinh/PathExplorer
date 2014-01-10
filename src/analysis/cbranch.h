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

#ifndef CBRANCH_H
#define CBRANCH_H

#include "instruction.h"
#include <boost/shared_ptr.hpp>
#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>

namespace analysis 
{

typedef boost::shared_ptr<UINT8> ptr_uint8_t;
typedef boost::unordered_set<ptr_uint8_t> ptr_uint8s_t;

class cbranch : public instruction
{
public:
  bool is_taken;
  bool is_resolved;
  bool is_bypassed;
  
  boost::unordered_map<bool, ptr_uint8s_t> inputs_lead_to_decision;

public:
  cbranch(const INS& current_instruction);
  cbranch(const instruction& other_instruction);
  void save_current_input(bool current_branch_decision);
};

typedef boost::shared_ptr<cbranch> ptr_cbranch_t;

} // end of analysis namespace

#endif // CBRANCH_H
