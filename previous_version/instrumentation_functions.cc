#include <pin.H>

#include <fstream>
#include <vector>
#include <map>
#include <set>

#include <boost/format.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>

#include "stuffs.h"
#include "branch.h"
#include "instruction.h"
#include "analysis_functions.h"
#include "tainting_functions.h"
#include "resolving_functions.h"
#include "logging_functions.h"

/*====================================================================================================================*/

extern std::map< ADDRINT,
       instruction >                                addr_ins_static_map;

extern std::vector<ADDRINT>                         explored_trace;

extern bool                                         in_tainting;

extern map_ins_io                                   dta_inss_io;

extern UINT8                                        received_msg_num;

extern boost::shared_ptr<boost::posix_time::ptime>  start_ptr_time;
extern boost::shared_ptr<boost::posix_time::ptime>  stop_ptr_time;

/**
 * @brief instrumentation function.
 * 
 * @param ins current examined instruction.
 * @param data not used.
 * @return VOID
 */
VOID ins_instrumenter(INS ins, VOID *data)
{
  // logging the parsed instructions statically
  ADDRINT ins_addr = INS_Address(ins);

  addr_ins_static_map[ins_addr] = instruction(ins);
//   addr_ins_static_map[ins_addr].contained_image = contained_image_name(ins_addr);

  if (
      false
//       || INS_IsCall(ins)
//       || INS_IsSyscall(ins)
//       || INS_IsSysret(ins)
//       || INS_IsNop(ins)
    ) 
  {
    // omit these instructions
  } 
  else 
  {
    if (received_msg_num == 1) 
    {
      if (!start_ptr_time) 
      {
        start_ptr_time.reset(new boost::posix_time::ptime(boost::posix_time::microsec_clock::local_time()));
      }

      if (in_tainting) 
      {
        /* START LOGGING */
        if (INS_IsSyscall(ins))
        {
          INS_InsertPredicatedCall(ins, IPOINT_BEFORE, 
                                   (AFUNPTR)logging_syscall_instruction_analyzer,
                                   IARG_INST_PTR,
                                   IARG_END);
        } 
        else 
        {
          // general logging
          INS_InsertPredicatedCall(ins, IPOINT_BEFORE, 
                                   (AFUNPTR)logging_general_instruction_analyzer,
                                   IARG_INST_PTR,
                                   IARG_END);

          if (addr_ins_static_map[ins_addr].category == XED_CATEGORY_COND_BR) 
          {
            // conditional branch logging
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, 
                                     (AFUNPTR)logging_cond_br_analyzer,
                                     IARG_INST_PTR,
                                     IARG_BRANCH_TAKEN,
                                     IARG_END);
          } 
          else 
          {
            if (INS_IsMemoryRead(ins)) 
            {
              // memory read logging
              INS_InsertPredicatedCall(ins, IPOINT_BEFORE, 
                                       (AFUNPTR)logging_mem_read_instruction_analyzer,
                                       IARG_INST_PTR,
                                       IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE,
                                       IARG_CONTEXT,
                                       IARG_END);
            }

            if (INS_IsMemoryWrite(ins)) 
            {
              // memory written logging
              INS_InsertPredicatedCall(ins, IPOINT_BEFORE, 
                                       (AFUNPTR)logging_mem_write_instruction_analyzer,
                                       IARG_INST_PTR,
                                       IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE,
                                       IARG_END );
            }
          }
        }

        /* START TAINTING */
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, 
                                 (AFUNPTR)tainting_general_instruction_analyzer,
                                 IARG_INST_PTR,
                                 IARG_END );
      } 
      else // in rollbacking 
      { 
        /* START RESOLVING */
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, 
                                 (AFUNPTR)resolving_ins_count_analyzer,
                                 IARG_INST_PTR,
                                 IARG_END );

        if (INS_IsMemoryRead(ins)) 
        {
  //           INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)resolving_mem_to_st_analyzer,
  //                                    IARG_INST_PTR,
  //                                    IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE,
  //                                    IARG_END);
        } 
        else 
        {
          if (INS_IsMemoryWrite(ins)) 
          {
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, 
                                     (AFUNPTR)resolving_st_to_mem_analyzer,
                                     IARG_INST_PTR,
                                     IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE,
                                     IARG_END);
          }
        }
        
        // note that conditional branches are always direct
        if (addr_ins_static_map[ins_addr].category == XED_CATEGORY_COND_BR) 
        {
          INS_InsertPredicatedCall(ins, IPOINT_BEFORE,
                                   (AFUNPTR)resolving_cond_branch_analyzer,
                                   IARG_INST_PTR,
                                   IARG_BRANCH_TAKEN,
                                   IARG_END);
        }
        else 
        {
          if (INS_IsIndirectBranchOrCall(ins)) 
          {
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, 
                                     (AFUNPTR)resolving_indirect_branch_call_analyzer, 
                                     IARG_INST_PTR, 
                                     IARG_BRANCH_TARGET_ADDR, 
                                     IARG_END);
          }
        }
      }
    }
  }

  return;
}

