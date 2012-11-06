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


#include "MemControl/FRFCFS-WQF/FRFCFS-WQF.h"


using namespace NVM;



/*
 *  This memory controller implements a 'dumb' first-ready first-come first-server controller
 *  with write queue. The write queue drain policy is 'dumb' in the sense that it only starts
 *  a drain when the write queue is completely full, and drains until empty.
 */
FRFCFS_WQF::FRFCFS_WQF( Interconnect *memory, AddressTranslator *translator )
  : WQF(*this), WQFc(&WQF) // Important! Initialize the predicates
{
  translator->GetTranslationMethod( )->SetOrder( 5, 1, 4, 3, 2 );

  SetMemory( memory );
  SetTranslator( translator );

  std::cout << "Created a First Ready First Come First Serve memory controller with write queue!" << std::endl;

  /* Memory controller options. */
  readQueueSize = 32;
  writeQueueSize = 8;
  starvationThreshold = 4;

  /* Memory controller statistics. */
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


FRFCFS_WQF::~FRFCFS_WQF( )
{
}



void FRFCFS_WQF::SetConfig( Config *conf )
{
  if( conf->KeyExists( "StarvationThreshold" ) )
    starvationThreshold = static_cast<unsigned int>( conf->GetValue( "StarvationThreshold" ) );

  if( conf->KeyExists( "ReadQueueSize" ) )
    readQueueSize = static_cast<unsigned int>( conf->GetValue( "ReadQueueSize" ) );

  if( conf->KeyExists( "WriteQueueSize" ) )
    writeQueueSize = static_cast<unsigned int>( conf->GetValue( "WriteQueueSize" ) );

  MemoryController::SetConfig( conf );
}


bool FRFCFS_WQF::QueueFull( NVMainRequest * /*req*/ )
{
  /*
   *  So this function is annoying. Ruby/gem5 will ask if the queue is full, but 
   *  does not provide any information about the next request which makes it 
   *  impossible to determine if the queue the next request will actually go to
   *  is full. Therefore we return true if any of the queues are full.
   */
  return ( (readQueue.size() >= readQueueSize) || (writeQueue.size() >= writeQueueSize) );
}



bool FRFCFS_WQF::IssueCommand( NVMainRequest *request )
{
  if( (request->type == READ  && readQueue.size()  >= readQueueSize) ||
      (request->type == WRITE && writeQueue.size() >= writeQueueSize) )
    {
      return false;
    }


  request->arrivalCycle = GetEventQueue()->GetCurrentCycle();

  if( request->type == READ )
    {
      readQueue.push_back( request );

      mem_reads++;
    }
  else if( request->type == WRITE )
    {
      writeQueue.push_back( request );

      mem_writes++;
    }
  else
    {
      return false;
    }


  return true;
}


bool FRFCFS_WQF::RequestComplete( NVMainRequest * request )
{
  /* Only reads and writes are sent back to NVMain and checked for in the transaction queue. */
  if( request->type == READ || request->type == WRITE )
    {
      request->status = MEM_REQUEST_COMPLETE; // this isn't really used anymore, but doesn't hurt 
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
    }


  if( request->owner == this )
    delete request;
  else
    GetParent( )->RequestComplete( request );

  return true;
}



bool FRFCFS_WQF::WriteQueueFull::operator() (uint64_t, uint64_t)
{
  /*
   *  If the write queue becomes full/passes our threshold, start the drain.
   *  Otherwise, if the write queue becomes empty/below a threshold, stop.
   */
  /* Notes: Can easily replace memoryController.writeQueueSize and 0 with a threshold. */
  if( draining == false && memoryController.writeQueue.size() >= memoryController.writeQueueSize )
    {
      draining = true;
    }
  else if( draining == true && memoryController.writeQueue.size() == 0 )
    {
      draining = false;
    }

  return draining;
}



void FRFCFS_WQF::Cycle( ncycle_t )
{
  NVMainRequest *nextRequest = NULL;


  /*
   *  Our scheduling algorithm for both the read and write queue is the same:
   *
   *  1) Issue any starved requests
   *  2) Issue row-buffer hits
   *  3) Issue any ready command
   *
   *  First we check the write queue then the read queue. The predicate returns
   *  true if we are draining. If we aren't draining the functions will return
   *  false always, and thus nothing from the write queue will be scheduled.
   */
  if( FindStarvedRequest( writeQueue, &nextRequest, WQF ) )
    {
      rb_miss++;
      starvation_precharges++;
    }
  else if( FindRowBufferHit( writeQueue, &nextRequest, WQF ) )
    {
      rb_hits++;
    }
  else if( FindOldestReadyRequest( writeQueue, &nextRequest, WQF ) )
    {
      rb_miss++;
    }
  else if( FindClosedBankRequest( writeQueue, &nextRequest, WQF ) )
    {
      rb_miss++;
    }

  
  /* Only issue reads if we aren't draining. */
  if( FindStarvedRequest( readQueue, &nextRequest, WQFc ) )
    {
      rb_miss++;
      starvation_precharges++;
    }
  else if( FindRowBufferHit( readQueue, &nextRequest, WQFc ) )
    {
      rb_hits++;
    }
  else if( FindOldestReadyRequest( readQueue, &nextRequest, WQFc ) )
    {
      rb_miss++;
    }
  else if( FindClosedBankRequest( readQueue, &nextRequest, WQFc ) )
    {
      rb_miss++;
    }


  /* Issue the memory transaction as a series of commands to the command queue. */
  if( nextRequest != NULL )
    {
      IssueMemoryCommands( nextRequest );
    }


  /* Issue memory commands from the command queue. */
  CycleCommandQueues( );
}




void FRFCFS_WQF::PrintStats( )
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
}
