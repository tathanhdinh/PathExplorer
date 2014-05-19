#include "../common.h"
#include <boost/graph/graphviz.hpp>
#include <cstdint>

auto addrint_to_hexstring (ADDRINT input) -> std::string
{
//  std::stringstream num_stream;
//  num_stream << "0x" << std::hex << input;
//  return num_stream.str();
  return static_cast<std::ostringstream&>(std::ostringstream()
                                          << "0x" << std::hex << input).str();

}


/**
 * @brief path_code_to_string
 */
auto path_code_to_string  (const path_code_t& path_code) -> std::string
{
  std::string code_str = "";
  /*path_code_t::const_iterator*/
//  for (auto code_iter = path_code.begin(); code_iter != path_code.end(); ++code_iter)
//  {
//    if (*code_iter) code_str.push_back('1');
//    else code_str.push_back('0');
//  }

  std::for_each(path_code.begin(), path_code.end(), [&](path_code_t::const_reference code_elem)
  {
//    if (code_elem) code_str.push_back('1');
//    else code_str.push_back('0');

    code_str.push_back(code_elem ? '1' : '0');
  });

  return code_str;
}


/**
 * @brief is_input_dep_cfi
 */
auto is_input_dep_cfi (ptr_instruction_t tested_ins) -> bool
{
  return (tested_ins->is_cond_direct_cf &&
          !std::static_pointer_cast<cond_direct_instruction>(tested_ins)->input_dep_addrs.empty());
};


/**
 * @brief is_resolved_cfi
 */
auto is_resolved_cfi (ptr_instruction_t tested_ins) -> bool
{
  return (tested_ins->is_cond_direct_cf &&
          !std::static_pointer_cast<cond_direct_instruction>(tested_ins)->input_dep_addrs.empty() &&
          std::static_pointer_cast<cond_direct_instruction>(tested_ins)->is_resolved);
};


/**
 * @brief look_for_saved_cfi_instance
 */
auto look_for_saved_instance (const ptr_cond_direct_ins_t cfi,
                              const path_code_t path_code) -> ptr_cond_direct_ins_t
{
//  tfm::format(std::cerr, "path code: %s\n", path_code_to_string(path_code));

  auto predicate = [&cfi,&path_code](ptr_cond_direct_ins_t examined_cfi) -> bool
  {
//    tfm::format(std::cerr, "examined path code: %s\n", path_code_to_string(examined_cfi->path_code));

    return ((cfi->address == examined_cfi->address) &&
            (cfi->exec_order == examined_cfi->exec_order) &&
            std::equal(examined_cfi->path_code.begin(),
                       examined_cfi->path_code.end(), path_code.begin()));
  };

  ptr_cond_direct_ins_t result_cfi;
  auto result_cfi_iter = std::find_if(detected_input_dep_cfis.begin(),
                                      detected_input_dep_cfis.end(), predicate);
  if (result_cfi_iter != detected_input_dep_cfis.end()) result_cfi = *result_cfi_iter;
  return result_cfi;
}


auto save_static_trace (const std::string& filename) -> void
{
  std::ofstream out_file(filename.c_str(), std::ofstream::out | std::ofstream::trunc);

  auto ins_iter = ins_at_addr.begin();
  for (; ins_iter != ins_at_addr.end(); ++ins_iter)
  {
    tfm::format(out_file, "%-15s %-50s %-25s %-25s\n", addrint_to_hexstring(ins_iter->first),
                ins_iter->second->disassembled_name, ins_iter->second->contained_image,
                ins_iter->second->contained_function);
  }
  out_file.close();

  return;
}


auto save_explored_trace (const std::string& filename) -> void
{
  std::ofstream out_file(filename.c_str(), std::ofstream::out | std::ofstream::trunc);
  out_file.close();

  return;
}


/**
 * @brief label write for the tainting graph
 */
class vertex_label_writer
{
public:
  vertex_label_writer(df_diagram& diagram) : tainting_graph(diagram) {}

//  template <typename Vertex>
  void operator()(std::ostream& vertex_label, /*Vertex*/df_vertex_desc vertex)
  {
    auto current_vertex = tainting_graph[vertex];
    if ((current_vertex->value.type() == typeid(ADDRINT)) &&
        (received_msg_addr <= boost::get<ADDRINT>(current_vertex->value)) &&
        (boost::get<ADDRINT>(current_vertex->value) < received_msg_addr + received_msg_size))
    {
      tfm::format(vertex_label, "[color=blue,style=filled,label=\"%s\"]", current_vertex->name);
    }
    else
    {
      tfm::format(vertex_label, "[color=black,label=\"%s\"]", current_vertex->name);
    }
  }

private:
  df_diagram tainting_graph;
};


/**
 * @brief edge write for the tainting graph
 */
