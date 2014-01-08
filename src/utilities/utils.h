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

#ifndef UTILS_H
#define UTILS_H

#include <pin.H>
#include <string>

namespace utilities 
{
	
class utils
{
public:
  static UINT8 random_uint8();
  static std::string remove_leading_zeros(std::string input);
  static std::string addrint2hexstring(ADDRINT input);
	static bool is_in_input_buffer(ADDRINT memory_address);
};

} // end of utilities namespace

#endif // UTILS_H
