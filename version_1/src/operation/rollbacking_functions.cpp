#include "rollbacking_functions.h"

#include "../base/instruction.h"
#include "../base/checkpoint.h"
#include "../base/cond_direct_instruction.h"

#include <boost/predef.h>
#include <boost/format.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>

/*================================================================================================*/

extern std::map<ADDRINT, ptr_instruction_t>     ins_at_addr;
extern std::map<UINT32, ptr_instruction_t>      ins_at_order;

extern UINT32                                   current_exec_order;

namespace logging = boost::log;
namespace sinks   = boost::log::sinks;
namespace sources = boost::log::sources;
typedef sinks::text_file_backend text_backend;
typedef sinks::synchronous_sink<text_backend>   sink_file_backend;
typedef logging::trivial::severity_level        log_level;

extern sources::severity_logger<log_level>      log_instance;
extern boost::shared_ptr<sink_file_backend>     log_sink;

/*================================================================================================*/

static ptr_cond_direct_instruction_t            active_cfi;
static UINT32                                   active_cfi_exec_order;
static ptr_checkpoint_t                         active_checkpoint;
static addrint_set                              active_modified_addrs;

/*================================================================================================*/

namespace rollbacking
{

inline static void rollback()
{
  // verify if the number of used rollbacks has reached limit
  if (active_cfi->used_rollback_num < active_cfi->max_rollback_num - 1)
  {
    // not reached yet, then just rollback again with a new value of the input
    active_cfi->used_rollback_num++;
    rollback_and_modify(active_checkpoint, active_modified_addrs);
  }
  else
  {
    // already reached, then restore the orginal value of the input
    if (active_cfi->used_rollback_num == active_cfi->max_rollback_num - 1)
    {
      if (!active_cfi->is_resolved) active_cfi->is_bypassed = true;
      active_cfi->used_rollback_num++;
      active_cfi.reset(); active_cfi_exec_order = 0;
      active_checkpoint.reset();
      rollback_and_restore(active_checkpoint, active_modified_addrs);
    }
    else
    {
      // exceeds
      BOOST_LOG_SEV(log_instance, logging::trivial::info)
          << boost::format("the number of used rollback exceeds its bound value");
      PIN_ExitApplication(1);
    }
  }
  return;
}

/*================================================================================================*/

static inline void get_next_active_checkpoint()
{
  std::vector<checkpoint_with_modified_addrs>::iterator chkpnt_iter, next_chkpnt_iter;
  if (active_checkpoint)
  {
    next_chkpnt_iter = active_cfi->checkpoints.begin(); chkpnt_iter = next_chkpnt_iter;
    while (++next_chkpnt_iter != active_cfi->checkpoints.end())
    {
      if (chkpnt_iter->first->execution_order == active_checkpoint->execution_order)
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
    active_checkpoint = active_cfi->checkpoints[0].first;
    active_modified_addrs = active_cfi->checkpoints[0].second;
  }
  return;
}

/*================================================================================================*/

/**
 * @brief This function aims to give a generic approach for solving control-flow instructions. The
 * main idea is to verify if the re-executed trace (i.e. rollback with a modified input) is the
 * same as the original trace: if there exists an instruction that does not occur in the original
 * trace then that must be resulted from a control-flow instruction which has changed the control
 * flow, so the new instruction will not be executed and we take a rollback.
 * The semantics of the function is quite sophisticated because there are several conditions to
 * check, we give in the following the explanation in the type of "literate programming".
 *
 * @param ins_addr: the address of the current examined instruction.
 * @return no return value.
 */
VOID generic_instruction(ADDRINT ins_addr)
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
      if (active_cfi_exec_order + 1 == current_exec_order)
      {
        // it is, then it will be marked as resolved
        active_cfi->is_resolved = true;
        active_cfi->is_bypassed = false;
      }
      else
      {
        // it is not, that means some other CFI (between the current CFI and the checkpoint) will
        // change the control flow
      }
      // in both cases, we need rollback
      rollback();
    }
    else
    {
      // not activated, then some errors have occurred
      BOOST_LOG_SEV(log_instance, logging::trivial::fatal)
          << boost::format("there is no active CFI but the original trace will change");
      PIN_ExitApplication(1);
    }
  }
  else
  {
    // is in, then verify if there exists currently a CFI needed to resolve
    if (active_cfi)
    {
      // exists, then verify if the executed instruction has exceeded this CFI
      if (current_exec_order <= active_cfi_exec_order)
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

  return;
}

/*================================================================================================*/

VOID mem_write_instruction(ADDRINT ins_addr, ADDRINT mem_addr, UINT32 mem_size)
{
  return;
}

/*================================================================================================*/

VOID control_flow_instruction(ADDRINT ins_addr)
{
  ptr_cond_direct_instruction_t current_cfi =
      boost::static_pointer_cast<cond_direct_instruction>(ins_at_order[current_exec_order]);
  if (!current_cfi->is_resolved && !current_cfi->is_bypassed)
  {
    if (!current_cfi->input_dep_addrs.empty() && !active_cfi)
    {
      active_cfi = current_cfi; active_checkpoint.reset();
      get_next_active_checkpoint();
    }
  }
  return;
}

/*================================================================================================*/

} // end of rollbacking namespace
