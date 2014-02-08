#ifndef OPERAND_H
#define OPERAND_H

#include <pin.H>
#include <string>
#include <boost/variant.hpp>
#include <boost/shared_ptr.hpp>

class operand
{
public:
  std::string name;
  boost::variant<ADDRINT, REG>  value;
  
public:
  operand(ADDRINT mem_addr);
  operand(REG reg);
};

typedef boost::shared_ptr<operand> ptr_operand_t;

#endif // OPERAND_H
