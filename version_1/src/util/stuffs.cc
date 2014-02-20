#include "../operation/common.h"


std::string addrint_to_hexstring(ADDRINT input)
{
  std::stringstream num_stream;
  num_stream << "0x" << std::hex << input;
  return num_stream.str();
}


void save_static_trace(const std::string& filename)
{
  std::ofstream out_file(filename.c_str(), std::ofstream::out | std::ofstream::trunc);

  std::map<ADDRINT, ptr_instruction_t>::iterator ins_iter = ins_at_addr.begin();
  for (; ins_iter != ins_at_addr.end(); ++ins_iter)
  {
    out_file << boost::format("%-15s %-50s %-25s %-25s\n")
                % addrint_to_hexstring(ins_iter->first) % ins_iter->second->disassembled_name
                % ins_iter->second->contained_image % ins_iter->second->contained_function;
  }
  out_file.close();

  return;
}


void save_explored_trace(const std::string& filename)
{
  std::ofstream out_file(filename.c_str(), 
                         std::ofstream::out | std::ofstream::trunc);
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
      vertex_label << boost::format("[color=blue,style=filled,label=\"%s\"]")
                      % current_vertex->name;
    }
    else
    {
      vertex_label << boost::format("[color=black,label=\"%s\"]") % current_vertex->name;
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
    edge_label << boost::format("[label=\"%s: %s\"]")
                  % current_edge % ins_at_order[current_edge]->disassembled_name;
  }

private:
  df_diagram tainting_graph;
};


/**
 * @brief save the tainting graph
 */
void save_tainting_graph(df_diagram& dta_graph, const std::string& filename)
{
  std::ofstream out_file(filename.c_str(), std::ofstream::out | std::ofstream::trunc );
  boost::write_graphviz(out_file, dta_graph, 
                        vertex_label_writer(dta_graph), edge_label_writer(dta_graph));
  out_file.close();

  return;
}
