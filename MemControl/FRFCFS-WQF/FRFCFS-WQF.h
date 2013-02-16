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

#ifndef __FRFCFS_WQF_WQF_H__
#define __FRFCFS_WQF_WQF_H__

#include "src/MemoryController.h"
#include <deque>

namespace NVM {

class FRFCFS_WQF : public MemoryController
{
  public:
    FRFCFS_WQF( Interconnect *memory, AddressTranslator *translator );
    ~FRFCFS_WQF( );


    bool IssueCommand( NVMainRequest *req );
    bool RequestComplete( NVMainRequest * request );

    void SetConfig( Config *conf );

    void Cycle( ncycle_t steps );

    bool QueueFull( NVMainRequest *req );

    void PrintStats( );

  private:
    /* separate read/write queue */
    NVMTransactionQueue readQueue;
    NVMTransactionQueue writeQueue;

    /* Cached Configuration Variables*/
    uint64_t writeQueueSize;
    uint64_t readQueueSize;

    /* write drain high and low watermark */
    uint64_t HighWaterMark;
    uint64_t LowWaterMark;

    /* write draining flag */
    bool     m_draining;

    /* Stats */
    uint64_t measuredLatencies, measuredQueueLatencies;
    float    averageLatency, averageQueueLatency;
    uint64_t mem_reads, mem_writes;
    uint64_t starvation_precharges;
    uint64_t rq_rb_hits;
    uint64_t rq_rb_miss;
    uint64_t wq_rb_hits;
    uint64_t wq_rb_miss;
    uint64_t m_request_per_drain;
    uint64_t m_drain_request_num;
    uint64_t m_min_request_per_drain;
    uint64_t m_max_request_per_drain;
    uint64_t m_drain_num;
    uint64_t m_drain_duration;
    uint64_t m_drain_max_duration;
    uint64_t m_drain_min_duration;
    uint64_t m_drain_start_cycle;
    uint64_t m_drain_end_cycle;
    uint64_t m_drain_enter_readqueue_num;
    uint64_t m_min_enter_readqueue_num;
    uint64_t m_max_enter_readqueue_num;
    uint64_t m_drain_readqueue_size;
    uint64_t m_drain_start_readqueue_size;
    uint64_t m_drain_end_readqueue_size;
    uint64_t m_min_start_readqueue_size;
    uint64_t m_max_start_readqueue_size;
    uint64_t m_last_drain_end_cycle;
    uint64_t m_drain_interval;
    uint64_t m_min_drain_interval;
    uint64_t m_max_drain_interval;
    uint64_t m_read_duration;
    uint64_t m_min_read_duration;
    uint64_t m_max_read_duration;
};

};

#endif
