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


#include "MemControl/LO-Cache/LO-Cache.h"
#include "include/NVMHelpers.h"
#include "NVM/nvmain.h"
#include "src/EventQueue.h"

#include <iostream>
#include <sstream>
#include <cstring>
#include <cassert>


using namespace NVM;


LO_Cache::LO_Cache( )
{
    std::cout << "Created a Latency Optimized DRAM Cache!" << std::endl;

    drcQueueSize = 32;
    starvationThreshold = 4;

    mainMemory = NULL;
    mainMemoryConfig = NULL;
    functionalCache = NULL;

    drc_hits = 0;
    drc_miss = 0;
    drc_fills = 0;
    drc_evicts = 0;
    drc_dirty_evicts = 0;

    rb_hits = 0;
    rb_miss = 0;
    starvation_precharges = 0;

    perfectFills = false;
    max_addr = 0;

    psInterval = 0;

    /*
    *  Queue options: One queue for all requests 
    *  or a second queue for fill/write requests
    */
    InitQueues( 1 );

    drcQueue = &(transactionQueues[0]);
}

LO_Cache::~LO_Cache( )
{

}

void LO_Cache::SetConfig( Config *conf, bool createChildren )
{
    ncounter_t rows, cols, word_size, lines;

    if( conf->KeyExists( "StarvationThreshold" ) )
        starvationThreshold = static_cast<ncounter_t>( conf->GetValue( "StarvationThreshold" ) );
    if( conf->KeyExists( "DRCQueueSize" ) )
        drcQueueSize = static_cast<ncounter_t>( conf->GetValue( "DRCQueueSize" ) );

    if( conf->KeyExists( "PerfectFills" ) && conf->GetString( "PerfectFills" ) == "true" )
        perfectFills = true;


    ranks = static_cast<ncounter_t>( conf->GetValue( "RANKS" ) );
    banks = static_cast<ncounter_t>( conf->GetValue( "BANKS" ) );
    rows  = static_cast<ncounter_t>( conf->GetValue( "ROWS" ) );
    cols  = static_cast<ncounter_t>( conf->GetValue( "COLS" ) );

    /*
     * Calculate the memory word size under the default burst length.
     * This will be the width of the bus multiplied by the number of
     * beats (cycles * data rate) a request of the default bust length
     * requires. Size BusWidth is in bits, divide by at after for bytes.
     */
    word_size = static_cast<ncounter_t>( conf->GetValue( "BusWidth" ) )
              * static_cast<ncounter_t>( conf->GetValue( "RATE" ) )
              * static_cast<ncounter_t>( conf->GetValue( "tBURST" ) );
    word_size /= 8;


    functionalCache = new CacheBank**[ranks];
    for( ncounter_t i = 0; i < ranks; i++ )
    {
        functionalCache[i] = new CacheBank*[banks];

        for( ncounter_t j = 0; j < banks; j++ )
        {
            /*
             *  The number of cache lines per row depends on the number
             *  of columns: N = (cols word_size) / (8 tag bytes + 64 cache line bytes)
             *  The LO-Cache has the data tag (8 bytes) along with 64
             *  bytes for the cache line. The cache is direct mapped,
             *  so we will have up to N cache lines + tags per row,
             *  an assoc of 1, and cache line size of 64 bytes.
             */
            lines = (cols * word_size) / 72;
            functionalCache[i][j] = new CacheBank( rows, lines, 1, 64 );
        }
    }

    MemoryController::SetConfig( conf, createChildren );

    SetDebugName( "LO-Cache", conf );
}

void LO_Cache::RegisterStats( )
{
    AddStat(drc_hits);
    AddStat(drc_miss);
    AddStat(drc_hitrate);
    AddStat(drc_fills);
    AddStat(drc_evicts);
    AddStat(drc_dirty_evicts);
    AddStat(rb_hits);
    AddStat(rb_miss);
    AddStat(starvation_precharges);

    MemoryController::RegisterStats( );
}

void LO_Cache::SetMainMemory( NVMain *mm )
{
    mainMemory = mm;
}

bool LO_Cache::IssueAtomic( NVMainRequest *req )
{
    uint64_t row, bank, rank, subarray;

    req->address.GetTranslatedAddress( &row, NULL, &bank, &rank, NULL, &subarray );

    if( req->address.GetPhysicalAddress() > max_addr ) max_addr = req->address.GetPhysicalAddress( );

    /*
    *  Check for a hit for statistical purposes first.
    */
    if( req->type == WRITE || req->type == WRITE_PRECHARGE || functionalCache[rank][bank]->Present( req->address ) )
    {
        drc_hits++;
    }
    else
    {
        /*
         *  Simply install this cache line, evicting another cache line 
         *  if needed.
         */
        NVMDataBlock dummy;

        if( functionalCache[rank][bank]->SetFull( req->address ) )
        {
            NVMAddress victim;

            (void)functionalCache[rank][bank]->ChooseVictim( req->address, &victim );
            (void)functionalCache[rank][bank]->Evict( victim, &dummy );

            drc_evicts++;
        }

        drc_miss++;
        drc_fills++;

        (void)functionalCache[rank][bank]->Install( req->address, dummy );
    }

    return true;
}

