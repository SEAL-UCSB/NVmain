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

#include "MemControl/FCFS/FCFS.h"
#include "SimInterface/GemsInterface/GemsInterface.h"

#include <iostream>


using namespace NVM;



/*
 *  This simple memory controller is an example memory controller for NVMain, and operates as follows:
 *
 *  - After each read or write is issued, the page is closed.
 *    For each request, it simply prepends an activate before the read/write and appends a precharge
 *  - This memory controller leaves all banks and ranks in active mode (it does not do power management)
 */
FCFS::FCFS( Interconnect *memory, AddressTranslator *translator )
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
  //translator->GetTranslationMethod( )->SetOrder( 5, 4, 1, 3, 2 );

  /*
   *  We'll need these classes later, so copy them. the "memory" and "translator" variables are
   *  defined in the protected section of the the MemoryController base class. 
   */
  SetMemory( memory );
  SetTranslator( translator );

  std::cout << "Created a Simple Close Page memory controller!" << std::endl;

  queueSize = 32;

  averageLatency = 0.0f;
  averageQueueLatency = 0.0f;

  measuredLatencies = 0;
  measuredQueueLatencies = 0;

  mem_reads = 0;
  mem_writes = 0;

  rb_hits = 0;
  rb_miss = 0;

  psInterval = 0;

  InitQueues( 1 );
}



void FCFS::SetConfig( Config *conf )
{
  if( conf->KeyExists( "QueueSize" ) )
    {
      queueSize = static_cast<unsigned int>( conf->GetValue( "QueueSize" ) );
    }

  MemoryController::SetConfig( conf );
}



bool FCFS::QueueFull( NVMainRequest * /*req*/ )
{
  return( transactionQueues[0].size() >= queueSize );
}




bool FCFS::RequestComplete( NVMainRequest * request )
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


  if( request->owner == this )
    delete request;
  else
    GetParent( )->RequestComplete( request );

  return true;
}


/*
 *  This method is called whenever a new transaction from the processor issued to
 *  this memory controller / channel. All scheduling decisions should be made here.
 */
bool FCFS::IssueCommand( NVMainRequest *req )
{
  /* Allow up to 16 read/writes outstanding. */
  if( transactionQueues[0].size( ) >= queueSize )
    return false;

  if( req->type == READ )
    mem_reads++;
  else
    mem_writes++;

  transactionQueues[0].push_back( req );

  /*
   *  Return whether the request could be queued. Return false if the queue is full.
   */
  return true;
}



void FCFS::Cycle( ncycle_t )
{
  NVMainRequest *nextReq = NULL;

  /* Simply get the oldest request */
  if( !FindOldestReadyRequest( transactionQueues[0], &nextReq ) )
    {
      /* No oldest ready request, check for non-activated banks. */
      (void)FindClosedBankRequest( transactionQueues[0], &nextReq );
    }

  if( nextReq != NULL )
    {
      IssueMemoryCommands( nextReq );
    }

  CycleCommandQueues( );
}



void FCFS::PrintStats( )
{
  std::cout << "i" << psInterval << "." << statName << id << ".mem_reads " << mem_reads << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".mem_writes " << mem_writes << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".rb_hits " << rb_hits << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".rb_miss " << rb_miss << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".averageLatency " << averageLatency << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".averageQueueLatency " << averageQueueLatency << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".measuredLatencies " << measuredLatencies << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".measuredQueueLatencies " << measuredQueueLatencies << std::endl;

  MemoryController::PrintStats( );

  psInterval++;
}

