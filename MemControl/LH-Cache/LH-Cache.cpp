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

#include "MemControl/LH-Cache/LH-Cache.h"
#include "include/NVMHelpers.h"
#include "NVM/nvmain.h"
#include "src/EventQueue.h"

#include <iostream>
#include <set>
#include <assert.h>

using namespace NVM;

LH_Cache::LH_Cache( )
    : locks(*this), FQF(*this), NWB(*this)
{
    //translator->GetTranslationMethod( )->SetOrder( 5, 1, 4, 3, 2, 6 );

    std::cout << "This Memory Controller is no longer maintained in favor of LO-Cache and is in a non-working state." << std::endl;
    std::cout << "This code is only provided for reference." << std::endl;

    exit(1);

    averageHitLatency = 0.0;
    averageHitQueueLatency = 0.0;
    averageMissLatency = 0.0;
    averageMissQueueLatency = 0.0;
    averageMMLatency = 0.0;
    averageMMQueueLatency = 0.0;
    averageFillLatency = 0.0;
    averageFillQueueLatency = 0.0;

    measuredHitLatencies = 0;
    measuredHitQueueLatencies = 0;
    measuredMissLatencies = 0;
    measuredMissQueueLatencies = 0;
    measuredMMLatencies = 0;
    measuredMMQueueLatencies = 0;
    measuredFillLatencies = 0;
    measuredFillQueueLatencies = 0;

    mem_reads = 0;
    mem_writes = 0;

    mm_reqs = 0;
    mm_reads = 0;
    fills = 0;

    rb_hits = 0;
    rb_miss = 0;

    drcHits = 0;
    drcMiss = 0;

    starvation_precharges = 0;

    psInterval = 0;

    InitQueues( 2 );

    functionalCache = NULL;

    useWriteBuffer = true;

    mainMemory = NULL;

    /* Alias */
    drcQueue = &(transactionQueues[0]);
    fillQueue = &(transactionQueues[1]);
}

LH_Cache::~LH_Cache( )
{
}

void LH_Cache::SetConfig( Config *conf, bool createChildren )
{
    /* Defaults */
    starvationThreshold = 4;
    drcQueueSize = 32;
    fillQueueSize = 8;
    useWriteBuffer = true;

    if( conf->KeyExists( "StarvationThreshold" ) )
        starvationThreshold = static_cast<unsigned int>( 
                conf->GetValue( "StarvationThreshold" ) );
    if( conf->KeyExists( "DRCQueueSize" ) )
        drcQueueSize = static_cast<uint64_t>( conf->GetValue( "DRCQueueSize" ) );
    if( conf->KeyExists( "FillQueueSize" ) )
        fillQueueSize = static_cast<uint64_t>( conf->GetValue( "FillQueueSize" ) );
    if( conf->KeyExists( "UseWriteBuffer" ) 
            && conf->GetString( "UseWriteBuffer" ) == "false" )
        useWriteBuffer = false;
    
    /*
     *  Lock banks between tag read and access. Initialize locks here.
     */
    ncounter_t banks, ranks;
    unsigned int i, j;

    ranks = static_cast<ncounter_t>( conf->GetValue( "RANKS" ) );
    banks = static_cast<ncounter_t>( conf->GetValue( "BANKS" ) );

    bankLocked = new bool*[ranks];
    functionalCache = new CacheBank**[ranks];
    for( i = 0; i < ranks; i++ )
    {
        bankLocked[i] = new bool[banks];
        functionalCache[i] = new CacheBank*[banks];

        for( j = 0; j < banks; j++ )
        {
            bankLocked[i][j] = false;
            functionalCache[i][j] = new CacheBank( 
                                         conf->GetValue( "ROWS" ), 1, 29, 64 );
        }
    }

    MemoryController::SetConfig( conf, createChildren );
}

