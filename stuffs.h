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

void        journal_buffer              (const std::string& filename, 
                                         UINT8* buffer_addr, UINT32 buffer_size);

void        assign_image_name           (ADDRINT ins_addr, std::string& img_name);

void        copy_instruction_mem_access (ADDRINT ins_addr, ADDRINT mem_addr, ADDRINT mem_size, UINT8 access_type);

void        journal_result              (UINT32 rollback_times, 
                                         UINT32 trace_size, UINT32 total_br_num, UINT32 resolved_br_num);

void        journal_result_total        (UINT32 max_rollback_times, UINT32 used_rollback_times, 
                                         UINT32 trace_size, UINT32 total_br_num, UINT32 resolved_br_num);

void        journal_static_trace        (const std::string& filename);

void        journal_explored_trace      (const std::string& filename, std::vector<ADDRINT>& trace);

void        journal_tainting_graph      (const std::string& filename);

void        journal_branch_messages     (ptr_branch& ptr_resolved_branch);

void        store_input                 (ptr_branch& ptr_br, bool br_taken);

void        print_debug_rollbacking_stop(ptr_branch& unexplored_ptr_branch);

void        print_debug_met_again       (ADDRINT ins_addr, ptr_branch &met_ptr_branch);

void        print_debug_failed_active_forward(ADDRINT ins_addr, ptr_branch& failed_ptr_branch);

void        print_debug_resolving_rollback(ADDRINT ins_addr, ptr_branch& new_ptr_branch);

void        print_debug_succeed         (ADDRINT ins_addr, ptr_branch& succeed_ptr_branch);

void        print_debug_resolving_failed(ADDRINT ins_addr, ptr_branch& failed_ptr_branch);

void        print_debug_found_new       (ADDRINT ins_addr, ptr_branch& found_ptr_branch);

void        print_debug_lost_forward    (ADDRINT ins_addr, ptr_branch& lost_ptr_branch);

void        print_debug_unknown_branch  (ADDRINT ins_addr, ptr_branch& unknown_ptr_branch);

void        print_debug_start_rollbacking();

void        print_debug_new_checkpoint  (ADDRINT ins_addr);

void        print_debug_new_branch      (ADDRINT ins_addr, ptr_branch& new_ptr_branch);

#endif // STUFFS_H
