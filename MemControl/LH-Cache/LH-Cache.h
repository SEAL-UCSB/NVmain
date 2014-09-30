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

#ifndef __MEMCONTROL_BASICDRC_H__
#define __MEMCONTROL_BASICDRC_H__

#include "Utils/Caches/CacheBank.h"
#include "MemControl/DRAMCache/AbstractDRAMCache.h"

namespace NVM {

#define DRC_TAGREAD1 tagGen->CreateTag("DRC_TAGREAD1")
#define DRC_TAGREAD2 tagGen->CreateTag("DRC_TAGREAD2")
#define DRC_TAGREAD3 tagGen->CreateTag("DRC_TAGREAD3")
#define DRC_MEMREAD  tagGen->CreateTag("DRC_MEMREAD")
#define DRC_FILL     tagGen->CreateTag("DRC_FILL")
#define DRC_ACCESS   tagGen->CreateTag("DRC_ACCESS")

class NVMain;


class LH_Cache : public AbstractDRAMCache 
{
  public:
    LH_Cache( );
    virtual ~LH_Cache( );

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

  protected:
    NVMainRequest *MakeTagRequest( NVMainRequest *triggerRequest, int tag );
    NVMainRequest *MakeTagWriteRequest( NVMainRequest *triggerRequest );
    NVMainRequest *MakeDRCRequest( NVMainRequest *triggerReqest );

    /* Predicate to determine if a bank is locked. */
    class BankLocked : public SchedulingPredicate
    {
        friend class LH_Cache;

      private:
        LH_Cache &memoryController;

      public:
        BankLocked( LH_Cache &_memoryController ) 
            : memoryController(_memoryController) { }

        bool operator() ( NVMainRequest* );
    };

    /* Predicate to determine if fill queue is full. */
    class FillQueueFull : public SchedulingPredicate
    {
        friend class LH_Cache;

      private:
        LH_Cache &memoryController;
        bool draining;

      public:
        FillQueueFull( LH_Cache &_memoryController ) 
            : memoryController(_memoryController), draining(false) { }

        bool operator() ( NVMainRequest* );
    };

    /* Predicate to determine if fill queue is full. */
    class NoWriteBuffering : public SchedulingPredicate
    {
        friend class LH_Cache;

      private:
        LH_Cache &memoryController;

      public:
        NoWriteBuffering( LH_Cache &_memoryController ) 
            : memoryController(_memoryController) { }

        bool operator() ( NVMainRequest* );
    };

    bool IssueDRCCommands( NVMainRequest *req );
    bool IssueFillCommands( NVMainRequest *req );

    void CalculateLatency( NVMainRequest *req, double *average, 
            uint64_t *measured );
    void CalculateQueueLatency( NVMainRequest *req, double *average, 
            uint64_t *measured );

    NVMTransactionQueue *drcQueue;
    NVMTransactionQueue *fillQueue;

    double averageHitLatency, averageHitQueueLatency;
    double averageMissLatency, averageMissQueueLatency;
    double averageMMLatency, averageMMQueueLatency;
    double averageFillLatency, averageFillQueueLatency;
    
    uint64_t measuredHitLatencies, measuredHitQueueLatencies;
    uint64_t measuredMissLatencies, measuredMissQueueLatencies;
    uint64_t measuredMMLatencies, measuredMMQueueLatencies;
    uint64_t measuredFillLatencies, measuredFillQueueLatencies;

    uint64_t mem_reads, mem_writes;
    uint64_t mm_reqs, mm_reads, fills;
    uint64_t rb_hits, rb_miss;
    uint64_t drcHits, drcMiss;
    uint64_t starvation_precharges;
    uint64_t psInterval;

    uint64_t fillQueueSize, drcQueueSize;

    bool **bankLocked;
    BankLocked locks;
    FillQueueFull FQF;
    NoWriteBuffering NWB;
    bool useWriteBuffer;

    NVMain *mainMemory;

    CacheBank ***functionalCache;
};

};

#endif
