#include "rollbacking_phase.h"
#include "common.h"

#include <algorithm>

/*================================================================================================*/

static ptr_cond_direct_instruction_t  active_cfi;
static ptr_checkpoint_t               active_checkpoint;
static UINT32                         max_rollback_num;
static UINT32                         used_rollback_num;
static UINT32                         tainted_trace_length;
static addrint_set_t                  active_modified_addrs;
static boost::shared_ptr<UINT8>       fresh_input;

/*================================================================================================*/

namespace rollbacking
{
static inline void rollback()
{
  // verify if the number of used rollbacks has reached its bound
  if (used_rollback_num < max_rollback_num - 1)
  {
    // not reached yet, then just rollback again with a new value of the input
    active_cfi->used_rollback_num++; used_rollback_num++;
    rollback_and_modify(active_checkpoint, active_modified_addrs);
  }
  else
  {
    // already reached, then restore the orginal value of the input
    if (used_rollback_num == max_rollback_num - 1)
    {
      active_cfi->used_rollback_num++; used_rollback_num++;
      rollback_and_restore(active_checkpoint, active_modified_addrs);
    }
#if !defined(NDEBUG)
    else
    {
      // exceeds
      log_file << boost::format("the number of used rollback exceeds its bound value");
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
  return;
}


static inline ptr_uint8_t calculate_projected_input(ptr_uint8_t original_input,
                                                    addrint_value_map_t& modified_addrs_with_values)
{
  // make a copy of the original input
  ptr_uint8_t projected_input = boost::make_shared<UINT8>(received_msg_size);
  std::copy(original_input.get(), original_input.get() + received_msg_size, projected_input.get());

  // update this copy with new values at modified addresses
  addrint_value_map_t::iterator addr_iter = modified_addrs_with_values.begin();
  for (; addr_iter != modified_addrs_with_values.end(); ++addr_iter)
  {
    projected_input.get()[addr_iter->first - received_msg_addr] = addr_iter->second;
  }
  return projected_input;
}


/**
 * @brief prepare_new_tainting_phase
 */
static inline void prepare_new_tainting_phase()
{
  // verify if there exists a resolved but unexplored CFI
  ptr_cond_direct_instructions_t::iterator cfi_iter = detected_input_dep_cfis.begin();
  for (; cfi_iter != detected_input_dep_cfis.end(); ++cfi_iter)
  {
    if ((*cfi_iter)->is_resolved && !(*cfi_iter)->is_explored) break;
  }
  if (cfi_iter != detected_input_dep_cfis.end())
  {
    // exists, then calculate a new input for the next tainting phase
    ptr_uint8_t new_input = calculate_projected_input((*cfi_iter)->fresh_input,
                                                      (*cfi_iter)->second_input_projections[0]);
    // set the CFI as explored
    (*cfi_iter)->is_explored = true;
    // and rollback to the first checkpoint with the new input
    saved_checkpoints[0]->rollback_with_new_input(received_msg_addr, new_input.get(),
                                                  received_msg_size);
  }
  else
  {
    // does not exist, namely all CFI are explored
#if !defined(NDEBUG)
    log_file << boost::format("stop exploring, all CFI have been explored");
#endif
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
  if (current_exec_order > tainted_trace_length)
  {
    // exceeds, namely the rollbacking phase should stop
    prepare_new_tainting_phase();
  }
  else
  {
    current_exec_order++;

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
          // it is, then it will be marked as resolved
          active_cfi->is_resolved = true;
          // push an input projection into the corresponding input list of the active CFI
          active_cfi->second_input_projections.push_back(input_on_active_modified_addrs());
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
        log_file << boost::format("there is no active CFI but the original trace will change");
        PIN_ExitApplication(1);
      }
#endif
    }
    else
    {
      // the executed instruction is in the original trace, then verify if there exists currently
      // some CFI needed to resolve
      if (active_cfi)
      {
        // exists, then verify if the executed instruction has exceeded this CFI
        if (current_exec_order <= active_cfi->exec_order)
        {
          // not exceeded yet, then do nothing
        }
        else
        {
          // just exceeded, then rollback
          rollback();
        }
      }
      else
      {
        // does not exist, then do nothing
      }
    }
  }
  return;
}


/**
 * @brief control_flow_instruction
 * @param ins_addr
 * @return
 */
VOID control_flow_instruction(ADDRINT ins_addr)
{
  // consider only CFIs that are beyond the exploring CFI
  if (!exploring_cfi || (exploring_cfi && (exploring_cfi->exec_order < current_exec_order)))
  {
    ptr_cond_direct_instruction_t current_cfi =
        boost::static_pointer_cast<cond_direct_instruction>(ins_at_order[current_exec_order]);
    // verify if the current CFI depends on the input
    if (!current_cfi->input_dep_addrs.empty())
    {
      // depends, then verify if it is resolved or bypassed
      if (!current_cfi->is_resolved && !current_cfi->is_bypassed)
      {
        // neither resolved nor bypassed, then verify if there exist already an active CFI needed
        // to resolve (i.e. the active CFI is already enabled)
        if (active_cfi)
        {
          // enabled, then verify if the current CFI is the active CFI
          if (current_exec_order == active_cfi->exec_order)
          {
            // is the active CFI, then verify if its current checkpoint is in the last rollback try
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
                // does not exist, then set the CFI as bypassed if it has been not resolved yet,
                // and disable it
                active_cfi->is_bypassed = !active_cfi->is_resolved; active_cfi.reset();
              }
            }
          }
          else
          {
            // the current CFI is not the active CFI, then do nothing
          }
        }
        else
        {
          // the active CFI is disabled, then first set the current CFI as the active CFI
          active_cfi = current_cfi; get_next_active_checkpoint();

          // make a copy of the fresh input
          active_cfi->fresh_input = boost::make_shared<UINT8>(received_msg_size);
          std::copy(fresh_input.get(), fresh_input.get() + received_msg_size,
                    current_cfi->fresh_input.get());

          // push an input projection into the corresponding input list of the active CFI
          active_cfi->first_input_projections.push_back(input_on_active_modified_addrs());

          // and rollback to resolve the new active CFI
          used_rollback_num = 0; rollback();
        }
      }
      else
      {
        // either resolved or bypassed, then do nothing
      }
    }
    else
    {
      // doest not depend, then do nothing
    }
  }
  else
  {
    // CPIs are not beyond the exploring CPI, then omit them
  }

  return;
}


/**
 * @brief mem_write_instruction
 * @param ins_addr
 * @param mem_addr
 * @param mem_length
 * @return
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
    std::vector<ptr_checkpoint_t>::iterator chkpnt_iter = saved_checkpoints.begin();
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
void initialize_rollbacking_phase(UINT32 trace_length_limit)
{
  // reinitialize some local variables
  active_cfi.reset(); active_checkpoint.reset(); active_modified_addrs.clear();
  used_rollback_num = 0; max_rollback_num = max_local_rollback.Value();
  tainted_trace_length = trace_length_limit;

  // make a fresh input copy
  fresh_input = boost::make_shared<UINT8>(received_msg_size);
  ptr_uint8_t original_input(reinterpret_cast<UINT8*>(received_msg_addr));
  if (exploring_cfi) original_input = exploring_cfi->fresh_input;
//  std::copy(original_input.get(), original_input.get() + received_msg_size, fresh_input.get());

  std::cerr << "initialize_rollbacking_phase\n";
  std::cerr << "hahaha1111sdfsdfsd\n";
  return;
}

} // end of rollbacking namespace
