/*
 *  This file is part of NVMain- A cycle accurate timing, bit-accurate
 *  energy simulator for non-volatile memory. Originally developed by 
 *  Matt Poremba at the Pennsylvania State University.
 *
 *  Website: http://www.cse.psu.edu/~poremba/nvmain/
 *  Email: mrp5060@psu.edu
 *
 *  ---------------------------------------------------------------------
 *
 *  If you use this software for publishable research, please include 
 *  the original NVMain paper in the citation list and mention the use 
 *  of NVMain.
 *
 */

#include "src/MemoryController.h"
#include "include/NVMainRequest.h"


using namespace NVM;



MemoryController::MemoryController( )
{
  transactionQueues = NULL;
  memory = NULL;
  translator = NULL;

  refreshUsed = false;
  refreshTimes = NULL;

  starvationThreshold = 4;
  starvationCounter = NULL;
  activateQueued = NULL;
  effectiveRow = NULL;
  
  curRank = 0;
  curBank = 0;
}

MemoryController::MemoryController( Interconnect *memory, AddressTranslator *translator )
{
  this->memory = memory;
  this->translator = translator;

  transactionQueues = NULL;
}

AddressTranslator *MemoryController::GetAddressTranslator( )
{
  return translator;
}


void MemoryController::InitQueues( unsigned int numQueues )
{
  if( transactionQueues != NULL )
    delete [] transactionQueues;

  transactionQueues = new NVMTransactionQueue[ numQueues ];

  for( unsigned int i = 0; i < numQueues; i++ )
    transactionQueues[i].clear( );
}


void MemoryController::Cycle( ncycle_t )
{
}


bool MemoryController::RequestComplete( NVMainRequest *request )
{
  bool rv = false;

  if( request->owner == this )
    {
      /* 
       *  Any activate/precharge/etc commands belong to the memory controller, and 
       *  we are in charge of deleting them!
       */
      delete request;
      rv = true;
    }
  else
    {
      GetParent( )->RequestComplete( request );
    }

  return rv;
}


bool MemoryController::QueueFull( NVMainRequest * /*request*/ )
{
  return false;
}


void MemoryController::SetMemory( Interconnect *mem )
{
  this->memory = mem;

  AddChild( mem );
  mem->SetParent( this );
}



Interconnect *MemoryController::GetMemory( )
{
  return (this->memory);
}



void MemoryController::SetTranslator( AddressTranslator *trans )
{
  this->translator = trans;
}



AddressTranslator *MemoryController::GetTranslator( )
{
  return (this->translator);
}


void MemoryController::SetConfig( Config *conf )
{
  this->config = conf;

  Params *params = new Params( );
  params->SetParams( conf );
  SetParams( params );
  
  /*
   *  This is essentially a copy of the timings inside the bank,
   *  but seems easier than adding interfaces to access the banks.
   */
  if( p->UseRefresh_set && p->UseRefresh )
    {
      refreshUsed = true;
      refreshTimes = new ncycle_t*[ p->RANKS ];
      for( ncounter_t i = 0; i < p->RANKS; i++ )
        refreshTimes [i] = new ncycle_t[ p->BANKS ];

      for( ncounter_t i = 0; i < p->BANKS; i++ )
        {
          ncycle_t nextRefresh;

          nextRefresh = static_cast<ncycle_t>(static_cast<float>(p->tRFI) / (static_cast<float>(p->ROWS) / static_cast<float>(p->RefreshRows)));

          /* Equivalent to per-bank refresh in LPDDR3 spec. */
          if( p->StaggerRefresh_set && p->StaggerRefresh )
            {
              nextRefresh = static_cast<ncycle_t>(static_cast<float>(nextRefresh) * (static_cast<float>(i + 1) / static_cast<float>(p->BANKS)));
            }

          for( ncounter_t j = 0; j < p->RANKS; j++ )
            {
              refreshTimes[j][i] = nextRefresh;
            }
        }
    }

  bankQueues = new std::deque<NVMainRequest *> * [p->RANKS];
  starvationCounter = new unsigned int * [p->RANKS];
  activateQueued = new bool * [p->RANKS];
  effectiveRow = new uint64_t * [p->RANKS];
  for( ncounter_t i = 0; i < p->RANKS; i++ )
    {
      bankQueues[i] = new std::deque<NVMainRequest *> [p->BANKS];
      starvationCounter[i] = new unsigned int[p->BANKS];
      activateQueued[i] = new bool[p->BANKS];
      effectiveRow[i] = new uint64_t[p->BANKS];
      for( ncounter_t j = 0; j < p->BANKS; j++ )
        {
          starvationCounter[i][j] = 0;
          activateQueued[i][j] = false;
          effectiveRow[i][j] = 0;
        }
    }
}


