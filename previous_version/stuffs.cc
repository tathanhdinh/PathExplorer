// #include <boost/cstdint.hpp>                                                    // for portable code

#include <map>
#include <set>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <limits>

#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/lookup_edge.hpp>


#include "instruction.h"
#include "variable.h"
#include "checkpoint.h"
#include "branch.h"

#include <pin.H>


/* ------------------------------------------------------------------------------------------------------------------ */
/*                                                       global variables                                             */
/* ------------------------------------------------------------------------------------------------------------------ */
extern std::map< ADDRINT,
       instruction >      addr_ins_static_map;
extern std::map< UINT32,
       instruction >      order_ins_dynamic_map;

extern vdep_graph                   dta_graph;
extern map_ins_io                   dta_inss_io;

extern std::vector<ptr_checkpoint>  saved_ptr_checkpoints;

extern ptr_branch                   active_ptr_branch;
extern ptr_branch                   exploring_ptr_branch;

extern std::vector<ptr_branch>      total_resolved_ptr_branches;
extern std::vector<ptr_branch>      found_new_ptr_branches;

extern std::map<UINT32, ptr_branch> order_input_indep_ptr_branch_map;
extern std::map<UINT32, ptr_branch> order_input_dep_ptr_branch_map;
extern std::map<UINT32, ptr_branch> order_tainted_ptr_branch_map;

extern std::vector<ADDRINT>         explored_trace;

// extern UINT32                       input_dep_branch_num;
// extern UINT32                       resolved_branch_num;

extern KNOB<UINT32>                 max_trace_length;
extern KNOB<BOOL>                   print_debug_text;

// extern std::ofstream                tainting_log_file;


/* ------------------------------------------------------------------------------------------------------------------ */
/*                                                         implementation                                             */
/* ------------------------------------------------------------------------------------------------------------------ */

std::string remove_leading_zeros(std::string input)
{
  std::string::iterator str_iter = input.begin();
  std::string output ("0x");

  while ((str_iter != input.end()) && 
         ((*str_iter == '0') || (*str_iter == 'x'))) 
  {
    ++str_iter;
  }

  while (str_iter != input.end())
  {
    output.push_back(*str_iter);
    ++str_iter;
  }

  return output;
}

/*====================================================================================================================*/

void journal_buffer(const std::string& filename, UINT8* buffer_addr, UINT32 buffer_size )
{
  std::ofstream file ( filename.c_str(),
                       std::ofstream::binary | std::ofstream::out | std::ofstream::trunc );

  std::copy ( buffer_addr, buffer_addr + buffer_size, std::ostreambuf_iterator<char> ( file ) );
  file.close();

  return;
}

/*====================================================================================================================*/

void journal_static_trace(const std::string& filename)
{
  std::ofstream out_file (filename.c_str(), std::ofstream::out | std::ofstream::trunc);

  std::map<ADDRINT, instruction>::iterator addr_ins_iter = addr_ins_static_map.begin();
  for (; addr_ins_iter != addr_ins_static_map.end(); ++addr_ins_iter) 
  {
//     out_file << boost::format("%-15s %-35s (r: %-i, w: %-i) %-25s %-25s\n")
    out_file << boost::format("%-15s %-35s %-25s %-25s\n")
                  % remove_leading_zeros(StringFromAddrint(addr_ins_iter->first)) % addr_ins_iter->second.disass
//                 % addr_ins_iter->second.mem_read_size % addr_ins_iter->second.mem_written_size
                  % addr_ins_iter->second.contained_image % addr_ins_iter->second.contained_function;
  }
  out_file.close();

//   std::string filename_wf = filename + "_wf";
//   out_file.open(filename_wf.c_str(), std::ofstream::out | std::ofstream::trunc);
//
//   addr_ins_iter = addr_ins_static_map.begin();
//   for (; addr_ins_iter != addr_ins_static_map.end(); ++addr_ins_iter)
//   {
//     out_file << boost::format("%-s\t%-s\n") % StringFromAddrint(addr_ins_iter->first) % addr_ins_iter->second.disass;
//   }
//   out_file.close();

  return;
}

