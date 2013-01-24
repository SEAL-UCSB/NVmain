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

#include <iostream>
#include <signal.h>


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

class NVMObject;

//extern uint64_t g_requests_alloced;


class NVMainRequest
{
 public:
  NVMainRequest( ) 
  { 
      bulkCmd = CMD_NOP; 
      threadId = 0; 
      tag = 0; 
      reqInfo = NULL; 
      arrivalCycle = 0; 
      issueCycle = 0; 
      queueCycle = 0;
      completionCycle = 0; 
      isPrefetch = false; 
      programCounter = 0; 
      owner = NULL;
      //g_requests_alloced++;
      //std::cout << g_requests_alloced << " NVMainRequests allocated. " << std::endl;
  };

  ~NVMainRequest( )
  { 
      //int a;
      //if( (void*)this < (void*)&a ) g_requests_alloced--;
  };

  NVMAddress address;            //< Address of request
  OpType type;                   //< Operation type of request (read, write, etc)
  BulkCommand bulkCmd;           //< Bulk Commands (i.e., Read+Precharge, Write+Precharge, etc)
  int threadId;                  //< Thread ID of issuing application
  NVMDataBlock data;             //< Data to be written, or data that would be read
  MemRequestStatus status;       //< Complete, incomplete, etc.
  NVMAccessType access;          //< User or kernel mode access
  int tag;                       //< User-defined tag for request
  void *reqInfo;                 //< User-defined info for request
  bool isPrefetch;               //< Whether request is a prefetch or not
  NVMAddress pfTrigger;          //< Address that triggered this prefetch
  uint64_t programCounter;       //< Program counter of CPU issuing request
  NVMObject *owner;              //< Pointer to the object that created this request

  ncycle_t arrivalCycle;         //< When the request arrived at the memory controller
  ncycle_t queueCycle;           //< When the memory controller accepted (queued) the request
  ncycle_t issueCycle;           //< When the memory controller issued the request to the interconnect (dequeued)
  ncycle_t completionCycle;      //< When the request was sent back to the requestor

  NVMainRequest& operator=( const NVMainRequest& );
  bool operator<( NVMainRequest m ) const;
};


inline
NVMainRequest& NVMainRequest::operator=( const NVMainRequest& m )
{
  address = m.address;
  type = m.type;
  bulkCmd = m.bulkCmd;
  threadId = m.threadId;
  data = m.data;
  status = m.status;
  access = m.access;
  tag = m.tag;
  reqInfo = m.reqInfo;
  isPrefetch = m.isPrefetch;
  pfTrigger = m.pfTrigger;
  programCounter = m.programCounter;
  owner = m.owner;

  arrivalCycle = m.arrivalCycle;
  queueCycle = m.queueCycle;
  issueCycle = m.issueCycle;
  completionCycle = m.completionCycle;

  return *this; 
}


inline
bool NVMainRequest::operator<( NVMainRequest m ) const
{
    return ( this < &m );
}


};


#endif


