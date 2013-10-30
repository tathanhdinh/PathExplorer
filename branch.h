#ifndef BRANCH_TAINTING_H
#define BRANCH_TAINTING_H

#include <pin.H>

#include <vector>
#include <set>

#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>

#include "checkpoint.h"

/*====================================================================================================================*/

class branch;

typedef boost::shared_ptr<branch>      ptr_branch;

typedef std::pair< std::set<REG>, 
                   std::set<REG> >     reg_io;
                   
typedef std::pair< std::set<UINT32>, 
                   std::set<UINT32> >  imm_io;
                   
typedef std::pair< std::set<ADDRINT>, 
                   std::set<ADDRINT> > mem_io;
                                      
typedef boost::tuple< reg_io, 
                      imm_io, 
                      mem_io,
                      bool   >         ins_io;

typedef std::map<ADDRINT, ins_io>      map_ins_io;

/*====================================================================================================================*/

class branch
{
public:
  ADDRINT               addr;
  std::vector<ADDRINT>  trace;
  bool                  br_taken;
  
  std::set<ADDRINT>     dep_input_addrs;
  std::set<ADDRINT>     dep_other_addrs;
  
  std::map< ADDRINT, 
            std::vector<UINT32> 
            >           dep_backward_traces;
  
  std::map< bool, 
            std::vector< boost::shared_ptr<UINT8> > 
          >             inputs;
    
  ptr_checkpoint        chkpnt;
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

#endif // BRANCH_TAINTING_H
