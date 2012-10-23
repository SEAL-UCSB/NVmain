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
  refreshWaitQueue.clear( );
  refreshNeeded = NULL;

  starvationThreshold = 4;
  starvationCounter = NULL;
  activateQueued = NULL;
  effectiveRow = NULL;
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


/*
 *  This function is called at every "clock cycle" simulated. If you need to write
 *  your own Cycle( ) function (for example to handle 1 queue per bank memory
 *  controllers) just overload this function, and call InitQueues() with the nubmer
 *  of queues you need.
 */
void MemoryController::Cycle( ncycle_t )
{
  NVMainRequest *nextReq;
  
  /*
   *  Check if the queue is empty, if not, we will attempt to issue the command to memory.
   */
  if( transactionQueues && !transactionQueues[0].empty() )
    {
      /*
       *  Get the first transaction from the queue.
       */
      nextReq = transactionQueues[0].front( );
	
      /*
       *  Find out if the command can be issued.
       */
      if( memory->IsIssuable( nextReq ) )
        {
          if( nextReq->type == READ )
            {
              EndCommand( nextReq, ENDMODE_CRITICAL_WORD_FIRST );
            }
          else if( nextReq->type == WRITE )
            {
              EndCommand( nextReq, ENDMODE_NORMAL );
            }

          nextReq->issueCycle = GetEventQueue()->GetCurrentCycle();

          /*
           *  If we can issue, send 
           */
          memory->IssueCommand( nextReq );

          transactionQueues[0].erase( transactionQueues[0].begin( ) );
        }
    }
}


void MemoryController::FlushCompleted( )
{
  std::map<NVMainRequest *, ncycle_t>::iterator it;


  for( it = completedCommands.begin( ); it != completedCommands.end( ); it++ )
    {
      if( it->second == 0 )
        {
          //std::cout << "MC: 0x" << std::hex << it->first->GetRequest( )->address.GetPhysicalAddress( )
          //          << std::dec << " completed" << std::endl;
          it->first->status = MEM_REQUEST_COMPLETE;

          RequestComplete( it->first );

          //std::cout << "Marking request 0x" << std::hex << (void*)it->first->GetRequest( ) << std::dec 
          //          << "completed" << std::endl;
          completedCommands.erase( it );
        }
      else
        {
          it->second--;
        }
    }
}


bool MemoryController::RequestComplete( NVMainRequest *request )
{
  bool rv = false;

  if( request->owner == this )
    {
      delete request;
      rv = true;
    }
  else
    {
      GetParent( )->RequestComplete( request );
    }

  return rv;
}


void MemoryController::EndCommand( NVMainRequest* req, MCEndMode endMode, ncycle_t customTime )
{
  ncycle_t endTime;

  if( endMode == ENDMODE_CRITICAL_WORD_FIRST )
    {
      /*
       *  End the command after the first data cycle. Assumes the bus size
       *  is greater than or equal to half the word size (in DDR), or the 
       *  bus size is greater then or equal to the word size (in SDR).
       */
      endTime = p->tCAS + 1; 

      completedCommands.insert( std::pair<NVMainRequest *, ncycle_t>( req, endTime ) );
    }
  else if( endMode == ENDMODE_NORMAL )
    {
      endTime = p->tCAS + p->tBURST;

      completedCommands.insert( std::pair<NVMainRequest *, ncycle_t>( req, endTime ) );
    }
  else if( endMode == ENDMODE_IMMEDIATE )
    {
      req->status = MEM_REQUEST_COMPLETE;

      completedCommands.insert( std::pair<NVMainRequest *, ncycle_t>( req, 0 ) );
    }
  else if( endMode == ENDMODE_CUSTOM )
    {
      completedCommands.insert( std::pair<NVMainRequest *, ncycle_t>( req, customTime ) );
    }
  else
    {
      std::cout << "Warning: Unknown endMode for EndCommand. This may result in a deadlock."
                << std::endl;
    }
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
  
  if( p->UseRefresh_set && p->UseRefresh )
    {
      refreshUsed = true;
      refreshNeeded = new bool*[ p->RANKS ];
      for( ncounter_t i = 0; i < p->RANKS; i++ )
        refreshNeeded[i] = new bool[ p->BANKS ];

      for( ncounter_t i = 0; i < p->RANKS; i++ )
        for( ncounter_t j = 0; j < p->BANKS; j++ )
          refreshNeeded[i][j] = false;
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


bool MemoryController::DummyPredicate::operator() (uint64_t, uint64_t)
{
  return true;
}


bool MemoryController::IssueMemoryCommands( NVMainRequest *req )
{
  bool rv = false;
  uint64_t rank, bank, row;

  req->address.GetTranslatedAddress( &row, NULL, &bank, &rank, NULL );

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
  for( unsigned int i = 0; i < p->RANKS; i++ )
    {
      for( unsigned int j = 0; j < p->BANKS; j++ )
        {
          if( !bankQueues[i][j].empty( )
              && memory->IsIssuable( bankQueues[i][j].at( 0 ) ) )
            {
              memory->IssueCommand( bankQueues[i][j].at( 0 ) );

              bankQueues[i][j].erase( bankQueues[i][j].begin( ) );
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
                }
            }
        }
    }
}




void MemoryController::PrintStats( )
{
  memory->PrintStats( );
  translator->PrintStats( );
}



NVMainRequest *MemoryController::BuildRefreshRequest( int rank, int bank )
{
  NVMainRequest * refReq = new NVMainRequest( );

  refReq->address.SetTranslatedAddress( 0, 0, bank, rank, 0 ); 
  refReq->type = REFRESH;
  refReq->bulkCmd = CMD_NOP;

  return refReq;
}


