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

namespace analysis 
{

typedef instruction_operand																				dataflow_vertex;
typedef UINT32 																					 					dataflow_edge;

typedef boost::adjacency_list<boost::listS, 
															boost::vecS, 
															boost::bidirectionalS, 
                              dataflow_vertex, 
															dataflow_edge>  					 					dataflow_graph;
                              
typedef boost::graph_traits<dataflow_graph>::vertex_descriptor 		dataflow_vertex_desc;
typedef boost::graph_traits<dataflow_graph>::edge_descriptor   		dataflow_edge_desc;
typedef boost::graph_traits<dataflow_graph>::vertex_iterator   		dataflow_vertex_iter;
typedef boost::graph_traits<dataflow_graph>::edge_iterator     		dataflow_edge_iter;

class dataflow
{
private:
  dataflow_graph 															forward_dataflow;
  dataflow_graph 															backward_dataflow;
  boost::unordered_set<dataflow_vertex_desc> 	outer_interface;
	
public:
	boost::unordered_set<ADDRINT>								modified_memory_addresses;
 
public:
  void propagate_along_instruction(UINT32 execution_order);
	void extract_inputs_instructions_dependance_maps();
	void arrange_checkpoints();
  void clear();
};

} // end of dataflow_analysis namespace

#endif // DATAFLOW_GRAPH_H