/*====================================================================================================================*/

VOID image_load_instrumenter(IMG loaded_img, VOID *data)
{
  std::cout << "image_load_instrumenter activated\n";

	const static std::string winsock_dll_name("WS2_32.dll");

	if (received_msg_num < 1)
	{
		// verify whether the winsock2 module is loaded
		boost::filesystem::path loaded_image_path(IMG_Name(loaded_img));
		std::cout << "Loaded module: " << loaded_image_path.filename() << "\n";
		if (loaded_image_path.filename() == winsock_dll_name)
		{
			std::cout << "winsock found\n";
			RTN recv_function = RTN_FindByName(loaded_img, "recv");
			if (RTN_Valid(recv_function))
			{
				std::cout << "recv instrumented\n";
				RTN_Open(recv_function);

				RTN_InsertCall(recv_function, IPOINT_BEFORE, (AFUNPTR)logging_before_recv_functions_analyzer,
											 IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
											 IARG_END);
				RTN_InsertCall(recv_function, IPOINT_AFTER, (AFUNPTR)logging_after_recv_functions_analyzer,
											 IARG_FUNCRET_EXITPOINT_VALUE,
											 IARG_END);

				RTN_Close(recv_function);
			}

			RTN recvfrom_function = RTN_FindByName(loaded_img, "recvfrom");
			if (RTN_Valid(recvfrom_function))
			{
				std::cout << "recvfrom instrumented\n";
				RTN_Open(recvfrom_function);

				RTN_InsertCall(recvfrom_function, IPOINT_BEFORE, 
											 (AFUNPTR)logging_before_recv_functions_analyzer,
											 IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
											 IARG_END);
				RTN_InsertCall(recvfrom_function, IPOINT_AFTER, 
											 (AFUNPTR)logging_after_recv_functions_analyzer,
											 IARG_FUNCRET_EXITPOINT_VALUE,
											 IARG_END);

				RTN_Close(recvfrom_function);
			}

			RTN wsarecv_function = RTN_FindByName(loaded_img, "WSARecv");
			if (RTN_Valid(wsarecv_function))
			{
				std::cout << "WSARecv instrumented\n";
				RTN_Open(wsarecv_function);

				RTN_InsertCall(wsarecv_function, IPOINT_BEFORE, 
											 (AFUNPTR)logging_before_wsarecv_functions_analyzer, 
											 IARG_FUNCARG_ENTRYPOINT_VALUE, 1, 
											 IARG_END);
				RTN_InsertCall(wsarecv_function, IPOINT_AFTER, 
											 (AFUNPTR)logging_after_wsarecv_funtions_analyzer, 
											 IARG_END);

				RTN_Close(wsarecv_function);
			}

			RTN wsarecvfrom_function = RTN_FindByName(loaded_img, "WSARecvFrom");
			if (RTN_Valid(wsarecvfrom_function))
			{
				std::cout << "WSARecvFrom instrumented\n";
				RTN_Open(wsarecvfrom_function);

				RTN_InsertCall(wsarecvfrom_function, IPOINT_BEFORE, 
											 (AFUNPTR)logging_before_wsarecv_functions_analyzer, 
											 IARG_FUNCARG_ENTRYPOINT_VALUE, 1, 
											 IARG_END);
				RTN_InsertCall(wsarecvfrom_function, IPOINT_AFTER, 
											 (AFUNPTR)logging_after_wsarecv_funtions_analyzer, 
											 IARG_END);

				RTN_Close(wsarecvfrom_function);
			}
		}
	}

	return;
}

/*====================================================================================================================*/

BOOL process_create_instrumenter(CHILD_PROCESS created_process, VOID* data)
{
  BOOST_LOG_TRIVIAL(warning) 
    << boost::format("new process created with id %d") % CHILD_PROCESS_GetId(created_process);
  return TRUE;
}
