#include <pin.H>

#include <boost/graph/lookup_edge.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/format.hpp>

#include "../base/branch.h"
#include "../base/operand.h"
#include "../base/instruction.h"

/*================================================================================================*/

extern df_diagram           dta_graph;
extern df_vertex_desc_set     dta_outer_vertices;

extern std::map<UINT32, ptr_instruction_t>  ins_at_order;

extern std::vector<ADDRINT>     explored_trace;
extern UINT32                   current_exec_order;

/*================================================================================================*/
// source variables construction
inline std::set<df_vertex_desc> source_variables(UINT32 idx)
{
  df_vertex_desc_set src_vertex_descs;
  df_vertex_desc_set::iterator outer_vertex_iter;
  df_vertex_desc new_vertex_desc;
  std::set<ptr_operand_t>::iterator src_operand_iter;
  for (src_operand_iter = ins_at_order[idx]->src_operands.begin();
       src_operand_iter != ins_at_order[idx]->src_operands.end(); ++src_operand_iter)
  {
    // verify if the current source operand is
    for (outer_vertex_iter = dta_outer_vertices.begin();
         outer_vertex_iter != dta_outer_vertices.end(); ++outer_vertex_iter)
    {
      // found in the outer interface
      if (((*src_operand_iter)->value.type() == dta_graph[*outer_vertex_iter]->value.type()) &&
          ((*src_operand_iter)->name == dta_graph[*outer_vertex_iter]->name))
      {
        src_vertex_descs.insert(*outer_vertex_iter);
        break;
      }
    }

    // not found
    if (outer_vertex_iter == dta_outer_vertices.end())
    {
      new_vertex_desc = boost::add_vertex(*src_operand_iter, dta_graph);
      dta_outer_vertices.insert(new_vertex_desc);
      src_vertex_descs.insert(new_vertex_desc);
    }
  }

  return src_vertex_descs;
}

/*================================================================================================*/
// destination variable construction
inline std::set<df_vertex_desc> destination_variables(UINT32 idx)
{
  std::set<df_vertex_desc> dst_vertex_descs;
  df_vertex_desc new_vertex_desc;
  df_vertex_desc_set::iterator outer_vertex_iter;
  df_vertex_desc_set::iterator next_vertex_iter;
  std::set<ptr_operand_t>::iterator dst_operand_iter;

  for (dst_operand_iter = ins_at_order[idx]->dst_operands.begin();
       dst_operand_iter != ins_at_order[idx]->dst_operands.end(); ++dst_operand_iter)
  {
    // verify if the current target operand is
    outer_vertex_iter = dta_outer_vertices.begin();
    for (next_vertex_iter = outer_vertex_iter;
         outer_vertex_iter != dta_outer_vertices.end(); outer_vertex_iter = next_vertex_iter)
    {
      ++next_vertex_iter;

      // found in the outer interface
      if (((*dst_operand_iter)->value.type() == dta_graph[*outer_vertex_iter]->value.type())
          && ((*dst_operand_iter)->name == dta_graph[*outer_vertex_iter]->name))
      {
        // then insert the current target operand into the graph
        new_vertex_desc = boost::add_vertex(*dst_operand_iter, dta_graph);

        // and modify the outer interface by replacing the old vertex with the new vertex
        dta_outer_vertices.erase(outer_vertex_iter);
        dta_outer_vertices.insert(new_vertex_desc);

        dst_vertex_descs.insert(new_vertex_desc);
        break;
      }
    }

    // not found
    if (outer_vertex_iter == dta_outer_vertices.end())
    {
      // then insert the current target operand into the graph
      new_vertex_desc = boost::add_vertex(*dst_operand_iter, dta_graph);

      // and modify the outer interface by insert the new vertex
      dta_outer_vertices.insert(new_vertex_desc);

      dst_vertex_descs.insert(new_vertex_desc);
    }
  }

  return dst_vertex_descs;
}

/*================================================================================================*/

VOID tainting_general_instruction_analyzer(ADDRINT ins_addr)
{
  UINT32 /*current_ins_order = explored_trace.size();*/
  current_ins_order = current_exec_order;

  std::set<df_vertex_desc> src_vertex_descs = source_variables(current_ins_order);
  std::set<df_vertex_desc> dst_vertex_descs = destination_variables(current_ins_order);

  std::set<df_vertex_desc>::iterator src_vertex_desc_iter;
  std::set<df_vertex_desc>::iterator dst_vertex_desc_iter;

  // insert the edges between each pair (source, destination) into the tainting graph
  for (src_vertex_desc_iter = src_vertex_descs.begin();
       src_vertex_desc_iter != src_vertex_descs.end(); ++src_vertex_desc_iter) 
  {
    for (dst_vertex_desc_iter = dst_vertex_descs.begin();
         dst_vertex_desc_iter != dst_vertex_descs.end(); ++dst_vertex_desc_iter) 
    {
//      boost::add_edge(*src_vertex_desc_iter, *dst_vertex_desc_iter,
//                      std::make_pair(ins_addr, current_ins_order), dta_graph);
      boost::add_edge(*src_vertex_desc_iter, *dst_vertex_desc_iter, current_ins_order, dta_graph);
    }
  }

  return;
}
