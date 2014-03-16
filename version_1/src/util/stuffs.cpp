#include "../operation/common.h"
#include <boost/graph/graphviz.hpp>

auto addrint_to_hexstring (ADDRINT input) -> std::string
{
//  std::stringstream num_stream;
//  num_stream << "0x" << std::hex << input;
//  return num_stream.str();
  return static_cast<std::ostringstream*>(&(std::ostringstream()
                                            << "0x" << std::hex << input))->str();
}


auto save_static_trace (const std::string& filename) -> void
{
  std::ofstream out_file(filename.c_str(), std::ofstream::out | std::ofstream::trunc);

  /*std::map<ADDRINT, ptr_instruction_t>::iterator*/auto ins_iter = ins_at_addr.begin();
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
  vertex_label_writer(df_diagram& diagram) : tainting_graph(diagram)
  {
  }

  template <typename Vertex>
  void operator()(std::ostream& vertex_label, Vertex vertex)
  {
    df_vertex current_vertex = tainting_graph[vertex];
    if ((current_vertex->value.type() == typeid(ADDRINT)) &&
        (received_msg_addr <= boost::get<ADDRINT>(current_vertex->value)) &&
        (boost::get<ADDRINT>(current_vertex->value) < received_msg_addr + received_msg_size))
    {
      tfm::format(vertex_label, "[color=blue,style=filled,label=\"%s\"]", current_vertex->name);
//      vertex_label << boost::format("[color=blue,style=filled,label=\"%s\"]")
//                      % current_vertex->name;
    }
    else
    {
      tfm::format(vertex_label, "[color=black,label=\"%s\"]", current_vertex->name);
//      vertex_label << boost::format("[color=black,label=\"%s\"]") % current_vertex->name;
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
  edge_label_writer(df_diagram& diagram) : tainting_graph(diagram)
  {
  }

  template <typename Edge>
  void operator()(std::ostream& edge_label, Edge edge)
  {
    df_edge current_edge = tainting_graph[edge];
    tfm::format(edge_label, "[label=\"%s: %s\"]", current_edge,
                ins_at_order[current_edge]->disassembled_name);
//    edge_label << boost::format("[label=\"%s: %s\"]")
//                  % current_edge % ins_at_order[current_edge]->disassembled_name;
  }

private:
  df_diagram tainting_graph;
};


/**
 * @brief save the tainting graph
 */
auto save_tainting_graph  (df_diagram& dta_graph, const std::string& filename) -> void
{
  std::ofstream out_file(filename.c_str(), std::ofstream::out | std::ofstream::trunc);
  boost::write_graphviz(out_file, dta_graph, vertex_label_writer(dta_graph),
                        edge_label_writer(dta_graph));
  out_file.close();

  return;
}


#if !defined(DISABLE_FSA)
auto path_code_to_string  (const path_code_t& path_code) -> std::string
{
  std::string code_str = "";
  /*path_code_t::const_iterator*/
  for (auto code_iter = path_code.begin(); code_iter != path_code.end(); ++code_iter)
  {
    if (*code_iter) code_str.push_back('1');
    else code_str.push_back('0');
  }
  return code_str;
}
#endif
