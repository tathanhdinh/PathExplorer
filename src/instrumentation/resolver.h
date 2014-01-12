/*
 * <one line to give the program's name and a brief idea of what it does.>
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

#ifndef RESOLVER_H
#define RESOLVER_H

#include <pin.H>

namespace instrumentation 
{

class resolver
{
public:
  static void generic_instruction_callback(ADDRINT instruction_address);
  static void cbranch_instruction_callback(bool is_branch_taken);
  static void indirectBrOrCall_instruction_callback(ADDRINT target_address);
};

} // end of instrumentation namespace

#endif // RESOLVER_H