bool LO_Cache::IssueFunctional( NVMainRequest *req )
{
    uint64_t bank, rank;

    req->address.GetTranslatedAddress( NULL, NULL, &bank, &rank, NULL, NULL );

    /* Write always hits. */
    if( req->type == WRITE || req->type == WRITE_PRECHARGE )
        return true;

    /* Reads hit if they are in the cache. */
    return functionalCache[rank][bank]->Present( req->address );
}

bool LO_Cache::IsIssuable( NVMainRequest * /*request*/, FailReason * /*fail*/ )
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

bool LO_Cache::IssueCommand( NVMainRequest *req )
{
    bool rv = true;

    if( req->address.GetPhysicalAddress() > max_addr ) max_addr = req->address.GetPhysicalAddress( );

    if( perfectFills && (req->type == WRITE || req->type == WRITE_PRECHARGE) )
    {
        uint64_t rank, bank;
        NVMDataBlock dummy;

        req->address.GetTranslatedAddress( NULL, NULL, &bank, &rank, NULL, NULL );

        if( functionalCache[rank][bank]->SetFull( req->address ) )
        {
            NVMAddress victim;

            (void)functionalCache[rank][bank]->ChooseVictim( req->address, &victim );
            (void)functionalCache[rank][bank]->Evict( victim, &dummy );

            drc_evicts++;
        }

        (void)functionalCache[rank][bank]->Install( req->address, dummy );

        drc_fills++;

        GetEventQueue()->InsertEvent( EventResponse, this, req, 
                                      GetEventQueue()->GetCurrentCycle()+1 );
    }
    else
    {
        Enqueue( 0, req );
    }
    
    return rv;
}

