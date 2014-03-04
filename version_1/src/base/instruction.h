#ifndef INSTRUCTION_H
#define INSTRUCTION_H

// these definitions are not necessary (defined already in the CMakeLists),
// they are added just to make qt-creator parsing headers
#if defined(_WIN32) || defined(_WIN64)
#ifndef TARGET_IA32
#define TARGET_IA32
#endif
#ifndef HOST_IA32
#define HOST_IA32
#endif
#ifndef TARGET_WINDOWS
#define TARGET_WINDOWS
#endif
#ifndef USING_XED
#define USING_XED
#endif
#endif

#include <pin.H>
extern "C"
{
#include <xed-interface.h>
}

#include <string>
#include <map>
#include <set>
#include <vector>

#include "operand.h"

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
};

typedef pept::shared_ptr<instruction>         ptr_instruction_t;
typedef std::map<ADDRINT, ptr_instruction_t>  addr_ins_map_t;
typedef std::map<UINT32, ptr_instruction_t>   order_ins_map_t;

#endif // INSTRUCTION_H