void LH_Cache::RegisterStats( )
{
    AddStat(mem_reads);
    AddStat(mem_writes);
    AddStat(rb_hits);
    AddStat(rb_miss);
    AddStat(drcHits);
    AddStat(drcMiss);
    AddStat(fills);

    AddStat(mm_reqs);
    AddStat(mm_reads);

    AddStat(starvation_precharges);

    AddStat(averageHitLatency);
    AddStat(measuredHitLatencies);
    AddStat(averageHitQueueLatency);
    AddStat(measuredHitQueueLatencies);

    AddStat(averageMissLatency);
    AddStat(measuredMissLatencies);
    AddStat(averageMissQueueLatency);
    AddStat(measuredMissQueueLatencies);

    AddStat(averageMMLatency);
    AddStat(measuredMMLatencies);
    AddStat(averageMMQueueLatency);
    AddStat(measuredMMQueueLatencies);

    AddStat(averageFillLatency);
    AddStat(measuredFillLatencies);
    AddStat(averageFillQueueLatency);
    AddStat(measuredFillQueueLatencies);
}

void LH_Cache::SetMainMemory( NVMain *mm )
{
    mainMemory = mm;
}

void LH_Cache::CalculateLatency( NVMainRequest *req, double *average, 
        uint64_t *measured )
{
    (*average) = (( (*average) * static_cast<double>(*measured))
                    + static_cast<double>(req->completionCycle)
                    - static_cast<double>(req->issueCycle))
                 / static_cast<double>((*measured)+1);
    (*measured) += 1;
}

void LH_Cache::CalculateQueueLatency( NVMainRequest *req, double *average, 
        uint64_t *measured )
{
    (*average) = (( (*average) * static_cast<double>(*measured))
                    + static_cast<double>(req->issueCycle)
                    - static_cast<double>(req->arrivalCycle))
                 / static_cast<double>((*measured)+1);
    (*measured) += 1;
}

bool LH_Cache::IssueAtomic( NVMainRequest *req )
{
    uint64_t rank, bank;
    NVMDataBlock dummy;

    req->address.GetTranslatedAddress( NULL, NULL, &bank, &rank, NULL, NULL );

    if( functionalCache[rank][bank]->SetFull( req->address ) ) 
    {
        NVMAddress victim;

        (void)functionalCache[rank][bank]->ChooseVictim( req->address, &victim );
        (void)functionalCache[rank][bank]->Evict( victim, &dummy );
    }

    (void)functionalCache[rank][bank]->Install( req->address, dummy ); 

    return true;
}

bool LH_Cache::IssueFunctional( NVMainRequest *req )
{
    uint64_t rank, bank;
    NVMDataBlock dummy;

    req->address.GetTranslatedAddress( NULL, NULL, &bank, &rank, NULL, NULL );

    return functionalCache[rank][bank]->Present( req->address );
}

bool LH_Cache::IsIssuable( NVMainRequest * /*request*/, FailReason * /*fail*/ )
{
    bool rv = true;

    /*
     *  Limit the number of commands in the queue. This will stall the caches/CPU.
     */ 
    if( drcQueue->size( ) >= drcQueueSize )
    {
        rv = false;
    }

    return rv;
}

bool LH_Cache::IssueCommand( NVMainRequest *req )
{
    if( drcQueue->size( ) >= drcQueueSize )
    {
        return false;
    }

    req->arrivalCycle = GetEventQueue()->GetCurrentCycle();

    /*
     *  We first check the DRAM cache *always.* If that misses, we then issue to
     *  main memory, which will trigger and install request and return the request
     *  to the higher-level caches.
     */
    drcQueue->push_back( req );

    if( req->type == READ )
        mem_reads++;
    else
        mem_writes++;

    return true;
}

