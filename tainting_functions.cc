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
inline void add_source_variables(ADDRINT ins_addr, vdep_vertex_desc_set& src_descs) 
{
  std::set<REG>::iterator      reg_iter;
  std::set<UINT32>::iterator   imm_iter;
  std::set<ADDRINT>::iterator  mem_iter;
    
  var_set src_vars;
  
  for (reg_iter = boost::get<0>(dta_inss_io[ins_addr]).first.begin(); 
       reg_iter != boost::get<0>(dta_inss_io[ins_addr]).first.end(); ++reg_iter)
  {
    src_vars.insert(variable(*reg_iter));
  }
    
  for (mem_iter = boost::get<2>(dta_inss_io[ins_addr]).first.begin(); 
       mem_iter != boost::get<2>(dta_inss_io[ins_addr]).first.end(); ++mem_iter)
  {
    src_vars.insert(variable(*mem_iter));
  }
  
  // insert the source variables into the tainting graph and its outer interface
  vdep_vertex_desc_set::iterator vertex_iter = dta_outer_vertices.begin();
//   vdep_vertex_desc_set::iterator last_vertex_iter = dta_outer_vertices.end();
  vdep_vertex_desc new_vertex_desc;
    
  std::cout << "src_vars size: " << src_vars.size() << " ";
  for (var_set::iterator src_iter = src_vars.begin(); src_iter != src_vars.end(); ++src_iter)
  {
    for (vertex_iter = dta_outer_vertices.begin(); vertex_iter != dta_outer_vertices.end(); ++vertex_iter) 
    {
      // the current source operand is found in the outer interface
      if (*src_iter == dta_graph[*vertex_iter]) 
      {
        src_descs.insert(*vertex_iter);
        break;
      }
    }
    
    // not found
    if (vertex_iter == dta_outer_vertices.end())        
    {
      new_vertex_desc = boost::add_vertex(*src_iter, dta_graph);
      
      dta_outer_vertices.insert(new_vertex_desc);

      src_descs.insert(new_vertex_desc);
    }    
  }
  
  std::cout << "src_descs size: " << src_descs.size();
  
  return;
}

/*====================================================================================================================*/
// destination variable construction 
inline void add_destination_variables(ADDRINT ins_addr, vdep_vertex_desc_set& dst_descs) 
{
  std::set<REG>::iterator      reg_iter;
  std::set<UINT32>::iterator   imm_iter;
  std::set<ADDRINT>::iterator  mem_iter;
  
  var_set dst_vars;
  
  for (reg_iter = boost::get<0>(dta_inss_io[ins_addr]).second.begin(); 
       reg_iter != boost::get<0>(dta_inss_io[ins_addr]).second.end(); ++reg_iter) 
  {
    dst_vars.insert(variable(*reg_iter));
  }
  
  for (mem_iter = boost::get<2>(dta_inss_io[ins_addr]).second.begin(); 
       mem_iter != boost::get<2>(dta_inss_io[ins_addr]).second.end(); ++mem_iter) 
  {
    dst_vars.insert(variable(*mem_iter));
  }
  
  // insert the destination variables into the tainting graph and its outer interface
  vdep_vertex_desc_set::iterator vertex_iter = dta_outer_vertices.begin();
  vdep_vertex_desc_set::iterator last_vertex_iter = dta_outer_vertices.end();
  vdep_vertex_desc_set::iterator next_vertex_iter;
  
  
  vdep_vertex_desc new_vertex_desc;
  
  for (var_set::iterator dst_iter = dst_vars.begin(); dst_iter != dst_vars.end(); ++dst_iter) 
  {
    for (next_vertex_iter = vertex_iter; vertex_iter != last_vertex_iter; vertex_iter = next_vertex_iter) 
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
        
        dst_descs.insert(new_vertex_desc);
        break;
      }
    }
    
    // not found
    if (vertex_iter == last_vertex_iter) 
    {
      new_vertex_desc = boost::add_vertex(*dst_iter, dta_graph);
      
      // modify the outer interface
      dta_outer_vertices.insert(new_vertex_desc);      
      
      dst_descs.insert(new_vertex_desc);
    }
  }
  
  std::cout << " dst_descs size: " << dst_descs.size() << "\n";
    
  return;
}

/*====================================================================================================================*/

VOID tainting_st_to_st_analyzer(ADDRINT ins_addr)
{  
  vdep_vertex_desc_set src_vertex_descs;
  vdep_vertex_desc_set dst_vertex_descs;
  
  add_source_variables(ins_addr, src_vertex_descs);
  add_destination_variables(ins_addr, dst_vertex_descs);
  
  // insert the edges between each pair (source, destination) into the tainting graph
  vdep_vertex_desc_set::iterator src_desc_iter = src_vertex_descs.begin();
  vdep_vertex_desc_set::iterator dst_desc_iter = dst_vertex_descs.begin();
  UINT32 current_ins_order = static_cast<UINT32>(explored_trace.size());
  
  for (; src_desc_iter != src_vertex_descs.end(); ++src_desc_iter)
  {
    for (; dst_desc_iter != dst_vertex_descs.end(); ++dst_desc_iter)
    {
      if (dta_graph[*src_desc_iter] == dta_graph[*dst_desc_iter])
      {
        // omit loopback edges
      }
      else
      {
        boost::add_edge(*src_desc_iter, *dst_desc_iter, std::make_pair(ins_addr, current_ins_order), dta_graph);
      }
    }
  }

  return;
}

/*====================================================================================================================*/

VOID tainting_mem_to_st_analyzer(ADDRINT ins_addr, ADDRINT mem_read_addr, UINT32 mem_read_size)
{
  // clear out input/output memory operands
  std::set<ADDRINT>().swap(boost::get<2>(dta_inss_io[ins_addr]).first);
  std::set<ADDRINT>().swap(boost::get<2>(dta_inss_io[ins_addr]).second);
  
  // update input memory operands
  for (UINT32 mem_idx = 0; mem_idx < mem_read_size; ++mem_idx)
  {
    boost::get<2>(dta_inss_io[ins_addr]).first.insert(mem_read_addr + mem_idx);
  }
  
  tainting_st_to_st_analyzer(ins_addr);
  
  return;
}

/*====================================================================================================================*/

VOID tainting_st_to_mem_analyzer(ADDRINT ins_addr, ADDRINT mem_written_addr, UINT32 mem_written_size)
{
   // clear out input/output memory operands
  std::set<ADDRINT>().swap(boost::get<2>(dta_inss_io[ins_addr]).first);
  std::set<ADDRINT>().swap(boost::get<2>(dta_inss_io[ins_addr]).second);
  
  // update output memory operands
  for (UINT32 mem_idx = 0; mem_idx < mem_written_size; ++mem_idx)
  {
    boost::get<2>(dta_inss_io[ins_addr]).second.insert(mem_written_addr + mem_idx);
  }
  
  tainting_st_to_st_analyzer(ins_addr);
  
  return;
}