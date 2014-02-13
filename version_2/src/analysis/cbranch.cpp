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

#include "cbranch.h"
#include "../main.h"

namespace analysis 
{
  
/**
 * @brief constructor for a conditional branch object: reuse the constructor in the super class.
 * 
 * @param current_instruction instruction object passed from PIN
 */
cbranch::cbranch(const INS& current_instruction) : instruction(current_instruction)
{
  this->is_resolved = false; this->is_bypassed = false;
}


/**
 * @brief copy constructor for a conditional branch object: reuse the copy constructor in the super 
 * class.
 * 
 * @param other_instruction instruction object passed from PIN
 */
cbranch::cbranch(const instruction& other_instruction) : instruction(other_instruction)
{
  this->is_resolved = false; this->is_bypassed = false;
}


/**
 * @brief Save the current input that leads to the current branch decision.
 * 
 * @param current_branch_decision current decision
 * @return void
 */
void cbranch::save_current_input(bool current_branch_decision)
{
  ptr_uint8_t new_input_buff(new UINT8[received_message_length]);
  PIN_SafeCopy(new_input_buff.get(), reinterpret_cast<UINT8*>(received_message_address), 
               received_message_length);
  this->inputs_lead_to_decision[current_branch_decision].insert(new_input_buff);
  return;
}


} // end of analysis namespace
