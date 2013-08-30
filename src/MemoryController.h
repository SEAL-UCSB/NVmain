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

#ifndef __MEMORYCONTROLLER_H__
#define __MEMORYCONTROLLER_H__

#include <string>
#include <vector>
#include "src/NVMObject.h"
#include "src/Config.h"
#include "src/Interconnect.h"
#include "src/AddressTranslator.h"
#include "src/Params.h"
#include "include/NVMainRequest.h"
#include <deque>
#include <iostream>
#include <list>


namespace NVM {


enum ProcessorOp { LOAD, STORE };

class SchedulingPredicate
{
  public:
    SchedulingPredicate( ) { }
    ~SchedulingPredicate( ) { }

    virtual bool operator() (NVMainRequest * /*request*/) { return true; }
};


/* Takes a scheduling predicate and returns the opposite of it's evaluation. */
class ComplementPredicate : public SchedulingPredicate
{
  private:
    ComplementPredicate() {}

    SchedulingPredicate *pred;

  public:
    explicit ComplementPredicate( SchedulingPredicate *_pred ) : pred(_pred) { }
    ~ComplementPredicate( ) { }

    bool operator() (NVMainRequest *request)
    {
        return !( (*pred)( request ) );
    }
};


class MemoryController : public NVMObject 
{
  public:
    MemoryController( );
    MemoryController( Interconnect *memory, AddressTranslator *translator );
    ~MemoryController( );


    void InitQueues( unsigned int numQueues );
    void InitBankQueues( unsigned int numQueues );

    virtual bool RequestComplete( NVMainRequest *request );
    virtual bool QueueFull( NVMainRequest *request );

    void SetMemory( Interconnect *mem );
    Interconnect *GetMemory( );

    void SetTranslator( AddressTranslator *trans );
    AddressTranslator *GetTranslator( );

    AddressTranslator *GetAddressTranslator( );

    void StatName( std::string name ) { statName = name; }
    virtual void PrintStats( );

    virtual void Cycle( ncycle_t steps ); 

    virtual void SetConfig( Config *conf );
    void SetParams( Params *params ) { p = params; }
    Config *GetConfig( );

    void SetID( unsigned int id );

  protected:
    Interconnect *memory;
    AddressTranslator *translator;
    Config *config;
    std::string statName;
    ncounter_t psInterval;

    std::list<NVMainRequest *> *transactionQueues;
    std::deque<NVMainRequest *> **bankQueues;

    bool **activateQueued;
    ncounter_t ***effectiveRow;
    ncounter_t ***effectiveMuxedRow;
    ncounter_t ***activeSubArray;
    ncounter_t ***starvationCounter;
    ncounter_t starvationThreshold;
    ncounter_t subArrayNum;

    bool *rankPowerDown;

    NVMainRequest *MakeActivateRequest( NVMainRequest *triggerRequest );
    NVMainRequest *MakeActivateRequest( const ncounter_t, const ncounter_t, 
                                        const ncounter_t, const ncounter_t, 
                                        const ncounter_t );
    NVMainRequest *MakeImplicitPrechargeRequest( NVMainRequest *triggerRequest );
    NVMainRequest *MakePrechargeRequest( NVMainRequest *triggerRequest );
    NVMainRequest *MakePrechargeRequest( const ncounter_t, const ncounter_t, 
                                         const ncounter_t, const ncounter_t, 
                                         const ncounter_t );
    NVMainRequest *MakePrechargeAllRequest( NVMainRequest *triggerRequest );
    NVMainRequest *MakePrechargeAllRequest( const ncounter_t, const ncounter_t, 
                                            const ncounter_t, const ncounter_t,
                                            const ncounter_t );
    NVMainRequest *MakeRefreshRequest( const ncounter_t, const ncounter_t, 
                                       const ncounter_t, const ncounter_t, 
                                       const ncounter_t );

