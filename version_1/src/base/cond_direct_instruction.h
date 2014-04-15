#ifndef COND_DIRECT_INSTRUCTION_H
#define COND_DIRECT_INSTRUCTION_H

#include "instruction.h"
#include "checkpoint.h"

#include <vector>

//#if __cplusplus < 199711L
//#include <boost/shared_ptr.hpp>
//#define pept boost
//#else
//#include <memory>
//#define pept std
//#endif

typedef std::shared_ptr<UINT8>                      ptr_uint8_t;
typedef std::set<ADDRINT>                           addrint_set_t;
typedef std::map<ADDRINT, UINT8>                    addrint_value_map_t;
typedef std::vector<addrint_value_map_t>            addrint_value_maps_t;
typedef std::pair<ptr_checkpoint_t, addrint_set_t>  checkpoint_with_modified_addrs;

#if !defined(DISABLE_FSA)
typedef std::vector<bool>                           path_code_t;
#endif

class cond_direct_instruction : public instruction
{
public:
  bool is_resolved;
  bool is_bypassed;
  bool is_explored;
  bool is_singular;

  UINT32 used_rollback_num;
  UINT32 exec_order;

#if !defined(DISABLE_FSA)
  path_code_t path_code;
#endif

  addrint_set_t                                 input_dep_addrs;
  addrint_value_maps_t                          first_input_projections;
  addrint_value_maps_t                          second_input_projections;
  std::vector<checkpoint_with_modified_addrs>   checkpoints;

  ptr_uint8_t                                   fresh_input;

public:
  cond_direct_instruction(const INS& ins);
  cond_direct_instruction(instruction& ins);
};

typedef std::shared_ptr<cond_direct_instruction> ptr_cond_direct_ins_t;
typedef std::vector<ptr_cond_direct_ins_t> ptr_cond_direct_inss_t;

#endif // COND_DIRECT_INSTRUCTION_H
