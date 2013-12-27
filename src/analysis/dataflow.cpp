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

#include "dataflow.h"
#include "../utilities/utils.h"
#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>
#include <boost/shared_ptr.hpp>

namespace analysis 
{

using namespace utilities;

extern boost::unordered_map<ADDRINT, 
                            boost::shared_ptr<instruction>
                            >         address_instruction_map;

extern boost::unordered_map<UINT32, 
                            ADDRINT>  execution_order_address_map;

extern boost::unordered_map<ADDRINT, 
														boost::unordered_set<UINT32>
														> 				memory_instructions_dependency_map;

extern boost::unordered_map<UINT32, 
														boost::unordered_set<ADDRINT>
														>					instruction_memories_dependency_map;														

/**
 * @brief insert a new instruction into the forward and backward data-flow graphs: the read/written 
 * registers of the instruction can be determined statically (in the loading time) but the 
 * read/written memories can only be determined in running time.
 * 
 * @param execution_order execution order of the inserted instruction
 * @return void
 */
void dataflow::propagate(UINT32 execution_order)
{
  boost::unordered_set<dataflow_vertex_desc>::iterator outer_interface_iter;
  
	ADDRINT ins_addr = execution_order_address_map[execution_order];
	boost::shared_ptr<instruction> inserted_ins = address_instruction_map[ins_addr];

	dataflow_vertex_desc newly_inserted_vertex;
	boost::unordered_set<instruction_operand, operand_hash>::iterator operand_iter;
	
	// construct the set of source vertex for the inserted instruction
	boost::unordered_set<dataflow_vertex_desc> source_vertices;
	// iterate in the list of the instruction's source operands
	for (operand_iter = inserted_ins->source_operands.begin(); 
			 operand_iter != inserted_ins->source_operands.end(); ++operand_iter) 
	{
		// verify if the source operand is in the outer interface
		for (outer_interface_iter = this->outer_interface.begin(); 
				 outer_interface_iter != this->outer_interface.end(); ++outer_interface_iter) 
		{
			// it is already in the outer interface
			if (this->forward_dataflow[*outer_interface_iter] == *operand_iter) 
			{
				// insert it directly into the set of source vertices.
				source_vertices.insert(*outer_interface_iter);
				break;
			}			
		}
		
		// the source operand is not in the outer interface
		if (outer_interface_iter == this->outer_interface.end()) 
		{
			// then insert it into the forward dependence graph,
			newly_inserted_vertex = boost::add_vertex(*operand_iter, this->forward_dataflow);
			// into the outer interface if it is not an immediate
			if ((*operand_iter).value.type() != typeid(UINT32)) 
			{
				this->outer_interface.insert(newly_inserted_vertex);
			}
			// into the set of source vertex for the inserted instruction
			source_vertices.insert(newly_inserted_vertex);
			
			// into the backward dependence graph
			boost::add_vertex(*operand_iter, this->backward_dataflow);
		}
	}
	
	// construct the set of target vertex for the inserted instruction
	boost::unordered_set<dataflow_vertex_desc> target_vertices;
	// iterate in the list of the instruction's target operands
	for (operand_iter = inserted_ins->target_operands.begin(); 
			 operand_iter != inserted_ins->target_operands.end(); ++operand_iter) 
	{
		// insert the target operand into the forward dependence graph
		newly_inserted_vertex = boost::add_vertex(*operand_iter, this->forward_dataflow);
		// into the set of target vertex for the inserted instruction
		target_vertices.insert(newly_inserted_vertex);
		
		// into the backward dependence graph
		boost::add_vertex(*operand_iter, this->backward_dataflow);
		
		// verify if it is in the outer interface
		for (outer_interface_iter = this->outer_interface.begin(); 
				 outer_interface_iter !=  this->outer_interface.end(); ++outer_interface_iter) 
		{
			// it is already in the outer interface
			if (this->forward_dataflow[*outer_interface_iter] == *operand_iter) 
			{
				// replace it in the interface
				this->outer_interface.insert(newly_inserted_vertex);
				this->outer_interface.erase(outer_interface_iter);
				break;
			}
		}
		
		// the target operand is not in the outer interface
		if (outer_interface_iter == this->outer_interface.end()) 
		{
			// insert it into the interface
			this->outer_interface.insert(newly_inserted_vertex);
		}
	}
	
	// constructions
	boost::unordered_set<dataflow_vertex_desc>::iterator source_iter;
	boost::unordered_set<dataflow_vertex_desc>::iterator target_iter;
	ADDRINT source_address;
	for (source_iter = source_vertices.begin(); source_iter != source_vertices.end(); ++source_iter) 
	{
		// insert the current instruction into the set of dependent instructions for a given 
		// input-dependent address by first verify if an element in the source is a memory address
		if (this->forward_dataflow[*source_iter].value.type() == typeid(ADDRINT))
		{
			// then verify if it is input-dependent
			source_address = boost::get<ADDRINT>(this->forward_dataflow[*source_iter].value);
			if ((utils::is_input_dependent(source_address)) && !target_vertices.empty()) 
			{
				// insert the current instruction into the set of dependent instructions
				memory_instructions_dependency_map[source_address].insert(execution_order);
				instruction_memories_dependency_map[execution_order].insert(source_address);
			}
		}
		
		for (target_iter = target_vertices.begin(); target_iter != target_vertices.end(); ++target_iter) 
		{
			// insert the hyper-edge between source and target vertices into data-flow graphs
			boost::add_edge(*source_iter, *target_iter, execution_order, this->forward_dataflow);
			boost::add_edge(*target_iter, *source_iter, execution_order, this->backward_dataflow);
		}
	}
	
  return;
}

} // end of analysis namespace
