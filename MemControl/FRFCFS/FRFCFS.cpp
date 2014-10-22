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

#include "MemControl/FRFCFS/FRFCFS.h"
#include "src/EventQueue.h"
#include "include/NVMainRequest.h"
#ifndef TRACE
#ifdef GEM5
  #include "SimInterface/Gem5Interface/Gem5Interface.h"
  #include "base/statistics.hh"
  #include "base/types.hh"
  #include "sim/core.hh"
  #include "sim/stat_control.hh"
#endif
#endif
#include <iostream>
#include <set>
#include <assert.h>

using namespace NVM;

FRFCFS::FRFCFS( )
{
    std::cout << "Created a First Ready First Come First Serve memory controller!"
        << std::endl;

    queueSize = 32;
    starvationThreshold = 4;

    averageLatency = 0.0f;
    averageQueueLatency = 0.0f;
    averageTotalLatency = 0.0f;

    measuredLatencies = 0;
    measuredQueueLatencies = 0;
    measuredTotalLatencies = 0;

    mem_reads = 0;
    mem_writes = 0;

    rb_hits = 0;
    rb_miss = 0;

    write_pauses = 0;

    starvation_precharges = 0;

    psInterval = 0;

    InitQueues( 1 );

    memQueue = &(transactionQueues[0]);
}

FRFCFS::~FRFCFS( )
{
    std::cout << "FRFCFS memory controller destroyed. " << memQueue->size( ) 
              << " commands still in memory queue." << std::endl;
}

void FRFCFS::SetConfig( Config *conf, bool createChildren )
{
    if( conf->KeyExists( "StarvationThreshold" ) )
    {
        starvationThreshold = static_cast<unsigned int>( conf->GetValue( "StarvationThreshold" ) );
    }

    if( conf->KeyExists( "QueueSize" ) )
    {
        queueSize = static_cast<unsigned int>( conf->GetValue( "QueueSize" ) );
    }

    MemoryController::SetConfig( conf, createChildren );

    SetDebugName( "FRFCFS", conf );
}

void FRFCFS::RegisterStats( )
{
    AddStat(mem_reads);
    AddStat(mem_writes);
    AddStat(rb_hits);
    AddStat(rb_miss);
    AddStat(starvation_precharges);
    AddStat(averageLatency);
    AddStat(averageQueueLatency);
    AddStat(averageTotalLatency);
    AddStat(measuredLatencies);
    AddStat(measuredQueueLatencies);
    AddStat(measuredTotalLatencies);
    AddStat(write_pauses);

    MemoryController::RegisterStats( );
}

bool FRFCFS::IsIssuable( NVMainRequest * /*request*/, FailReason * /*fail*/ )
{
    bool rv = true;

    /*
     *  Limit the number of commands in the queue. This will stall the caches/CPU.
     */ 
    if( memQueue->size( ) >= queueSize )
    {
        rv = false;
    }

    return rv;
}

/*
 *  This method is called whenever a new transaction from the processor issued to
 *  this memory controller / channel. All scheduling decisions should be made here.
 */
bool FRFCFS::IssueCommand( NVMainRequest *req )
{
    if( !IsIssuable( req ) )
    {
        return false;
    }

    req->arrivalCycle = GetEventQueue()->GetCurrentCycle();

    /* 
     *  Just push back the read/write. It's easier to inject dram commands than break it up
     *  here and attempt to remove them later.
     */
    Enqueue( 0, req );

    if( req->type == READ )
        mem_reads++;
    else
        mem_writes++;

    /*
     *  Return whether the request could be queued. Return false if the queue is full.
     */
    return true;
}

bool FRFCFS::RequestComplete( NVMainRequest * request )
{
    if( request->type == WRITE || request->type == WRITE_PRECHARGE )
    {
        /* 
         *  Put cancelled requests at the head of the write queue
         *  like nothing ever happened.
         */
        if( request->flags & NVMainRequest::FLAG_CANCELLED 
            || request->flags & NVMainRequest::FLAG_PAUSED )
        {
            Prequeue( 0, request );

            return true;
        }
    }

    /* Only reads and writes are sent back to NVMain and checked for in the transaction queue. */
    if( request->type == READ 
        || request->type == READ_PRECHARGE 
        || request->type == WRITE 
        || request->type == WRITE_PRECHARGE )
    {
        request->status = MEM_REQUEST_COMPLETE;
        request->completionCycle = GetEventQueue()->GetCurrentCycle();

        /* Update the average latencies based on this request for READ/WRITE only. */
        averageLatency = ((averageLatency * static_cast<double>(measuredLatencies))
                           + static_cast<double>(request->completionCycle)
                           - static_cast<double>(request->issueCycle))
                       / static_cast<double>(measuredLatencies+1);
        measuredLatencies += 1;

        averageQueueLatency = ((averageQueueLatency * static_cast<double>(measuredQueueLatencies))
                                + static_cast<double>(request->issueCycle)
                                - static_cast<double>(request->arrivalCycle))
                            / static_cast<double>(measuredQueueLatencies+1);
        measuredQueueLatencies += 1;

        averageTotalLatency = ((averageTotalLatency * static_cast<double>(measuredTotalLatencies))
                                + static_cast<double>(request->completionCycle)
                                - static_cast<double>(request->arrivalCycle))
                            / static_cast<double>(measuredTotalLatencies+1);
        measuredTotalLatencies += 1;
    }

    return MemoryController::RequestComplete( request );
}

void FRFCFS::Cycle( ncycle_t steps )
{
    NVMainRequest *nextRequest = NULL;

    /* Check for starved requests BEFORE row buffer hits. */
    if( FindStarvedRequest( *memQueue, &nextRequest ) )
    {
        rb_miss++;
        starvation_precharges++;
    }
    /* Check for row buffer hits. */
    else if( FindRowBufferHit( *memQueue, &nextRequest) )
    {
        rb_hits++;
    }
    /* Check if the address is accessible through any other means. */
    else if( FindCachedAddress( *memQueue, &nextRequest ) )
    {
    }
    else if( FindWriteStalledRead( *memQueue, &nextRequest ) )
    {
        if( nextRequest != NULL )
            write_pauses++;
    }
    /* Find the oldest request that can be issued. */
    else if( FindOldestReadyRequest( *memQueue, &nextRequest ) )
    {
        rb_miss++;
    }
    /* Find requests to a bank that is closed. */
    else if( FindClosedBankRequest( *memQueue, &nextRequest ) )
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

void FRFCFS::CalculateStats( )
{
    MemoryController::CalculateStats( );
}



