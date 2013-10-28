#include "instruction.h"

#include <pin.H>

#include "stuffs.h"

/*====================================================================================================================*/

instruction::instruction()
{
}

/*====================================================================================================================*/

instruction::instruction(const INS& ins)
{
  this->address           = INS_Address(ins);
  this->disass            = INS_Disassemble(ins);
  this->category          = static_cast<xed_category_enum_t>(INS_Category(ins));
  this->mem_read_size     = INS_MemoryReadSize(ins);
  this->mem_written_size  = INS_MemoryWriteSize(ins);
  
  UINT32 reg_id;
  REG    reg;
  
  UINT32 max_num_rregs = INS_MaxNumRRegs(ins);
  for (reg_id = 0; reg_id < max_num_rregs; ++reg_id) 
  {
    reg = INS_RegR(ins, reg_id);
    if (reg != REG_INST_PTR)
    {
      this->src_regs.insert(reg);
    }
  }
  
  UINT32 max_num_wregs = INS_MaxNumWRegs(ins);
  for (reg_id = 0; reg_id < max_num_wregs; ++reg_id) 
  {
    reg = INS_RegW(ins, reg_id);
    if ((reg != REG_INST_PTR) || INS_IsBranchOrCall(ins) || INS_IsRet(ins))
    {
      this->dst_regs.insert(reg);
    }
  }
}

/*====================================================================================================================*/

instruction::instruction(const instruction& other)
{
  this->address           = other.address;
  this->disass            = other.disass;
  this->category          = other.category;
  
  this->mem_read_size     = other.mem_read_size;
  this->mem_written_size  = other.mem_written_size;  
  
  this->src_regs          = other.src_regs;
  this->dst_regs          = other.dst_regs;
  this->src_mems          = other.src_mems;
  this->dst_mems          = other.dst_mems;
}

/*====================================================================================================================*/

instruction& instruction::operator=(const instruction& other)
{
  this->address           = other.address;
  this->disass            = other.disass;
  this->category          = other.category;
  
  this->mem_read_size     = other.mem_read_size;
  this->mem_written_size  = other.mem_written_size;
  
  this->src_regs          = other.src_regs;
  this->dst_regs          = other.dst_regs;
  this->src_mems          = other.src_mems;
  this->dst_mems          = other.dst_mems;
  
  return *this;
}
