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

// void        journal_buffer          (const std::string& filename,
//                                      UINT8* buffer_addr, 
//                                      UINT32 buffer_size );

void        journal_static_trace    (const std::string& filename);

void        journal_explored_trace  (const std::string& filename);

void        journal_tainting_graph  (const std::string& filename);

void        journal_branch_messages (ptr_branch& ptr_resolved_branch);

void        journal_tainting_log    ();

void        store_input             (ptr_branch& ptr_br, bool br_taken);

std::string remove_leading_zeros    (std::string input);

std::string addrint_to_hexstring    (ADDRINT input);

#endif // STUFFS_H