Config *MemoryController::GetConfig( )
{
  return (this->config);
}


void MemoryController::SetID( unsigned int id )
{
  this->id = id;
}


NVMainRequest *MemoryController::MakeActivateRequest( NVMainRequest *triggerRequest )
{
  NVMainRequest *activateRequest = new NVMainRequest( );

  activateRequest->type = ACTIVATE;
  activateRequest->issueCycle = GetEventQueue()->GetCurrentCycle();
  activateRequest->address = triggerRequest->address;
  activateRequest->owner = this;

  return activateRequest;
}


NVMainRequest *MemoryController::MakePrechargeRequest( NVMainRequest *triggerRequest )
{
  NVMainRequest *prechargeRequest = new NVMainRequest( );

  prechargeRequest->type = PRECHARGE;
  prechargeRequest->issueCycle = GetEventQueue()->GetCurrentCycle();
  prechargeRequest->address = triggerRequest->address;
  prechargeRequest->owner = this;

  return prechargeRequest;
}



NVMainRequest *MemoryController::MakeRefreshRequest( NVMainRequest *triggerRequest )
{
  NVMainRequest *refreshRequest = new NVMainRequest( );

  refreshRequest->type = REFRESH;
  refreshRequest->issueCycle = GetEventQueue()->GetCurrentCycle();
  refreshRequest->address = triggerRequest->address;
  refreshRequest->owner = this;

  return refreshRequest;
}


bool MemoryController::FindStarvedRequest( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **starvedRequest )
{
  DummyPredicate pred;

  return FindStarvedRequest( transactionQueue, starvedRequest, pred );
}


bool MemoryController::FindStarvedRequest( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **starvedRequest, SchedulingPredicate& pred )
{
  bool rv = false;
  std::list<NVMainRequest *>::iterator it;

  for( it = transactionQueue.begin(); it != transactionQueue.end(); it++ )
    {
      uint64_t rank, bank, row;

      (*it)->address.GetTranslatedAddress( &row, NULL, &bank, &rank, NULL );

      if( activateQueued[rank][bank] && effectiveRow[rank][bank] != row    /* The effective row is not the row of this request. */
          && starvationCounter[rank][bank] >= starvationThreshold          /* This bank has reached it's starvation threshold. */
          && bankQueues[rank][bank].empty()                                /* No requests are currently issued to this bank. */
          && pred( rank, bank ) )                                          /* User-defined predicate is true. */
        {
          *starvedRequest = (*it);
          transactionQueue.erase( it );

          rv = true;
          break;
        }
    }

  return rv;
}



bool MemoryController::FindRowBufferHit( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **hitRequest )
{
  DummyPredicate pred;

  return FindRowBufferHit( transactionQueue, hitRequest, pred );
}



bool MemoryController::FindRowBufferHit( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **hitRequest, SchedulingPredicate& pred )
{
  bool rv = false;
  std::list<NVMainRequest *>::iterator it;

  for( it = transactionQueue.begin(); it != transactionQueue.end(); it++ )
    {
      uint64_t rank, bank, row;

      (*it)->address.GetTranslatedAddress( &row, NULL, &bank, &rank, NULL );

      if( activateQueued[rank][bank] && effectiveRow[rank][bank] == row    /* The effective row is the row of this request. */
          && bankQueues[rank][bank].empty()                                /* No requests are currently issued to this bank. */
          && pred( rank, bank ) )                                          /* User-defined predicate is true. */
        {
          *hitRequest = (*it);
          transactionQueue.erase( it );

          rv = true;
          break;
        }
    }

  return rv;
}



bool MemoryController::FindOldestReadyRequest( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **oldestRequest )
{
  DummyPredicate pred;

  return FindOldestReadyRequest( transactionQueue, oldestRequest, pred );
}


bool MemoryController::FindOldestReadyRequest( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **oldestRequest, SchedulingPredicate& pred )
{
  bool rv = false;
  std::list<NVMainRequest *>::iterator it;

  for( it = transactionQueue.begin(); it != transactionQueue.end(); it++ )
    {
      uint64_t rank, bank, row;

      (*it)->address.GetTranslatedAddress( &row, NULL, &bank, &rank, NULL );

      if( activateQueued[rank][bank] 
          && bankQueues[rank][bank].empty()                                /* No requests are currently issued to this bank (Ready). */
          && pred( rank, bank ) )                                          /* User-defined predicate is true. */
        {
          *oldestRequest = (*it);
          transactionQueue.erase( it );
          
          rv = true;
          break;
        }
    }

  return rv;
}


