#include "instruction.h"
#include "../util/stuffs.h"

/*================================================================================================*/

instruction::instruction(const INS& ins)
{
  // collect some informations
  this->address               = INS_Address(ins);
  this->disassembled_name     = INS_Disassemble(ins);
  
  IMG ins_img = IMG_FindByAddress(this->address);
  if (IMG_Valid(ins_img))
    this->contained_image     = IMG_Name(ins_img);
  else
    this->contained_image     = "";
  this->contained_function    = RTN_FindNameByAddress(this->address);
  
  this->is_syscall            = INS_IsSyscall(ins);
  // some workarround to detect if the instruction is mapped from the kernel
#if defined(_WIN32) || defined(_WIN64)
  this->is_mapped_from_kernel = (this->contained_image.find("ntdll.dll") != std::string::npos);
#elif defined(__gnu_linux__)
  this->is_mapped_from_kernel = this->contained_image.empty();
#endif
  this->is_mem_read           = INS_IsMemoryRead(ins);
  this->is_mem_write          = INS_IsMemoryWrite(ins);

  OPCODE ins_opcode = INS_Opcode(ins);
  this->is_cond_direct_cf     = ((INS_Category(ins) == XED_CATEGORY_COND_BR) ||
                                 (INS_HasRealRep(ins) &&
                                  ((ins_opcode == XED_ICLASS_CMPSB) ||
                                   (ins_opcode == XED_ICLASS_CMPSD) ||
                                   (ins_opcode == XED_ICLASS_CMPSW) ||
                                   (ins_opcode == XED_ICLASS_CMPSQ) ||
                                   (ins_opcode == XED_ICLASS_SCASB) ||
                                   (ins_opcode == XED_ICLASS_SCASW) ||
                                   (ins_opcode == XED_ICLASS_SCASD) ||
                                   (ins_opcode == XED_ICLASS_SCASW))));
  this->is_uncond_indirect_cf = INS_IsIndirectBranchOrCall(ins);
  this->has_mem_read2         = INS_HasMemoryRead2(ins);
  this->has_real_rep          = INS_HasRealRep(ins);

  // collect read/write registers
//  UINT32        register_id, register_num;
  REG           curr_register;
  ptr_operand_t new_operand;
  
  auto register_num = INS_MaxNumRRegs(ins); auto register_id = 0;
  for (/*auto register_id = 0*/; register_id < register_num; ++register_id)
  {
    curr_register = INS_RegR(ins, register_id);
    // the source register is not considered when it is the instruction pointer, namely the control 
    // tainting will not be considered
    if (curr_register != REG_INST_PTR) 
    {
      if (INS_IsRet(ins) && (curr_register == REG_STACK_PTR)) 
      {
        // do nothing: when the instruction is ret, the esp (and rsp) register will be used 
        // implicitly to point out the address of popped value; to eliminate the 
        // excessive dependence, this register is not considered.
      }
      else 
      {
        new_operand.reset(new operand(curr_register));
        this->src_operands.insert(new_operand);
      }
    }
  }
  
  register_num = INS_MaxNumWRegs(ins); register_id = 0;
  for (/*auto register_id = 0*/; register_id < register_num; ++register_id)
  {
    curr_register = INS_RegW(ins, register_id);
    if ((curr_register != REG_INST_PTR) ||
        INS_IsBranchOrCall(ins) || INS_IsRet(ins) || INS_HasRealRep(ins))
    {
      if ((curr_register == REG_STACK_PTR) && INS_IsRet(ins))
      {
        // do nothing: see above
      }
      else
      {
        new_operand.reset(new operand(curr_register));
        this->dst_operands.insert(new_operand);
      }
    }
    else
    {
      // do nothing if the register is the instruction pointer and the instruction is not a kind
      // that can change the ip then the register is not considered
    }
  }
}