bool LO_Cache::RequestComplete( NVMainRequest *req )
{
    bool rv = false;

    if( req->type == REFRESH )
    {
        ProcessRefreshPulse( req );
    }
    else if( req->owner == this )
    {
        if( req->tag == DRC_FILL )
        {
            /* Install the missed request */
            uint64_t rank, bank;
            NVMDataBlock vicData;
            bool dirtyEvict = false;
            NVMAddress victim;

            req->address.GetTranslatedAddress( NULL, NULL, &bank, &rank, NULL, NULL );

            if( functionalCache[rank][bank]->SetFull( req->address )
                && !functionalCache[rank][bank]->Present( req->address ) )
            {
                (void)functionalCache[rank][bank]->ChooseVictim( req->address, &victim );
                dirtyEvict = functionalCache[rank][bank]->Evict( victim, &vicData );

                drc_evicts++;
            }

            (void)functionalCache[rank][bank]->Install( req->address, req->data );

            drc_fills++;

            /* If we are replacing a dirty block we need to write back to main
             * memory. We are assuming the DRC read miss contains the data and
             * it is held somewhere while the new cache entry is fetched so
             * that we do not need to re-issue a read request for the data.
             */
            if (dirtyEvict)
            {
                NVMainRequest *memReq = new NVMainRequest( );

                memReq->address = victim;
                memReq->owner = this;
                memReq->tag = DRC_EVICT;
                memReq->type = WRITE;
                memReq->data = vicData;
                memReq->arrivalCycle = GetEventQueue()->GetCurrentCycle();

                if (mainMemory->IsIssuable( memReq, NULL )) {
                    mainMemory->IssueCommand( memReq );
                } else {
                    /* If the request is not issuable to main memory we need to save the request
                     * and issue it later in time, e.g., when a main memory request completes.
                     * Otherwise, this request to main memory would be lost. */
                    mainMemory->EnqueuePendingMemoryRequests( memReq );
                }

                drc_dirty_evicts++;
            }
        }
        /*
         *  Intercept memory read requests from misses to create a fill request.
         */
        else if( req->tag == DRC_MEMREAD )
        {

            /* Issue as a fill request. */
            NVMainRequest *fillReq = new NVMainRequest( );

            *fillReq = *req;
            fillReq->owner = this;
            fillReq->tag = DRC_FILL;
            fillReq->type = WRITE;
            fillReq->arrivalCycle = GetEventQueue()->GetCurrentCycle();

            this->IssueCommand( fillReq );

            /* Find the original request and send back to requestor. */
            assert( outstandingFills.count( req ) > 0 );
            NVMainRequest *originalReq = outstandingFills[req];
            outstandingFills.erase( req );

            GetParent( )->RequestComplete( originalReq );
            rv = false;
        }

        /* All other tag types (e.g., DRC_EVICT) only need to free memory
         * used for the request and nothing else.
         */
        delete req;
        rv = true;
    }
    /*
     *  Intercept read and write requests from parent modules
     */
    else
    {
        uint64_t rank, bank;

        req->address.GetTranslatedAddress( NULL, NULL, &bank, &rank, NULL, NULL );

        if( req->type == WRITE || req->type == WRITE_PRECHARGE )
        {
            /*
             *  LOCache has no associativity -- Just replace whatever is in the set.
             */
            NVMDataBlock vicData;
            bool dirtyEvict = false;
            NVMAddress victim;

            if( functionalCache[rank][bank]->SetFull( req->address )
                && !functionalCache[rank][bank]->Present( req->address ) )
            {
                (void)functionalCache[rank][bank]->ChooseVictim( req->address, &victim );
                dirtyEvict = functionalCache[rank][bank]->Evict( victim, &vicData );

                drc_evicts++;
            }

            (void)functionalCache[rank][bank]->Install( req->address, req->data );

            /* Send back to requestor. */
            GetParent( )->RequestComplete( req );
            rv = false;

            /* If we are replacing a dirty block we need to write back to main
             * memory. We are assuming the DRC read miss contains the data and
             * it is held somewhere while the new cache entry is fetched so
             * that we do not need to re-issue a read request for the data.
             */
            if (dirtyEvict)
            {
                NVMainRequest *memReq = new NVMainRequest( );

                memReq->address = victim;
                memReq->owner = this;
                memReq->tag = DRC_EVICT;
                memReq->type = WRITE;
                memReq->data = vicData;
                memReq->arrivalCycle = GetEventQueue()->GetCurrentCycle();

                if (mainMemory->IsIssuable( memReq, NULL )) {
                    mainMemory->IssueCommand( memReq );
                } else {
                    /* If the request is not issuable to main memory we need to save the request
                     * and issue it later in time, e.g., when a main memory request completes.
                     * Otherwise, this request to main memory would be lost. */
                    mainMemory->EnqueuePendingMemoryRequests( memReq );
                }

                drc_dirty_evicts++;
            }
        }
        else if( req->type == READ || req->type == READ_PRECHARGE )
        {
            uint64_t rank, bank;

            req->address.GetTranslatedAddress( NULL, NULL, &bank, &rank, NULL, NULL );

            /* Check for a hit. */
            bool hit = functionalCache[rank][bank]->Present( req->address );

            /* On a miss, send to main memory. */
            if( !hit )
            {
                /* Issue as a fill request. */
                NVMainRequest *memReq = new NVMainRequest( );

                *memReq = *req;
                memReq->owner = this;
                memReq->tag = DRC_MEMREAD;
                memReq->type = READ;
                memReq->arrivalCycle = GetEventQueue()->GetCurrentCycle();

                assert( outstandingFills.count( req ) == 0 );
                outstandingFills.insert( std::pair<NVMainRequest*, NVMainRequest*>( memReq, req ) );

                if (mainMemory->IsIssuable( memReq, NULL )) {
                    mainMemory->IssueCommand( memReq );
                } else {
                    /* If the request is not issuable to main memory we need to save the request
                     * and issue it later in time, e.g., when a main memory request completes.
                     * Otherwise, this request to main memory would be lost. */
                    mainMemory->EnqueuePendingMemoryRequests( memReq );
                }

                drc_miss++;
            }
            else
            {
                /* Send back to requestor. */
                GetParent( )->RequestComplete( req );
                rv = false;

                drc_hits++;
            }
        }
    }

    return rv;
}

void LO_Cache::Cycle( ncycle_t steps )
{
    NVMainRequest *nextRequest = NULL;

    /* Check for starved requests BEFORE row buffer hits. */
    if( FindStarvedRequest( *drcQueue, &nextRequest ) )
    {
        rb_miss++;
        starvation_precharges++;
    }
    /* Check for row buffer hits. */
    else if( FindRowBufferHit( *drcQueue, &nextRequest) )
    {
        rb_hits++;
    }
    /* Find the oldest request that can be issued. */
    else if( FindOldestReadyRequest( *drcQueue, &nextRequest ) )
    {
        rb_miss++;
    }
    /* Find requests to a bank that is closed. */
    else if( FindClosedBankRequest( *drcQueue, &nextRequest ) )
    {
        rb_miss++;
    }
    else
    {
        nextRequest = NULL;
    }

    /* Issue the commands for this transaction. */
    if( nextRequest != NULL )
    {
        IssueMemoryCommands( nextRequest );
    }

    /* Issue any commands in the command queues. */
    CycleCommandQueues( );

    MemoryController::Cycle( steps );
}

