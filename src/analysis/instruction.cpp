/*
 * Copyright (C) 2013  Ta Thanh Dinh <thanhdinh.ta@inria.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "instruction.h"

namespace analysis
{
  
/**
 * @brief constructor for a construction object, many static information about instruction will be 
 * pre-determined in this function.
 * 
 * @param current_instruction instruction object passed from PIN
 */
instruction::instruction(const INS& current_instruction)
{
  this->address           = INS_Address(current_instruction);
  this->dissasembled_name = INS_Disassemble(current_instruction);
  
  // determine if the instruction is a system call
  this->is_syscall = INS_IsSyscall(current_instruction);
  
  // get the name of the library and the name of function consisting the instruction
  this->contained_library = "";
  IMG ins_img = IMG_FindByAddress(this->address);
  if (IMG_Valid(ins_img)) this->contained_library = IMG_Name(ins_img);
  this->contained_function = RTN_FindNameByAddress(this->address);
  
  // determine if the instruction is mapped from the kernel space (thanks to Igor Skochinsky on the 
  // reverseengineering.stackexchange.com who has explained it and my colleague Fabrice Sabatier 
  // who has shown me how to handle it)
  this->is_vdso = false;
  if (this->contained_function.empty()) this->is_vdso = true;
  
  // determine if the instruction read/write from/into memory
  this->is_memory_read = INS_IsMemoryRead(current_instruction);
  this->is_memory_write = INS_IsMemoryWrite(current_instruction);
  
  // determine if the instruction is a conditional branch
  this->is_conditional_branch = (INS_Category(current_instruction) == XED_CATEGORY_COND_BR);
  
  // determine if the instruction is an indirect branch or call, note that an instruction in 
  // x86(-64) architecture cannot be a conditional and an indirect one at once, namely 
  // "is conditional branch" and "is indirect branch or call" are mutually exclusive
  this->is_indirect_branch_or_call = INS_IsIndirectBranchOrCall(current_instruction);
  
  // the source and target registers of an instruction can be determined statically
  REG current_register;
  uint8_t register_id;
  uint8_t register_number; 
  
  // source operands as read registers
  register_number = INS_MaxNumRRegs(current_instruction);
  for (register_id = 0; register_id < register_number; ++register_id) 
  {
    current_register = INS_RegR(current_instruction, register_id);
    
    // the source pointer is not considered when it is the instruction pointer, 
    // namely the control tainting is not counted.
    if (current_register != REG_INST_PTR) 
    {
      if (INS_IsRet(current_instruction) && (current_register == REG_STACK_PTR))
      {
        // do nothing: when the instruction is ret, the esp (and rsp) register will be used 
        // implicitly to point out the address of popped value; to eliminate the 
        // excessive dependence, this register is not considered.
      }
      else 
      {
        this->source_operands.insert(instruction_operand(current_register));
      }
    }
  }
  
  // target operands as written registers
  register_number = INS_MaxNumWRegs(current_instruction);
  for (register_id = 0; register_id < register_number; ++register_id) 
  {
    current_register = INS_RegW(current_instruction, register_id);
    if (current_register != REG_INST_PTR) 
    {
      if (INS_IsRet(current_instruction) && (current_register == REG_STACK_PTR))
      {
        // do nothing: see the explication above
      }
      else 
      {
        this->target_operands.insert(instruction_operand(current_register));
      }
    }
  }
  
}


/**
 * @brief copy constructor for an instruction object: since the taken analysis is trace-based so an 
 * instruction (at a given address) has multiple instances, all of them have the same static 
 * information (obtained in the above constructor) but have different dynamic information (e.g. 
 * accessed memory addresses)
 * 
 * @param other_instruction other instruction to copy.
 */
instruction::instruction(const instruction& other_instruction)
{
  // copy all static information
  this->address = other_instruction.address;
  this->dissasembled_name = other_instruction.dissasembled_name;
  this->contained_library = other_instruction.contained_library;
  this->contained_function = other_instruction.contained_function;
  this->is_syscall = other_instruction.is_syscall;
  this->is_vdso = other_instruction.is_vdso;
  this->is_memory_read = other_instruction.is_memory_read;
  this->is_memory_write = other_instruction.is_memory_write;
  this->is_conditional_branch = other_instruction.is_conditional_branch;
}


/**
 * @brief the read or written memories of an instruction cannot be determined statically, so they 
 * need to be updated gradually in the running time.
 * 
 * @param access_address the beginning read/written address
 * @param access_length the read/written length
 * @param read_or_written read (false) or written (true)
 * @return void
 */
void instruction::update_memory_access_info(ADDRINT access_address, UINT8 access_length, 
                                memory_access_t access_type)
{
  ADDRINT address;
  ADDRINT upper_bound_address = access_address + access_length;

  switch (access_type) 
  {
    case MEMORY_READ:
      for (address = access_address; address < upper_bound_address; ++address) 
      {
        this->source_operands.insert(instruction_operand(address));
      }
      break;
      
    case MEMORY_WRITE:
      for (address = access_address; address < upper_bound_address; ++address) 
      {
        this->target_operands.insert(instruction_operand(address));
      }
      break;
      
    default:
      break;
  }
  
  return;
}


} // end of dataflow_analysis namespace