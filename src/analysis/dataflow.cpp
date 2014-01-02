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
#include "../main.h"
#include "../utilities/utils.h"
#include "../engine/checkpoint.h"
#include <boost/container/set.hpp>
#include <boost/graph/breadth_first_search.hpp>

namespace analysis 
{

using namespace utilities;

static boost::unordered_map<ADDRINT, exeorders_t> exeorders_afffected_by_memory_at;
static boost::unordered_map<UINT32, addresses_t>  memories_affecting_exeorder_at;

extern boost::unordered_map<ADDRINT, UINT8>       original_value_of_memory_at;

typedef instruction_operand dataflow_vertex;
typedef UINT32              dataflow_edge;

typedef boost::adjacency_list<boost::listS, boost::vecS, 
                              boost::bidirectionalS, 
                              dataflow_vertex, 
                              dataflow_edge>                      dataflow_graph;
                              
typedef boost::graph_traits<dataflow_graph>::vertex_descriptor    dataflow_vertex_desc;
typedef boost::graph_traits<dataflow_graph>::edge_descriptor      dataflow_edge_desc;
typedef boost::graph_traits<dataflow_graph>::vertex_iterator      dataflow_vertex_iter;
typedef boost::graph_traits<dataflow_graph>::edge_iterator        dataflow_edge_iter;

static dataflow_graph                              forward_dataflow;
static dataflow_graph                              backward_dataflow;
static boost::unordered_set<dataflow_vertex_desc>  outer_interface;
  
/**
 * @brief BFS visitor to discover all dependent edges from a vertex.
 * 
 */
class dataflow_bfs_visitor : public boost::default_bfs_visitor 
{
public:
	boost::container::set<dataflow_edge_desc> examined_edges;

