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

#include "MemControl/StackFCFSTimer/StackFCFSTimer.h"
#include "Interconnect/StackInterface/StackInterface.h"
#include "SimInterface/GemsInterface/GemsInterface.h"

#include <iostream>
#include <cmath>
#include <cstdlib>


using namespace NVM;




StackFCFSTimer::StackFCFSTimer( Interconnect *memory, AddressTranslator *translator )
{
  /*
   *  First, we need to define how the memory is translated. The main() function already 
   *  sets the width of each partition based on the number of channels, ranks, banks, rows, and
   *  columns. For example, 4 banks would be 2-bits. 1 channel is 0-bits. 1024 columns is 10 bits
   *  of the address. Here, you just need to specify what order the partitions are in.
   *
   *  In the set order function, you define the order of the row, column, bank, rank then channel.
   *  For example, to set column as the lowest bits, it must be 1st in the order, so the second
   *  parameter should be set to 1.
   *
   *  In this system, the address is broken up as follows:
   *
   *  -------------------------------------------------------------------------------------------
   *  |     CHANNEL     |         ROW         |       BANK      |      RANK     |    COLUMN     |
   *  -------------------------------------------------------------------------------------------
   *
   *  So the orders are column first, rank second, bank third, row fourth, channel fifth.
   *
   *  void SetOrder( int row, int col, int bank, int rank, int channel );
   */
  //translator->GetTranslationMethod( )->SetOrder( 5, 4, 1, 2, 3 );
  translator->GetTranslationMethod( )->SetOrder( 5, 1, 4, 3, 2 );

  /*
   *  We'll need these classes later, so copy them. the "memory" and "translator" variables are
   *  defined in the protected section of the the MemoryController base class. 
   */
  SetMemory( memory );
  SetTranslator( translator );

  /*
   *  This memory controller will have two queues: An issue queue, and a re-issue or NACK queue.
   *  The issue queue is commandQueue[0] and the NACK queue is commandQueue[1].
   */
  InitQueues( 2 );

  configSet = false;
  MLR_value = 100;
  MLW_value = 100;
  slotTimer = 0;
  nackCount = 0;
  nackRequests = 0;
  averageAccess = 0.0f;
  accessCount = 0;

  std::cout << "Created a Stack Interface FCFS memory controller!" << std::endl;
}



unsigned int StackFCFSTimer::GetMLValue( BulkCommand /*cmd*/ )
{
  unsigned int retval = MLR_value;

  /*
  if( cmd == CMD_ACTREADPRE || cmd == CMD_PU_ACT_READ_PRE_PDPF )
    retval = MLR_value;
  else if( cmd == CMD_ACTWRITEPRE || cmd == CMD_PU_ACT_WRITE_PRE_PDPF )
    retval = MLW_value;
  else
    std::cout << "StackFCFSTimer: Can't determine ML value for unknown bulk command: "
	      << cmd << std::endl;
  */

  return retval;
}



/*
 *  This method is called whenever a new transaction from the processor issued to
 *  this memory controller / channel. All scheduling decisions should be made here.
 */
int StackFCFSTimer::StartCommand( MemOp *mop )
{
  MemOp *nextOp;

  
  /*
   *  Limit the number of commands in the queue. This will stall the caches/CPU.
   */
  if( commandQueue[0].size( ) >= 16 || commandQueue[1].size( ) >= 16 )
    return false;

  
  /*
   *  The stack interface always issues an activate+read+precharge bulk command.
   *  We will look for a slot in the stack interface network in Cycle().
   */
  nextOp = new MemOp( );

  *nextOp = *mop;

  if( nextOp->GetOperation( ) == READ )
    nextOp->SetBulkCmd( CMD_ACTREADPRE );
  else if( nextOp->GetOperation( ) == WRITE )
    nextOp->SetBulkCmd( CMD_ACTWRITEPRE );
  else
    std::cout << "StackFCFS: Unknown next op: " << nextOp->GetOperation( ) << std::endl;


  /*
   *  We need to send the command as the first command in the bulk
   *  command. The bank will automatically figure out the implicit
   *  commands from the bulk command.
   */
  nextOp->SetOperation( ACTIVATE );

  commandQueue[0].push_back( nextOp ); 

  /*
   *  Return whether the request could be queued. Return false if the queue is full.
   */
  return true;
}