    bool FindStarvedRequest( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **starvedRequest );
    bool FindRowBufferHit( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **hitRequest );
    bool FindOldestReadyRequest( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **oldestRequest );
    bool FindClosedBankRequest( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **closedRequest );
    bool FindStarvedRequests( std::list<NVMainRequest *>& transactionQueue, std::vector<NVMainRequest *>& starvedRequests );
    bool FindRowBufferHits( std::list<NVMainRequest *>& transactionQueue, std::vector<NVMainRequest *>& hitRequests );
    bool FindOldestReadyRequests( std::list<NVMainRequest *>& transactionQueue, std::vector<NVMainRequest *>& oldestRequests );
    bool FindClosedBankRequests( std::list<NVMainRequest *>& transactionQueue, std::vector<NVMainRequest *>& closedRequests );

    bool IssueMemoryCommands( NVMainRequest *req );
    void CycleCommandQueues( );

    bool FindStarvedRequest( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **starvedRequest, NVM::SchedulingPredicate& p );
    bool FindRowBufferHit( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **hitRequest, NVM::SchedulingPredicate& p );
    bool FindOldestReadyRequest( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **oldestRequest, NVM::SchedulingPredicate& p );
    bool FindClosedBankRequest( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **closedRequest, NVM::SchedulingPredicate& p );
    bool FindStarvedRequests( std::list<NVMainRequest *>& transactionQueue, std::vector<NVMainRequest *>& starvedRequests, NVM::SchedulingPredicate& p  );
    bool FindRowBufferHits( std::list<NVMainRequest *>& transactionQueue, std::vector<NVMainRequest *>& hitRequests, NVM::SchedulingPredicate& p  );
    bool FindOldestReadyRequests( std::list<NVMainRequest *>& transactionQueue, std::vector<NVMainRequest *>& oldestRequests, NVM::SchedulingPredicate& p  );
    bool FindClosedBankRequests( std::list<NVMainRequest *>& transactionQueue, std::vector<NVMainRequest *>& closedRequests, NVM::SchedulingPredicate& p  );
    /* IsLastRequest() tells whether no other request has the row buffer hit in the transaction queue */
    virtual bool IsLastRequest( std::list<NVMainRequest *>& transactionQueue, NVMainRequest *request); 
    /* curRank and curBank record the starting rank (bank) index for the rank-(bank-) level scheduling */
    ncounter_t curRank, curBank; 
    /* MoveRankBank() increment the curRank and/or curBank according to the scheduling scheme */
    void MoveRankBank(); 
    /* record how many refresh should be handled */
    ncounter_t **delayedRefreshCounter; 

    /* indicate whether the bank need to be refreshed immediately */
    bool **bankNeedRefresh;

    /* indicate how long a bank should be refreshed */
    ncycle_t m_tREFI; 
    /* indicate the number of bank groups for refresh */
    ncounter_t m_refreshBankNum; 
    /* return true if the delayed refresh in the corresponding bank reach the threshold */
    bool NeedRefresh(const ncounter_t, const ncounter_t); 
    /* basically, it increment the delayedRefreshCounter and generate the next refresh pulse */
    void ProcessRefreshPulse( NVMainRequest* ); 
    /* return true if ALL command queues in the bank group are empty */
    bool IsRefreshBankQueueEmpty(const ncounter_t, const ncounter_t); 

    /* set the refresh flag for a given bank group */
    void SetRefresh(const ncounter_t, const ncounter_t); 
    /* reset the refresh flag for a given bank group */
    void ResetRefresh(const ncounter_t, const ncounter_t); 
    
    /* increment the delayedRefreshCounter in a given bank group */
    void IncrementRefreshCounter(const ncounter_t, const ncounter_t); 
    /* decrement the delayedRefreshCounter in a given bank group */
    void DecrementRefreshCounter(const ncounter_t, const ncounter_t); 
    
    /* next Refresh rank and bank */
    ncounter_t nextRefreshRank, nextRefreshBank; 
    /* issue REFRESH command if necessary; otherwise do nothing */
    virtual bool HandleRefresh( ); 

    /* check whether any all command queues in the rank are empty */
    bool RankQueueEmpty( const ncounter_t& );

    void PowerDown( const ncounter_t& );
    void PowerUp( const ncounter_t& );
    virtual void HandleLowPower( );
    
    class DummyPredicate : public SchedulingPredicate
    {
      public:
        bool operator() ( NVMainRequest* request );
    };

    ncounter_t id;

    Params *p;
};

};

#endif