	template <typename Edge, typename Graph>
	void tree_edge(Edge e, const Graph& g) 
	{
		examined_edges.insert(e);
		return;
	}	
};


/**
 * @brief in inserting a new instruction into the data-flow graph, its source operands are 
 * considered as source vertices of a hyper-edge. To insert this edge to current data-flow graph, 
 * the source vertices need to be connected to the outer-interface of the graph. Note that the 
 * outer-interface will be modified in this connection.
 * 
 * @param execution_order execution order of the insert instruction
 * @param outer_interface current outer-interface of the forward data-flow graph
 * @param forward_dataflow forward data-flow graph
 * @param backward_dataflow backward data-flow graph
 * @return source vertices of the hyper-edge
 */														
static inline boost::unordered_set<dataflow_vertex_desc> 
construct_source_vertices(UINT32 execution_order, 
                          boost::shared_ptr<instruction> inserted_ins,
                          boost::unordered_set<dataflow_vertex_desc>& outer_interface, 
                          dataflow_graph& forward_dataflow, dataflow_graph& backward_dataflow) 
{
	boost::unordered_set<dataflow_vertex_desc>::iterator outer_interface_iter;
	
	dataflow_vertex_desc newly_inserted_vertex;
	boost::unordered_set<instruction_operand, operand_hash>::iterator operand_iter;
	
	// construct the set of source vertex for the inserted instruction
	boost::unordered_set<dataflow_vertex_desc> source_vertices;
	// iterate in the list of the instruction's source operands
	for (operand_iter = inserted_ins->source_operands.begin(); 
			 operand_iter != inserted_ins->source_operands.end(); ++operand_iter) 
	{
		// verify if the source operand is in the outer interface
		for (outer_interface_iter = outer_interface.begin(); 
				 outer_interface_iter != outer_interface.end(); ++outer_interface_iter) 
		{
			// it is already in the outer interface
			if (forward_dataflow[*outer_interface_iter] == *operand_iter) 
			{
				// insert it directly into the set of source vertices.
				source_vertices.insert(*outer_interface_iter);
				break;
			}			
		}
		
		// the source operand is not in the outer interface
		if (outer_interface_iter == outer_interface.end()) 
		{
			// then insert it into the forward dependence graph,
			newly_inserted_vertex = boost::add_vertex(*operand_iter, forward_dataflow);
			// into the outer interface if it is not an immediate
			if ((*operand_iter).value.type() != typeid(UINT32)) 
			{
				outer_interface.insert(newly_inserted_vertex);
			}
			// into the set of source vertex for the inserted instruction
			source_vertices.insert(newly_inserted_vertex);
			
			// into the backward dependence graph
			boost::add_vertex(*operand_iter, backward_dataflow);
		}
	}
	
	return source_vertices;
}


/**
 * @brief in inserting a new instruction into the data-flow graph, its target operands are 
 * considered as target vertices of a hyper-edge, the outer-interface will be updated. Note that 
 * the function has an important side-effect: it modifies the map "original_value_at_address" which 
 * will be used in storing checkpoints.
 * 
 * @param execution_order execution order of the insert instruction
 * @param outer_interface current outer-interface of the forward data-flow graph
 * @param forward_dataflow forward data-flow graph
 * @param backward_dataflow backward data-flow graph
 * @return target vertices of the hyper-edge
 */	
static inline boost::unordered_set<dataflow_vertex_desc> 
construct_target_vertices(UINT32 execution_order, boost::shared_ptr<instruction> inserted_ins,
                          boost::unordered_set<dataflow_vertex_desc>& outer_interface, 
													dataflow_graph& forward_dataflow, dataflow_graph& backward_dataflow) 
{
	boost::unordered_set<dataflow_vertex_desc>::iterator outer_interface_iter;
  boost::unordered_set<instruction_operand, operand_hash>::iterator operand_iter;
	dataflow_vertex_desc newly_inserted_vertex;
	ADDRINT mem_addr;
	
	// construct the set of target vertex for the inserted instruction
	boost::unordered_set<dataflow_vertex_desc> target_vertices;
	// iterate in the list of the instruction's target operands
	for (operand_iter = inserted_ins->target_operands.begin(); 
			 operand_iter != inserted_ins->target_operands.end(); ++operand_iter) 
	{
		// verify if the target operand is a memory address
		if (operand_iter->value.type() == typeid(ADDRINT)) 
		{
      mem_addr = boost::get<ADDRINT>(operand_iter->value);
      if (original_value_of_memory_at.find(mem_addr) == original_value_of_memory_at.end()) 
      {
        // save the original value at this address if it does not exist yet
        original_value_of_memory_at[mem_addr] = *(reinterpret_cast<UINT8*>(mem_addr));
      }
		}
		
		// insert the target operand into the forward dependence graph
		newly_inserted_vertex = boost::add_vertex(*operand_iter, forward_dataflow);
		// into the set of target vertex for the inserted instruction
		target_vertices.insert(newly_inserted_vertex);
		
		// into the backward dependence graph
		boost::add_vertex(*operand_iter, backward_dataflow);
		
		// verify if the target operand is in the outer interface
		for (outer_interface_iter = outer_interface.begin(); 
				 outer_interface_iter !=  outer_interface.end(); ++outer_interface_iter) 
		{
			// it is already in the outer interface
			if (forward_dataflow[*outer_interface_iter] == *operand_iter) 
			{
				// remove the old 
				outer_interface.erase(outer_interface_iter);
				break;
			}
		}
		// insert the new
		outer_interface.insert(newly_inserted_vertex);
	}
	
	return target_vertices;
}


/**
 * @brief insert a new instruction into the forward and backward data-flow graphs: the read/written 
 * registers of the instruction can be determined statically (in the loading time) but the 
 * read/written memories can only be determined in running time.
 * 
 * @param execution_order execution order of the inserted instruction
 * @return void
 */
void dataflow::propagate_along_instruction(UINT32 execution_order)
{
  boost::unordered_set<dataflow_vertex_desc>::iterator outer_interface_iter;  
  ptr_instruction_t executed_ins =  instruction_at_exeorder[execution_order];
	
	// construct the set of source vertex for the inserted instruction
	boost::unordered_set<dataflow_vertex_desc> source_vertices;
	source_vertices = construct_source_vertices(execution_order, executed_ins, 
																							outer_interface, 
																							forward_dataflow, backward_dataflow);
	
	// construct the set of target vertex for the inserted instruction
	boost::unordered_set<dataflow_vertex_desc> target_vertices;
	target_vertices = construct_target_vertices(execution_order, executed_ins, 
																							outer_interface, 
																							forward_dataflow, backward_dataflow);
	
	// construct the hyper-edge
	boost::unordered_set<dataflow_vertex_desc>::iterator source_iter;
	boost::unordered_set<dataflow_vertex_desc>::iterator target_iter;
	for (source_iter = source_vertices.begin(); source_iter != source_vertices.end(); ++source_iter) 
	{		
		for (target_iter = target_vertices.begin(); target_iter != target_vertices.end(); ++target_iter) 
		{
			// insert the hyper-edge between source and target vertices into data-flow graphs
			boost::add_edge(*source_iter, *target_iter, execution_order, forward_dataflow);
			boost::add_edge(*target_iter, *source_iter, execution_order, backward_dataflow);
		}
	}
	
  return;
}


/**
 * @brief the following information will be extracted from the data-flow
 *  1. for each memory address: a set of instruction execution orders that propagate information 
 *     of this address, the results are stored in the map exeorders_afffected_by_memory_at,
 *  2. for each instruction execution order: a set of memory addresses whose information 
 *     propagate to this order, the results are stored in the map memories_affecting_exeorder_at.
 * 
 * @return void
 */
static void determine_inputs_instructions_dependance()
{
  
	dataflow_vertex_iter vertex_iter;
	dataflow_vertex_iter vertex_last_iter;
	
	ADDRINT memory_address;
	UINT32 ins_order;
	dataflow_bfs_visitor curr_visitor;
	boost::container::set<dataflow_edge_desc>::iterator dataflow_edge_iter;
	
	// iterate over vertices in the forward data-flow graph
	boost::tie(vertex_iter, vertex_last_iter) = boost::vertices(forward_dataflow);
	for (; vertex_iter != vertex_last_iter; ++vertex_iter) 
	{
		// verify if the operand corresponding to the vertex is a memory address
		if (forward_dataflow[*vertex_iter].value.type() == typeid(ADDRINT)) 
		{
			memory_address = boost::get<ADDRINT>(forward_dataflow[*vertex_iter].value);
			if (utils::is_input_buffer(memory_address)) 
			{
				// take BFS from this vertex to find out all dependent edge descriptors
				curr_visitor.examined_edges.clear();
				boost::breadth_first_search(forward_dataflow, *vertex_iter, boost::visitor(curr_visitor));
				// update the list of dependent instructions
				for (dataflow_edge_iter = curr_visitor.examined_edges.begin(); 
						 dataflow_edge_iter != curr_visitor.examined_edges.end(); ++dataflow_edge_iter) 
				{
					ins_order = forward_dataflow[*dataflow_edge_iter];
					exeorders_afffected_by_memory_at[memory_address].insert(ins_order); // see 1
					memories_affecting_exeorder_at[ins_order].insert(memory_address);   // see 2
				}
			}
		}
	}
	
	return;
}


/**
 * @brief the following information will be calculated from the two maps above
 *  1. for each conditional branch: the set of checkpoints so that if the program re-executes from 
 *     any of its elements with some modification on the input buffer, then the new execution may 
 *     lead to a new decision of the branch.
 * 
 * @return void
 */
static void determine_branches_checkpoints_dependance()
{
  boost::unordered_map<UINT32, ptr_conditional_branch_t>::iterator branch_iter;
  for (branch_iter = branch_at_exeorder.begin(); branch_iter != branch_at_exeorder.end(); 
       ++branch_iter)
  {
    //
  }
  return;
}

/**
 * @brief the following information will be extracted from the executed instructions
 *  1. for each memory address: the list of instruction execution orders that propagate information 
 *     of this address,
 *  2. for each instruction execution order: the list of memory addresses whose information 
 *     propagate to this order,
 * 
 * @return void
 */
void dataflow::analyze_executed_instructions()
{
  determine_inputs_instructions_dependance();
  determine_branches_checkpoints_dependance();
  return;
}

} // end of analysis namespace
