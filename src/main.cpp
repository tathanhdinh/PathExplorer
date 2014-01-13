/*
 * Copyright (C) 2014  Ta Thanh Dinh <thanhdinh.ta@inria.fr>
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

#include "main.h"
#include "instrumentation/dbi.h"

using namespace instrumentation;

bool debug_enabled;

ADDRINT received_message_address;
INT32 received_message_length;
UINT32 current_execorder;
UINT32 exectrace_max_length;
UINT32 total_reexec_times;
UINT32 max_local_reexec_number;

boost::unordered_map<ADDRINT, ptr_instruction_t> instruction_at_address;
boost::unordered_map<UINT32, ptr_instruction_t> instruction_at_execorder;
boost::unordered_map<UINT32, ptr_cbranch_t> cbranch_at_execorder;
boost::unordered_map<UINT32, ptr_checkpoint_t> checkpoint_at_execorder;
boost::unordered_map<UINT32, exeorders_t> checkpoint_execorders_of_cbranch_at_execorder;
boost::unordered_map<UINT32, ptr_insoperands_t> outerface_at_execorder;
boost::unordered_map<ADDRINT, UINT8> original_msgstate_at_address;
boost::unordered_map<ADDRINT, UINT8> original_memstate_at_address;
boost::unordered_map<ADDRINT, UINT8> current_memstate_at_address;
boost::unordered_set<ptr_bridge_t> bridges_on_exectrace;

/**
 * @brief callback to initialize trace exploration.
 * 
 * @param data unused parameters because all initialized data are global variables
 * @return VOID
 */
VOID start_exploring(VOID* data)
{
  instruction_at_address.clear();
  instruction_at_execorder.clear();
  cbranch_at_execorder.clear();
  return;
}


/**
 * @brief callback to finalize some operations (e.g. save logs, etc).
 * 
 * @param code unused 
 * @param data ...
 * @return VOID
 */
VOID stop_exploring(INT32 code, VOID* data)
{
  return;
}


int main(int argc, char* argv[])
{
  // initialize PIN
  PIN_InitSymbols();
  PIN_Init(argc, argv);
  
  // setup instrumentation functions
  PIN_AddApplicationStartFunction(start_exploring, 0);
  INS_AddInstrumentFunction(dbi::instrument_instruction_before, 0);
  PIN_AddSyscallEntryFunction(dbi::instrument_syscall_enter, 0);
  PIN_AddSyscallExitFunction(dbi::instrument_syscall_exit, 0);
  PIN_AddFiniFunction(stop_exploring, 0);
  
  // pass control to PIN
  PIN_StartProgram();
  return 0;
}
