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
#include "include/NVMAddress.h"
#include "include/NVMainRequest.h"
#include "src/Params.h"

#include <iostream>

namespace NVM {

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

    bool WouldConflict( uint64_t checkRow );
    bool IsIssuable( NVMainRequest *req, FailReason *reason = NULL );
    bool IssueCommand( NVMainRequest *req );
    bool RequestComplete( NVMainRequest *req );

    void SetConfig( Config *c );
    void SetParams( Params *params ) { p = params; }

    SubArrayState GetState( );

    bool Idle( );
    ncycle_t GetDataCycles( ) { return dataCycles; }
    void GetEnergy( float&, float&, float&, float& );
    ncounter_t GetReads( ) { return reads; }
    ncounter_t GetWrites( ) { return writes; }

    ncycle_t GetNextActivate( ) { return nextActivate; }
    ncycle_t GetNextRead( ) { return nextRead; }
    ncycle_t GetNextWrite( ) { return nextWrite; }
    ncycle_t GetNextPrecharge( ) { return nextPrecharge; }
    ncycle_t GetActiveWaits( ) { return actWaits; }
    uint64_t GetOpenRow( ) { return openRow; }

    void SetName( std::string );
    void SetId( ncounter_t );
    void PrintStats( );
    void StatName( std::string name ) { statName = name; }

    ncounter_t GetId( );
    std::string GetName( );

    void Cycle( ncycle_t );

  private:
    Config *conf;
    std::string statName;
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
    bool writeCycle;
    WriteMode writeMode;
    ncounter_t dataCycles;

    ncounter_t actWaits;
    ncounter_t actWaitTime;

    float subArrayEnergy;
    float activeEnergy;
    float burstEnergy;
    float refreshEnergy;

    ncounter_t reads, writes, activates, precharges, refreshes;
    ncounter_t idleTimer;

    ncounter_t openRow;

    EnduranceModel *endrModel;

    ncounter_t subArrayId;
 
    Params *p;
};

};

#endif
