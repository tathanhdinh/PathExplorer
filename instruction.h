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


/* ------------------------------------------------------------------------------------------------------------------ */
/*                                                           data types                                               */
/* ------------------------------------------------------------------------------------------------------------------ */
class instruction;
// class checkpoint;

typedef enum 
{
  syscall_inexist  = 0,
  syscall_sendto   = 44,
  syscall_recvfrom = 45
} syscall_id;

/* ------------------------------------------------------------------------------------------------------------------ */
/*                                                  class declaration                                                 */
/* ------------------------------------------------------------------------------------------------------------------ */
class instruction
{
public:                                                                         
  ADDRINT             address;                                                       
  std::string         disass;
  std::string         contained_image;
  std::string         contained_function;

  OPCODE              opcode;
  xed_category_enum_t category;
  
  UINT32              mem_read_size;
  UINT32              mem_written_size;
  
  std::set<REG>       src_regs, dst_regs;
  std::set<ADDRINT>   src_mems, dst_mems;
  
public:
  instruction();
  instruction(INS const& ins);
  instruction(instruction const& other_ins);
  instruction& operator=(instruction const& other_ins);
};

#endif // INSTRUCTION_H
