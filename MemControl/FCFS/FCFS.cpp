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

#include "MemControl/FCFS/FCFS.h"
#include "src/EventQueue.h"

#include <iostream>

using namespace NVM;

/*
 *  This simple memory controller is an example memory controller for NVMain, 
 *  it operates as follows:
 *
 *  - After each read or write is issued, the page is closed.
 *    For each request, it simply prepends an activate before the read/write and 
 *    appends a precharge
 *  - This memory controller leaves all banks and ranks in active mode 
 *    (it does not do power management)
 */
FCFS::FCFS( )
{
    std::cout << "Created a FCFS memory controller!" << std::endl;

    queueSize = 32;

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

    psInterval = 0;

    InitQueues( 1 );
}

void FCFS::SetConfig( Config *conf, bool createChildren )
{
    if( conf->KeyExists( "QueueSize" ) )
    {
        queueSize = static_cast<unsigned int>( conf->GetValue( "QueueSize" ) );
    }

    MemoryController::SetConfig( conf, createChildren );

    SetDebugName( "FCFS", conf );
}

void FCFS::RegisterStats( )
{
    AddStat(mem_reads);
    AddStat(mem_writes);
    AddStat(rb_hits);
    AddStat(rb_miss);
    AddStat(averageLatency);
    AddStat(averageQueueLatency);
    AddStat(averageTotalLatency);
    AddStat(measuredLatencies);
    AddStat(measuredQueueLatencies);
    AddStat(measuredTotalLatencies);

    MemoryController::RegisterStats( );
}

bool FCFS::RequestComplete( NVMainRequest * request )
{
    /* 
     * Only reads and writes are sent back to NVMain and 
     * checked for in the transaction queue. 
     */
    if( request->type == READ 
        || request->type == READ_PRECHARGE 
        || request->type == WRITE 
        || request->type == WRITE_PRECHARGE
        )
    {
        request->status = MEM_REQUEST_COMPLETE;
        request->completionCycle = GetEventQueue()->GetCurrentCycle();

        /* Update the average latencies based on this request for READ/WRITE only. */
        averageLatency = ((averageLatency * static_cast<double>(measuredLatencies))
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

bool FCFS::IsIssuable( NVMainRequest * /*request*/, FailReason * /*fail*/ )
{
    bool rv = true;

    /* Allow up to 16 read/writes outstanding. */
    if( transactionQueues[0].size( ) >= queueSize )
        rv = false;

    return rv;
}

/*
 *  This method is called whenever a new transaction from the processor issued to
 *  this memory controller / channel. All scheduling decisions should be made here.
 */
bool FCFS::IssueCommand( NVMainRequest *request )
{
    /* Allow up to 16 read/writes outstanding. */
    if( !IsIssuable( request ) )
        return false;

    if( request->type == READ )
        mem_reads++;
    else
        mem_writes++;

    Enqueue( 0, request );

    /*
     * Return whether the request could be queued. Return false if the queue is full.
     */
    return true;
}

void FCFS::Cycle( ncycle_t steps )
{
    NVMainRequest *nextReq = NULL;

    /* Simply get the oldest request */
    if( !FindOldestReadyRequest( transactionQueues[0], &nextReq ) )
    {
        /* No oldest ready request, check for non-activated banks. */
        (void)FindClosedBankRequest( transactionQueues[0], &nextReq );
    }

    if( nextReq != NULL )
    {
        IssueMemoryCommands( nextReq );
    }

    CycleCommandQueues( );

    MemoryController::Cycle( steps );
}

void FCFS::CalculateStats( )
{
    MemoryController::CalculateStats( );
}