bool MemoryController::FindClosedBankRequest( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **closedRequest )
{
  DummyPredicate pred;

  return FindClosedBankRequest( transactionQueue, closedRequest, pred );
}


bool MemoryController::FindClosedBankRequest( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **closedRequest, SchedulingPredicate& pred )
{
  bool rv = false;
  std::list<NVMainRequest *>::iterator it;

  for( it = transactionQueue.begin(); it != transactionQueue.end(); it++ )
    {
      uint64_t rank, bank, row;

      (*it)->address.GetTranslatedAddress( &row, NULL, &bank, &rank, NULL );

      if( !activateQueued[rank][bank]                                      /* This bank is closed, anyone can issue. */
          && bankQueues[rank][bank].empty()                                /* No requests are currently issued to this bank (Ready). */
          && pred( rank, bank ) )                                          /* User defined predicate is true. */
        {
          *closedRequest = (*it);
          transactionQueue.erase( it );
          
          rv = true;
          break;
        }
    }

  return rv;
}


/*
 *  Slightly modify the scheduling functions form MemoryController.cpp to return a list instead
 *  of just a single request
 */
bool MemoryController::FindStarvedRequests( std::list<NVMainRequest *>& transactionQueue, std::vector<NVMainRequest *>& starvedRequests )
{
  DummyPredicate pred;

  return FindStarvedRequests( transactionQueue, starvedRequests );
}



bool MemoryController::FindStarvedRequests( std::list<NVMainRequest *>& transactionQueue, std::vector<NVMainRequest *>& starvedRequests, SchedulingPredicate& pred )
{
  bool rv = false;
  std::list<NVMainRequest *>::iterator it;

  for( it = transactionQueue.begin(); it != transactionQueue.end(); it++ )
    {
      uint64_t rank, bank, row;

      (*it)->address.GetTranslatedAddress( &row, NULL, &bank, &rank, NULL );

      if( activateQueued[rank][bank] && effectiveRow[rank][bank] != row    /* The effective row is not the row of this request. */
          && starvationCounter[rank][bank] >= starvationThreshold          /* This bank has reached it's starvation threshold. */
          && bankQueues[rank][bank].empty()                                /* No requests are currently issued to this bank. */
          && pred( rank, bank ) )
        {
          starvedRequests.push_back( (*it) );
          transactionQueue.erase( it );

          rv = true;
        }
    }

  return rv;
}



bool MemoryController::FindRowBufferHits( std::list<NVMainRequest *>& transactionQueue, std::vector<NVMainRequest *>& hitRequests )
{
  DummyPredicate pred;

  return FindRowBufferHits( transactionQueue, hitRequests, pred );
}



bool MemoryController::FindRowBufferHits( std::list<NVMainRequest *>& transactionQueue, std::vector<NVMainRequest* >& hitRequests, SchedulingPredicate& pred )
{
  bool rv = false;
  std::list<NVMainRequest *>::iterator it;

  for( it = transactionQueue.begin(); it != transactionQueue.end(); it++ )
    {
      uint64_t rank, bank, row;

      (*it)->address.GetTranslatedAddress( &row, NULL, &bank, &rank, NULL );

      if( activateQueued[rank][bank] && effectiveRow[rank][bank] == row    /* The effective row is the row of this request. */
          && bankQueues[rank][bank].empty()                                /* No requests are currently issued to this bank. */
          && pred( rank, bank ) )                                          /* User-defined predicate is true. */
        {
          hitRequests.push_back( (*it) );
          transactionQueue.erase( it );

          rv = true;
        }
    }

  return rv;
}




bool MemoryController::FindOldestReadyRequests( std::list<NVMainRequest *>& transactionQueue, std::vector<NVMainRequest *> &oldestRequests )
{
  DummyPredicate pred;

  return FindOldestReadyRequests( transactionQueue, oldestRequests, pred );
}


bool MemoryController::FindOldestReadyRequests( std::list<NVMainRequest *>& transactionQueue, std::vector<NVMainRequest *>& oldestRequests, SchedulingPredicate& pred )
{
  bool rv = false;
  std::list<NVMainRequest *>::iterator it;

  for( it = transactionQueue.begin(); it != transactionQueue.end(); it++ )
    {
      uint64_t rank, bank, row;

      (*it)->address.GetTranslatedAddress( &row, NULL, &bank, &rank, NULL );

      if( activateQueued[rank][bank] 
          && bankQueues[rank][bank].empty()                                /* No requests are currently issued to this bank (Ready). */
          && pred( rank, bank ) )                                          /* User-defined predicate is true. */
        {
          oldestRequests.push_back( (*it) );
          transactionQueue.erase( it );
          
          rv = true;
        }
    }

  return rv;
}


