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
#include "../analysis/dataflow.h"
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/compressed_pair.hpp>

namespace engine
{

using namespace analysis;  

class checkpoint
{
public:
  CONTEXT                                 cpu_context;
  UINT32                                  jumping_point;
  boost::unordered_map<ADDRINT, UINT8>    memory_state_at;
  boost::unordered_set<ptr_insoperand_t>  alive_operands;
  boost::unordered_set<ADDRINT>           memory_addresses_to_modify;
  
  boost::unordered_map<ADDRINT, UINT8>    memory_change_log;
  
public:
  checkpoint(CONTEXT* current_context);
  void log_before_execution(ADDRINT memory_written_address, UINT8 memory_written_length); 
  void modify_input();
  void restore_input();
};

typedef boost::shared_ptr<checkpoint> ptr_checkpoint_t;

} // end of engine namespace
#endif // CHECKPOINT_H
