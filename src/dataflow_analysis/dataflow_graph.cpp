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
#include <boost/shared_ptr.hpp>

namespace dataflow_analysis 
{

extern boost::unordered_map<ADDRINT, 
                            boost::shared_ptr<instruction>
                            >         address_instruction_map;
extern boost::unordered_map<UINT32, 
                            ADDRINT>  execution_order_address_map;


/**
 * @brief insert a new instruction into the forward and backward dependence graphs: the read/written 
 * the read/written registers of the instruction can be determined statically (in the loading time) 
 * but the read/written memories can only be determined in running time.
 * 
 * @param execution_order execution order of the inserted instruction
 * @return void
 */
void dataflow_graph::propagate_forward(UINT32 execution_order)
{
  boost::unordered_set<depgraph_vertex_desc> source_vertices;
  boost::unordered_set<depgraph_vertex_desc> target_vertices;
  
  boost::unordered_set<depgraph_vertex_desc>::iterator outer_interface_iter;
  boost::unordered_set<depgraph_vertex_desc>::iterator source_vertices_iter;
  boost::unordered_set<depgraph_vertex_desc>::iterator target_vertices_iter;
  
	ADDRINT ins_addr = execution_order_address_map[execution_order];
	boost::shared_ptr<instruction> inserted_ins = address_instruction_map[ins_addr];
	
	boost::unordered_set<depgraph_edge_desc> affected_src_vertex;
	
	// iterate in the list of the instruction's source operands
	boost::unordered_set<instruction_operand, operand_hash>::iterator src_operand_iter;
	for (src_operand_iter = inserted_ins->source_operands.begin(); 
			 src_operand_iter != inserted_ins->source_operands.end(); ++src_operand_iter) 
	{
		// verify if the source operand is in the outer interface
		for (outer_interface_iter = this->outer_interface.begin(); 
				 outer_interface_iter != this->outer_interface.end(); ++outer_interface_iter) 
		{
			if (forward_dependence_graph[*outer_interface] == *src_operand_iter) 
			{
				// the source operand is already in the outer interface, insert it directly into the list 
				// affected source vertex.
				affected_src_vertex.insert(*outer_interface);
				break;
			}			
		}
		
		// the source operand is not in the outer interface
		if (outer_interface_iter == this->outer_interface.end()) 
		{
			//
		}
	}
	
  return;
}

}
