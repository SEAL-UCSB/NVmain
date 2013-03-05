/*******************************************************************************
* Copyright (c) 2012-2013, The Microsystems Design Labratory (MDL)
* Department of Computer Science and Engineering, The Pennsylvania State University
* All rights reserved.
* 
* This source code is part of NVMain - A cycle accurate timing, bit accurate
* energy simulator for both volatile (e.g., DRAM) and nono-volatile memory
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
FRFCFS_WQF::FRFCFS_WQF( Interconnect *memory, AddressTranslator *translator )
{
    SetMemory( memory );
    SetTranslator( translator );

    std::cout << "Created a First Ready First Come First Serve memory \
        controller with write queue!" << std::endl;

    /* empty the read/write queue */
    readQueue.clear();
    writeQueue.clear();

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

    /* Memory controller statistics. */
    averageLatency = 0.0f;
    averageQueueLatency = 0.0f;

    rq_rb_hits = 0;
    rq_rb_miss = 0;
    wq_rb_hits = 0;
    wq_rb_miss = 0;
    m_request_per_drain = 0;
    m_min_request_per_drain = 1e10;
    m_max_request_per_drain = 0;
    m_drain_num = 0;
    m_drain_duration = 0;
    m_drain_max_duration = 0;
    m_drain_min_duration = 1e10;
    m_drain_start_cycle = 0;
    m_drain_end_cycle = 0;
    m_last_drain_end_cycle = 0;
    m_drain_request_num = 0;
    m_min_drain_interval = 1e10;
    m_max_drain_interval = 0;
    m_read_duration = 0;
    m_min_read_duration = 1e10;
    m_max_read_duration = 0;
    m_drain_enter_readqueue_num = 0;
    m_min_enter_readqueue_num = 1e10;
    m_max_enter_readqueue_num = 0;
    m_drain_readqueue_size = 0;
    m_drain_start_readqueue_size = 0;
    m_drain_end_readqueue_size = 0;
    m_min_start_readqueue_size = 1e10;
    m_max_start_readqueue_size = 0;
    m_draining = false;

    measuredLatencies = 0;
    measuredQueueLatencies = 0;

    mem_reads = 0;
    mem_writes = 0;

    starvation_precharges = 0;

    psInterval = 0;
}

FRFCFS_WQF::~FRFCFS_WQF( )
{
}

void FRFCFS_WQF::SetConfig( Config *conf )
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

    MemoryController::SetConfig( conf );
}

bool FRFCFS_WQF::QueueFull( NVMainRequest * /*req*/ )
{
    /*
     * So this function is annoying. Ruby/gem5 will ask if the queue is full, but 
     * does not provide any information about the next request which makes it 
     * impossible to determine if the queue the next request will actually go to
     * is full. Therefore we return true if any of the queues are full.
     */
    return ( (readQueue.size() >= readQueueSize) 
            || (writeQueue.size() >= writeQueueSize) );
}

