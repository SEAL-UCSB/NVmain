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
  commandQueue = NULL;
  memory = NULL;
  translator = NULL;
  currentCycle = 0;
  this->SendCallback = NULL;
  this->RecvCallback = NULL;

  InitQueues( 1 );

  refreshUsed = false;
  refreshWaitQueue.clear( );
  refreshNeeded = NULL;
}

MemoryController::MemoryController( Interconnect *memory, AddressTranslator *translator )
{
  this->memory = memory;
  this->translator = translator;

  commandQueue = NULL;
}

AddressTranslator *MemoryController::GetAddressTranslator( )
{
  return translator;
}


void MemoryController::InitQueues( unsigned int numQueues )
{
  if( commandQueue != NULL )
    delete [] commandQueue;

  commandQueue = new std::vector<MemOp *>[numQueues];

  for( unsigned int i = 0; i < numQueues; i++ )
    commandQueue[i].clear( );
}


/*
 *  This function is called at every "clock cycle" simulated. If you need to write
 *  your own Cycle( ) function (for example to handle 1 queue per bank memory
 *  controllers) just overload this function, and call InitQueues() with the nubmer
 *  of queues you need.
 */
void MemoryController::Cycle( )
{
  MemOp *nextOp;
  
  /*
   *  Check if the queue is empty, if not, we will attempt to issue the command to memory.
   */
  if( !commandQueue[0].empty() )
    {
      /*
       *  Get the first transaction from the queue.
       */
      nextOp = commandQueue[0].front( );
	
      /*
       *  Find out if the command can be issued.
       */
      if( memory->IsIssuable( nextOp ) )
        {
          if( nextOp->GetOperation( ) == READ )
            {
              EndCommand( nextOp, ENDMODE_CRITICAL_WORD_FIRST );
            }
          else if( nextOp->GetOperation( ) == WRITE )
            {
              EndCommand( nextOp, ENDMODE_NORMAL );
            }

          nextOp->GetRequest( )->issueCycle = currentCycle;

          /*
           *  If we can issue, send 
           */
          memory->IssueCommand( nextOp );

          commandQueue[0].erase( commandQueue[0].begin( ) );
        }
    }


  currentCycle++;
  memory->Cycle( );
}


void MemoryController::FlushCompleted( )
{
  std::map<MemOp *, unsigned int>::iterator it;


  for( it = completedCommands.begin( ); it != completedCommands.end( ); it++ )
    {
      if( it->second == 0 )
        {
          //std::cout << "MC: 0x" << std::hex << it->first->GetRequest( )->address.GetPhysicalAddress( )
          //          << std::dec << " completed" << std::endl;
          it->first->GetRequest( )->status = MEM_REQUEST_COMPLETE;
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


void MemoryController::RequestComplete( NVMainRequest * /*request*/ )
{

}


void MemoryController::EndCommand( MemOp* mop, MCEndMode endMode, unsigned int customTime )
{
  unsigned int endTime;

  if( endMode == ENDMODE_CRITICAL_WORD_FIRST )
    {
      /*
       *  End the command after the first data cycle. Assumes the bus size
       *  is greater than or equal to half the word size (in DDR), or the 
       *  bus size is greater then or equal to the word size (in SDR).
       */
      endTime = config->GetValue( "tCAS" ) + 1; 

      completedCommands.insert( std::pair<MemOp *, unsigned int>( mop, endTime ) );
    }
  else if( endMode == ENDMODE_NORMAL )
    {
      endTime = config->GetValue( "tCAS" ) + config->GetValue( "tBURST" );

      completedCommands.insert( std::pair<MemOp *, unsigned int>( mop, endTime ) );
    }
  else if( endMode == ENDMODE_IMMEDIATE )
    {
      mop->GetRequest( )->status = MEM_REQUEST_COMPLETE;
    }
  else if( endMode == ENDMODE_CUSTOM )
    {
      completedCommands.insert( std::pair<MemOp *, unsigned int>( mop, customTime ) );
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

  NVMNetNode *parentInfo = new NVMNetNode( NVMNETDESTTYPE_MC, 0, 0, 0 );
  memory->AddParent( this, parentInfo );
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
  
  if( conf->GetString( "UseRefresh" ) == "true" )
    {
      refreshUsed = true;
      refreshNeeded = new bool*[ conf->GetValue( "RANKS" ) ];
      for( int i = 0; i < conf->GetValue( "RANKS" ); i++ )
        refreshNeeded[i] = new bool[ conf->GetValue( "BANKS" ) ];

      for( int i = 0; i < config->GetValue( "RANKS" ); i++ )
        for( int j = 0; j < config->GetValue( "BANKS" ); j++ )
          refreshNeeded[i][j] = false;
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


void MemoryController::SendMessage( unsigned int dest, void *message, int latency )
{
  MemoryControllerMessage *msg;

  msg = new MemoryControllerMessage( );

  msg->src = this->id;
  msg->dest = dest;
  msg->message = message;
  msg->latency = latency;

  (this->manager->*SendCallback)( msg );

  delete msg;
}


void MemoryController::RecvMessages(  )
{
  MemoryControllerMessage *msg;
  int rv = MSG_FOUND;

  while( rv == MSG_FOUND )
    {
      msg = new MemoryControllerMessage( );
      
      /* Receive callback will use this to find messages ready. */
      msg->dest = this->id;
      
      rv = (this->manager->*RecvCallback)( msg );

      /* Process Message using overloaded function */
      ProcessMessage( msg );

      delete msg;
    }
}


void MemoryController::ProcessMessage( MemoryControllerMessage */*msg*/ )
{
  /*
   *  This function should be overloaded, but it's not required.
   */
}


void MemoryController::SetSendCallback( MemoryControllerManager *manager, void (MemoryControllerManager::*sendCallback)( MemoryControllerMessage * ) )
{
  this->manager = manager;
  this->SendCallback = sendCallback;
}


void MemoryController::SetRecvCallback( MemoryControllerManager *manager, int  (MemoryControllerManager::*recvCallback)( MemoryControllerMessage * ) )
{
  this->manager = manager;
  this->RecvCallback = recvCallback;
}


void MemoryController::PrintStats( )
{
  memory->PrintStats( );
  translator->PrintStats( );
}



MemOp *MemoryController::BuildRefreshRequest( int rank, int bank )
{
  MemOp *refOp = new MemOp( );
  NVMainRequest * refReq = new NVMainRequest( );

  refReq->address.SetTranslatedAddress( 0, 0, bank, rank, 0 ); 
  refReq->type = REFRESH;
  refReq->bulkCmd = CMD_NOP;

  refOp->SetAddress( refReq->address );
  refOp->SetOperation( REFRESH );
  refOp->SetRequest( refReq );

  return refOp;
}


