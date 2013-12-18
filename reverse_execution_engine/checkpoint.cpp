/*
 * <one line to give the program's name and a brief idea of what it does.>
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

#include "checkpoint.h"

extern std::vector<ADDRINT> explored_trace;

/*================================================================================================*/

checkpoint::checkpoint(ADDRINT current_address, 
                       CONTEXT* current_context, 
                       std::vector<ADDRINT>& current_trace)
{
  this->address = current_address;
  PIN_SaveContext(current_context, this->cpu_context.get());
  this->trace = current_trace;
}

/*================================================================================================*/

void checkpoint::log_memory_written(ADDRINT memory_written_address, 
                                    UINT8 memory_written_length)
{
  ADDRINT upper_bound_address = memory_written_address + memory_written_length;
  ADDRINT address = memory_written_address;
  
  for (; address < upper_bound_address; ++address) 
  {
    if (this->memory_log.find(address) == this->memory_log.end()) 
    {
      this->memory_log[address] = *(reinterpret_cast<UINT8*>(address));
    }
  }
  
  return;
}

/*================================================================================================*/

void move_backward(ptr_checkpoint& target_checkpoint)
{
  // restore the explored trace
  explored_trace = target_checkpoint->trace;
  explored_trace.pop_back();
  
  
}