#ifndef CHECKPOINT_H
#define CHECKPOINT_H

#include <vector>
#include <map>
#include <set>

#include <boost/cstdint.hpp>
#include <boost/unordered_map.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/graph/graph_concepts.hpp>
#include <pin.H>

#include "instruction.h"

/*====================================================================================================================*/

class checkpoint;

typedef boost::shared_ptr<CONTEXT>            ptr_context;
typedef boost::shared_ptr<checkpoint>         ptr_checkpoint;

// void modify_input                             (std::set<ADDRINT>& dep_mems);

void replace_input                            (UINT8* backup_input_addr);

void rollback_with_input_replacement          (ptr_checkpoint& ptr_chkpnt, UINT8* backup_input_addr);

void rollback_with_input_random_modification  (ptr_checkpoint& ptr_chkpnt, std::set<ADDRINT>& dep_mems);

/*====================================================================================================================*/
                
class checkpoint
{
public:
  ADDRINT                   addr;
  ptr_context               ptr_ctxt;
  
  std::map<ADDRINT, UINT8>  mem_read_log;     // map between a read address and the original value at this address
  std::map<ADDRINT, UINT8>  mem_written_log;  // map between a written address and the original value at this address
  
  std::set<ADDRINT>         dep_mems;
  
  std::vector<ADDRINT>      trace;
  
  UINT32                    rollback_times;
    
public:
  checkpoint();
  checkpoint(ADDRINT ip_addr, CONTEXT* new_ptr_ctxt, const std::vector<ADDRINT>& current_trace, 
             ADDRINT msg_read_addr, UINT32 msg_read_size); 
  
  checkpoint& operator=(checkpoint const& other_chkpnt);
  
  void mem_written_logging(ADDRINT ins_addr, ADDRINT mem_addr, UINT32 mem_length);
//   void mem_read_logging(ADDRINT ins_addr, ADDRINT mem_addr, UINT32 mem_length);
  
};

class ptr_checkpoint_less 
{
public:
  bool operator()(ptr_checkpoint const& a, ptr_checkpoint const& b) 
  {
    return (a->trace.size() < b->trace.size());
  }
};

#endif // CHECKPOINT_H

