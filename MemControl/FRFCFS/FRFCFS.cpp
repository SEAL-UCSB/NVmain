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

#include "MemControl/FRFCFS/FRFCFS.h"

#ifndef TRACE
#include "SimInterface/Gem5Interface/Gem5Interface.h"
#include "base/statistics.hh"
#include "base/types.hh"
#include "sim/core.hh"
#include "sim/stat_control.hh"
#endif

#include <iostream>
#include <set>
#include <assert.h>


using namespace NVM;



/*
 *  This simple memory controller is an example memory controller for NVMain, and operates as follows:
 *
 *  - After each read or write is issued, the page is closed.
 *    For each request, it simply prepends an activate before the read/write and appends a precharge
 *  - This memory controller leaves all banks and ranks in active mode (it does not do power management)
 */
FRFCFS::FRFCFS( Interconnect *memory, AddressTranslator *translator )
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
  translator->GetTranslationMethod( )->SetOrder( 5, 1, 4, 3, 2 );
  //translator->GetTranslationMethod( )->SetOrder( 3, 2, 1, 4, 5 );
  //translator->GetTranslationMethod( )->SetOrder( 3, 4, 5, 2, 1 );
  //translator->GetTranslationMethod( )->SetOrder( 5, 1, 2, 3, 4 );

  /*
   *  We'll need these classes later, so copy them. the "memory" and "translator" variables are
   *  defined in the protected section of the the MemoryController base class. 
   */
  SetMemory( memory );
  SetTranslator( translator );

  std::cout << "Created a First Ready First Come First Serve memory controller!" << std::endl;

  averageLatency = 0.0f;
  averageQueueLatency = 0.0f;

  measuredLatencies = 0;
  measuredQueueLatencies = 0;

  mem_reads = 0;
  mem_writes = 0;

  rb_hits = 0;
  rb_miss = 0;

  starvation_precharges = 0;
}


FRFCFS::~FRFCFS( )
{
  std::cout << "FRFCFS memory controller destroyed. " << commandQueue[0].size( ) 
            << " commands still in memory queue." << std::endl;
}


void FRFCFS::SetConfig( Config *conf )
{
  int ranks, banks;

  ranks = conf->GetValue( "RANKS" );
  banks = conf->GetValue( "BANKS" );

  if( conf->KeyExists( "FRFCFS_StarvationThreshold" ) )
    {
      starvationThreshold = static_cast<unsigned int>( conf->GetValue( "FRFCFS_StarvationThreshold" ) );
    }
  else
    {
      starvationThreshold = 4;
    }

  starved = new unsigned int*[ ranks ];
  actQueued = new bool * [ ranks ];
  actRow = new uint64_t * [ ranks ];
  bankQueue = new std::deque<MemOp *> * [ ranks ];
  for( int i = 0; i < ranks; i++ )
    {
      starved[i] = new unsigned int[banks];
      actQueued[i] = new bool [ banks ];
      actRow[i] = new uint64_t [ banks ];
      bankQueue[i] = new std::deque<MemOp *> [ banks ];
      for( int j = 0; j < banks; j++ )
        {
          starved[i][j] = 0;
          actQueued[i][j] = false;
          actRow[i][j] = 0;
        }
    }

  numBanks = banks;
  numRanks = ranks;


  conf->SetSimInterface( new Gem5Interface( ) );
  MemoryController::SetConfig( conf );
}


/*
 *  This method is called whenever a new transaction from the processor issued to
 *  this memory controller / channel. All scheduling decisions should be made here.
 */
int FRFCFS::StartCommand( MemOp *mop )
{
  /*
   *  Limit the number of commands in the queue. This will stall the caches/CPU.
   */ 
  if( memQueue.size( ) >= 15000 )
    {
      return false;
    }

  mop->SetBulkCmd( CMD_NOP );
  mop->GetRequest( )->arrivalCycle = currentCycle;

  /* 
   *  Just push back the read/write. It's easier to inject dram commands than break it up
   *  here and attempt to remove them later.
   */
  memQueue.push_back( mop );

  if( mop->GetOperation( ) == READ )
    mem_reads++;
  else
    mem_writes++;

  Gem5Interface *face = (Gem5Interface*)GetConfig( )->GetSimInterface( );
  unsigned int inst;
  inst = face->GetInstructionCount( 0 );

  /*
   *  Return whether the request could be queued. Return false if the queue is full.
   */
  return true;
}


