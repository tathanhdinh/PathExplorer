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
  this->address  = INS_Address(ins);
  this->disass   = INS_Disassemble(ins);
  this->category = static_cast<xed_category_enum_t>(INS_Category(ins));
  
  if (INS_IsMemoryRead(ins))
  {
    this->mem_read_size = INS_MemoryReadSize(ins);
  }
  else 
  {
    this->mem_read_size = 0;
  }
  
  if (INS_IsMemoryWrite(ins))
  {
    this->mem_written_size = INS_MemoryWriteSize(ins);
  }
  else 
  {
    this->mem_written_size = 0;
  }
  
  std::map<ADDRINT, UINT8>().swap(this->mem_read_map);
  std::map<ADDRINT, UINT8>().swap(this->mem_written_map);
}

/*====================================================================================================================*/

instruction::instruction(const instruction& other)
{
  this->address           = other.address;
  this->disass            = other.disass;
  this->category          = other.category;
  
  this->mem_read_size     = other.mem_read_size;
  this->mem_written_size  = other.mem_written_size;
  
  this->mem_read_map      = other.mem_read_map;
  this->mem_written_map   = other.mem_written_map;
}

/*====================================================================================================================*/

instruction& instruction::operator=(const instruction& other)
{
  this->address           = other.address;
  this->disass            = other.disass;
  this->category          = other.category;
  
  this->mem_read_size     = other.mem_read_size;
  this->mem_written_size  = other.mem_written_size;
  
  this->mem_read_map      = other.mem_read_map;
  this->mem_written_map   = other.mem_written_map;
  
  return *this;
}
