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
*   Tao Zhang       ( Email: tzz106 at cse dot psu dot edu
*                     Website: http://www.cse.psu.edu/~tzz106 )
*******************************************************************************/

#include "MemControl/FRFCFS-WQF/FRFCFS-WQF.h"
#include "src/EventQueue.h"

#include <cassert>

using namespace NVM;

/*
 * This memory controller implements a 'dumb' first-ready first-come first-serve 
 * controller *  with write queue. The write queue drain policy is 'dumb' in 
 * the sense that it only starts a drain when the write queue is completely full
 * and drains until empty.
 */
FRFCFS_WQF::FRFCFS_WQF( ) : readQueueId(0), writeQueueId(1)
{
    std::cout << "Created a First Ready First Come First Serve memory \
        controller with write queue!" << std::endl;

    InitQueues( 2 );

    readQueue = &(transactionQueues[readQueueId]);
    writeQueue = &(transactionQueues[writeQueueId]);

    /* Memory controller options. */
    readQueueSize = 32;
    writeQueueSize = 8;
    starvationThreshold = 4;
    /*
     * initialize the high and low watermark. the high watermark is set the same
     * as size of write queue. the low watermark is set 0 by default.
     */
    HighWaterMark = writeQueueSize;
    LowWaterMark = 0;

    /* Drain control / statistics variables. */
    force_drain = false;
    m_draining = false;
    m_drain_start_cycle = 0;
    m_drain_end_cycle = 0;
    m_last_drain_end_cycle = 0;
    m_drain_start_readqueue_size = 0;
    m_drain_end_readqueue_size = 0;
    m_request_per_drain = 0;

    /* Memory controller statistics. */
    averageLatency = 0.0f;
    averageQueueLatency = 0.0f;
    averageTotalLatency = 0.0f;
    measuredLatencies = 0;
    measuredQueueLatencies = 0;
    measuredTotalLatencies = 0;
    starvation_precharges = 0;

    mem_reads = 0;
    mem_writes = 0;
    rq_rb_hits = 0;
    rq_rb_miss = 0;
    wq_rb_hits = 0;
    wq_rb_miss = 0;

    total_drains = 0;
    total_drain_writes = 0;
    average_writes_per_drain = 0.0;
    minimum_drain_writes = 1e10;
    maximum_drain_writes = 0;

    total_drain_cycles = 0;
    average_drain_cycles = 0.0;
    maximum_drain_cycles = 0;
    minimum_drain_cycles = 1e10;

    total_non_drain_cycles = 0;
    average_drain_spacing = 0.0;
    minimum_drain_spacing = 1e10;
    maximum_drain_spacing = 0;

    total_read_cycles = 0;
    average_read_spacing = 0.0;
    minimum_read_spacing = 1e10;
    maximum_read_spacing = 0;

    total_readqueue_size = 0;
    average_predrain_readqueue_size = 0.0;
    minimum_predrain_readqueue_size = 1e10;
    maximum_predrain_readqueue_size = 0;

    total_reads_during_drain = 0;
    average_reads_during_drain = 0.0;
    minimum_reads_during_drain = 1e10;
    maximum_reads_during_drain = 0;
}

FRFCFS_WQF::~FRFCFS_WQF( )
{
}

void FRFCFS_WQF::SetConfig( Config *conf, bool createChildren )
{
    if( conf->KeyExists( "StarvationThreshold" ) )
        starvationThreshold = static_cast<unsigned int>( 
                conf->GetValue( "StarvationThreshold" ) );

    if( conf->KeyExists( "ReadQueueSize" ) )
        readQueueSize = static_cast<unsigned int>( 
              conf->GetValue( "ReadQueueSize" ) );

    if( conf->KeyExists( "WriteQueueSize" ) )
        writeQueueSize = static_cast<unsigned int>( 
                conf->GetValue( "WriteQueueSize" ) );

    /*
     * set low and high watermark for the write drain. the write drain will
     * start once the number of bufferred write reaches the high watermark
     * "HighWaterMark". the write drain will stop as the number of write is
     * lower than the low watermark "LowWaterMark"
     */
    if( conf->KeyExists( "HighWaterMark" ) )
        HighWaterMark = static_cast<unsigned int>( 
                conf->GetValue( "HighWaterMark" ) );

    if( conf->KeyExists( "LowWaterMark" ) )
        LowWaterMark = static_cast<unsigned int>( 
                conf->GetValue( "LowWaterMark" ) );

    /* sanity check */
    if( HighWaterMark > writeQueueSize )
    {
        HighWaterMark = writeQueueSize;
        std::cout << "NVMain Warning: high watermark can NOT be larger than write \
            queue size. Has reset it to equal." << std::endl;
    }
    else if( LowWaterMark > HighWaterMark )
    {
        LowWaterMark = 0;
        std::cout << "NVMain Warning: low watermark can NOT be larger than high \
            watermark. Has reset it to 0." << std::endl;
    }

    MemoryController::SetConfig( conf, createChildren );

    SetDebugName( "FRFCFS-WQF", conf );
}

void FRFCFS_WQF::RegisterStats( )
{
    AddStat(mem_reads);
    AddStat(mem_writes);
    AddStat(rq_rb_hits);
    AddStat(rq_rb_miss);
    AddStat(wq_rb_hits);
    AddStat(wq_rb_miss);

    AddStat(total_drains);
    AddStat(total_drain_writes);
    AddStat(average_writes_per_drain);
    AddStat(minimum_drain_writes);
    AddStat(maximum_drain_writes);

    AddStat(total_drain_cycles);
    AddStat(average_drain_cycles);
    AddStat(minimum_drain_cycles);
    AddStat(maximum_drain_cycles);

    AddStat(total_non_drain_cycles);
    AddStat(average_drain_spacing);
    AddStat(minimum_drain_spacing);
    AddStat(maximum_drain_spacing);

    AddStat(total_read_cycles);
    AddStat(average_read_spacing);
    AddStat(minimum_read_spacing);
    AddStat(maximum_read_spacing);

    AddStat(total_readqueue_size);
    AddStat(average_predrain_readqueue_size);
    AddStat(minimum_predrain_readqueue_size);
    AddStat(maximum_predrain_readqueue_size);

    AddStat(total_reads_during_drain);
    AddStat(average_reads_during_drain);
    AddStat(minimum_reads_during_drain);
    AddStat(maximum_reads_during_drain);

    AddStat(starvation_precharges);
    AddStat(averageLatency);
    AddStat(averageQueueLatency);
    AddStat(averageTotalLatency);
    AddStat(measuredLatencies);
    AddStat(measuredQueueLatencies);
    AddStat(measuredTotalLatencies);

    MemoryController::RegisterStats( );
}

bool FRFCFS_WQF::IsIssuable( NVMainRequest *request, FailReason * /*fail*/ )
{
    bool rv = true;

    /* during a write drain, no write can enqueue */
    if( (request->type == READ  && readQueue->size()  >= readQueueSize) 
            || (request->type == WRITE && ( writeQueue->size() >= writeQueueSize 
                    || m_draining == true || force_drain == true ) ) )
    {
        rv = false;
    }

    return rv;
}

bool FRFCFS_WQF::IssueCommand( NVMainRequest *request )
{
    /* during a write drain, no write can enqueue */
    if( !IsIssuable( request ) )
    {
        return false;
    }

    request->arrivalCycle = GetEventQueue()->GetCurrentCycle();

    if( request->type == READ )
    {
        Enqueue( readQueueId, request );

        mem_reads++;
    }
    else if( request->type == WRITE )
    {
        Enqueue( writeQueueId, request );

        mem_writes++;
    }
    else
    {
        return false;
    }

    return true;
}

bool FRFCFS_WQF::RequestComplete( NVMainRequest * request )
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
            Prequeue( writeQueueId, request );

            return true;
        }
    }

    /* 
     * Only reads and writes are sent back to NVMain and checked for in the 
     * transaction queue 
     */
    if( request->type == READ 
        || request->type == READ_PRECHARGE 
        || request->type == WRITE 
        || request->type == WRITE_PRECHARGE )
    {
        /* this isn't really used anymore, but doesn't hurt */
        request->status = MEM_REQUEST_COMPLETE; 
        request->completionCycle = GetEventQueue()->GetCurrentCycle();

        /* 
         * Update the average latencies based on this request for READ/WRITE 
         * only 
         */
        averageLatency = ((averageLatency 
                           * static_cast<double>(measuredLatencies))
                           + static_cast<double>(request->completionCycle)
                           - static_cast<double>(request->issueCycle))
                       / static_cast<double>(measuredLatencies+1);
        measuredLatencies += 1;

        averageQueueLatency = ((averageQueueLatency 
                                * static_cast<double>(measuredQueueLatencies))
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

void FRFCFS_WQF::Cycle( ncycle_t steps )
{
    /* check whether it is the time to switch from read to write drain */
    if( m_draining == false && writeQueue->size() >= HighWaterMark && force_drain == false )
    {
        /* record the drain start cycle */
        m_drain_start_cycle = GetEventQueue()->GetCurrentCycle();
        m_drain_start_readqueue_size = readQueue->size();

        /* switch to write drain */
        m_draining = true;
    }
    /* or, if the write drain has completed */
    else if( m_draining == true && writeQueue->size() <= LowWaterMark && force_drain == false )
    {
        /* record the drain end cycle */
        m_drain_end_cycle = GetEventQueue()->GetCurrentCycle();

        /* drop the first drain since gem5 may have fast-forwarding */
        if( m_last_drain_end_cycle != 0 )
        { 
            /* calculate the number of enqueuing reads during a drain */
            m_drain_end_readqueue_size = readQueue->size();
            uint64_t tmpEnterReadQueue = m_drain_end_readqueue_size 
                                            - m_drain_start_readqueue_size;

            total_readqueue_size += m_drain_start_readqueue_size;
            total_reads_during_drain += tmpEnterReadQueue;
            
            /* selectively record the max and min read queue size */
            if( minimum_predrain_readqueue_size > m_drain_start_readqueue_size )
                minimum_predrain_readqueue_size = m_drain_start_readqueue_size;
            else if( maximum_predrain_readqueue_size < m_drain_start_readqueue_size )
                maximum_predrain_readqueue_size = m_drain_start_readqueue_size;

            /* selectively record the max and min enter read queue number */
            if( minimum_reads_during_drain > tmpEnterReadQueue )
                minimum_reads_during_drain = tmpEnterReadQueue;
            else if( maximum_reads_during_drain < tmpEnterReadQueue )
                maximum_reads_during_drain = tmpEnterReadQueue;

            /* calculate the drain duration */
            uint64_t tmpDuration = m_drain_end_cycle - m_drain_start_cycle;
            uint64_t tmpInterval = m_drain_end_cycle - m_last_drain_end_cycle;

            total_drain_cycles += tmpDuration;
            /* selectively record the max and min drain duration time */
            if( maximum_drain_cycles < tmpDuration )
                maximum_drain_cycles = tmpDuration;
            else if( minimum_drain_cycles > tmpDuration )
                minimum_drain_cycles = tmpDuration;

            /* 
             * increment the drain numbers and the total number of drained 
             * write requests 
             */
            total_drains++;
            total_drain_writes += m_request_per_drain;

            /* 
             * selectively record the max and min write request number during 
             * a drain 
             */
            if( minimum_drain_writes > m_request_per_drain )
                minimum_drain_writes = m_request_per_drain;
            else if( maximum_drain_writes < m_request_per_drain )
                maximum_drain_writes = m_request_per_drain;

            /* calculate the interval between two write drains */
            total_non_drain_cycles += tmpInterval;

            /* selectively record the max and min write drain interval */
            if( minimum_drain_spacing > tmpInterval )
                minimum_drain_spacing = tmpInterval;
            else if( maximum_drain_spacing < tmpInterval )
                maximum_drain_spacing = tmpInterval;

            /* calculate the read duration */
            uint64_t tmpReadDuration = tmpInterval - tmpDuration;
            total_read_cycles += tmpReadDuration;

            /* selectively record the max and min read duration */
            if( minimum_read_spacing > tmpReadDuration )
                minimum_read_spacing = tmpReadDuration;
            else if( maximum_read_spacing < tmpReadDuration )
                maximum_read_spacing = tmpReadDuration;
        }
        /* reset the counter */
        m_request_per_drain = 0;

        /* record the last drain end cycle */
        m_last_drain_end_cycle = m_drain_end_cycle;

        /* switch to read */
        m_draining = false;
    }

    /*
     *  Our scheduling algorithm for both the read and write queue is the same:
     *
     *  1) Issue any starved requests
     *  2) Issue row-buffer hits
     *  3) Issue any ready command
     *
     *  First we check the write queue then the read queue. The predicate returns
     *  true if we are draining. If we aren't draining the functions will return
     *  false always, and thus nothing from the write queue will be scheduled.
     */
    
    NVMainRequest *nextRequest = NULL;

    /* if we are draining the write, only write queue is checked */
    if( m_draining == true || (force_drain == true && readQueue->size() == 0) )
    {
        if( FindStarvedRequest( *writeQueue, &nextRequest ) )
        {
            wq_rb_miss++;
            starvation_precharges++;
            m_request_per_drain++;
        }
        else if( FindRowBufferHit( *writeQueue, &nextRequest ) )
        {
            wq_rb_hits++;
            m_request_per_drain++;
        }
        else if( FindCachedAddress( *writeQueue, &nextRequest ) )
        {
        }
        else if( FindOldestReadyRequest( *writeQueue, &nextRequest ) )
        {
            wq_rb_miss++;
            m_request_per_drain++;
        }
        else if( FindClosedBankRequest( *writeQueue, &nextRequest ) )
        {
            wq_rb_miss++;
            m_request_per_drain++;
        }
    }
    /* else, only read queue is checked */
    else
    {
        if( FindStarvedRequest( *readQueue, &nextRequest ) )
        {
            rq_rb_miss++;
            starvation_precharges++;
        }
        else if( FindRowBufferHit( *readQueue, &nextRequest ) )
        {
            rq_rb_hits++;
        }
        else if( FindCachedAddress( *readQueue, &nextRequest ) )
        {
        }
        else if( FindOldestReadyRequest( *readQueue, &nextRequest ) )
        {
            rq_rb_miss++;
        }
        else if( FindClosedBankRequest( *readQueue, &nextRequest ) )
        {
            rq_rb_miss++;
        }
    }

    /* Issue the memory transaction as a series of commands to the command queue. */
    if( nextRequest != NULL )
    {
        /* If we are draining, do not allow write cancellation or pausing. */
        if( m_draining == true || force_drain == true )
            nextRequest->flags |= NVMainRequest::FLAG_FORCED;

        IssueMemoryCommands( nextRequest );
    }

    /* Issue memory commands from the command queue. */
    CycleCommandQueues( );

    MemoryController::Cycle( steps );
}

void FRFCFS_WQF::CalculateStats( )
{
    if( total_drains > 0 )
    {
        average_writes_per_drain = static_cast<double>(total_drain_writes) / static_cast<double>(total_drains);
        average_drain_cycles = static_cast<double>(total_drain_cycles) / static_cast<double>(total_drains);
        average_drain_spacing = static_cast<double>(total_non_drain_cycles) / static_cast<double>(total_drains);
        average_read_spacing = static_cast<double>(total_read_cycles) / static_cast<double>(total_drains);
        average_predrain_readqueue_size = static_cast<double>(total_readqueue_size) / static_cast<double>(total_drains);
        average_reads_during_drain = static_cast<double>(total_reads_during_drain) / static_cast<double>(total_drains);
    }
    else
    {
        average_writes_per_drain = 0.0;
        average_drain_cycles = 0.0;
        average_drain_spacing = 0.0;
        average_read_spacing = 0.0;
        average_predrain_readqueue_size = 0.0;
        average_reads_during_drain = 0.0;
    }

    MemoryController::CalculateStats( );
}

bool FRFCFS_WQF::Drain( )
{
    force_drain = true;

    return true;
}

