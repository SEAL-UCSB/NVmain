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

#include "MemControl/StatController/StatController.h"


#include <iostream>


using namespace NVM;


/*
 *  This "memory controller" just takes access statistics.
 */
StatController::StatController( Interconnect *memory, AddressTranslator *translator )
{
  /*
   *  First, we need to define how the memory is translated. The main() function already 
   *  sets the width of each partition based on the number of channels, ranks, banks, rows, and
   *  columns. For example, 4 banks would be 2-bits. 1 channel is 0-bits. 1024 columns is 10 bits
   *  of the address. Here, you just need to specify what order the partitions are in.
   *
   *  In this system, the address is broken up as follows:
   *
   *  -------------------------------------------------------------------------------------------
   *  |     COLUMN     |         RANK         |       BANK      |      ROW     |    CHANNEL     |
   *  -------------------------------------------------------------------------------------------
   */
  translator->GetTranslationMethod( )->SetOrder( 4, 1, 3, 2, 5 );


  /*
   *  We'll need these classes later, so copy them to our private member variables. 
   */
  this->memory = memory;
  this->translator = translator;

  std::cout << "Created a Stat memory controller!" << std::endl;

  maxAccesses = 0;

  numWrites = 0;
  numReads = 0;
}



int StatController::StartCommand( MemOp *mop )
{
  uint64_t addr = mop->GetRequest( )->address.GetPhysicalAddress( );

  if( !addressMap.count( addr ) )
    {
      addressMap.insert( std::pair< uint64_t, uint64_t >( addr, 1 ) ); 
      if( maxAccesses < 1 )
        maxAccesses = 1;
    }
  else
    {
      addressMap[ addr ]++;
      if( addressMap[ addr ] > maxAccesses )
        maxAccesses = addressMap[ addr ];
    }


  if( mop->GetRequest( )->type == READ )
    numReads++;
  else if( mop->GetRequest( )->type == WRITE )
    numWrites++;


  /*
   *  Since we are only taking stats, just mark the request as complete.
   */
  mop->GetRequest( )->status = MEM_REQUEST_COMPLETE;

  /*
   *  Return if the request could be queued or not. Return false if the queue is full.
   */
  return true;
}



void StatController::PrintStats( )
{
  std::map< uint64_t, uint64_t > accessDist;
  std::map< uint64_t, uint64_t >::iterator it;

  for( int i = (int)maxAccesses; i >= 0; --i )
    {
      accessDist.insert( std::pair< uint64_t, uint64_t >( i, 0 ) );

      for( it = addressMap.begin( ); it != addressMap.end( ); ++it )
        {
          if( it->second == static_cast<uint64_t>(i) )
            accessDist[ i ]++;
        }
    }

  std::cout << "Access counts range from 0 to " << maxAccesses << std::endl;
  std::cout << "Writes: " << numWrites << ". Reads: " << numReads << std::endl;

  for( it = accessDist.begin( ); it != accessDist.end( ); ++it )
    {
      std::cout << it->first << " accesses to " << it->second << " addresses" << std::endl;
    }

  /* Remember to call the base class so we get statistics for the rest
   * of the memory system!
   */
  MemoryController::PrintStats( );
}


