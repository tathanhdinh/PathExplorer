#ifndef COND_DIRECT_INSTRUCTION_H
#define COND_DIRECT_INSTRUCTION_H

#include "instruction.h"

class cond_direct_instruction : public instruction
{
public:
  cond_direct_instruction(const INS& ins);
};

typedef boost::shared_ptr<cond_direct_instruction> ptr_cond_direct_instruction_t;

#endif // COND_DIRECT_INSTRUCTION_H
