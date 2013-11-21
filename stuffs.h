#ifndef STUFFS_H
#define STUFFS_H

#include <vector>
#include <utility>
#include <iostream>

#include <pin.H>

#include "instruction.h"
// #include "partial_trace.h"
#include "branch.h"


/* ------------------------------------------------------------------------------------------------------------------ */
/*                                                     exported functions                                             */
/* ------------------------------------------------------------------------------------------------------------------ */

void        journal_buffer ( const std::string& filename,
                             UINT8* buffer_addr, UINT32 buffer_size );

// std::string contained_image_name ( ADDRINT ins_addr );

void        copy_instruction_mem_access ( ADDRINT ins_addr, ADDRINT mem_addr, ADDRINT mem_size, UINT8 access_type );

void        journal_result ( UINT32 rollback_times,
                             UINT32 trace_size, UINT32 total_br_num, UINT32 resolved_br_num );

void        journal_result_total ( UINT32 max_rollback_times, UINT32 used_rollback_times,
                                   UINT32 trace_size, UINT32 total_br_num, UINT32 resolved_br_num );

void        journal_static_trace ( const std::string& filename );

void        journal_explored_trace ( const std::string& filename, std::vector<ADDRINT>& trace );

void        journal_tainting_graph ( const std::string& filename );

void        journal_branch_messages ( ptr_branch& ptr_resolved_branch );

void        journal_tainting_log();

void        store_input ( ptr_branch& ptr_br, bool br_taken );

void        print_debug_message_received();

void        print_debug_start_rollbacking();

void        print_debug_new_checkpoint ( ADDRINT ins_addr );

void        print_debug_mem_read ( ADDRINT ins_addr, ADDRINT mem_read_addr, UINT32 mem_read_size );

void        print_debug_mem_written ( ADDRINT ins_addr, ADDRINT mem_write_addr, UINT32 mem_write_size );

void        print_debug_reg_to_reg ( ADDRINT ins_addr, UINT32 num_sregs, UINT32 num_dregs );

void        print_debug_dep_branch ( ADDRINT ins_addr, ptr_branch& new_ptr_branch );

void        print_debug_indep_branch ( ADDRINT ins_addr, ptr_branch& indep_ptr_branch );

std::string remove_leading_zeros ( std::string input );

#endif // STUFFS_H
