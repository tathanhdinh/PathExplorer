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

#ifndef CONDITIONAL_BRANCH_H
#define CONDITIONAL_BRANCH_H

#include "instruction.h"

namespace analysis 
{

class conditional_branch : public instruction
{
public:
  bool is_taken;

public:
  conditional_branch(const INS& current_instruction);
  conditional_branch(const instruction& other_instruction);
//   conditional_branch(const conditional_branch& other);
//   conditional_branch& operator=(const conditional_branch& other);
//   bool operator==(const conditional_branch& other);
};

typedef boost::shared_ptr<conditional_branch> ptr_cbranch_t;

} // end of analysis namespace

#endif // CONDITIONAL_BRANCH_H
