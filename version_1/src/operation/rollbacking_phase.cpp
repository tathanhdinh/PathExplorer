#include "tainting_phase.h"
#include "common.h"
#include "../util/stuffs.h"

#include <cstdlib>
#include <limits>
#include <algorithm>

/*================================================================================================*/

static ptr_cond_direct_ins_t    active_cfi;
static ptr_checkpoint_t         active_checkpoint;
static ptr_checkpoint_t         first_checkpoint;
static UINT32                   max_rollback_num;
static UINT32                   used_rollback_num;
static UINT32                   tainted_trace_length;
static addrint_set_t            active_modified_addrs;
static addrint_value_map_t      active_modified_addrs_values;
static ptr_uint8_t              fresh_input;
static ptr_uint8_t              tainting_input;

/*================================================================================================*/

namespace rollbacking
{
static inline void initialize_values_at_active_modified_addrs()
{
  active_modified_addrs_values.clear();
  addrint_set_t::iterator addr_iter = active_modified_addrs.begin();
  for (; addr_iter != active_modified_addrs.end(); ++addr_iter)
  {
    active_modified_addrs_values[*addr_iter] = 0;
  }
  return;
}


static inline void generate_testing_values()
{
  addrint_value_map_t::iterator addr_iter = active_modified_addrs_values.begin();
  for (; addr_iter != active_modified_addrs_values.end(); ++addr_iter)
  {
    addr_iter->second = rand() % std::numeric_limits<UINT8>::max();
  }
  return;
}


static inline void rollback()
{
  // verify if the number of used rollbacks has reached its bound
  if (used_rollback_num < max_rollback_num - 1)
  {
    // not reached yet, then just rollback again with a new value of the input
    active_cfi->used_rollback_num++; used_rollback_num++; generate_testing_values();
    rollback_with_modified_input(active_checkpoint, current_exec_order,
                                 active_modified_addrs_values);
  }
  else
  {
    // already reached, then restore the orginal value of the input
    if (used_rollback_num == max_rollback_num - 1)
    {
      active_cfi->used_rollback_num++; used_rollback_num++;
      rollback_with_original_input(active_checkpoint, current_exec_order);
    }
#if !defined(NDEBUG)
    else
    {
      // exceeds
      tfm::format(log_file, "fatal: the number of used rollback (%d) exceeds its bound value (%d)\n",
                  used_rollback_num, max_rollback_num);
      PIN_ExitApplication(1);
    }
#endif
  }
  return;
}


/**
 * @brief get_next_active_checkpoint
 */
static inline void get_next_active_checkpoint()
{
  std::vector<checkpoint_with_modified_addrs>::iterator chkpnt_iter, next_chkpnt_iter;
  // verify if there exist an enabled active checkpoint
  if (active_checkpoint)
  {
    // exist, then find the next checkpoint in the checkpoint list of the current active CFI
    next_chkpnt_iter = active_cfi->checkpoints.begin(); chkpnt_iter = next_chkpnt_iter;
    while (++next_chkpnt_iter != active_cfi->checkpoints.end())
    {
      if (chkpnt_iter->first->exec_order == active_checkpoint->exec_order)
      {
        active_checkpoint = next_chkpnt_iter->first;
        active_modified_addrs = next_chkpnt_iter->second;
        break;
      }
      chkpnt_iter = next_chkpnt_iter;
    }

    if (next_chkpnt_iter == active_cfi->checkpoints.end())
    {
      active_checkpoint.reset();
      active_modified_addrs.clear();
    }
  }
  else
  {
    // doest not exist, then the active checkpoint is assigned as the first checkpoint of the
    // current active CFI
    active_checkpoint = active_cfi->checkpoints[0].first;
    active_modified_addrs = active_cfi->checkpoints[0].second;
  }

  initialize_values_at_active_modified_addrs();
  return;
}


static inline void calculate_tainting_input(ptr_uint8_t original_input,
                                            addrint_value_map_t& modified_addrs_with_values)
{
  // make a copy of the original input
  tainting_input.reset(new UINT8[received_msg_size]);
  std::copy(original_input.get(), original_input.get() + received_msg_size, tainting_input.get());

  // update this copy with new values at modified addresses
  addrint_value_map_t::iterator addr_iter = modified_addrs_with_values.begin();
  for (; addr_iter != modified_addrs_with_values.end(); ++addr_iter)
  {
    tainting_input.get()[addr_iter->first - received_msg_addr] = addr_iter->second;
  }
  return;
}


/**
 * @brief prepare_new_tainting_phase
 */
static inline void prepare_new_tainting_phase()
{
  // verify if there exists a resolved but unexplored CFI
  ptr_cond_direct_inss_t::iterator cfi_iter = detected_input_dep_cfis.begin();
  for (; cfi_iter != detected_input_dep_cfis.end(); ++cfi_iter)
  {
    if ((*cfi_iter)->is_resolved && !(*cfi_iter)->is_explored) break;
  }
  if (cfi_iter != detected_input_dep_cfis.end())
  {
    // exists, then set the CFI as explored
    exploring_cfi = *cfi_iter; exploring_cfi->is_explored = true;
    // calculate a new input for the next tainting phase
    calculate_tainting_input(exploring_cfi->fresh_input, exploring_cfi->second_input_projections[0]);

    // initialize new tainting phase
    current_running_phase = tainting_phase; tainting::initialize_tainting_phase();

#if !defined(NDEBUG)
    tfm::format(log_file, "explore the CFI %s at %d, start tainting\n",
                exploring_cfi->disassembled_name, exploring_cfi->exec_order);
//    log_file.flush();
#endif

    // rollback to the first checkpoint with the new input
    PIN_RemoveInstrumentation();
    rollback_with_new_input(first_checkpoint, current_exec_order, received_msg_addr,
                            received_msg_size, tainting_input.get());
  }
  else
  {
    // does not exist, namely all CFI are explored
#if !defined(NDEBUG)
    log_file << "stop exploring, all CFI have been explored\n";
#endif
    PIN_ExitApplication(0);
  }
  return;
}


/**
 * @brief get a projection of input on the active modified addresses
 */
static inline addrint_value_map_t input_on_active_modified_addrs()
{
  addrint_set_t::iterator addr_iter = active_modified_addrs.begin();
  addrint_value_map_t input_proj;
  for (; addr_iter != active_modified_addrs.end(); ++addr_iter)
  {
    input_proj[*addr_iter] = *(reinterpret_cast<UINT8*>(*addr_iter));
  }
  return input_proj;
}


/**
 * @brief This function aims to give a generic approach for solving control-flow instructions. The
 * main idea is to verify if the re-executed trace (i.e. rollback with a modified input) is the
 * same as the original trace: if there exists an instruction that does not occur in the original
 * trace then that must be resulted from a control-flow instruction which has changed the control
 * flow, so the new instruction will not be executed and we take a rollback.
 * Its semantics is quite sophisticated because there are several conditions to check.
 *
 * @param ins_addr: the address of the current examined instruction.
 * @return no return value.
 */
VOID generic_instruction(ADDRINT ins_addr)
{
  // verify if the execution order of the instruction exceeds the last CFI
  if (current_exec_order >= tainted_trace_length)
  {
    // exceeds, namely the rollbacking phase should stop
    prepare_new_tainting_phase();
  }
  else
  {
    current_exec_order++;
//#if !defined(NDEBUG)
//    tfm::format(std::cerr, "%-3d %-15s %-50s %-25s %-25s\n", current_exec_order,
//                addrint_to_hexstring(ins_addr), ins_at_addr[ins_addr]->disassembled_name,
//                ins_at_addr[ins_addr]->contained_image, ins_at_addr[ins_addr]->contained_function);
//#endif

    // verify if the executed instruction is in the original trace
    if (ins_at_order[current_exec_order]->address != ins_addr)
    {
      // is not in, then verify if the current control-flow instruction (abbr. CFI) is activated
      if (active_cfi)
      {
        // activated, that means the rollback from some checkpoint of this CFI will change the
        // control-flow, then verify if the CFI is the just previous executed instruction
        if (active_cfi->exec_order + 1 == current_exec_order)
        {
#if !defined(NDEBUG)
          if (!active_cfi->is_resolved)
          {
            tfm::format(log_file, "the CFI %s at %d is resolved\n", active_cfi->disassembled_name,
                        active_cfi->exec_order);
          }
#endif
          // it is, then it will be marked as resolved
          active_cfi->is_resolved = true;
          // push an input projection into the corresponding input list of the active CFI
          if (active_cfi->second_input_projections.empty())
          {
            active_cfi->second_input_projections.push_back(input_on_active_modified_addrs());
          }
        }
        else
        {
          // it is not, that means some other CFI (between the current CFI and the checkpoint) will
          // change the control flow
        }
        // in both cases, we need rollback
        rollback();
      }
#if !defined(NDEBUG)
      else
      {
        // not activated, then some errors have occurred
        tfm::format(log_file,
                    "fatal: there is no active CFI but the original trace changes (%s %d)\n",
                    addrint_to_hexstring(ins_addr), current_exec_order);
        PIN_ExitApplication(1);
      }
#endif
    }
    else
    {
      // the executed instruction is in the original trace, then verify if there exists active CFI
      // and the executed instruction has exceeded this CFI
      if (active_cfi && (current_exec_order > active_cfi->exec_order)) rollback();
    }
  }

  return;
}


/**
 * @brief control_flow_instruction
 */
VOID control_flow_instruction(ADDRINT ins_addr)
{
  ptr_cond_direct_ins_t current_cfi;

  // consider only CFIs that are beyond the exploring CFI
  if (!exploring_cfi || (exploring_cfi && (exploring_cfi->exec_order < current_exec_order)))
  {
    // verify if there exists already an active CFI in resolving
    if (active_cfi)
    {
      // yes, then verify if the current execution order reaches the active CFI
      if (current_exec_order < active_cfi->exec_order)
      {
        // not reached yes, then do nothing
      }
      else
      {
        // reached, then normally the current CFI should be the active one
        if (current_exec_order == active_cfi->exec_order)
        {
          // verify if its current checkpoint is in the last rollback try
          if (used_rollback_num == max_rollback_num)
          {
            // yes, then verify if there exists another checkpoint
            get_next_active_checkpoint();
            if (active_checkpoint)
            {
              // exists, then rollback to the new active checkpoint
              used_rollback_num = 0; rollback();
            }
            else
            {
#if !defined(NDEBUG)
              if (!active_cfi->is_resolved)
              {
                tfm::format(log_file, "the CFI %s at %d is bypassed\n",
                            active_cfi->disassembled_name, active_cfi->exec_order);
              }
#endif
              // the next checkpoint does not exist, all of tests reserved for it have been used
              active_cfi->is_bypassed = !active_cfi->is_resolved; active_cfi.reset();
              used_rollback_num = 0;
            }
          }

        }
      }
    }
    else
    {
      // there is no CFI in resolving, then verify if the current CFI depends on the input
      current_cfi = pept::static_pointer_cast<cond_direct_instruction>(
            ins_at_order[current_exec_order]);
      if (!current_cfi->input_dep_addrs.empty())
      {
        // yes, then set it as the active CFI
        active_cfi = current_cfi; get_next_active_checkpoint();
#if !defined(NDEBUG)
        tfm::format(log_file, "the CFI %s at %d is activated\n", active_cfi->disassembled_name,
                    active_cfi->exec_order);
#endif
        // make a copy of the fresh input
        active_cfi->fresh_input.reset(new UINT8[received_msg_size]);
        std::copy(fresh_input.get(), fresh_input.get() + received_msg_size,
                  active_cfi->fresh_input.get());

        // push an input projection into the corresponding input list of the active CFI
        if (active_cfi->first_input_projections.empty())
        {
          active_cfi->first_input_projections.push_back(input_on_active_modified_addrs());
        }

        // and rollback to resolve the new active CFI
        used_rollback_num = 0; rollback();
      }
    }
  }

  return;
}


/**
 * @brief tracking instructions that write memory
 */
VOID mem_write_instruction(ADDRINT ins_addr, ADDRINT mem_addr, UINT32 mem_length)
{
  // verify if the active checkpoint is enabled
  if (active_checkpoint)
  {
    // yes, namely we are now in some "reverse" execution from it to the active CFI, so this
    // checkpoint needs to track memory write instructions
    active_checkpoint->mem_write_tracking(mem_addr, mem_length);
  }
  else
  {
    // no, namely we are now in normal "forward" execution, so all checkpoint until the current
    // execution order need to track memory write instructions
    ptr_checkpoints_t::iterator chkpnt_iter = saved_checkpoints.begin();
    for (; chkpnt_iter != saved_checkpoints.end(); ++chkpnt_iter)
    {
      if ((*chkpnt_iter)->exec_order <= current_exec_order)
      {
        (*chkpnt_iter)->mem_write_tracking(mem_addr, mem_length);
      }
      else
      {
        break;
      }
    }
  }
  return;
}


/**
 * @brief initialize_rollbacking_phase
 */
void initialize(UINT32 trace_length_limit)
{
  // reinitialize some local variables
  active_cfi.reset(); active_checkpoint.reset(); first_checkpoint = saved_checkpoints[0];
  active_modified_addrs.clear(); tainted_trace_length = trace_length_limit;
  used_rollback_num = 0; max_rollback_num = max_local_rollback.Value();

  // make a fresh input copy
  fresh_input.reset(new UINT8[received_msg_size]);
  UINT8* new_rollbacking_input = reinterpret_cast<UINT8*>(received_msg_addr);
  if (exploring_cfi) new_rollbacking_input = exploring_cfi->fresh_input.get();
  std::copy(new_rollbacking_input, new_rollbacking_input + received_msg_size, fresh_input.get());

  return;
}

} // end of rollbacking namespace
