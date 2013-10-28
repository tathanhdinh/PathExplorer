#include <pin.H>

#include <boost/graph/lookup_edge.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/format.hpp>

#include "variable.h"
#include "branch.h"

/*====================================================================================================================*/

extern vdep_graph           dta_graph;
extern map_ins_io           dta_inss_io;
extern vdep_vertex_desc_set dta_outer_vertices;

extern std::vector<ADDRINT> explored_trace;

extern std::ofstream        tainting_log_file;

/*====================================================================================================================*/
// source variables construction 
inline std::set<vdep_vertex_desc> source_variables(ADDRINT ins_addr, 
                                                       ADDRINT mem_read_addr, UINT32 mem_read_size) 
{
  std::set<REG>::iterator      reg_iter;
  std::set<UINT32>::iterator   imm_iter;
  std::set<ADDRINT>::iterator  mem_iter;
    
  var_set src_vars;
  std::set<vdep_vertex_desc> src_vertex_descs;
  
  for (reg_iter = addr_ins_static_map[ins_addr].src_regs.begin(); 
       reg_iter != addr_ins_static_map[ins_addr].src_regs.end(); ++reg_iter) 
  {
    src_vars.insert(variable(*reg_iter));
  }
    
  for (UINT32 idx = 0; idx < mem_read_size; ++idx) 
  {
    src_vars.insert(variable(mem_read_addr + idx));
  }
  
  // insert the source variables into the tainting graph and its outer interface
  vdep_vertex_desc_set::iterator vertex_iter;
  vdep_vertex_desc new_vertex_desc;
    
  for (var_set::iterator src_iter = src_vars.begin(); src_iter != src_vars.end(); ++src_iter)
  {
    for (vertex_iter = dta_outer_vertices.begin(); vertex_iter != dta_outer_vertices.end(); ++vertex_iter) 
    {
      // the current source operand is found in the outer interface
      if (*src_iter == dta_graph[*vertex_iter]) 
      {
        src_vertex_descs.insert(*vertex_iter);
        break;
      }
    }
    
    // not found
    if (vertex_iter == dta_outer_vertices.end())        
    {
      new_vertex_desc = boost::add_vertex(*src_iter, dta_graph);
      
      dta_outer_vertices.insert(new_vertex_desc);

      src_vertex_descs.insert(new_vertex_desc);
    }    
  }
    
  return src_vertex_descs;
}

/*====================================================================================================================*/
// destination variable construction 
inline std::set<vdep_vertex_desc> destination_variables(ADDRINT ins_addr, 
                                                        ADDRINT mem_written_addr, UINT32 mem_written_size) 
{
  std::set<REG>::iterator      reg_iter;
  std::set<UINT32>::iterator   imm_iter;
  std::set<ADDRINT>::iterator  mem_iter;
  
  var_set dst_vars;
  std::set<vdep_vertex_desc> dst_vertex_descs;
  
  for (reg_iter = addr_ins_static_map[ins_addr].dst_regs.begin(); 
       reg_iter != addr_ins_static_map[ins_addr].dst_regs.end(); ++reg_iter) 
  {
    dst_vars.insert(variable(*reg_iter));
  }
  
  for (UINT32 idx = 0; idx < mem_written_size; ++idx) 
  {
    dst_vars.insert(variable(mem_written_addr + idx));
  }
  
  // insert the destination variables into the tainting graph and its outer interface
  vdep_vertex_desc_set::iterator vertex_iter;
  vdep_vertex_desc_set::iterator last_vertex_iter;
  vdep_vertex_desc_set::iterator next_vertex_iter;
  
  vdep_vertex_desc new_vertex_desc;
  
  for (var_set::iterator dst_iter = dst_vars.begin(); dst_iter != dst_vars.end(); ++dst_iter) 
  {
    vertex_iter = dta_outer_vertices.begin();
    for (next_vertex_iter = vertex_iter; vertex_iter != dta_outer_vertices.end(); vertex_iter = next_vertex_iter) 
    {
      ++next_vertex_iter;
      
      // the current destination operand is found in the outer interface
      if (*dst_iter == dta_graph[*vertex_iter]) 
      {
        // first, insert a new vertex into the dependency graph
        new_vertex_desc = boost::add_vertex(*dst_iter, dta_graph);
        
        // then modify the outer interface
        dta_outer_vertices.erase(vertex_iter);
        dta_outer_vertices.insert(new_vertex_desc);
        
        dst_vertex_descs.insert(new_vertex_desc);
        break;
      }
    }
    
    // not found
    if (vertex_iter == dta_outer_vertices.end()) 
    {
      new_vertex_desc = boost::add_vertex(*dst_iter, dta_graph);
      
      // modify the outer interface
      dta_outer_vertices.insert(new_vertex_desc);      
      
      dst_vertex_descs.insert(new_vertex_desc);
    }
  }
      
  return dst_vertex_descs;
}

/*====================================================================================================================*/

VOID tainting_st_to_st_analyzer(ADDRINT ins_addr, 
                                ADDRINT mem_read_addr, UINT32 mem_read_size, 
                                ADDRINT mem_written_addr, UINT32 mem_written_size)
{  
  std::set<vdep_vertex_desc> src_vertex_descs = source_variables(ins_addr, mem_read_addr, mem_read_size);
  std::set<vdep_vertex_desc> dst_vertex_descs = destination_variables(ins_addr, mem_written_addr, mem_written_size);
  
  // insert the edges between each pair (source, destination) into the tainting graph
  std::set<vdep_vertex_desc>::iterator src_vertex_desc_iter;
  std::set<vdep_vertex_desc>::iterator dst_vertex_desc_iter;
  UINT32 current_ins_order = static_cast<UINT32>(explored_trace.size());
  
  for (src_vertex_desc_iter = src_vertex_descs.begin(); 
       src_vertex_desc_iter != src_vertex_descs.end(); ++src_vertex_desc_iter)
  {
    for (dst_vertex_desc_iter = dst_vertex_descs.begin(); 
         dst_vertex_desc_iter != dst_vertex_descs.end(); ++dst_vertex_desc_iter)
    {
      boost::add_edge(*src_vertex_desc_iter, *dst_vertex_desc_iter, 
                      std::make_pair(ins_addr, current_ins_order), dta_graph);
    }
  }

  return;
}