bool LH_Cache::RequestComplete( NVMainRequest *req )
{
    bool rv = false;

    req->completionCycle = GetEventQueue()->GetCurrentCycle();

    if( req->tag == DRC_TAGREAD3 )
    {
        bool miss;
        uint64_t rank, bank;
        NVMainRequest *originalRequest = static_cast<NVMainRequest *>(req->reqInfo);
        ncounter_t queueId = GetCommandQueueId( req->address );

        req->address.GetTranslatedAddress( NULL, NULL, &bank, &rank, NULL, NULL );

        /*  Check functional cache for hit or miss status here */
        miss = true;
        if( originalRequest->type == WRITE 
                || functionalCache[rank][bank]->Present( req->address ) )
            miss = false;

        /* If it is a hit, issue a request to the bank for the cache line */
        if( !miss )
        {
            commandQueues[queueId].push_back( MakeDRCRequest( req ) );

            drcHits++;
        }
        /* 
         * If it is a miss, issue a request to main memory for the cache line 
         * to be filled.
         */
        else
        {
            NVMainRequest *memReq = new NVMainRequest( );

            *memReq = *req;
            memReq->owner = this;
            memReq->tag = DRC_MEMREAD;

            mm_reqs++;

            /* TODO: Figure out what to do if this fails. */
            mainMemory->IssueCommand( memReq );

            drcMiss++;
        }

        /* 
         *  In either case, unlock the bank. 
         * 
         *  For a miss, we need to go to main memory, so unlock since this is time consuming.
         *  For a hit, we already injected the DRC request into the bank queue, so we can't 
         *             issue anyways.
         */
        bankLocked[rank][bank] = false;

    }
    else if( req->tag == DRC_MEMREAD )
    {
        /* Issue new fill request to drcQueue to be filled */
        NVMainRequest *fillReq = new NVMainRequest( );

        *fillReq = *req;
        fillReq->owner = this;
        fillReq->tag = DRC_FILL;
        fillReq->arrivalCycle = GetEventQueue()->GetCurrentCycle();

        /* TODO: Figure out what to do if this is full. */
        if( useWriteBuffer )
            fillQueue->push_back( fillReq );
        else
            drcQueue->push_back( fillReq );

        mm_reads++;

        CalculateLatency( req, &averageMMLatency, &measuredMMLatencies );
        CalculateQueueLatency( req, &averageMMQueueLatency, &measuredMMQueueLatencies );

        /* Mark the original request complete */
        NVMainRequest *originalRequest = static_cast<NVMainRequest *>(req->reqInfo);

        GetParent( )->RequestComplete( originalRequest );

        originalRequest->completionCycle = GetEventQueue()->GetCurrentCycle();

        CalculateLatency( originalRequest, &averageMissLatency, 
                &measuredMissLatencies );

        CalculateQueueLatency( originalRequest, &averageMissQueueLatency, 
                &measuredMissQueueLatencies );

    }
    else if( req->tag == DRC_FILL )
    {
        fills++;

        /* Fill complete, just calculate some stats */
        CalculateLatency( req, &averageFillLatency, 
                &measuredFillLatencies );

        CalculateQueueLatency( req, &averageFillQueueLatency, 
                &measuredFillQueueLatencies );
    }
    else if( req->tag == DRC_ACCESS )
    {
        CalculateLatency( req, &averageHitLatency, &measuredHitLatencies );
        CalculateQueueLatency( req, &averageHitQueueLatency, 
                &measuredHitQueueLatencies );
    }

    if( req->type == REFRESH )
        ProcessRefreshPulse( req );
    else if( req->owner == this )
    {
        delete req;
        rv = true;
    }
    else
    {
        GetParent( )->RequestComplete( req );
        rv = false;
    }

    return rv;
}

bool LH_Cache::FillQueueFull::operator() ( NVMainRequest * /*request*/ )
{
    if( memoryController.useWriteBuffer && draining == false
        && memoryController.fillQueue->size() >= memoryController.fillQueueSize )
    {
        draining = true;
    }
    else if( memoryController.fillQueue->size() == 0
             && draining == true )
    {
        draining = false;
    }

    return draining;
}

bool LH_Cache::BankLocked::operator() ( NVMainRequest * request )
{
    bool rv = false;
    uint64_t bank, rank;
    request->address.GetTranslatedAddress( NULL, NULL, &bank, &rank, NULL, NULL );

    if( memoryController.bankLocked[rank][bank] == false
        && !memoryController.FQF( request ) )
        rv = true;

    return rv;
}

bool LH_Cache::NoWriteBuffering::operator() ( NVMainRequest * /*request*/ )
{
    return !memoryController.useWriteBuffer;
}