void FRFCFS::RequestComplete( NVMainRequest * request )
{
  /*
   *  Search for the original request and mark it complete.
   */
  std::deque<MemOp *>::iterator it;
  MemOp *originalMemOp;
  NVMainRequest *originalRequest;

  originalMemOp = (MemOp *)request->reqInfo;
  originalRequest = originalMemOp->GetRequest( );

  for( it = issuedQueue.begin( ); it != issuedQueue.end( ); it++ )
    {
      if( (*it)->GetRequest( ) == originalRequest )
        break;
    }

  assert( it != issuedQueue.end( ) );

  originalRequest->status = MEM_REQUEST_COMPLETE;
  originalRequest->completionCycle = currentCycle;

  /* Update the average latencies based on this request. */
  averageLatency = ((averageLatency * static_cast<float>(measuredLatencies))
                     + static_cast<float>(originalRequest->completionCycle)
                     - static_cast<float>(originalRequest->issueCycle))
                 / static_cast<float>(measuredLatencies+1);
  measuredLatencies += 1;

  averageQueueLatency = ((averageQueueLatency * static_cast<float>(measuredQueueLatencies))
                          + static_cast<float>(originalRequest->issueCycle)
                          - static_cast<float>(originalRequest->arrivalCycle))
                      / static_cast<float>(measuredQueueLatencies+1);
  measuredQueueLatencies += 1;

  //std::cout << "Request arriving at " << originalRequest->arrivalCycle << " and issued at "
  //          << originalRequest->issueCycle << " completed at " << currentCycle << std::endl;

  /* Remove original request from issued queue. */
  issuedQueue.erase( it );
}


