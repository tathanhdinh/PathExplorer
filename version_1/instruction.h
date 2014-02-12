#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <string>
#include <map>
#include <set>
#include <vector>

#include <pin.H>
extern "C"
{
#include <xed-interface.h>
}

#include "operand.h"

typedef enum 
{
  syscall_inexist  = 0,
  syscall_sendto   = 44,
  syscall_recvfrom = 45
} syscall_id;

class instruction
{
public:
  ADDRINT       address;
  std::string   disassembled_name;
  std::string   contained_image;
  std::string   contained_function;
    
  bool  is_syscall;
  bool  is_mem_read;
  bool  is_mem_write;
  bool  is_mapped_from_kernel;
  bool  is_cond_direct_cf;     // conditional control-flow instructions are always direct
  bool  is_uncond_indirect_cf; // unconditional control-flow instructions are always indirect
  bool  has_mem_read2;
  bool  has_real_rep;

  std::set<ptr_operand_t> src_operands;
  std::set<ptr_operand_t> dst_operands;
  
public:
  instruction();
  instruction(const INS& ins);
//  instruction(instruction const& other_ins);
//  instruction& operator=(instruction const& other_ins);
};

typedef boost::shared_ptr<instruction> ptr_instruction_t;

#endif // INSTRUCTION_H
