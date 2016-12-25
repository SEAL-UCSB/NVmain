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

#ifndef __MEMORYCONTROLLER_H__
#define __MEMORYCONTROLLER_H__

#include <string>
#include <vector>
#include "src/NVMObject.h"
#include "src/Config.h"
#include "src/Interconnect.h"
#include "src/AddressTranslator.h"
#include "include/NVMainRequest.h"
#include <deque>
#include <iostream>
#include <list>


namespace NVM {


enum ProcessorOp { LOAD, STORE };
enum QueueModel { PerRankQueues, PerBankQueues, PerSubArrayQueues };

/*
 *  If the transaction queue has higher priority, it is possible for a
 *  transaction to be inserted into the command queue AND issued in the
 *  same clock cycle. 
 *
 *  By default, the transaction queue has *lower* priority to more closely
 *  model an execution driven order.
 */
const int transactionQueuePriority = 30;
const int commandQueuePriority = 40;
const int refreshPriority = 20;
const int lowPowerPriority = 10;
const int cleanupPriority = -10;

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
    ~MemoryController( );


    void InitQueues( unsigned int numQueues );
    void InitBankQueues( unsigned int numQueues );

    virtual bool RequestComplete( NVMainRequest *request );
    virtual bool IsIssuable( NVMainRequest *request, FailReason *fail );
    ncycle_t NextIssuable( NVMainRequest *request );

    virtual void RegisterStats( );
    virtual void CalculateStats( );

    void CommandQueueCallback( void *data );
    void CleanupCallback( void *data );
    void RefreshCallback( void *data );
    virtual void Cycle( ncycle_t steps ); 

    virtual void SetConfig( Config *conf, bool createChildren = true );
    void SetMappingScheme( );
    Config *GetConfig( );

    void SetID( unsigned int id );
    unsigned int GetID( );

  protected:
    Interconnect *memory;
    Config *config;
    ncounter_t psInterval;
    ncycle_t lastCommandWake;
    ncounter_t wakeupCount;
    ncycle_t lastIssueCycle;

    std::list<NVMainRequest *> *transactionQueues;
    std::deque<NVMainRequest *> *commandQueues;
    ncounter_t commandQueueCount;
    ncounter_t transactionQueueCount;
    QueueModel queueModel;

    ncounter_t GetCommandQueueId( NVMAddress addr );

    bool **activateQueued;
    bool **refreshQueued;
    ncounter_t ***effectiveRow;
    ncounter_t ***effectiveMuxedRow;
    ncounter_t ***activeSubArray;
    ncounter_t ***starvationCounter;
    ncounter_t starvationThreshold;
    ncounter_t subArrayNum;

    bool *rankPowerDown;

    bool TransactionAvailable( ncounter_t queueId );
    void ScheduleCommandWake( );
    void Prequeue( ncounter_t queueNum, NVMainRequest *request );
    void Enqueue( ncounter_t queueNum, NVMainRequest *request );

    NVMainRequest *MakeCachedRequest( NVMainRequest *triggerRequest );
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
    NVMainRequest *MakePowerdownRequest( OpType pdOp,
                                         const ncounter_t rank );
    NVMainRequest *MakePowerupRequest( const ncounter_t rank );

    bool FindStarvedRequest( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **starvedRequest );
    bool FindCachedAddress( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **accessibleRequest );
    bool FindRowBufferHit( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **hitRequest );
    bool FindWriteStalledRead( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **hitRequest );
    bool FindOldestReadyRequest( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **oldestRequest );
    bool FindClosedBankRequest( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **closedRequest );
    bool FindStarvedRequests( std::list<NVMainRequest *>& transactionQueue, std::vector<NVMainRequest *>& starvedRequests );
    bool FindRowBufferHits( std::list<NVMainRequest *>& transactionQueue, std::vector<NVMainRequest *>& hitRequests );
    bool FindOldestReadyRequests( std::list<NVMainRequest *>& transactionQueue, std::vector<NVMainRequest *>& oldestRequests );
    bool FindClosedBankRequests( std::list<NVMainRequest *>& transactionQueue, std::vector<NVMainRequest *>& closedRequests );


    bool IssueMemoryCommands( NVMainRequest *req );
    void CycleCommandQueues( );

    bool FindStarvedRequest( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **starvedRequest, NVM::SchedulingPredicate& p );
    bool FindCachedAddress( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **accessibleRequest, NVM::SchedulingPredicate& p );
    bool FindRowBufferHit( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **hitRequest, NVM::SchedulingPredicate& p );
    bool FindWriteStalledRead( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **hitRequest, NVM::SchedulingPredicate& p );
    bool FindOldestReadyRequest( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **oldestRequest, NVM::SchedulingPredicate& p );
    bool FindClosedBankRequest( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **closedRequest, NVM::SchedulingPredicate& p );
    bool FindStarvedRequests( std::list<NVMainRequest *>& transactionQueue, std::vector<NVMainRequest *>& starvedRequests, NVM::SchedulingPredicate& p  );
    bool FindRowBufferHits( std::list<NVMainRequest *>& transactionQueue, std::vector<NVMainRequest *>& hitRequests, NVM::SchedulingPredicate& p  );
    bool FindOldestReadyRequests( std::list<NVMainRequest *>& transactionQueue, std::vector<NVMainRequest *>& oldestRequests, NVM::SchedulingPredicate& p  );
    bool FindClosedBankRequests( std::list<NVMainRequest *>& transactionQueue, std::vector<NVMainRequest *>& closedRequests, NVM::SchedulingPredicate& p  );

    /* IsLastRequest() tells whether no other request has the row buffer hit in the transaction queue */
    virtual bool IsLastRequest( std::list<NVMainRequest *>& transactionQueue, NVMainRequest *request); 
    /* curQueue records the starting index for queue round-robin level scheduling */
    ncounter_t curQueue;
    /* MoveCurrentQueue() increment curQueue */
    void MoveCurrentQueue( ); 
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

    /* reset the refresh queued flag for a given bank group */
    void ResetRefreshQueued( const ncounter_t bank, const ncounter_t rank );
    
    /* increment the delayedRefreshCounter in a given bank group */
    void IncrementRefreshCounter(const ncounter_t, const ncounter_t); 
    /* decrement the delayedRefreshCounter in a given bank group */
    void DecrementRefreshCounter(const ncounter_t, const ncounter_t); 
    
    ncycle_t handledRefresh;
    /* next Refresh rank and bank */
    ncounter_t nextRefreshRank, nextRefreshBank; 
    /* issue REFRESH command if necessary; otherwise do nothing */
    virtual bool HandleRefresh( ); 

    /* check whether any all command queues in the rank are empty */
    bool RankQueueEmpty( const ncounter_t& );

    void PowerDown( const ncounter_t& );
    void PowerUp( const ncounter_t& );
    virtual void HandleLowPower( );

    /* Check if a command queue is empty or will be cleaned up. */
    bool EffectivelyEmpty( const ncounter_t& );
    
    class DummyPredicate : public SchedulingPredicate
    {
      public:
        bool operator() ( NVMainRequest* request );
    };

    ncounter_t id;

    /* Stats */
    ncounter_t simulation_cycles;
};

};

#endif