void StackFCFSTimer::Cycle( )
{
  MemOp *nextOp;
  unsigned int issueQueue;


  /*
   *  This memory controller requires some configuration, so we need to configure
   *  if this was not already done, since configuration is not available in the
   *  constructor.
   */
  if( !configSet )
    {
      Config *conf = GetConfig( );

      rankCount = (unsigned int)conf->GetValue( "RANKS" );
      bankCount = (unsigned int)conf->GetValue( "BANKS" );
      
      bankTimer = new unsigned int * [ rankCount ];
      
      for( unsigned int i = 0; i < rankCount; i++ )
        {
          bankTimer[i] = new unsigned int[ bankCount ];
          for( unsigned int j = 0; j < bankCount; j++ )
            bankTimer[i][j] = 0;
        }      
      
      MLR_value = ( conf->GetValue( "tRCD" ) + conf->GetValue( "tBURST" ) + conf->GetValue( "tRTP" ) + 
                  conf->GetValue( "tRP" ) ) / 2;
      MLR_value *= (unsigned int)ceil( (double)(conf->GetValue( "CPUFreq" )) / (double)(conf->GetValue( "CLK" )) );

      MLW_value = conf->GetValue( "tRCD" ) + conf->GetValue( "tBURST" ) + conf->GetValue( "tCWD" ) +
                  conf->GetValue( "tWR" ) + conf->GetValue( "tRP" );
      MLW_value *= (unsigned int)ceil( (double)(conf->GetValue( "CPUFreq" )) / (double)(conf->GetValue( "CLK" )) );

      slotLen = conf->GetValue( "tBURST" ) + conf->GetValue( "tRTRS" );
      slotLen *= (unsigned int)ceil( (double)(conf->GetValue( "CPUFreq" )) / (double)(conf->GetValue( "CLK" )) );

      configSet = true;
    }


  
  /*
   *  Check if the queues are empty, if not, we will attempt to issue the command to memory.
   */
  if( !commandQueue[0].empty( ) || !commandQueue[1].empty( ) )
    {
      /*
       *  Make sure the bank timer is at 0 to prevent potentially many conflicting
       *  requests in a row. Search for the first request that is ready.
       */
      uint64_t bank, rank;
      bool foundReady = false;
      std::vector<MemOp *>::iterator iter;
      
      nextOp = NULL;
      issueQueue = 1;

      for( iter = commandQueue[1].begin( ); iter != commandQueue[1].end( ); ++iter )
        {
          nextOp = *iter;

          nextOp->GetAddress( ).GetTranslatedAddress( NULL, NULL, &bank, &rank, NULL );

          if( memory->IsIssuable( nextOp ) && bankTimer[rank][bank] == 0 && slotTimer == 0 )
            {
              foundReady = true;
              issueQueue = 1;

              iter = commandQueue[1].erase( iter );
              commandQueue[1].insert( commandQueue[1].begin( ), nextOp );

              break;
            }
        }
      
      if( !foundReady )
        {
          for( iter = commandQueue[0].begin( ); iter != commandQueue[0].end( ); ++iter )
            {
              nextOp = *iter;

              nextOp->GetAddress( ).GetTranslatedAddress( NULL, NULL, &bank, &rank, NULL );

              if( GetMemory( )->IsIssuable( nextOp ) && bankTimer[rank][bank] == 0 && slotTimer == 0 )
                {
                  foundReady = true;
                  issueQueue = 0;

                  iter = commandQueue[0].erase( iter );
                  commandQueue[0].insert( commandQueue[0].begin( ), nextOp );

                  break;
                }
            }
        }

      if( foundReady )
        nextOp = commandQueue[issueQueue].front( );
      else
        nextOp = NULL;


      if( nextOp != NULL )
        nextOp->GetAddress( ).GetTranslatedAddress( NULL, NULL, &bank, &rank, NULL );

      /*
       *  Find out if the command can be issued.
       */
      if( nextOp != NULL && memory->IsIssuable( nextOp ) && bankTimer[rank][bank] == 0 && slotTimer == 0 )
        {
          /*
           *  Copy bulk command to the nvmain request.
           */
          nextOp->GetRequest( )->bulkCmd = nextOp->GetBulkCmd( );


          /*
           *  If we can issue, send 
           */
          memory->IssueCommand( nextOp );

          bankTimer[rank][bank] = GetMLValue( nextOp->GetBulkCmd( ) );
          slotTimer = slotLen;

          if( !accessTime.count( nextOp->GetAddress( ).GetPhysicalAddress( ) ) )
            accessTime[ nextOp->GetAddress( ).GetPhysicalAddress( ) ] = currentCycle;

          commandQueue[issueQueue].erase( commandQueue[issueQueue].begin( ) );
        }
    }

  /*
   *  Check for completed operations.
   */
  StackInterface *stack;
  StackRequest *sreq;

  stack = (StackInterface *)GetMemory( );

  if( stack->GetCompletedRequest( &sreq ) )
    {
      if( stack->GetCompletedRequest( &sreq ) )
        {
          std::cout << "StackFCFSTimer: Error: Two requests in same slot." << std::endl;
          exit( 0 );
        }

      /* NACK */
      if( sreq->status == ACK_NACK )
        {
          //std::cout << "Warning: Request was NACK'd!!!!!: " 
          //	    << sreq->memOp->GetAddress( ).GetPhysicalAddress( ) << std::endl;
          
          /*
           *  Add this request to the END of the NACK queue (basically this
           *  is the entire reason for have two queues -- We can't simply
           *  place the request at the front of the issue queue, since this
           *  will cause NACK'd requests to be out of order.
           *
           *  If the NACK'd request was previously NACK'd, add it to the front.
           */
          if( !nackList.count( sreq->memOp->GetAddress( ).GetPhysicalAddress( ) ) )
            commandQueue[1].push_back( sreq->memOp );
          else
            commandQueue[1].insert( commandQueue[1].begin( ), sreq->memOp );

          /*
           *  Count the number of times this request was NACK'd
           */
          if( !nackList.count( sreq->memOp->GetAddress( ).GetPhysicalAddress( ) ) )
            nackList[ sreq->memOp->GetAddress( ).GetPhysicalAddress( ) ] = 0;
          else
            nackList[ sreq->memOp->GetAddress( ).GetPhysicalAddress( ) ]++;
        }
      /* ACK */
      else
        {
          if( sreq->memOp->GetOperation( ) == READ || sreq->memOp->GetOperation( ) == WRITE 
              || sreq->memOp->GetBulkCmd( ) == CMD_READPRE || sreq->memOp->GetBulkCmd( ) == CMD_WRITEPRE )
            {
              EndCommand( sreq->memOp, ENDMODE_NORMAL );
            }
          else if( sreq->memOp->GetBulkCmd( ) == CMD_ACTREADPRE || sreq->memOp->GetBulkCmd( ) == CMD_ACTWRITEPRE )
            {
              unsigned int endTime;

              endTime = config->GetValue( "tRCD" ) + config->GetValue( "tCAS" ) + config->GetValue( "tBURST" );

              EndCommand( sreq->memOp, ENDMODE_CUSTOM, endTime );
            }

          /*
           *  If the request is in the NACK list, increment the number of NACK'd
           *  requests and total number of times NACK'd
           */
          uint64_t nackd;

          if( ( nackd = nackList.count( sreq->memOp->GetAddress( ).GetPhysicalAddress( ) ) ) != 0 )
            {
              std::map< uint64_t, unsigned char >::iterator nackIter;

              nackCount += nackd;
              nackRequests++;
              
              nackIter = nackList.find( sreq->memOp->GetAddress( ).GetPhysicalAddress( ) );
              nackList.erase( nackIter );
            }

          /*
           *  Calculate the average memory access time by adding this result.
           *
           *  New Average = ( Old Average * Num Access + This Access Time ) / ( Num Accesses + 1 )
           */
          double thisAccess;
          std::map< uint64_t, uint64_t >::iterator timeIter;

          thisAccess = (double)( currentCycle - accessTime[ sreq->memOp->GetAddress( ).GetPhysicalAddress( ) ] );

          timeIter = accessTime.find( sreq->memOp->GetAddress( ).GetPhysicalAddress( ) );
          accessTime.erase( timeIter );

          //std::cout << "Request completed after " << thisAccess << " cycles." << std::endl;

          averageAccess = ( averageAccess * (double)accessCount + thisAccess ) / ( (double)accessCount + 1.0 );
          accessCount++;
        }

      delete sreq;
    }

  currentCycle++;
  memory->Cycle( );


  if( slotTimer > 0 )
    slotTimer--;

  for( unsigned int i = 0; i < rankCount; i++ )
    for( unsigned int j = 0; j < bankCount; j++ )
      if( bankTimer[i][j] > 0 )
        bankTimer[i][j]--;
}


void StackFCFSTimer::PrintStats( )
{
  std::cout << "Controller Stats:" << std::endl;
  std::cout << " --- Average Access Time: " << averageAccess << std::endl
	    << " --- Number of Accesses: " << accessCount << std::endl;
  std::cout << " --- NACK requests: " << nackRequests << std::endl
	    << " --- Number of NACKs: " << nackCount << std::endl;
  std::cout << " --- Requests in Access queue: " << commandQueue[0].size( ) << std::endl
	    << " --- Requests in NACK queue: " << commandQueue[1].size( ) << std::endl;

  MemoryController::PrintStats( );
}