/*====================================================================================================================*/

void journal_explored_trace(const std::string& filename)
{
  std::ofstream out_file(filename.c_str(), 
                         std::ofstream::out | std::ofstream::trunc);

  std::vector<ADDRINT>::iterator trace_iter = explored_trace.begin();
  for (; trace_iter != explored_trace.end(); ++trace_iter) 
  {
    out_file << boost::format("%-20s %-45s")
                  % remove_leading_zeros(StringFromAddrint(*trace_iter)) 
                      % addr_ins_static_map[*trace_iter].disass;

    std::map<ADDRINT, UINT8>::iterator mem_access_iter;
    out_file << boost::format("(R: %i   W: %i)\n") 
                  % addr_ins_static_map[*trace_iter].mem_read_size 
                  % addr_ins_static_map[*trace_iter].mem_written_size;
  }
  out_file.close();

  return;
}

/*====================================================================================================================*/

void journal_tainting_graph(const std::string& filename)
{
  std::ofstream out_file(filename.c_str(), 
                         std::ofstream::out | std::ofstream::trunc );
  boost::write_graphviz(out_file, dta_graph, 
                        vertex_label_writer(dta_graph), 
                        edge_label_writer(dta_graph));
  out_file.close();

  return;
}

/*====================================================================================================================*/

void store_input ( ptr_branch& ptr_br, bool br_taken )
{
  boost::shared_ptr<UINT8> new_input ( new UINT8 [received_msg_size] );
  PIN_SafeCopy ( new_input.get(), reinterpret_cast<UINT8*> ( received_msg_addr ), received_msg_size );
  ptr_br->inputs[br_taken].push_back ( new_input );

  return;
}

/*====================================================================================================================*/

// void print_debug_message_received()
// {
//   if (print_debug_text) 
//   {
//     std::cout << "\033[33mThe first message saved at " << remove_leading_zeros(StringFromAddrint(received_msg_addr))
//               << " with size " << received_msg_size << ".\033[0m\n";
//     std::cout << "-------------------------------------------------------------------------------------------------\n";
//     std::cout << "\033[33mStart tainting phase with maximum trace size "
//               << max_trace_length.Value() << ".\033[0m\n";
//   }
//   return;
// }

/*====================================================================================================================*/

// void print_debug_start_rollbacking()
// {
//   if (print_debug_text) 
//   {
//     std::string tainted_trace_filename("tainted_trace");
//     if (exploring_ptr_branch) 
//     {
//       std::stringstream ss;
//       ss << exploring_ptr_branch->trace.size();
//       tainted_trace_filename = tainted_trace_filename + "_" + ss.str();
//     }
//     journal_explored_trace(tainted_trace_filename.c_str(), explored_trace);
//   }
// 
//   return;
// }

/*====================================================================================================================*/