bool FRFCFS_WQF::IssueCommand( NVMainRequest *request )
{
    /* during a write drain, no write can enqueue */
    if( (request->type == READ  && readQueue.size()  >= readQueueSize) 
            || (request->type == WRITE && ( writeQueue.size() >= writeQueueSize 
                    || m_draining == true ) ) )
    {
        return false;
    }

    request->arrivalCycle = GetEventQueue()->GetCurrentCycle();

    if( request->type == READ )
    {
        readQueue.push_back( request );

        mem_reads++;
    }
    else if( request->type == WRITE )
    {
        writeQueue.push_back( request );

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
    /* 
     * Only reads and writes are sent back to NVMain and checked for in the 
     * transaction queue 
     */
    if( request->type == READ || request->type == READ_PRECHARGE 
            || request->type == WRITE || request->type == WRITE_PRECHARGE )
    {
        /* this isn't really used anymore, but doesn't hurt */
        request->status = MEM_REQUEST_COMPLETE; 
        request->completionCycle = GetEventQueue()->GetCurrentCycle();

        /* 
         * Update the average latencies based on this request for READ/WRITE 
         * only 
         */
        averageLatency = ((averageLatency 
                           * static_cast<float>(measuredLatencies))
                           + static_cast<float>(request->completionCycle)
                           - static_cast<float>(request->issueCycle))
                       / static_cast<float>(measuredLatencies+1);
        measuredLatencies += 1;

        averageQueueLatency = ((averageQueueLatency 
                                * static_cast<float>(measuredQueueLatencies))
                                + static_cast<float>(request->issueCycle)
                                - static_cast<float>(request->arrivalCycle))
                            / static_cast<float>(measuredQueueLatencies+1);
        measuredQueueLatencies += 1;
    }

    return MemoryController::RequestComplete( request );
}

void FRFCFS_WQF::Cycle( ncycle_t steps )
{
    /* check whether it is the time to switch from read to write drain */
    if( m_draining == false && writeQueue.size() >= HighWaterMark )
    {
        /* record the drain start cycle */
        m_drain_start_cycle = GetEventQueue()->GetCurrentCycle();
        m_drain_start_readqueue_size = readQueue.size();

        /* switch to write drain */
        m_draining = true;
    }
    /* or, if the write drain has completed */
    else if( m_draining == true && writeQueue.size() <= LowWaterMark )
    {
        /* record the drain end cycle */
        m_drain_end_cycle = GetEventQueue()->GetCurrentCycle();

        /* drop the first drain since gem5 may have fast-forwarding */
        if( m_last_drain_end_cycle != 0 )
        { 
            /* calculate the number of enqueuing reads during a drain */
            m_drain_end_readqueue_size = readQueue.size();
            uint64_t tmpEnterReadQueue = m_drain_end_readqueue_size 
                                            - m_drain_start_readqueue_size;

            m_drain_readqueue_size += m_drain_start_readqueue_size;
            m_drain_enter_readqueue_num += tmpEnterReadQueue;
            
            /* selectively record the max and min read queue size */
            if( m_min_start_readqueue_size > m_drain_start_readqueue_size )
                m_min_start_readqueue_size = m_drain_start_readqueue_size;
            else if( m_max_start_readqueue_size < m_drain_start_readqueue_size )
                m_max_start_readqueue_size = m_drain_start_readqueue_size;

            /* selectively record the max and min enter read queue number */
            if( m_min_enter_readqueue_num > tmpEnterReadQueue )
                m_min_enter_readqueue_num = tmpEnterReadQueue;
            else if( m_max_enter_readqueue_num < tmpEnterReadQueue )
                m_max_enter_readqueue_num = tmpEnterReadQueue;

            /* calculate the drain duration */
            uint64_t tmpDuration = m_drain_end_cycle - m_drain_start_cycle;
            uint64_t tmpInterval = m_drain_end_cycle - m_last_drain_end_cycle;

            m_drain_duration += tmpDuration;
            /* selectively record the max and min drain duration time */
            if( m_drain_max_duration < tmpDuration )
                m_drain_max_duration = tmpDuration;
            else if( m_drain_min_duration > tmpDuration )
                m_drain_min_duration = tmpDuration;

            /* 
             * increment the drain numbers and the total number of drained 
             * write requests 
             */
            m_drain_num++;
            m_drain_request_num += m_request_per_drain;

            /* 
             * selectively record the max and min write request number during 
             * a drain 
             */
            if( m_min_request_per_drain > m_request_per_drain )
                m_min_request_per_drain = m_request_per_drain;
            else if( m_max_request_per_drain < m_request_per_drain )
                m_max_request_per_drain = m_request_per_drain;

            /* calculate the interval between two write drains */
            m_drain_interval += tmpInterval;

            /* selectively record the max and min write drain interval */
            if( m_min_drain_interval > tmpInterval )
                m_min_drain_interval = tmpInterval;
            else if( m_max_drain_interval < tmpInterval )
                m_max_drain_interval = tmpInterval;

            /* calculate the read duration */
            uint64_t tmpReadDuration = tmpInterval - tmpDuration;
            m_read_duration += tmpReadDuration;

            /* selectively record the max and min read duration */
            if( m_min_read_duration > tmpReadDuration )
                m_min_read_duration = tmpReadDuration;
            else if( m_max_read_duration < tmpReadDuration )
                m_max_read_duration = tmpReadDuration;
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
    if( m_draining == true )
    {
        if( FindStarvedRequest( writeQueue, &nextRequest ) )
        {
            wq_rb_miss++;
            starvation_precharges++;
            m_request_per_drain++;
        }
        else if( FindRowBufferHit( writeQueue, &nextRequest ) )
        {
            wq_rb_hits++;
            m_request_per_drain++;
        }
        else if( FindOldestReadyRequest( writeQueue, &nextRequest ) )
        {
            wq_rb_miss++;
            m_request_per_drain++;
        }
        else if( FindClosedBankRequest( writeQueue, &nextRequest ) )
        {
            wq_rb_miss++;
            m_request_per_drain++;
        }
    }
    /* else, only read queue is checked */
    else
    {
        if( FindStarvedRequest( readQueue, &nextRequest ) )
        {
            rq_rb_miss++;
            starvation_precharges++;
        }
        else if( FindRowBufferHit( readQueue, &nextRequest ) )
        {
            rq_rb_hits++;
        }
        else if( FindOldestReadyRequest( readQueue, &nextRequest ) )
        {
            rq_rb_miss++;
        }
        else if( FindClosedBankRequest( readQueue, &nextRequest ) )
        {
            rq_rb_miss++;
        }
    }

    /* Issue the memory transaction as a series of commands to the command queue. */
    if( nextRequest != NULL )
    {
        IssueMemoryCommands( nextRequest );
    }

    /* Issue memory commands from the command queue. */
    CycleCommandQueues( );

    MemoryController::Cycle( steps );
}

void FRFCFS_WQF::PrintStats( )
{
    std::cout << "i" << psInterval << "." << statName << id 
        << ".mem_reads " << mem_reads << std::endl;
    std::cout << "i" << psInterval << "." << statName << id 
        << ".mem_writes " << mem_writes << std::endl;
    std::cout << "i" << psInterval << "." << statName << id 
        << ".rq_rb_hits " << rq_rb_hits << std::endl;
    std::cout << "i" << psInterval << "." << statName << id 
        << ".rq_rb_miss " << rq_rb_miss << std::endl;
    std::cout << "i" << psInterval << "." << statName << id 
        << ".wq_rb_hits " << wq_rb_hits << std::endl;
    std::cout << "i" << psInterval << "." << statName << id 
        << ".wq_rb_miss " << wq_rb_miss << std::endl;
    std::cout << "i" << psInterval << "." << statName << id 
        << ".total drain numbers " << m_drain_num << std::endl;
    std::cout << "i" << psInterval << "." << statName << id 
        << ".total drained write numbers " << m_drain_request_num << std::endl;
    if( m_drain_num > 0 )
    {
        std::cout << "i" << psInterval << "." << statName << id 
            << ".average write numbers in a drain " 
            << ( m_drain_request_num / m_drain_num ) << std::endl;
        std::cout << "i" << psInterval << "." << statName << id 
            << ".minimum write numbers in a drain " 
            << m_min_request_per_drain << std::endl;
        std::cout << "i" << psInterval << "." << statName << id 
            << ".maximum write numbers in a drain " 
            << m_max_request_per_drain << std::endl;
        std::cout << "i" << psInterval << "." << statName << id 
            << ".total drain durations " << m_drain_duration << std::endl;
        std::cout << "i" << psInterval << "." << statName << id 
            << ".average drain duration " 
            << ( m_drain_duration / m_drain_num ) << std::endl;
        std::cout << "i" << psInterval << "." << statName << id 
            << ".minimum drain duration " << m_drain_min_duration << std::endl;
        std::cout << "i" << psInterval << "." << statName << id 
            << ".maximum drain duration " << m_drain_max_duration << std::endl;
        std::cout << "i" << psInterval << "." << statName << id 
            << ".total drain interval " << m_drain_interval << std::endl;
        std::cout << "i" << psInterval << "." << statName << id 
            << ".average drain interval " 
            << ( m_drain_interval / m_drain_num ) << std::endl;
        std::cout << "i" << psInterval << "." << statName << id 
            << ".minimum drain interval " << m_min_drain_interval << std::endl;
        std::cout << "i" << psInterval << "." << statName << id 
            << ".maximum drain interval " << m_max_drain_interval << std::endl;
        std::cout << "i" << psInterval << "." << statName << id 
            << ".total read duration " << m_read_duration << std::endl;
        std::cout << "i" << psInterval << "." << statName << id 
            << ".average read duration " 
            << ( m_read_duration / m_drain_num ) << std::endl;
        std::cout << "i" << psInterval << "." << statName << id 
            << ".minimum read duration " << m_min_read_duration << std::endl;
        std::cout << "i" << psInterval << "." << statName << id 
            << ".maximum read duration " << m_max_read_duration << std::endl;
        std::cout << "i" << psInterval << "." << statName << id 
            << ".total read queue size before drain " 
            << m_drain_readqueue_size << std::endl;
        std::cout << "i" << psInterval << "." << statName << id 
            << ".average read queue size before drain " 
            << ( m_drain_readqueue_size / m_drain_num ) << std::endl;
        std::cout << "i" << psInterval << "." << statName << id 
            << ".minimum read queue size before drain " 
            << m_min_start_readqueue_size << std::endl;
        std::cout << "i" << psInterval << "." << statName << id 
            << ".maximum read queue size before drain " 
            << m_max_start_readqueue_size << std::endl;
        std::cout << "i" << psInterval << "." << statName << id 
            << ".total enter read queue during drain " 
            << m_drain_enter_readqueue_num << std::endl;
        std::cout << "i" << psInterval << "." << statName << id 
            << ".average enter read queue during drain " 
            << ( m_drain_enter_readqueue_num / m_drain_num ) << std::endl;
        std::cout << "i" << psInterval << "." << statName << id 
            << ".minimum enter read queue during drain " 
            << m_min_enter_readqueue_num << std::endl;
        std::cout << "i" << psInterval << "." << statName << id 
            << ".maximum enter read queue during drain " 
            << m_max_enter_readqueue_num << std::endl;
    }
    std::cout << "i" << psInterval << "." << statName << id 
        << ".starvation_precharges " << starvation_precharges << std::endl;
    std::cout << "i" << psInterval << "." << statName << id 
        << ".averageLatency " << averageLatency << std::endl;
    std::cout << "i" << psInterval << "." << statName << id 
        << ".averageQueueLatency " << averageQueueLatency << std::endl;
    std::cout << "i" << psInterval << "." << statName << id 
        << ".measuredLatencies " << measuredLatencies << std::endl;
    std::cout << "i" << psInterval << "." << statName << id 
        << ".measuredQueueLatencies " << measuredQueueLatencies << std::endl;

    MemoryController::PrintStats( );

    psInterval++;
}
