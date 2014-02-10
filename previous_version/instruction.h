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

/* ---------------------------------------------------------------------------------------------- */
/*                                            class declaration                                   */
/* ---------------------------------------------------------------------------------------------- */
class instruction
{
public:
  ADDRINT             address;
  std::string         disassembled_name;
  
  std::string         contained_image;
  std::string         contained_function;
    
  bool                is_syscall;
  bool                is_mem_read;
  bool                is_mem_write;
  bool                is_mapped_from_kernel;
  bool                is_cbranch;
  bool                is_indirect_cf;
  bool                has_mem_read2;

  std::set<ptr_operand_t> src_operands;
  std::set<ptr_operand_t> dst_operands;
  
public:
  instruction();
  instruction(INS const& ins);
  instruction(instruction const& other_ins);
  instruction& operator=(instruction const& other_ins);
};

typedef boost::shared_ptr<instruction> ptr_instruction_t;

#endif // INSTRUCTION_H
