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


#ifndef __MEMCONTROL_LOCACHE_H__
#define __MEMCONTROL_LOCACHE_H__


#include "Utils/Caches/CacheBank.h"
#include "MemControl/DRAMCache/AbstractDRAMCache.h"


#include <map>


namespace NVM {


#define DRC_MEMREAD tagGen->CreateTag("DRC_MEMREAD")
#define DRC_FILL    tagGen->CreateTag("DRC_FILL")
#define DRC_EVICT   tagGen->CreateTag("DRC_EVICT")


class NVMain;


class LO_Cache : public AbstractDRAMCache
{
  public:
    LO_Cache( );
    virtual ~LO_Cache( );

    void SetConfig( Config *conf, bool createChildren = true );
    void SetMainMemory( NVMain *mm );

    bool IssueAtomic( NVMainRequest *req );
    bool IsIssuable( NVMainRequest *request, FailReason *reason = NULL );
    bool IssueCommand( NVMainRequest *req );
    bool IssueFunctional( NVMainRequest *req );
    bool RequestComplete( NVMainRequest *req );

    void Cycle( ncycle_t );

    void RegisterStats( );
    void CalculateStats( );

    void CreateCheckpoint( std::string dir );
    void RestoreCheckpoint( std::string dir );

  private:
    NVMTransactionQueue *drcQueue;
    NVMain *mainMemory;
    Config *mainMemoryConfig;
    CacheBank ***functionalCache;

    ncounter_t starvationThreshold;
    ncounter_t drcQueueSize;
    ncounter_t ranks, banks;

    ncounter_t drc_hits, drc_miss;
    ncounter_t drc_evicts, drc_fills;
    ncounter_t drc_dirty_evicts;
    ncounter_t rb_hits, rb_miss;
    ncounter_t starvation_precharges;

    bool perfectFills;
    uint64_t max_addr;
    double drc_hitrate;

    std::map<NVMainRequest *, NVMainRequest *> outstandingFills;
};


};


#endif
