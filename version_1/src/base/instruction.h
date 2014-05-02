#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include "../parsing_helper.h"
#include "operand.h"

#include <pin.H>
extern "C"
{
#include <xed-interface.h>
}

#include <map>
#include <set>
#include <vector>

class instruction
{
public:
  ADDRINT       address;
  std::string   disassembled_name;
  std::string   contained_image;
  std::string   contained_function;
    
  bool is_syscall;
  bool is_mem_read;
  bool is_mem_write;
  bool is_mapped_from_kernel;
  bool is_cond_direct_cf;     // conditional control-flow instructions are always direct
  bool is_uncond_indirect_cf; // unconditional control-flow instructions are always indirect
  bool has_mem_read2;
  bool has_real_rep;

#if defined(_WIN32) || defined(_WIN64)
  bool is_in_msg_receiving;
#endif

  ptr_operand_set_t src_operands;
  ptr_operand_set_t dst_operands;

public:
  instruction();
  instruction(const INS& ins);
};

typedef std::shared_ptr<instruction>          ptr_instruction_t;
typedef std::map<ADDRINT, ptr_instruction_t>  addr_ins_map_t;
typedef std::map<UINT32, ptr_instruction_t>   order_ins_map_t;

#endif // INSTRUCTION_H
