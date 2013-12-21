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

#ifndef CHECKPOINT_H
#define CHECKPOINT_H

#include <pin.H>

#include <boost/unordered_map.hpp>
#include <boost/container/map.hpp>
#include <boost/container/vector.hpp>
#include <boost/shared_ptr.hpp>

namespace reverse_execution_engine
{

boost::unordered_map<ADDRINT, UINT8> total_memory_state;

class checkpoint
{
public:
  ADDRINT                               address;
  CONTEXT                               cpu_context;
  
  boost::unordered_map<ADDRINT, UINT8>  memory_log;
  boost::unordered_map<ADDRINT, UINT8>  local_memory_state;
  boost::container::vector<ADDRINT>     trace;
  
public:
  checkpoint(ADDRINT current_address, CONTEXT* current_context);
  
  void log_before_execution(ADDRINT memory_written_address, UINT8 memory_written_length);  
};

void log_after_execution(ADDRINT memory_written_address, UINT8 memory_written_length);
void move_backward(boost::shared_ptr<checkpoint>& target_checkpoint);
void move_forward(boost::shared_ptr<checkpoint>& target_checkpoint);

}
#endif // CHECKPOINT_H