void LO_Cache::CalculateStats( )
{
    drc_hitrate = 0.0;
    if( drc_hits+drc_miss > 0 )
        drc_hitrate = static_cast<float>(drc_hits) / static_cast<float>(drc_miss+drc_hits);

    MemoryController::CalculateStats( );
}

void LO_Cache::CreateCheckpoint( std::string dir )
{
    /* Use our statName as the file to write in the checkpoint directory. */
    for( ncounter_t rankIdx = 0; rankIdx < ranks; rankIdx++ )
    {
        for( ncounter_t bankIdx = 0; bankIdx < banks; bankIdx++ )
        {
            std::stringstream cpt_file;
            cpt_file.str("");
            cpt_file << dir << "/" << statName << "_r";
            cpt_file << rankIdx << "_b" << bankIdx;

            std::ofstream cpt_handle;
            
            cpt_handle.open( cpt_file.str().c_str(), std::ofstream::out | std::ofstream::trunc | std::ofstream::binary );

            if( !cpt_handle.is_open( ) )
            {
                std::cout << "LO_Cache: Warning: Could not open checkpoint file: " << cpt_file.str() << "!" << std::endl;
            }
            else
            {
                /* Iterate over cache sets, since they may not be allocated contiguously. */
                for( uint64_t set = 0; set < functionalCache[rankIdx][bankIdx]->numSets; set++ )
                {
                    cpt_handle.write( (const char *)(functionalCache[rankIdx][bankIdx]->cacheEntry[set]), 
                                      sizeof(CacheEntry)*functionalCache[rankIdx][bankIdx]->numAssoc );
                }

                cpt_handle.close( );
            }

            /* Write checkpoint information. */
            /* Note: For future compatability only at the memory. This is not read during restoration. */
            std::string cpt_info = cpt_file.str() + ".json";

            cpt_handle.open( cpt_info.c_str(), std::ofstream::out | std::ofstream::trunc | std::ofstream::binary );

            if( !cpt_handle.is_open() )
            {
                std::cout << "LO_Cache: Warning: Could not open checkpoint info file: " << cpt_info << "!" << std::endl;
            }
            else
            {
                std::string cpt_info_str = "{\n\t\"Version\": 1\n}";
                cpt_handle.write( cpt_info_str.c_str(), cpt_info_str.length() ); 

                cpt_handle.close();
            }
        }
    }

    NVMObject::CreateCheckpoint( dir );
}

void LO_Cache::RestoreCheckpoint( std::string dir )
{
    for( ncounter_t rankIdx = 0; rankIdx < ranks; rankIdx++ )
    {
        for( ncounter_t bankIdx = 0; bankIdx < banks; bankIdx++ )
        {
            std::stringstream cpt_file;
            cpt_file.str("");
            cpt_file << dir << "/" << statName << "_r";
            cpt_file << rankIdx << "_b" << bankIdx;

            std::ifstream cpt_handle;

            cpt_handle.open( cpt_file.str().c_str(), std::ifstream::ate | std::ifstream::binary );

            if( !cpt_handle.is_open( ) )
            {
                std::cout << "LO_Cache: Warning: Could not open checkpoint file: " << cpt_file.str() << "!" << std::endl;
            }
            else
            {
                std::streampos expectedSize = sizeof(CacheEntry)*functionalCache[rankIdx][bankIdx]->numSets*functionalCache[rankIdx][bankIdx]->numAssoc;
                if( cpt_handle.tellg( ) != expectedSize )
                {
                    std::cout << "LO_Cache: Warning: Expected checkpoint size differs from DRAM cache configuration. Skipping restore." << std::endl;
                }
                else
                {
                    cpt_handle.close( );

                    cpt_handle.open( cpt_file.str().c_str(), std::ifstream::in | std::ifstream::binary );

                    /* Iterate over cache sets, since they may not be allocated contiguously. */
                    for( uint64_t set = 0; set < functionalCache[rankIdx][bankIdx]->numSets; set++ )
                    {
                        cpt_handle.read( (char *)(functionalCache[rankIdx][bankIdx]->cacheEntry[set]), 
                                          sizeof(CacheEntry)*functionalCache[rankIdx][bankIdx]->numAssoc );
                    }

                    cpt_handle.close( );

                    std::cout << "LO_Cache: Checkpoint read " << (sizeof(CacheEntry)*functionalCache[rankIdx][bankIdx]->numAssoc*functionalCache[rankIdx][bankIdx]->numSets) << " bytes." << std::endl;
                }
            }
        }
    }

    NVMObject::RestoreCheckpoint( dir );
}
