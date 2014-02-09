#include "instruction.h"

#include <pin.H>
extern "C" {
#include <xed-interface.h>
}

#include "stuffs.h"
//#include "path_explorer.h"

/*====================================================================================================================*/

// inline static std::string contained_image_name(ADDRINT ins_addr)
// {
//   IMG ins_img = IMG_FindByAddress(ins_addr);
//   std::string img_name = "";
//   
//   if (IMG_Valid(ins_img)) 
//   {
//     img_name = IMG_Name(ins_img);
//   }
// 
//   return img_name;
// }

/*================================================================================================*/

// instruction::instruction()
// {
// }

/*================================================================================================*/

instruction::instruction(const INS& ins)
{
  this->address               = INS_Address(ins);
  this->disassembled_name     = INS_Disassemble(ins);
  
  IMG ins_img = IMG_FindByAddress(this->address);
  if (IMG_Valid(ins_img)) 
  {
    this->contained_image     = IMG_Name(ins_img);
  }
  else 
  {
    this->contained_image     = "";
  }
  this->contained_function    = RTN_FindNameByAddress(this->address);
  
  this->is_syscall            = INS_IsSyscall(ins);
  this->is_mapped_from_kernel = !(this->contained_image.empty());
  this->is_mem_read           = INS_IsMemoryRead(ins);
  this->is_mem_write          = INS_IsMemoryWrite(ins);
  this->is_cbranch            = (INS_Category(ins) == XED_CATEGORY_COND_BR);
  
  

  UINT32        register_id, register_num;
  REG           curr_register;
  ptr_operand_t new_operand;
  
  register_num = INS_MaxNumRRegs(ins);
  for (register_id = 0; register_id < register_num; ++register_id) 
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
  
  register_num = INS_MaxNumWRegs(ins);
  for (register_id = 0; register_id < register_num; ++register_id) 
  {
    curr_register = INS_RegW(ins, register_id);
    // see the same explication above
    if (curr_register != REG_INST_PTR) 
    {
      if (INS_IsRet(ins) && (curr_register == REG_STACK_PTR)) 
      {
        // do nothing
      }
      else 
      {
        new_operand.reset(new operand(curr_register));
        this->trg_operands.insert(new_operand);
      }
    }
  }

  this->has_mem_read2 = INS_HasMemoryRead2(ins);
}

/*================================================================================================*/

instruction::instruction(const instruction& other)
{
  this->address               = other.address;
  this->disassembled_name     = other.disassembled_name;

  this->contained_image       = other.contained_image;
  this->contained_function    = other.contained_function;
  
  this->is_syscall            = other.is_syscall;
  this->is_mem_read           = other.is_mem_read;
  this->is_mem_write          = other.is_mem_write;
  this->is_mapped_from_kernel = other.is_mapped_from_kernel;
  this->is_cbranch            = other.is_cbranch;
  this->has_mem_read2         = other.has_mem_read2;
  
  this->src_operands          = other.src_operands;
  this->trg_operands          = other.trg_operands;
}

/*================================================================================================*/
// there is not dynamically initialized variables, so copy constructor and assignment are the same
instruction& instruction::operator=(const instruction& other)
{
  this->address               = other.address;
  this->disassembled_name     = other.disassembled_name;

  this->contained_image       = other.contained_image;
  this->contained_function    = other.contained_function;
  
  this->is_syscall            = other.is_syscall;
  this->is_mem_read           = other.is_mem_read;
  this->is_mem_write          = other.is_mem_write;
  this->is_mapped_from_kernel = other.is_mapped_from_kernel;
  this->is_cbranch            = other.is_cbranch;
  this->has_mem_read2         = other.has_mem_read2;
  
  this->src_operands          = other.src_operands;
  this->trg_operands          = other.trg_operands;

  return *this;
}