class edge_label_writer
{
public:
  edge_label_writer(df_diagram& diagram) : tainting_graph(diagram) {}

//  template <typename Edge>
  void operator()(std::ostream& edge_label, /*Edge*/df_edge_desc edge)
  {
    auto current_edge = tainting_graph[edge];
//    tfm::format(edge_label, "[label=\"%s: %s\"]", current_edge,
//                ins_at_order[current_edge]->disassembled_name);
    tfm::format(edge_label, "[label=\"%s\"]", current_edge);
  }

private:
  df_diagram tainting_graph;
};


/**
 * @brief save the tainting graph
 */
auto save_tainting_graph (df_diagram& dta_graph, const std::string& filename) -> void
{
  std::ofstream out_file(filename.c_str(), std::ofstream::out | std::ofstream::trunc);
  boost::write_graphviz(out_file, dta_graph, vertex_label_writer(dta_graph),
                        edge_label_writer(dta_graph));
  out_file.close();

  return;
}


/**
 * @brief inspired from http://goo.gl/mz9fex
 */
auto show_exploring_progress () -> void
{
  UINT32 resolved_cfi_num = 0, singular_cfi_num = 0, explored_cfi_num = 0;
  std::for_each(detected_input_dep_cfis.begin(), detected_input_dep_cfis.end(),
                [&](ptr_cond_direct_ins_t cfi)
  {
    if (cfi->is_resolved) resolved_cfi_num++;
    if (cfi->is_singular) singular_cfi_num++;
    if (cfi->is_explored) explored_cfi_num++;
  });

  static uint32_t total_progress = 80;
  decltype(total_progress) current_progress =
      (total_progress * total_rollback_times) / max_total_rollback_times;

  tfm::printf("[");
  for (auto idx = 0; idx < total_progress; ++idx)
  {
    if (idx < current_progress) tfm::printf("=");
    else if (idx == current_progress) tfm::printf(">");
    else tfm::printf(" ");
  }
  tfm::format(std::cout, "] %6.2f%% (%d/%d/%d/%d) resolved/explored/singular/total CFI\n",
              100.0 * static_cast<double>(total_rollback_times) / static_cast<double>(max_total_rollback_times),
              resolved_cfi_num, explored_cfi_num, singular_cfi_num, detected_input_dep_cfis.size());
  std::cout.flush();

  return;
}


auto show_cfi_logged_inputs () -> void
{
//  typedef decltype(detected_input_dep_cfis) cfis_t;
  std::for_each(detected_input_dep_cfis.begin(), detected_input_dep_cfis.end(),
                [&](decltype(detected_input_dep_cfis)::const_reference cfi_elem)
  {
    if (cfi_elem->is_resolved)
      tfm::format(std::cerr, "logged inputs of CFI %s at execution order %d: first %d, second %d\n",
                  addrint_to_hexstring(cfi_elem->address), cfi_elem->exec_order,
                  cfi_elem->first_input_projections.size(),
                  cfi_elem->second_input_projections.size());
  });
  return;
}


auto save_cfi_inputs (const std::string& filename) -> void
{
  // lambda function to save projected inputs of a cfi
  auto save_inputs_of_cfi =
      [&](const ptr_cond_direct_ins_t cfi, const std::string& filename) -> void
  {
    auto save_maps = [&](addrint_value_maps_t input_maps, std::ofstream& output_file) -> void
    {
      std::for_each(input_maps.begin(), input_maps.end(),
                    [&](addrint_value_maps_t::const_reference addr_value_map)
      {
        std::for_each(addr_value_map.begin(), addr_value_map.end(),
                      [&](addrint_value_map_t::const_reference addr_value)
        {
          tfm::format(output_file, "%10s:%3d ", addrint_to_hexstring(addr_value.first),
                      addr_value.second);
        });
        tfm::format(output_file, "\n");
      });
    };

    auto generic_filename = path_code_to_string(cfi->path_code) + "_" +
        addrint_to_hexstring(cfi->address)  + "_" + filename;
//    tfm::format(std::cerr, "%s\n", generic_filename);

    std::ofstream first_output_file(("0_" + generic_filename).c_str(),
                                    std::ofstream::out | std::ofstream::trunc);
    save_maps(cfi->first_input_projections, first_output_file);
    first_output_file.close();

    std::ofstream second_output_file(("1_" + generic_filename).c_str(),
                                     std::ofstream::out | std::ofstream::trunc);
    save_maps(cfi->second_input_projections, second_output_file);
    second_output_file.close();
    return;
  };

//  typedef decltype(detected_input_dep_cfis) cfis_t;
  std::for_each(detected_input_dep_cfis.begin(), detected_input_dep_cfis.end(),
                [&](decltype(detected_input_dep_cfis)::const_reference cfi)
  {
    if (cfi->is_resolved/* || cfi->is_bypassed*/)
    {
      save_inputs_of_cfi(cfi, filename);
    }
  });

  return;
}
