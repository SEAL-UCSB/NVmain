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
*******************************************************************************/

#include "MemControl/FCFS/FCFS.h"
#include "SimInterface/GemsInterface/GemsInterface.h"
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
FCFS::FCFS( Interconnect *memory, AddressTranslator *translator )
{
    /*
     *  We'll need these classes later, so copy them. the "memory" and 
     *  "translator" variables are *  defined in the protected section of 
     *  the MemoryController base class. 
     */
    SetMemory( memory );
    SetTranslator( translator );

    std::cout << "Created a Simple Close Page memory controller!" << std::endl;

    queueSize = 32;

    averageLatency = 0.0f;
    averageQueueLatency = 0.0f;

    measuredLatencies = 0;
    measuredQueueLatencies = 0;

    mem_reads = 0;
    mem_writes = 0;

    rb_hits = 0;
    rb_miss = 0;

    psInterval = 0;

    InitQueues( 1 );
}

void FCFS::SetConfig( Config *conf )
{
    if( conf->KeyExists( "QueueSize" ) )
    {
        queueSize = static_cast<unsigned int>( conf->GetValue( "QueueSize" ) );
    }

    MemoryController::SetConfig( conf );
}

bool FCFS::QueueFull( NVMainRequest * /*req*/ )
{
    return( transactionQueues[0].size() >= queueSize );
}

bool FCFS::RequestComplete( NVMainRequest * request )
{
    /* 
     * Only reads and writes are sent back to NVMain and 
     * checked for in the transaction queue. 
     */
    if( request->type == READ || request->type == WRITE )
    {
        request->status = MEM_REQUEST_COMPLETE;
        request->completionCycle = GetEventQueue()->GetCurrentCycle();

        /* Update the average latencies based on this request for READ/WRITE only. */
        averageLatency = ((averageLatency * static_cast<float>(measuredLatencies))
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

    if( request->type == REFRESH )
        ProcessRefreshPulse( request );
    else if( request->owner == this )
        delete request;
    else
        GetParent( )->RequestComplete( request );

    return true;
}

/*
 *  This method is called whenever a new transaction from the processor issued to
 *  this memory controller / channel. All scheduling decisions should be made here.
 */
bool FCFS::IssueCommand( NVMainRequest *req )
{
    /* Allow up to 16 read/writes outstanding. */
    if( transactionQueues[0].size( ) >= queueSize )
        return false;

    if( req->type == READ )
        mem_reads++;
    else
        mem_writes++;

    transactionQueues[0].push_back( req );

    /*
     * Return whether the request could be queued. Return false if the queue is full.
     */
    return true;
}

void FCFS::Cycle( ncycle_t )
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
}

void FCFS::PrintStats( )
{
    std::cout << "i" << psInterval << "." << statName << id << ".mem_reads " << mem_reads << std::endl;
    std::cout << "i" << psInterval << "." << statName << id << ".mem_writes " << mem_writes << std::endl;
    std::cout << "i" << psInterval << "." << statName << id << ".rb_hits " << rb_hits << std::endl;
    std::cout << "i" << psInterval << "." << statName << id << ".rb_miss " << rb_miss << std::endl;
    std::cout << "i" << psInterval << "." << statName << id << ".averageLatency " << averageLatency << std::endl;
    std::cout << "i" << psInterval << "." << statName << id << ".averageQueueLatency " << averageQueueLatency << std::endl;
    std::cout << "i" << psInterval << "." << statName << id << ".measuredLatencies " << measuredLatencies << std::endl;
    std::cout << "i" << psInterval << "." << statName << id << ".measuredQueueLatencies " << measuredQueueLatencies << std::endl;

    MemoryController::PrintStats( );

    psInterval++;
}

