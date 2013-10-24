#include <pin.H>

#include <boost/graph/lookup_edge.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/format.hpp>

#include "variable.h"
#include "branch.h"

/*====================================================================================================================*/

extern vdep_graph           dta_graph;
extern map_ins_io           dta_inss_io;

extern std::vector<ADDRINT> explored_trace;

extern std::ofstream        tainting_log_file;

/*====================================================================================================================*/
// source variables construction 
inline void add_source_variables(ADDRINT ins_addr, std::vector<vdep_vertex_desc>& src_descs) 
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
  
//   for (imm_iter = boost::get<1>(dta_inss_io[ins_addr]).first.begin();
//        imm_iter != boost::get<1>(dta_inss_io[ins_addr]).first.end(); ++imm_iter)
//   {
//     src_vars.insert(variable(*imm_iter));
//   }
  
  for (mem_iter = boost::get<2>(dta_inss_io[ins_addr]).first.begin(); 
       mem_iter != boost::get<2>(dta_inss_io[ins_addr]).first.end(); ++mem_iter)
  {
    src_vars.insert(variable(*mem_iter));
  }
  
  // insert the source variables into the tainting graph
  vdep_vertex_iter vertex_iter;
  vdep_vertex_iter last_vertex_iter;
  
  for (var_set::iterator src_iter = src_vars.begin(); src_iter != src_vars.end(); ++src_iter)
  {
    for (boost::tie(vertex_iter, last_vertex_iter) = boost::vertices(dta_graph); 
         vertex_iter != last_vertex_iter; ++vertex_iter)
    {
      if (*src_iter == dta_graph[*vertex_iter])
      {
        src_descs.push_back(*vertex_iter);
        break;
      }
    }
    
    if (vertex_iter == last_vertex_iter)
    {
      src_descs.push_back(boost::add_vertex(*src_iter, dta_graph));
    }
  }
  
  return;
}

/*====================================================================================================================*/
// destination variable construction 
inline void add_destination_variables(ADDRINT ins_addr, std::vector<vdep_vertex_desc>& dst_descs) 
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
  
  vdep_vertex_iter vertex_iter;
  vdep_vertex_iter last_vertex_iter;
  
  for (var_set::iterator dst_iter = dst_vars.begin(); dst_iter != dst_vars.end(); ++dst_iter) 
  {
    for (boost::tie(vertex_iter, last_vertex_iter) = boost::vertices(dta_graph); 
         vertex_iter != last_vertex_iter; ++vertex_iter) 
    {
      if (*dst_iter == dta_graph[*vertex_iter]) 
      {
        dst_descs.push_back(*vertex_iter);
        break;
      }
    }
    
    if (vertex_iter == last_vertex_iter) 
    {
      dst_descs.push_back(boost::add_vertex(*dst_iter, dta_graph));
    }
  }
    
  return;
}

/*====================================================================================================================*/

VOID tainting_st_to_st_analyzer(ADDRINT ins_addr)
{  
  std::vector<vdep_vertex_desc> src_vertex_descs;
  std::vector<vdep_vertex_desc> dst_vertex_descs;
  
  add_destination_variables(ins_addr, dst_vertex_descs);
  add_source_variables(ins_addr, src_vertex_descs);
  
  // insert the edges between each pair (source, destination) into the tainting graph
  std::vector<vdep_vertex_desc>::iterator src_desc_iter;
  std::vector<vdep_vertex_desc>::iterator dst_desc_iter;
  UINT32 current_ins_order = static_cast<UINT32>(explored_trace.size());
  
  for (src_desc_iter = src_vertex_descs.begin(); src_desc_iter != src_vertex_descs.end(); 
       ++src_desc_iter)
  {
    for (dst_desc_iter = dst_vertex_descs.begin(); dst_desc_iter != dst_vertex_descs.end(); 
         ++dst_desc_iter)
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