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

namespace analysis 
{

class cbranch : public instruction
{
public:
  bool is_taken;
  bool is_resolved;
  bool is_bypassed;

public:
  cbranch(const INS& current_instruction);
  cbranch(const instruction& other_instruction);
//   conditional_branch(const conditional_branch& other);
//   conditional_branch& operator=(const conditional_branch& other);
//   bool operator==(const conditional_branch& other);
};

typedef boost::shared_ptr<cbranch> ptr_cbranch_t;

} // end of analysis namespace

#endif // CBRANCH_H
