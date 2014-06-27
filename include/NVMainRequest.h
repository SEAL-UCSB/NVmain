/*******************************************************************************
* Copyright (c) 2012-2014, The Microsystems Design Labratory (MDL)
* Department of Computer Science and Engineering, The Pennsylvania State University
* All rights reserved.
* 
* This source code is part of NVMain - A cycle accurate timing, bit accurate
* energy simulator for both volatile (e.g., DRAM) and non-volatile memory
* (e.g., PCRAM). The source code is free and you can redistribute and/or
* modify it by providing that the following conditions are met:
* 
*  1) Redistributions of source code must retain the above copyright notice,
*     this list of conditions and the following disclaimer.
* 
*  2) Redistributions in binary form must reproduce the above copyright notice,
*     this list of conditions and the following disclaimer in the documentation
*     and/or other materials provided with the distribution.
* 
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
* 
* Author list: 
*   Matt Poremba    ( Email: mrp5060 at psu dot edu 
*                     Website: http://www.cse.psu.edu/~poremba/ )
*******************************************************************************/

#ifndef __NVMAINREQUESTzz_H__
#define __NVMAINREQUESTzz_H__

#include "include/NVMAddress.h"
#include "include/NVMDataBlock.h"
#include "include/NVMTypes.h"
#include <iostream>
#include <signal.h>

namespace NVM {


enum OpType 
{ 
    NOP = 0,        /* No Operation */
    ACTIVATE,       /* a.k.a. RAS */
    READ,           /* a.k.a. CAS-R */ 
    READ_PRECHARGE, /* CAS-R with implicit PRECHARGE */ 
    WRITE,          /* a.k.a. CAS-W */  
    WRITE_PRECHARGE,/* CAS-W with implicit PRECHARGE */ 
    PRECHARGE,      /* PRECHARGE */
    PRECHARGE_ALL,  /* PRECHARGE all sub-arrays */
    POWERDOWN_PDA,  /* Active PowerDown */
    POWERDOWN_PDPF, /* Standby PowerDown with Fast Exit */
    POWERDOWN_PDPS, /* Standby PowerDown with Slow Exit */
    POWERUP,        /* PowerUp */
    REFRESH,        /* Refresh */
    BUS_READ,       /* Data bus read burst */
    BUS_WRITE,      /* Data bus write burst */ 
    CACHED_READ,    /* Check if read is cached anywhere in hierarchy. */
    CACHED_WRITE    /* Check if write is cached anywhere in hierarchy. */
};

enum MemRequestStatus 
{ 
    MEM_REQUEST_INCOMPLETE, /* Incomplete request */
    MEM_REQUEST_COMPLETE,   /* finished request */
    MEM_REQUEST_RETRY,      /* request that retried */
    MEM_REQUEST_NUM 
};      

enum NVMAccessType 
{ 
    UNKNOWN_ACCESS,     /* Undefined access right */
    SUPERVISOR_ACCESS,  /* Kernel access right */
    USER_ACCESS         /* User access right */
};

enum BulkCommand 
{ 
    /* TODO: do we still need this */
    CMD_NOP = 0, 
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

class NVMainRequest
{
  public:
    NVMainRequest( ) 
    { 
        type = NOP;
        bulkCmd = CMD_NOP; 
        threadId = 0; 
        tag = 0; 
        reqInfo = NULL; 
        flags = 0;
        arrivalCycle = 0; 
        issueCycle = 0; 
        queueCycle = 0;
        completionCycle = 0; 
        isPrefetch = false; 
        programCounter = 0; 
        burstCount = 1;
        writeProgress = 0;
        cancellations = 0;
        owner = NULL;
    };

    ~NVMainRequest( )
    { 
    };

    NVMAddress address;            //< Address of request
    OpType type;                   //< Operation type of request (read, write, etc)
    BulkCommand bulkCmd;           //< Bulk Commands (i.e., Read+Precharge, Write+Precharge, etc)
    ncounters_t threadId;                  //< Thread ID of issuing application
    NVMDataBlock data;             //< Data to be written, or data that would be read
    NVMDataBlock oldData;          //< Data that was previously at this address (pre-write)
    MemRequestStatus status;       //< Complete, incomplete, etc.
    NVMAccessType access;          //< User or kernel mode access
    int tag;                       //< User-defined tag for request (frontend only)
    void *reqInfo;                 //< User-defined info for request (frontend only)
    uint64_t flags;                //< Flags for NVMain (backend only)
    bool isPrefetch;               //< Whether request is a prefetch or not
    NVMAddress pfTrigger;          //< Address that triggered this prefetch
    uint64_t programCounter;       //< Program counter of CPU issuing request
    ncounter_t burstCount;         //< Number of bursts (used for variable-size requests.
    NVMObject *owner;              //< Pointer to the object that created this request

    ncycle_t arrivalCycle;         //< When the request arrived at the memory controller
    ncycle_t queueCycle;           //< When the memory controller accepted (queued) the request
    ncycle_t issueCycle;           //< When the memory controller issued the request to the interconnect (dequeued)
    ncycle_t completionCycle;      //< When the request was sent back to the requestor

    ncycle_t writeProgress;        //< Number of cycles remaining for write request
    ncycle_t cancellations;        //< Number of times this request was cancelled

    const NVMainRequest& operator=( const NVMainRequest& );
    bool operator<( NVMainRequest m ) const;

    enum NVMainRequestFlags
    {
        FLAG_LAST_REQUEST = 1,          // Last request for a row in the transaction queue
        FLAG_IS_READ = 2,               // Is a read (i.e., READ or READ_PRE, etc.)
        FLAG_IS_WRITE = 4,              // Is a write (i.e., WRITE or WRITE_PRE, etc.)
        FLAG_CANCELLED = 8,             // This write was cancelled
        FLAG_PAUSED = 16,               // This write was paused
        FLAG_FORCED = 32,               // This write can not be paused or cancelled
        FLAG_PRIORITY = 64,             // Request (or precursor) that takes priority over write
        FLAG_ISSUED = 128,              // Request has left the command queue
        FLAG_COUNT
    };

};

inline
const NVMainRequest& NVMainRequest::operator=( const NVMainRequest& m )
{
    address = m.address;
    type = m.type;
    bulkCmd = m.bulkCmd;
    threadId = m.threadId;
    data = m.data;
    oldData = m.oldData;
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