bool MemoryController::FindClosedBankRequests( std::list<NVMainRequest *>& transactionQueue, std::vector<NVMainRequest *> &closedRequests )
{
  DummyPredicate pred;

  return FindClosedBankRequests( transactionQueue, closedRequests, pred );
}


bool MemoryController::FindClosedBankRequests( std::list<NVMainRequest *>& transactionQueue, std::vector<NVMainRequest *> &closedRequests, SchedulingPredicate& pred )
{
  bool rv = false;
  std::list<NVMainRequest *>::iterator it;

  for( it = transactionQueue.begin(); it != transactionQueue.end(); it++ )
    {
      uint64_t rank, bank, row;

      (*it)->address.GetTranslatedAddress( &row, NULL, &bank, &rank, NULL );

      if( !activateQueued[rank][bank]                                      /* This bank is closed, anyone can issue. */
          && bankQueues[rank][bank].empty()                                /* No requests are currently issued to this bank (Ready). */
          && pred( rank, bank ) )                                          /* User defined predicate is true. */
        {
          closedRequests.push_back( (*it ) );
          transactionQueue.erase( it );
          
          rv = true;
        }
    }

  return rv;
}

bool MemoryController::FindPrechargableBank( uint64_t *preRank, uint64_t *preBank )
{
    std::list<NVMainRequest *>::iterator it;

    for( ncounter_t rankIdx = 0; rankIdx < p->RANKS; rankIdx++ )
        for( ncounter_t bankIdx = 0; bankIdx < p->BANKS; bankIdx++ )
        {
            /* if the bank is open and no command in the queue, then the bank
             * can be closed since there is no command relative to this bank
             * Note: this function has lowest priority and should be used at
             * the end of the controller scheduling
             */
            ncounter_t i = (curRank + rankIdx)%p->RANKS;
            ncounter_t j = (curBank + bankIdx)%p->BANKS;
            if( activateQueued[i][j] && bankQueues[i][j].empty() )
            {
                *preRank = i;
                *preBank = j;
                return true;
            }    
        }

    return false;
}


bool MemoryController::DummyPredicate::operator() (uint64_t, uint64_t)
{
  return true;
}


bool MemoryController::IssueMemoryCommands( NVMainRequest *req )
{
  bool rv = false;
  uint64_t rank, bank, row;

  req->address.GetTranslatedAddress( &row, NULL, &bank, &rank, NULL );

  /*
   *  This function assumes the memory controller uses any predicates when
   *  scheduling. They will not be re-checked here.
   */

  if( !activateQueued[rank][bank] && bankQueues[rank][bank].empty() )
    {
      /* Any activate will request the starvation counter */
      starvationCounter[rank][bank] = 0;
      activateQueued[rank][bank] = true;
      effectiveRow[rank][bank] = row;

      req->issueCycle = GetEventQueue()->GetCurrentCycle();

      bankQueues[rank][bank].push_back( MakeActivateRequest( req ) );
      bankQueues[rank][bank].push_back( req );

      rv = true;
    }
  else if( activateQueued[rank][bank] && effectiveRow[rank][bank] != row && bankQueues[rank][bank].empty() )
    {
      /* Any activate will request the starvation counter */
      starvationCounter[rank][bank] = 0;
      activateQueued[rank][bank] = true;
      effectiveRow[rank][bank] = row;

      req->issueCycle = GetEventQueue()->GetCurrentCycle();

      bankQueues[rank][bank].push_back( MakePrechargeRequest( req ) );
      bankQueues[rank][bank].push_back( MakeActivateRequest( req ) );
      bankQueues[rank][bank].push_back( req );

      rv = true;
    }
  else if( activateQueued[rank][bank] && effectiveRow[rank][bank] == row )
    {
      starvationCounter[rank][bank]++;

      req->issueCycle = GetEventQueue()->GetCurrentCycle();

      bankQueues[rank][bank].push_back( req );

      rv = true;
    }
  else
    {
      rv = false;
    }

  return rv;
}


