#ifndef BRANCH_TAINTING_H
#define BRANCH_TAINTING_H

#include <pin.H>

#include <vector>
#include <set>

#include <boost/shared_ptr.hpp>

#include "checkpoint.h"

/*================================================================================================*/

class branch
{
public:
  ADDRINT               addr;
//  std::vector<ADDRINT>  trace;
  UINT32                execution_order;
  bool                  br_taken;
  
  std::set<ADDRINT>     dep_input_addrs;
  std::set<ADDRINT>     dep_other_addrs;
  
  std::map< ADDRINT, 
            std::vector<UINT32> 
          >             dep_backward_traces;
  
  std::map< bool, 
            std::vector< boost::shared_ptr<UINT8> > 
          >             inputs;
    
  ptr_checkpoint_t        checkpoint;
  std::map< ptr_checkpoint_t,
            std::set<ADDRINT>, 
            ptr_checkpoint_less
          >             nearest_checkpoints;
          
  std::map< ptr_checkpoint_t,
            UINT32, 
            ptr_checkpoint_less 
          >             econ_execution_length;
  
  bool                  is_resolved;
  bool                  is_just_resolved;
  bool                  is_bypassed;
  
  bool                  is_explored;
  
public:
  branch(ADDRINT ins_addr, bool br_taken);
  branch(const branch& other);
  branch& operator=(const branch& other);
  bool operator==(const branch& other);
};

typedef boost::shared_ptr<branch> ptr_branch_t;

#endif // BRANCH_TAINTING_H