void FRFCFS::Cycle( )
{
  std::deque<MemOp *>::iterator it;
  bool scheduled = false;


  /* Check for starvation. */
  for( it = memQueue.begin( ) ; it != memQueue.end( ); it++ )
    {
      if( memQueue.size( ) == 0 || scheduled == true )
        break;


      uint64_t rank, bank, row;

      (*it)->GetRequest( )->address.GetTranslatedAddress( &row, NULL, &bank, &rank, NULL );

      /* If the bank is activated, and the activate row is not this row, and we can issue, and the bank is starved; issue */
      if( actQueued[rank][bank] 
          && actRow[rank][bank] != row
          && starved[rank][bank] >= starvationThreshold 
          && bankQueue[rank][bank].empty() )
        {
          MemOp *preOp = new MemOp( );
          MemOp *actOp = new MemOp( );
          NVMainRequest *preReq = new NVMainRequest( );
          NVMainRequest *actReq = new NVMainRequest( );

          preReq->type = PRECHARGE;
          preReq->bulkCmd = CMD_NOP;
          preReq->issueCycle = currentCycle;
          preOp->SetOperation( PRECHARGE );
          preOp->SetRequest( preReq );

          preReq->address.SetTranslatedAddress( 0, 0, bank, rank, 0 );
          preOp->SetAddress( preReq->address );

          actReq->type = ACTIVATE;
          actReq->bulkCmd = CMD_NOP;
          actReq->issueCycle = currentCycle;
          actOp->SetOperation( ACTIVATE );
          actOp->SetRequest( actReq );

          actReq->address.SetPhysicalAddress( (*it)->GetRequest( )->address.GetPhysicalAddress( ) );
          actReq->address.SetTranslatedAddress( row, 0, bank, rank, 0 );
          actOp->SetAddress( actReq->address );

          bankQueue[rank][bank].push_back( preOp );
          bankQueue[rank][bank].push_back( actOp );

          actQueued[rank][bank] = true;
          actRow[rank][bank] = row;


          MemOp *memOp = new MemOp( );
          NVMainRequest *memReq = new NVMainRequest( );

          *memOp = *(*it);
          *memReq = *(memOp->GetRequest( ));

          memReq->issueController = this;
          memReq->issueCycle = currentCycle;
          memReq->reqInfo = (void*)((*it));
          memOp->SetRequest( memReq );

          (*it)->GetRequest( )->issueCycle = currentCycle;
 
          bankQueue[rank][bank].push_back( memOp );
          issuedQueue.push_back( (*it) );
          memQueue.erase( it );

          starved[rank][bank] = 0; // reset starvation counter

          scheduled = true;
          rb_miss++; // we'll call is a row buffer miss so the numbers add up
          starvation_precharges++;
          break;
        }
    }

  
  /* Look for row buffer hits */
  for( it = memQueue.begin( ) ; it != memQueue.end( ); it++ )
    {
      if( memQueue.size( ) == 0 || scheduled == true )
        break;


      uint64_t rank, bank, row;

      (*it)->GetRequest( )->address.GetTranslatedAddress( &row, NULL, &bank, &rank, NULL );

      //std::cout << "0x" << std::hex << (*it)->GetRequest( )->address.GetPhysicalAddress( )
      //          << std::dec << " translates to " << rank << "," << bank << "," << row << std::endl;

      if( actQueued[rank][bank] && actRow[rank][bank] == row && bankQueue[rank][bank].empty() 
          /*&& starved[rank][bank] < starvationThreshold*/ )
        {
          MemOp *memOp = new MemOp( );
          NVMainRequest *memReq = new NVMainRequest( );

          *memOp = *(*it);
          *memReq = *(memOp->GetRequest( ));

          memReq->issueController = this;
          memReq->issueCycle = currentCycle;
          memReq->reqInfo = (void*)((*it));
          memOp->SetRequest( memReq );

          (*it)->GetRequest( )->issueCycle = currentCycle;
 
          bankQueue[rank][bank].push_back( memOp );
          issuedQueue.push_back( (*it) );
          memQueue.erase( it );

          starved[rank][bank]++;

          scheduled = true;
          rb_hits++;
          break;
        }
    }


  /* Look for banks we can close and open a new row */
  for( it = memQueue.begin( ) ; it != memQueue.end( ); it++ )
    {
      if( memQueue.size( ) == 0 || scheduled == true )
        break;


      uint64_t rank, bank, row;

      (*it)->GetRequest( )->address.GetTranslatedAddress( &row, NULL, &bank, &rank, NULL );

      if( actQueued[rank][bank] && actRow[rank][bank] != row && bankQueue[rank][bank].empty() )
        {
          MemOp *preOp = new MemOp( );
          MemOp *actOp = new MemOp( );
          NVMainRequest *preReq = new NVMainRequest( );
          NVMainRequest *actReq = new NVMainRequest( );

          preReq->type = PRECHARGE;
          preReq->bulkCmd = CMD_NOP;
          preReq->issueCycle = currentCycle;
          preOp->SetOperation( PRECHARGE );
          preOp->SetRequest( preReq );

          preReq->address.SetTranslatedAddress( 0, 0, bank, rank, 0 );
          preOp->SetAddress( preReq->address );

          actReq->type = ACTIVATE;
          actReq->bulkCmd = CMD_NOP;
          actReq->issueCycle = currentCycle;
          actOp->SetOperation( ACTIVATE );
          actOp->SetRequest( actReq );

          actReq->address.SetPhysicalAddress( (*it)->GetRequest( )->address.GetPhysicalAddress( ) );
          actReq->address.SetTranslatedAddress( row, 0, bank, rank, 0 );
          actOp->SetAddress( actReq->address );

          bankQueue[rank][bank].push_back( preOp );
          bankQueue[rank][bank].push_back( actOp );

          actQueued[rank][bank] = true;
          actRow[rank][bank] = row;


          MemOp *memOp = new MemOp( );
          NVMainRequest *memReq = new NVMainRequest( );

          *memOp = *(*it);
          *memReq = *(memOp->GetRequest( ));

          memReq->issueController = this;
          memReq->issueCycle = currentCycle;
          memReq->reqInfo = (void*)((*it));
          memOp->SetRequest( memReq );

          (*it)->GetRequest( )->issueCycle = currentCycle;
 
          bankQueue[rank][bank].push_back( memOp );
          issuedQueue.push_back( (*it) );
          memQueue.erase( it );

          starved[rank][bank] = 0;

          scheduled = true;
          rb_miss++;
          break;
        }
    }


  /* Look for closed banks we can issue to. */
  for( it = memQueue.begin( ) ; it != memQueue.end( ); it++ )
    {
      if( memQueue.size( ) == 0 || scheduled == true )
        break;


      uint64_t rank, bank, row;

      (*it)->GetRequest( )->address.GetTranslatedAddress( &row, NULL, &bank, &rank, NULL );

      if( !actQueued[rank][bank] && bankQueue[rank][bank].empty() )
        {
          MemOp *actOp = new MemOp( );
          NVMainRequest *actReq = new NVMainRequest( );

          actReq->type = ACTIVATE;
          actReq->bulkCmd = CMD_NOP;
          actReq->issueCycle = currentCycle;
          actOp->SetOperation( ACTIVATE );
          actOp->SetRequest( actReq );

          actReq->address.SetPhysicalAddress( (*it)->GetRequest( )->address.GetPhysicalAddress( ) );
          actReq->address.SetTranslatedAddress( row, 0, bank, rank, 0 );
          actOp->SetAddress( actReq->address );

          bankQueue[rank][bank].push_back( actOp );

          actQueued[rank][bank] = true;
          actRow[rank][bank] = row;


          MemOp *memOp = new MemOp( );
          NVMainRequest *memReq = new NVMainRequest( );

          *memOp = *(*it);
          *memReq = *(memOp->GetRequest( ));

          memReq->issueController = this;
          memReq->issueCycle = currentCycle;
          memReq->reqInfo = (void*)((*it));
          memOp->SetRequest( memReq );

          (*it)->GetRequest( )->issueCycle = currentCycle;
 
          bankQueue[rank][bank].push_back( memOp );
          issuedQueue.push_back( (*it) );
          memQueue.erase( it );

          starved[rank][bank] = 0;

          scheduled = true;
          rb_miss++;
          break;
        }
    }



  /*
   *  Finally, actually do the issuing of the raw DRAM commands to the banks.
   */
  for( unsigned int i = 0; i < numRanks; i++ )
    {
      for( unsigned int j = 0; j < numBanks; j++ )
        {
          if( !bankQueue[i][j].empty( )
              && memory->IsIssuable( bankQueue[i][j].at( 0 ) ) )
            {
              memory->IssueCommand( bankQueue[i][j].at( 0 ) );

              bankQueue[i][j].erase( bankQueue[i][j].begin( ) );
            }
          else if( !bankQueue[i][j].empty( ) )
            {
              MemOp *queueHead = bankQueue[i][j].at( 0 );

              if( ( currentCycle - queueHead->GetRequest( )->issueCycle ) > 1000000 )
                {
                  std::cout << "WARNING: Operation could not be sent to memory after a very long time: "
                            << std::endl; 
                  std::cout << "         Address: 0x" << std::hex 
                            << queueHead->GetRequest( )->address.GetPhysicalAddress( )
                            << std::dec << ". Queued time: " << queueHead->GetRequest( )->arrivalCycle
                            << ". Current time: " << currentCycle << ". Type: " 
                            << queueHead->GetOperation( ) << std::endl;
                }
            }
        }
    }



  currentCycle++;
  memory->Cycle( );
}


void FRFCFS::PrintStats( )
{
  std::cout << "i" << psInterval << "." << statName << id << ".mem_reads " << mem_reads << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".mem_writes " << mem_writes << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".rb_hits " << rb_hits << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".rb_miss " << rb_miss << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".starvation_precharges " << starvation_precharges << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".averageLatency " << averageLatency << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".averageQueueLatency " << averageQueueLatency << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".measuredLatencies " << measuredLatencies << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".measuredQueueLatencies " << measuredQueueLatencies << std::endl;

  MemoryController::PrintStats( );

  psInterval++;

#ifndef TRACE
  std::cout << "DUMPING GEM5 TRACE" << std::endl;
  if( GetConfig( )->KeyExists( "CTL_DUMP" ) && GetConfig( )->GetString( "CTL_DUMP" ) == "true" )
    Stats::schedStatEvent( true, false );
#endif
}



