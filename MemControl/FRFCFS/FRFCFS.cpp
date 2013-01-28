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

  /*
   *  We'll need these classes later, so copy them. the "memory" and "translator" variables are
   *  defined in the protected section of the the MemoryController base class. 
   */
  SetMemory( memory );
  SetTranslator( translator );

  std::cout << "Created a First Ready First Come First Serve memory controller!" << std::endl;

  queueSize = 32;
  starvationThreshold = 4;

  averageLatency = 0.0f;
  averageQueueLatency = 0.0f;

  measuredLatencies = 0;
  measuredQueueLatencies = 0;

  mem_reads = 0;
  mem_writes = 0;

  rb_hits = 0;
  rb_miss = 0;

  starvation_precharges = 0;

  psInterval = 0;
}


FRFCFS::~FRFCFS( )
{
  std::cout << "FRFCFS memory controller destroyed. " << transactionQueues[0].size( ) 
            << " commands still in memory queue." << std::endl;
}


void FRFCFS::SetConfig( Config *conf )
{
  if( conf->KeyExists( "StarvationThreshold" ) )
    {
      starvationThreshold = static_cast<unsigned int>( conf->GetValue( "StarvationThreshold" ) );
    }

  if( conf->KeyExists( "QueueSize" ) )
    {
      queueSize = static_cast<unsigned int>( conf->GetValue( "QueueSize" ) );
    }

  MemoryController::SetConfig( conf );
}


bool FRFCFS::QueueFull( NVMainRequest * /*req*/ )
{
  return (memQueue.size() >= queueSize);
}


/*
 *  This method is called whenever a new transaction from the processor issued to
 *  this memory controller / channel. All scheduling decisions should be made here.
 */
bool FRFCFS::IssueCommand( NVMainRequest *req )
{
  /*
   *  Limit the number of commands in the queue. This will stall the caches/CPU.
   */ 
  if( memQueue.size( ) >= queueSize )
    {
      return false;
    }

  req->arrivalCycle = GetEventQueue()->GetCurrentCycle();

  /* 
   *  Just push back the read/write. It's easier to inject dram commands than break it up
   *  here and attempt to remove them later.
   */
  memQueue.push_back( req );

  if( req->type == READ )
    mem_reads++;
  else
    mem_writes++;

#ifndef TRACE
  //Gem5Interface *face = (Gem5Interface*)GetConfig( )->GetSimInterface( );
  //cpu_insts = static_cast<uint64_t>(face->GetInstructionCount( 0 ));
#endif 

  //std::cout << "Request for 0x" << std::hex << req->address.GetPhysicalAddress( ) << std::dec
  //          << " arrived at " << req->arrivalCycle << std::endl;

  /*
   *  Return whether the request could be queued. Return false if the queue is full.
   */
  return true;
}


bool FRFCFS::RequestComplete( NVMainRequest * request )
{
  /* Only reads and writes are sent back to NVMain and checked for in the transaction queue. */
  if( request->type == READ || request->type == WRITE )
    {
      request->status = MEM_REQUEST_COMPLETE;
      request->completionCycle = GetEventQueue()->GetCurrentCycle();

      /* Update the average latencies based on this request for READ/WRITE only. */
      averageLatency = ((averageLatency * static_cast<float>(measuredLatencies))
                         + static_cast<float>(request->completionCycle)
                         - static_cast<float>(request->issueCycle))
                     / static_cast<float>(measuredLatencies+1);
      measuredLatencies += 1;

      averageQueueLatency = ((averageQueueLatency * static_cast<float>(measuredQueueLatencies))
                              + static_cast<float>(request->issueCycle)
                              - static_cast<float>(request->arrivalCycle))
                          / static_cast<float>(measuredQueueLatencies+1);
      measuredQueueLatencies += 1;

      //std::cout << "Request for 0x" << std::hex << request->address.GetPhysicalAddress( ) << std::dec
      //          << " arriving at " << request->arrivalCycle << " and issued at "
      //          << request->issueCycle << " completed at " << GetEventQueue()->GetCurrentCycle() 
      //          << std::endl;
    }

  if( request->type == REFRESH )
      ProcessRefreshPulse( request );
  else
  {
    if( request->owner == this )
      delete request;
    else
      GetParent( )->RequestComplete( request );
  }


  return true;
}


void FRFCFS::Cycle( ncycle_t )
{

  NVMainRequest *nextRequest = NULL;


  /* Check for starved requests BEFORE row buffer hits. */
  if( FindStarvedRequest( memQueue, &nextRequest ) )
    {
      rb_miss++;
      starvation_precharges++;
    }
  /* Check for row buffer hits. */
  else if( FindRowBufferHit( memQueue, &nextRequest) )
    {
      rb_hits++;
    }
  /* Find the oldest request that can be issued. */
  else if( FindOldestReadyRequest( memQueue, &nextRequest ) )
    {
      rb_miss++;
    }
  /* Find requests to a bank that is closed. */
  else if( FindClosedBankRequest( memQueue, &nextRequest ) )
    {
      rb_miss++;
    }
  else
    {
      nextRequest = NULL;
    }

  /* Issue the commands for this transaction. */
  if( nextRequest != NULL )
    {
      IssueMemoryCommands( nextRequest );
    }

  /* Issue any commands in the command queues. */
  CycleCommandQueues( );

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
  std::cout << "i" << psInterval << "." << statName << id << ".cpu_insts " << cpu_insts << std::endl;

  MemoryController::PrintStats( );

  psInterval++;
}



