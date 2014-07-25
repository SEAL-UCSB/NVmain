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

#ifndef __DDR3BANK_H__
#define __DDR3BANK_H__

#include <stdint.h>
#include <map>

#include "src/Bank.h"
#include "src/Config.h"
#include "src/EnduranceModel.h"
#include "include/NVMAddress.h"
#include "include/NVMainRequest.h"
#include "src/SubArray.h"
#include "src/Stats.h"

#include <iostream>

namespace NVM {

/*
 *  We only use five bank states because our timing and energy parameters
 *  only tell us the delay of the entire read/write cycle to one bank.
 *  Even though all banks should be powered down in lockstep, we use three
 *  bank states to indicate different PowerDown modes. In addition, as all
 *  banks are powered up, some banks may be active directly according to
 *  different PowerDown states.
 *
 *  In the case of non-volatile memory, consecutive reads and writes do
 *  not need to consider the case when reads occur before tRAS, since
 *  data is not destroyed during read, and thus does not need to be
 *  written back to the row.
 */
enum DDR3BankState 
{ 
    DDR3BANK_UNKNOWN,  /***< Unknown state. Uh oh. */
    DDR3BANK_OPEN,     /***< Bank has an active subarray  */
    DDR3BANK_CLOSED,   /***< Bank is idle. */
    DDR3BANK_PDPF,     /***< Bank is in precharge powered down, fast exit mode */
    DDR3BANK_PDA,      /***< Bank is in active powered down mode */
    DDR3BANK_PDPS      /***< Bank is in precharge powered down, slow exit mode */
};

class DDR3Bank : public Bank
{
  public:
    DDR3Bank( );
    ~DDR3Bank( );

    virtual bool IsIssuable( NVMainRequest *req, FailReason *reason = NULL );
    virtual bool IssueCommand( NVMainRequest *req );
    virtual ncycle_t NextIssuable( NVMainRequest *request );

    virtual void SetConfig( Config *c, bool createChildren = true );

    DDR3BankState GetState( );

    virtual bool Idle( );
    virtual ncycle_t GetDataCycles( ) { return dataCycles; }
    virtual void CalculatePower( );
    virtual double GetPower( ); 

    virtual uint64_t GetOpenRow( ) { return openRow; }
    virtual std::deque<ncounter_t>& GetOpenSubArray( ) { return activeSubArrayQueue; }

    virtual void SetName( std::string );
    virtual void SetId( ncounter_t );

    virtual void RegisterStats( );
    virtual void CalculateStats( );

    virtual ncounter_t GetId( );
    virtual std::string GetName( );

    virtual void Cycle( ncycle_t steps );

  protected:
    std::deque<ncounter_t> activeSubArrayQueue;
    ncounter_t MATWidth;
    ncounter_t MATHeight;
    ncounter_t subArrayNum;

    DDR3BankState state;
    BulkCommand nextCommand;
    NVMainRequest lastOperation;

    ncounter_t dataCycles;
    ncounter_t activeCycles;
    ncounter_t standbyCycles;
    ncounter_t fastExitActiveCycles;
    ncounter_t fastExitPrechargeCycles;
    ncounter_t slowExitPrechargeCycles;
    ncounter_t powerCycles;

    ncycle_t lastActivate;
    ncycle_t nextActivate;
    ncycle_t nextPrecharge;
    ncycle_t nextRead;
    ncycle_t nextWrite;
    ncycle_t nextRefresh;
    ncycle_t nextRefreshDone;
    ncycle_t nextPowerDown;
    ncycle_t nextPowerDownDone;
    ncycle_t nextPowerUp;
    bool writeCycle;
    WriteMode writeMode;

    ncounter_t actWaits;
    ncounter_t actWaitTotal;
    double actWaitAverage;

    double bankEnergy;
    double activeEnergy;
    double burstEnergy;
    double refreshEnergy;
    double bankPower;
    double activePower;
    double burstPower;
    double refreshPower;

    double utilization;
    double bandwidth;
    
    int dummyStat;

    uint64_t averageEndurance, worstCaseEndurance;

    ncounter_t reads, writes, activates, precharges, refreshes;
    ncounter_t idleTimer;

    uint64_t openRow;

    ncounter_t bankId;
 
    virtual bool Activate( NVMainRequest *request );
    virtual bool Read( NVMainRequest *request );
    virtual bool Write( NVMainRequest *request );
    virtual bool Precharge( NVMainRequest *request );
    virtual bool Refresh( NVMainRequest *request );
    virtual bool PowerUp( NVMainRequest *request );
    virtual bool PowerDown( NVMainRequest *request );
};

};

#endif
