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

#include "dataflow_graph.h"
#include <boost/unordered_map.hpp>

namespace dataflow_analysis 
{

extern boost::unordered_map<ADDRINT, instruction> address_instruction_map;
extern boost::unordered_map<UINT32, ADDRINT>      excution_order_address_map;

/*================================================================================================*/

void dataflow_graph::propagate_forward(ADDRINT instruction_address)
{
}

}