void journal_tainting_log()
{
  static UINT32 exploring_times = 0;
  
  exploring_times++;
  std::stringstream tainting_log_filename;
  tainting_log_filename << "tainting_log_";
  tainting_log_filename << exploring_times;
  
  std::vector<ADDRINT>::iterator ins_addr_iter;

  std::set<REG>::iterator reg_iter;

  std::stringstream sstream_sregs;
  std::stringstream sstream_dregs;
  std::stringstream sstream_smems;
  std::stringstream sstream_dmems;
  std::stringstream sstream_bpaths;

  UINT32 idx;

  std::vector<ptr_branch>::iterator ptr_branch_iter;
  ptr_branch idx_ptr_branch;

  if (print_debug_text) 
  {
    std::ofstream tainting_log_file(tainting_log_filename.str().c_str(), std::ofstream::trunc);
    std::ofstream backward_log_file("backward_traces_log", std::ofstream::trunc);

    for (idx = (*saved_ptr_checkpoints.begin())->trace.size(); 
         idx <= explored_trace.size(); ++idx) 
    {
      sstream_sregs.str("");
      sstream_sregs.clear();
      
      sstream_dregs.str("");
      sstream_dregs.clear();
      
      sstream_smems.str("");
      sstream_smems.clear();
      
      sstream_dmems.str("");
      sstream_dmems.clear();
      
      sstream_bpaths.str("");
      sstream_bpaths.clear();

      sstream_sregs << "sregs: ";
      for (reg_iter = order_ins_dynamic_map[idx].src_regs.begin(); 
           reg_iter != order_ins_dynamic_map[idx].src_regs.end(); ++reg_iter ) 
      {
        sstream_sregs << REG_StringShort(*reg_iter) << " ";
      }

      sstream_dregs << "dregs: ";
      for (reg_iter = order_ins_dynamic_map[idx].dst_regs.begin();
           reg_iter != order_ins_dynamic_map[idx].dst_regs.end(); ++reg_iter) 
      {
        sstream_dregs << REG_StringShort(*reg_iter) << " ";
      }

      if (order_ins_dynamic_map[idx].category == XED_CATEGORY_COND_BR) 
      {
        sstream_dmems << "dmems: []";

        idx_ptr_branch = order_tainted_ptr_branch_map[idx];

        if (idx_ptr_branch->dep_input_addrs.empty()) 
        {
          if (idx_ptr_branch->dep_other_addrs.empty()) 
          {
            sstream_smems << "smems: []";
          } 
          else 
          {
            sstream_smems << "smems: ["
                          << remove_leading_zeros(StringFromAddrint(*(idx_ptr_branch->dep_other_addrs.begin()))) << ","
                          << remove_leading_zeros(StringFromAddrint(*(idx_ptr_branch->dep_other_addrs.rbegin()))) << "]";
          }

          // input independent branch
          tainting_log_file << boost::format("\033[33m%-4i %-16s %-34s %-16s %-19s %-40s %-40s %-2i %s %s\033[0m\n")
                            % idx % remove_leading_zeros(StringFromAddrint(order_ins_dynamic_map[idx].address))
                            % order_ins_dynamic_map[idx].disass
                            % sstream_sregs.str() % sstream_dregs.str() % sstream_smems.str() % sstream_dmems.str()
                            % idx_ptr_branch->br_taken
                            % order_ins_dynamic_map[idx].contained_image
                            % order_ins_dynamic_map[idx].contained_function;
        } 
        else 
        {
          sstream_smems << "smems: ["
                        << remove_leading_zeros(StringFromAddrint(*(idx_ptr_branch->dep_input_addrs.begin()))) << ","
                        << remove_leading_zeros(StringFromAddrint(*(idx_ptr_branch->dep_input_addrs.rbegin()))) << "]";

          sstream_bpaths << "\033[36mbpaths at " << idx << "\033[0m\n";
          std::map< ADDRINT, std::vector<UINT32> >::iterator addr_bpath_iter;
          std::vector<UINT32>::iterator ins_order_iter;

          ADDRINT root_addr;

          for (addr_bpath_iter = idx_ptr_branch->dep_backward_traces.begin();
                addr_bpath_iter != idx_ptr_branch->dep_backward_traces.end(); ++addr_bpath_iter) 
          {
            root_addr = addr_bpath_iter->first;

            if ((received_msg_addr <= root_addr) && (root_addr < received_msg_addr + received_msg_size)) 
            {
              sstream_bpaths << "\033[32m";
            }

            sstream_bpaths << remove_leading_zeros(StringFromAddrint(addr_bpath_iter->first)) << ": ";

            for (ins_order_iter = addr_bpath_iter->second.begin();
                  ins_order_iter != addr_bpath_iter->second.end(); ++ins_order_iter) 
            {
              sstream_bpaths << *ins_order_iter << " ";
            }
            sstream_bpaths << "\033[0m\n";
          }
          sstream_bpaths << "======================================================\n";

          // an input dependent branch
          tainting_log_file << boost::format("\033[32m%-4i %-16s %-34s %-16s %-19s %-40s %-40s %-2i %-4i %s %s\033[0m\n")
                            % idx % remove_leading_zeros(StringFromAddrint(order_ins_dynamic_map[idx].address))
                            % order_ins_dynamic_map[idx].disass
                            % sstream_sregs.str() % sstream_dregs.str() % sstream_smems.str() % sstream_dmems.str()
                            % idx_ptr_branch->br_taken % idx_ptr_branch->checkpoint->trace.size()
                            % order_ins_dynamic_map[idx].contained_image
                            % order_ins_dynamic_map[idx].contained_function;

          backward_log_file << sstream_bpaths.str();
        }
      } 
      else // not a conditional branch instruction
      { 
        if (order_ins_dynamic_map[idx].src_mems.empty()) 
        {
          sstream_smems << "smems: []";
        } 
        else 
        {
          sstream_smems << "smems: ["
                        << remove_leading_zeros(StringFromAddrint(*(order_ins_dynamic_map[idx].src_mems.begin()))) << ","
                        << remove_leading_zeros(StringFromAddrint(*(order_ins_dynamic_map[idx].src_mems.rbegin()))) << "]";
        }

        if (order_ins_dynamic_map[idx].dst_mems.empty()) {
          sstream_dmems << "dmems: []";
        } else {
          sstream_dmems << "dmems: ["
                        << remove_leading_zeros(StringFromAddrint(*(order_ins_dynamic_map[idx].dst_mems.begin()))) << ","
                        << remove_leading_zeros(StringFromAddrint(*(order_ins_dynamic_map[idx].dst_mems.rbegin()))) << "]";
        }

        if (!order_ins_dynamic_map[idx].src_mems.empty() &&
            (std::max(*order_ins_dynamic_map[idx].src_mems.begin(), received_msg_addr ) <=
             std::min(*order_ins_dynamic_map[idx].src_mems.rbegin(), received_msg_addr + received_msg_size - 1))) 
        {
          // a checkpoint
          tainting_log_file << boost::format ("\033[36m%-4i %-16s %-34s %-16s %-19s %-40s %-40s %s %s\033[0m\n")
                                % idx % remove_leading_zeros(StringFromAddrint(order_ins_dynamic_map[idx].address))
                                % order_ins_dynamic_map[idx].disass
                                % sstream_sregs.str() % sstream_dregs.str() % sstream_smems.str() % sstream_dmems.str()
                                % order_ins_dynamic_map[idx].contained_image
                                % order_ins_dynamic_map[idx].contained_function;
        } 
        else 
        {
          // a normal instruction
          tainting_log_file << boost::format("\033[0m%-4i %-16s %-34s %-16s %-19s %-40s %-40s %s %s\033[0m\n")
                                % idx % remove_leading_zeros(StringFromAddrint(order_ins_dynamic_map[idx].address))
                                % order_ins_dynamic_map[idx].disass
                                % sstream_sregs.str() % sstream_dregs.str() % sstream_smems.str() % sstream_dmems.str()
                                % order_ins_dynamic_map[idx].contained_image
                                % order_ins_dynamic_map[idx].contained_function;
        }
      }
    }

    tainting_log_file.close();
    backward_log_file.close();
  }

  return;
}

/*====================================================================================================================*/

void journal_branch_messages(ptr_branch& ptr_resolved_branch)
{
  std::stringstream msg_number_name;
  std::string msg_file_name;
  std::string br_taken_name;

  std::vector< boost::shared_ptr<UINT8> >::iterator msg_number_iter;
  
  std::map< bool, 
            std::vector< boost::shared_ptr<UINT8> >
          >::iterator msg_map_iter = ptr_resolved_branch->inputs.begin();
          
  for (; msg_map_iter != ptr_resolved_branch->inputs.end(); ++msg_map_iter) 
  {
    br_taken_name = msg_map_iter->first ? "taken" : "nottaken";
    msg_file_name = "msg_" + br_taken_name + "_";

    msg_number_iter = msg_map_iter->second.begin();
    for (; msg_number_iter != msg_map_iter->second.end(); ++msg_number_iter) 
    {
      msg_number_name << (msg_number_iter - msg_map_iter->second.begin());
      journal_buffer((msg_file_name + msg_number_name.str()).c_str(), 
                     (*msg_number_iter).get(), received_msg_size);
      msg_number_name.clear();
      msg_number_name.str("");
    }
  }

  return;
}
