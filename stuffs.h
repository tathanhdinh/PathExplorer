#ifndef STUFFS_H
#define STUFFS_H

#include <vector>
#include <utility>
#include <iostream>

#include <pin.H>

#include "instruction.h"
#include "branch.h"


/* ------------------------------------------------------------------------------------------------------------------ */
/*                                                     exported functions                                             */
/* ------------------------------------------------------------------------------------------------------------------ */

void        journal_buffer          (const std::string& filename,
                                     UINT8* buffer_addr, 
                                     UINT32 buffer_size );

void        journal_result          (UINT32 rollback_times, 
                                     UINT32 trace_size, 
                                     UINT32 total_br_num, 
                                     UINT32 resolved_br_num );

void        journal_result_total    (UINT32 max_rollback_times, 
                                     UINT32 used_rollback_times,
                                     UINT32 trace_size, 
                                     UINT32 total_br_num, 
                                     UINT32 resolved_br_num);

void        journal_static_trace    (const std::string& filename);

void        journal_explored_trace  (const std::string& filename);

void        journal_tainting_graph  (const std::string& filename);

void        journal_branch_messages (ptr_branch& ptr_resolved_branch);

void        journal_tainting_log    ();

void        store_input             (ptr_branch& ptr_br, bool br_taken);

std::string remove_leading_zeros    (std::string input);

#endif // STUFFS_H