void MemoryController::CycleCommandQueues( )
{
  for( ncounter_t rankIdx = 0; rankIdx < p->RANKS; rankIdx++ )
    {
      for( ncounter_t bankIdx = 0; bankIdx < p->BANKS; bankIdx++ )
        {
          ncounter_t i = (curRank + rankIdx)%p->RANKS;
          ncounter_t j = (curBank + bankIdx)%p->BANKS;
          FailReason fail;

          /* 
           *  Check for refresh. Wait for the command queue to be empty to
           *  make 
           */
          if( p->UseRefresh
              && refreshTimes[i][j] <= GetEventQueue()->GetCurrentCycle() 
              && bankQueues[i][j].empty() )
            {
              NVMainRequest tmpReq;

              tmpReq.address.SetTranslatedAddress( 0, 0, j, i, 0 ); 

              bankQueues[i][j].push_front( MakeRefreshRequest( &tmpReq ) );

              refreshTimes[i][j] += static_cast<ncycle_t>(static_cast<float>(p->tRFI) / (static_cast<float>(p->ROWS) / static_cast<float>(p->RefreshRows)));

              // added by Tao @ 01/23/2013
              /* 
               * if this bank needs refresh then other banks in this rank need
               * refresh too. so we do not need to move rank or bank 
               */
            }

          if( !bankQueues[i][j].empty( )
              && memory->IsIssuable( bankQueues[i][j].at( 0 ), &fail ) )
            {
              memory->IssueCommand( bankQueues[i][j].at( 0 ) );

              bankQueues[i][j].erase( bankQueues[i][j].begin( ) );

              MoveRankBank();
              return ; // we should return since one time only one command can be issued
            }
          else if( p->UseRefresh && (fail.reason == OPEN_REFRESH_WAITING || fail.reason == CLOSED_REFRESH_WAITING) )
            {
              //// annotated by Tao @ 01/25/2013 
              //// // Note: This may interrupt some read/write request, we can probably just wait
              //// // for that to complete before doing this, since there is a rather large (tREFI - 8*tREFI)
              //// // window to issue refresh requests.
              //// if( fail.reason == OPEN_REFRESH_WAITING )
              ////   {
              ////     /* Reactivate this row after precharge. */
              ////     bankQueues[i][j].push_front( MakeActivateRequest( bankQueues[i][j].at(0) ) );
              ////   }

              bankQueues[i][j].push_front( MakeRefreshRequest( bankQueues[i][j].at(0) ) );

              refreshTimes[i][j] += static_cast<ncycle_t>(static_cast<float>(p->tRFI) / (static_cast<float>(p->ROWS) / static_cast<float>(p->RefreshRows)));

              if( fail.reason == OPEN_REFRESH_WAITING )
                {
                  bankQueues[i][j].push_front( MakePrechargeRequest( bankQueues[i][j].at(0) ) );
                }
            }
          else if( p->UseRefresh && fail.reason == REFRESH_OPEN_FAILURE )
            {
              bankQueues[i][j].push_front( MakePrechargeRequest( bankQueues[i][j].at(0) ) );

              /* We precharged, so we need to mark the row as closed. */
              activateQueued[i][j] = false;
            }
          else if( !bankQueues[i][j].empty( ) )
            {
              NVMainRequest *queueHead = bankQueues[i][j].at( 0 );

              if( ( GetEventQueue()->GetCurrentCycle() - queueHead->issueCycle ) > 1000000 )
                {
                  std::cout << "WARNING: Operation could not be sent to memory after a very long time: "
                            << std::endl; 
                  std::cout << "         Address: 0x" << std::hex 
                            << queueHead->address.GetPhysicalAddress( )
                            << std::dec << ". Queued time: " << queueHead->arrivalCycle
                            << ". Current time: " << GetEventQueue()->GetCurrentCycle() << ". Type: " 
                            << queueHead->type << std::endl;

                  /* 
                   * added by Tao @ 01/25/2012, avoid print too much warning
                   * that eat the disk space 
                   */
                  exit(1);
                }
            }
        }
    }
}

/* 
 * MoveRankBank() increment curRank and/or curBank according to the scheduling
 * scheme
 * 0 -- Fixed Scheduling from Rank0 and Bank0
 * 1 -- Rank-first round-robin
 * 2 -- Bank-first round-robin
 */
void MemoryController::MoveRankBank( )
{
    if( p->ScheduleScheme == 1 )
    {
        /* increment Rank. if all ranks are looked, increment Bank then */
        curRank++;
        if( curRank == p->RANKS )
        {
            curRank = 0;
            curBank = (curBank + 1)%p->BANKS;
        }
    }
    else if( p->ScheduleScheme == 2 )
    {
        /* increment Bank. if all banks are looked, increment Rank then */
        curBank++;
        if( curBank == p->BANKS )
        {
            curBank = 0;
            curRank = (curRank + 1)%p->RANKS;
        }
    }

    /* if fixed scheduling is used, we do nothing */
}



void MemoryController::PrintStats( )
{
  memory->PrintStats( );
  translator->PrintStats( );
}



