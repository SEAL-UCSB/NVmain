/*
 *  This file is part of NVMain- A cycle accurate timing, bit-accurate
 *  energy simulator for non-volatile memory. Originally developed by 
 *  Matt Poremba at the Pennsylvania State University.
 *
 *  Website: http://www.cse.psu.edu/~poremba/nvmain/
 *  Email: mrp5060@psu.edu
 *
 *  ---------------------------------------------------------------------
 *
 *  If you use this software for publishable research, please include 
 *  the original NVMain paper in the citation list and mention the use 
 *  of NVMain.
 *
 */

#ifndef __NVMAINREQUESTzz_H__
#define __NVMAINREQUESTzz_H__


#include "include/NVMAddress.h"
#include "include/NVMDataBlock.h"
#include "include/NVMTypes.h"


namespace NVM {


enum OpType { NOP, ACTIVATE, READ, WRITE, PRECHARGE, POWERDOWN_PDA, POWERDOWN_PDPF, POWERDOWN_PDPS, POWERUP, REFRESH };
enum MemRequestStatus { MEM_REQUEST_INCOMPLETE, MEM_REQUEST_COMPLETE, MEM_REQUEST_RETRY, MEM_REQUEST_NUM };
enum NVMAccessType { UNKNOWN_ACCESS, SUPERVISOR_ACCESS, USER_ACCESS };
enum BulkCommand { CMD_NOP = 0, 
		   CMD_PRE,
		   CMD_READPRE,
		   CMD_READ2PRE,
		   CMD_READ3PRE,
		   CMD_READ4PRE,
		   CMD_WRITEPRE,
		   CMD_WRITE2PRE,
		   CMD_WRITE3PRE,
		   CMD_WRITE4PRE,
		   CMD_ACTREADPRE, // 10
		   CMD_ACTREAD2PRE,
		   CMD_ACTREAD3PRE,
		   CMD_ACTREAD4PRE,
		   CMD_ACTWRITEPRE,
		   CMD_ACTWRITE2PRE,
		   CMD_ACTWRITE3PRE,
		   CMD_ACTWRITE4PRE,
		   CMD_PU_ACT_READ_PRE_PDPF,
		   CMD_PU_ACT_WRITE_PRE_PDPF,
		   CMD_ACT_READ_PRE_PDPF, // 20
		   CMD_ACT_WRITE_PRE_PDPF,
		   CMD_READ_PRE_PDPF,
		   CMD_WRITE_PRE_PDPF,
		   CMD_PRE_PDPF,
		   CMD_PDPF,
		   CMD_PU_ACT_READ_PRE,
		   CMD_PU_ACT_WRITE_PRE,
};

class MemoryController;
class MemOp;
class Interconnect;

class NVMainRequest
{
 public:
  NVMainRequest( ) 
  { 
      bulkCmd = CMD_NOP; 
      threadId = 0; 
      tag = 0; 
      issueController = NULL;
      issueInterconnect = NULL;
      reqInfo = NULL; 
      arrivalCycle = 0; 
      issueCycle = 0; 
      completionCycle = 0; 
      isPrefetch = false; 
      programCounter = 0; 
  };

  ~NVMainRequest( ) { };

  NVMAddress address;
  OpType type;
  BulkCommand bulkCmd;
  int threadId;
  NVMDataBlock data;
  MemRequestStatus status;
  NVMAccessType access;
  int tag;
  void *reqInfo;
  MemoryController *issueController;
  Interconnect *issueInterconnect;
  MemOp *memop;
  bool isPrefetch;
  NVMAddress pfTrigger;
  uint64_t programCounter;

  ncycle_t arrivalCycle;
  ncycle_t issueCycle;
  ncycle_t completionCycle;

  NVMainRequest operator=( NVMainRequest m );
};


inline
NVMainRequest NVMainRequest::operator=( NVMainRequest m )
{
  address = m.address;
  type = m.type;
  bulkCmd = m.bulkCmd;
  threadId = m.threadId;
  data = m.data;
  status = m.status;
  access = m.access;
  tag = m.tag;
  issueController = m.issueController;
  issueInterconnect = m.issueInterconnect;
  pfTrigger = m.pfTrigger;
  programCounter = m.programCounter;

  return *this; 
}


};


#endif


