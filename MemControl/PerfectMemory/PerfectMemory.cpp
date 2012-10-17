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

#include "MemControl/PerfectMemory/PerfectMemory.h"

#ifndef TRACE
#include "base/statistics.hh"
#include "base/types.hh"
#include "sim/core.hh"
#include "sim/stat_control.hh"
#endif

#include <iostream>


using namespace NVM;


/*
 *  This simple memory controller is an example memory controller for NVMain, and operates as follows:
 *
 *  - Commands are returned immediately to the simulator as if the memory access latency is 0.
 */
PerfectMemory::PerfectMemory( Interconnect *memory, AddressTranslator *translator )
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

  std::cout << "Created a Perfect Memory memory controller!" << std::endl;
}



/*
 *  This method is called whenever a new transaction from the processor issued to
 *  this memory controller / channel. All scheduling decisions should be made here.
 */
bool PerfectMemory::IssueCommand( NVMainRequest *req )
{
  EndCommand( req, ENDMODE_IMMEDIATE );

  /*
   *  Return whether the request could be queued. Return false if the queue is full.
   */
  return true;
}


void PerfectMemory::Cycle( )
{
}


void PerfectMemory::PrintStats( )
{
#ifndef TRACE
  if( GetConfig( )->KeyExists( "CTL_DUMP" ) && GetConfig( )->GetString( "CTL_DUMP" ) == "true" )
    Stats::schedStatEvent( true, false );
#endif
}