void LH_Cache::Cycle( ncycle_t /*steps*/ )
{
    NVMainRequest *nextRequest = NULL;


    /* Check fill queue (write buffering). */
    if( FindStarvedRequest( *fillQueue, &nextRequest, FQF ) )
    {
        rb_miss++;
        starvation_precharges++;
    }
    else if( FindRowBufferHit( *fillQueue, &nextRequest, FQF ) )
    {
        rb_hits++;
    }
    else if( FindOldestReadyRequest( *fillQueue, &nextRequest, FQF ) )
    {
        rb_miss++;
    }
    else if( FindClosedBankRequest( *fillQueue, &nextRequest, FQF ) )
    {
        rb_miss++;
    }

    /* Check request queue. */
    else if( FindStarvedRequest( *drcQueue, &nextRequest, locks ) )
    {
        rb_miss++;
        starvation_precharges++;
    }
    else if( FindRowBufferHit( *drcQueue, &nextRequest, locks ) )
    {
        rb_hits++;
    }
    else if( FindOldestReadyRequest( *drcQueue, &nextRequest, locks ) )
    {
        rb_miss++;
    }
    else if( FindClosedBankRequest( *drcQueue, &nextRequest, locks ) )
    {
        rb_miss++;
    }

    /* Check fill queue (no write buffering). */
    else if( FindStarvedRequest( *fillQueue, &nextRequest, NWB ) )
    {
        rb_miss++;
        starvation_precharges++;
    }
    else if( FindRowBufferHit( *fillQueue, &nextRequest, NWB ) )
    {
        rb_hits++;
    }
    else if( FindOldestReadyRequest( *fillQueue, &nextRequest, NWB ) )
    {
        rb_miss++;
    }
    else if( FindClosedBankRequest( *fillQueue, &nextRequest, NWB ) )
    {
        rb_miss++;
    }

    if( nextRequest != NULL )
    {
        if( nextRequest->tag == DRC_FILL )
            IssueFillCommands( nextRequest );
        else
            IssueDRCCommands( nextRequest );
    }

    CycleCommandQueues( );
}

NVMainRequest *LH_Cache::MakeTagRequest( NVMainRequest *triggerRequest, int tag )
{
    NVMainRequest *tagRequest = new NVMainRequest( );

    tagRequest->type = READ;
    tagRequest->issueCycle = GetEventQueue()->GetCurrentCycle();
    tagRequest->address = triggerRequest->address;
    tagRequest->tag = tag;
    tagRequest->owner = this;

    /* The reqInfo pointer will point to the original request from cache. */
    tagRequest->reqInfo = static_cast<void *>( triggerRequest );

    return tagRequest;
}

NVMainRequest *LH_Cache::MakeDRCRequest( NVMainRequest *triggerRequest)
{
    /* Retreive the original request. */
    NVMainRequest *drcRequest = static_cast<NVMainRequest *>(triggerRequest->reqInfo);

    drcRequest->tag = DRC_ACCESS;

    /* Set the request as issued now. */
    drcRequest->issueCycle = GetEventQueue()->GetCurrentCycle();

    return drcRequest;
}

NVMainRequest *LH_Cache::MakeTagWriteRequest( NVMainRequest *triggerRequest )
{
    NVMainRequest *tagRequest = new NVMainRequest( );

    tagRequest->type = WRITE;
    tagRequest->issueCycle = GetEventQueue()->GetCurrentCycle();
    tagRequest->address = triggerRequest->address;
    tagRequest->owner = this;

    /* The reqInfo pointer will point to the original request from cache. */
    tagRequest->reqInfo = static_cast<void *>( triggerRequest );

    return tagRequest;
}

