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

#ifndef DATAFLOW_GRAPH_H
#define DATAFLOW_GRAPH_H

#include "instruction.h"
#include "instruction_operand.h"
#include <pin.H>
#include <boost/shared_ptr.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/unordered_set.hpp>
#include <boost/compressed_pair.hpp>
#include <boost/unordered_map.hpp>

namespace analysis 
{

class dataflow
{
public:
  static void propagate_along_instruction(UINT32 execution_order);
  static void analyze_executed_instructions();
  static boost::unordered_set<ptr_insoperand_t> current_outerface();
  static void clear();
};

} // end of dataflow_analysis namespace

#endif // DATAFLOW_GRAPH_H
