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

#ifndef __FRFCFS_WQF_WQF_H__
#define __FRFCFS_WQF_WQF_H__

#include "src/MemoryController.h"
#include <deque>

namespace NVM {

class FRFCFS_WQF : public MemoryController
{
  public:
    FRFCFS_WQF( );
    ~FRFCFS_WQF( );

    bool IssueCommand( NVMainRequest *request );
    bool IsIssuable( NVMainRequest *request, FailReason *fail = NULL );
    bool RequestComplete( NVMainRequest *request );

    void SetConfig( Config *conf, bool createChildren = true );

    void Cycle( ncycle_t steps );
    bool Drain( );

    void RegisterStats( );
    void CalculateStats( );

  private:
    /* separate read/write queue */
    NVMTransactionQueue *readQueue;
    NVMTransactionQueue *writeQueue;

    const int readQueueId;
    const int writeQueueId;

    /* Cached Configuration Variables*/
    uint64_t writeQueueSize;
    uint64_t readQueueSize;

    /* write drain high and low watermark */
    uint64_t HighWaterMark;
    uint64_t LowWaterMark;

    /* write draining flag */
    bool     m_draining;
    bool     force_drain;

    /* State variables */
    uint64_t m_request_per_drain;
    uint64_t m_drain_start_cycle;
    uint64_t m_drain_end_cycle;
    uint64_t m_drain_start_readqueue_size;
    uint64_t m_drain_end_readqueue_size;
    uint64_t m_last_drain_end_cycle;

    /* Stats */
    uint64_t measuredLatencies, measuredQueueLatencies, measuredTotalLatencies;
    double   averageLatency, averageQueueLatency, averageTotalLatency;
    double   average_writes_per_drain;
    double   average_drain_cycles;
    double   average_drain_spacing;
    double   average_read_spacing;
    double   average_predrain_readqueue_size;
    double   average_reads_during_drain;
    uint64_t mem_reads, mem_writes;
    uint64_t starvation_precharges;
    uint64_t rq_rb_hits;
    uint64_t rq_rb_miss;
    uint64_t wq_rb_hits;
    uint64_t wq_rb_miss;
    uint64_t total_drain_writes;
    uint64_t minimum_drain_writes;
    uint64_t maximum_drain_writes;
    uint64_t total_drains;
    uint64_t total_drain_cycles;
    uint64_t maximum_drain_cycles;
    uint64_t minimum_drain_cycles;
    uint64_t total_reads_during_drain;
    uint64_t minimum_reads_during_drain;
    uint64_t maximum_reads_during_drain;
    uint64_t total_readqueue_size;
    uint64_t minimum_predrain_readqueue_size;
    uint64_t maximum_predrain_readqueue_size;
    uint64_t total_non_drain_cycles;
    uint64_t minimum_drain_spacing;
    uint64_t maximum_drain_spacing;
    uint64_t total_read_cycles;
    uint64_t minimum_read_spacing;
    uint64_t maximum_read_spacing;
};

};

#endif
