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

sources::severity_logger<log_level>             log_instance;
boost::shared_ptr<sink_file_backend>            log_sink;

/*================================================================================================*/

static ptr_cond_direct_instruction_t            active_cfi;
static ptr_checkpoint_t                         active_chkpnt;
static addrint_set                              active_modified_addrs;

/*================================================================================================*/

namespace rollbacking
{

inline static void rollback()
{
  // verify if the number of used rollbacks has reached limit
  if (active_cfi->used_rollback_num < active_cfi->max_rollback_num - 1)
  {
    // not reached yet
    active_cfi->used_rollback_num++;
    rollback_and_modify(active_chkpnt, active_modified_addrs);
  }
  else
  {
    // already reached
    if (active_cfi->used_rollback_num == active_cfi->max_rollback_num - 1)
    {
      active_cfi->used_rollback_num++;
      rollback_and_restore(active_chkpnt, active_modified_addrs);
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

/**
 * @brief This function aims to give a generic approach for solving control-flow instructions. It
 * semantics is then quite sophisticated because there are several conditions to check, we give the
 * explanation in the type of "literate programming".
 * @param ins_addr: the address of the current examined instruction.
 * @return no return.
 */
VOID generic_instruction(ADDRINT ins_addr)
{
  current_exec_order++;

  // verify if the next executed instruction exists the original trace
  if (ins_at_order[current_exec_order]->address != ins_addr)
  {
    // exists, then verify if the current control-flow instruction (abbr CFI) is activated
    if (active_cfi)
    {
      // activated, that means the rollback from some checkpoint of this CFI will change the
      // control-flow, then
      // verify if the CFI is the just previous executed instruction
      if (ins_at_order[current_exec_order - 1]->address == active_cfi->address)
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
      // not activated, then some error have occurred
      BOOST_LOG_SEV(log_instance, logging::trivial::info)
          << boost::format("there is no active CFI but the original trace will change");
      PIN_ExitApplication(1);
    }
  }
  else
  {
    // does not exist, then
    // verify if there exists an conditional direct control-flow instruction needed to resolve
    if (active_cfi)
    {
      // exists, then

    }
    else
    {
      // does not exist, then do nothing
    }
  }

  return;
}

} // end of rollbacking namespace