bool LH_Cache::IssueDRCCommands( NVMainRequest *req )
{
    bool rv = false;
    uint64_t rank, bank, row, subarray;
    ncounter_t queueId = GetCommandQueueId( req->address );

    req->address.GetTranslatedAddress( &row, NULL, &bank, &rank, NULL, &subarray );

    if( !activateQueued[rank][bank] && commandQueues[queueId].empty() )
    {
        /* Any activate will request the starvation counter */
        starvationCounter[rank][bank] = 0;
        activateQueued[rank][bank] = true;
        effectiveRow[rank][bank][subarray] = row;

        req->issueCycle = GetEventQueue()->GetCurrentCycle();

        commandQueues[queueId].push_back( MakeActivateRequest( req ) );
        commandQueues[queueId].push_back( MakeTagRequest( req, DRC_TAGREAD1 ) );
        commandQueues[queueId].push_back( MakeTagRequest( req, DRC_TAGREAD2 ) );
        commandQueues[queueId].push_back( MakeTagRequest( req, DRC_TAGREAD3 ) );
        bankLocked[rank][bank] = true;

        rv = true;
    }
    else if( activateQueued[rank][bank] && effectiveRow[rank][bank][subarray] != row && commandQueues[queueId].empty() )
    {
        /* Any activate will request the starvation counter */
        starvationCounter[rank][bank] = 0;
        activateQueued[rank][bank] = true;
        effectiveRow[rank][bank][subarray] = row;

        req->issueCycle = GetEventQueue()->GetCurrentCycle();

        commandQueues[queueId].push_back( MakePrechargeRequest( req ) );
        commandQueues[queueId].push_back( MakeActivateRequest( req ) );
        commandQueues[queueId].push_back( MakeTagRequest( req, DRC_TAGREAD1 ) );
        commandQueues[queueId].push_back( MakeTagRequest( req, DRC_TAGREAD2 ) );
        commandQueues[queueId].push_back( MakeTagRequest( req, DRC_TAGREAD3 ) );
        bankLocked[rank][bank] = true;

        rv = true;
    }
    else if( activateQueued[rank][bank] && effectiveRow[rank][bank][subarray] == row )
    {
        starvationCounter[rank][bank]++;

        req->issueCycle = GetEventQueue()->GetCurrentCycle();

        commandQueues[queueId].push_back( MakeTagRequest( req, DRC_TAGREAD1 ) );
        commandQueues[queueId].push_back( MakeTagRequest( req, DRC_TAGREAD2 ) );
        commandQueues[queueId].push_back( MakeTagRequest( req, DRC_TAGREAD3 ) );
        bankLocked[rank][bank] = true;

        rv = true;
    }
    else
    {
        rv = false;
    }

    return rv;
}

bool LH_Cache::IssueFillCommands( NVMainRequest *req )
{
    bool rv = false;
    uint64_t rank, bank, row, subarray;
    ncounter_t queueId = GetCommandQueueId( req->address );

    req->address.GetTranslatedAddress( &row, NULL, &bank, &rank, NULL, &subarray );

    if( !activateQueued[rank][bank] && commandQueues[queueId].empty() )
    {
        /* Any activate will request the starvation counter */
        starvationCounter[rank][bank] = 0;
        activateQueued[rank][bank] = true;
        effectiveRow[rank][bank][subarray] = row;

        req->issueCycle = GetEventQueue()->GetCurrentCycle();

        commandQueues[queueId].push_back( MakeActivateRequest( req ) );
        commandQueues[queueId].push_back( MakeTagWriteRequest( req ) );
        commandQueues[queueId].push_back( req );

        rv = true;
    }
    else if( activateQueued[rank][bank] && effectiveRow[rank][bank][subarray] != row 
            && commandQueues[queueId].empty() )
    {
        /* Any activate will request the starvation counter */
        starvationCounter[rank][bank] = 0;
        activateQueued[rank][bank] = true;
        effectiveRow[rank][bank][subarray] = row;

        req->issueCycle = GetEventQueue()->GetCurrentCycle();

        commandQueues[queueId].push_back( MakePrechargeRequest( req ) );
        commandQueues[queueId].push_back( MakeActivateRequest( req ) );
        commandQueues[queueId].push_back( MakeTagWriteRequest( req ) );
        commandQueues[queueId].push_back( req );

        rv = true;
    }
    else if( activateQueued[rank][bank] && effectiveRow[rank][bank][subarray] == row )
    {
        starvationCounter[rank][bank]++;

        req->issueCycle = GetEventQueue()->GetCurrentCycle();

        commandQueues[queueId].push_back( MakeTagWriteRequest( req ) );
        commandQueues[queueId].push_back( req );

        rv = true;
    }
    else
    {
        rv = false;
    }

    return rv;
}

void LH_Cache::CalculateStats( )
{
    MemoryController::CalculateStats( );
}
