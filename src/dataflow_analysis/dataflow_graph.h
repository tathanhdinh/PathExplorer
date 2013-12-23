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
#include <boost/graph/adjacency_list.hpp>

namespace dataflow_analysis 
{

typedef boost::adjacency_list<boost::listS, boost::vecS, boost::bidirectionalS, 
                              instruction_operand, instruction>  dependence_graph;
                              
typedef boost::graph_traits<dependence_graph>::vertex_descriptor depgraph_vertex_desc;
typedef boost::graph_traits<dependence_graph>::edge_descriptor   depgraph_edge_desc;
typedef boost::graph_traits<dependence_graph>::vertex_iterator   depgraph_vertex_iter;
typedef boost::graph_traits<dependence_graph>::edge_iterator     depgraph_edge_iter;

class dataflow_graph
{
public:
  dependence_graph forward_dependence_graph;
  dependence_graph backward_dependence_graph;
  
  void propagate_forward(ADDRINT instruction_address);
  void construct_backward();
};

} // end of dataflow_analysis namespace

#endif // DATAFLOW_GRAPH_H
