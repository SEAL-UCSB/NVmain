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
*   Tao Zhang       ( Email: tzz106 at cse dot psu dot edu
*                     Website: http://www.cse.psu.edu/~tzz106 )
*******************************************************************************/

#ifndef __SUBARRAY_H__
#define __SUBARRAY_H__

#include <stdint.h>
#include <map>

#include "src/NVMObject.h"
#include "src/Config.h"
#include "src/EnduranceModel.h"
#include "src/DataEncoder.h"
#include "include/NVMAddress.h"
#include "include/NVMainRequest.h"
#include "src/Params.h"

#include <iostream>

namespace NVM {

class Event;

/*
 *  We only use four subarray states because we use distributed timing control
 *  No PowerDown state is implemented since it does not make sense to apply 
 *  PowerDown to a subarray. As a result, PowerDown is management by Bank
 *
 *  In the case of non-volatile memory, consecutive reads and writes do
 *  not need to consider the case when reads occur before tRAS, since
 *  data is not destroyed during read, and thus does not need to be
 *  written back to the row.
 */
enum SubArrayState 
{ 
    SUBARRAY_UNKNOWN,     /* Unknown state. Uh oh. */
    SUBARRAY_OPEN,        /* SubArray has an open row */
    SUBARRAY_CLOSED,      /* SubArray is idle. */
    SUBARRAY_PRECHARGING, /* SubArray is precharging and return to SUBARRAY_CLOSED */
    SUBARRAY_REFRESHING   /* SubArray is refreshing and return to SUBARRAY_CLOSED */
};

enum WriteMode 
{
    WRITE_BACK, /* only modify the row buffer */
    WRITE_THROUGH, /* modify both row buffer and cell */
    DELAYED_WRITE /* data is stored in a write buffer */
};

class SubArray : public NVMObject
{
  public:
    SubArray( );
    ~SubArray( );

    bool Activate( NVMainRequest *request );
    bool Read( NVMainRequest *request );
    bool Write( NVMainRequest *request );
    bool Precharge( NVMainRequest *request );
    bool Refresh( NVMainRequest *request );

    bool IsIssuable( NVMainRequest *req, FailReason *reason = NULL );
    bool IssueCommand( NVMainRequest *req );
    bool RequestComplete( NVMainRequest *req );
    ncycle_t NextIssuable( NVMainRequest *request );

    void SetConfig( Config *c, bool createChildren = true );

    SubArrayState GetState( );

    bool BetweenWriteIterations( );

    bool Idle( );
    ncycle_t GetDataCycles( ) { return dataCycles; }

    ncycle_t GetNextActivate( ) { return nextActivate; }
    ncycle_t GetNextRead( ) { return nextRead; }
    ncycle_t GetNextWrite( ) { return nextWrite; }
    ncycle_t GetNextPrecharge( ) { return nextPrecharge; }
    ncycle_t GetActiveWaits( ) { return actWaits; }
    uint64_t GetOpenRow( ) { return openRow; }

    void SetName( std::string );
    void SetId( ncounter_t );

    void RegisterStats( );
    void CalculateStats( );

    ncounter_t GetId( );
    std::string GetName( );

    void Cycle( ncycle_t );
    bool IsWriting( ) { return isWriting; }

  private:
    Config *conf;
    ncounter_t psInterval;

    ncounter_t MATWidth;
    ncounter_t MATHeight;

    SubArrayState state;
    BulkCommand nextCommand;
    NVMainRequest lastOperation;

    ncycle_t lastActivate;
    ncycle_t nextActivate;
    ncycle_t nextPrecharge;
    ncycle_t nextRead;
    ncycle_t nextWrite;
    ncycle_t nextPowerDown;
    bool writeCycle;
    std::vector<NVMainRequest *> writeBackRequests;
    bool isWriting;
    ncycle_t writeEnd;
    ncycle_t writeStart;
    std::set<ncycle_t> writeIterationStarts;
    NVMainRequest *writeRequest;
    NVM::Event *writeEvent;
    ncycle_t writeEventTime;
    WriteMode writeMode;
    ncycle_t nextActivatePreWrite;
    ncycle_t nextPrechargePreWrite;
    ncycle_t nextReadPreWrite;
    ncycle_t nextWritePreWrite;
    ncycle_t nextPowerDownPreWrite;
    ncounter_t dataCycles;
    ncycle_t worstCaseWrite;
    ncounter_t num00Writes;
    ncounter_t num01Writes;
    ncounter_t num10Writes;
    ncounter_t num11Writes;
    double averageWriteTime;
    ncounter_t measuredWriteTimes;
    ncounter_t averageWriteIterations;
    double averagePausesPerRequest;
    ncounter_t measuredPauses;
    double averagePausedRequestProgress;
    ncounter_t measuredProgresses;

    ncounter_t cancelledWrites;
    ncounter_t cancelledWriteTime;
    ncounter_t pausedWrites;

    ncounter_t actWaits;
    ncounter_t actWaitTotal;
    double actWaitAverage;

    double subArrayEnergy;
    double activeEnergy;
    double burstEnergy;
    double writeEnergy;
    double refreshEnergy;

    uint64_t worstCaseEndurance, averageEndurance;

    ncounter_t reads, writes, activates, precharges, refreshes;
    ncounter_t idleTimer;

    ncounter_t openRow;

    DataEncoder *dataEncoder;
    EnduranceModel *endrModel;

    ncounter_t subArrayId;
 
    std::map<uint64_t, uint64_t> mlcTimingMap;
    std::map<uint64_t, uint64_t> cancelCountMap;
    std::map<double, uint64_t> wpPauseMap;
    std::map<double, uint64_t> wpCancelMap;
    std::string mlcTimingHisto;
    std::string cancelCountHisto;
    std::string wpPauseHisto;
    std::string wpCancelHisto;

    ncycle_t WriteCellData( NVMainRequest *request );
    void CheckWritePausing( );

    ncycle_t UpdateEndurance( NVMainRequest *request );

    ncounter_t Count32MLC2( uint8_t value, uint32_t data );
    ncounter_t CountBitsMLC2( uint8_t value, uint32_t *data, ncounter_t words );
    ncounter_t Count32MLC1( uint32_t data );
    ncounter_t CountBitsMLC1( uint8_t value, uint32_t *data, ncounter_t words );
};

};

#endif
