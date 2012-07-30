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

#include "src/Queue.h"


using namespace NVM;




Queue::Queue( )
{
  cmdBus = NULL;
  dataBus = NULL;

  instructions.clear( );
}


Queue::~Queue( )
{
  
}



void Queue::PushFront( MemOp mop )
{
  instructions.push_front( mop );
}

void Queue::Enqueue( MemOp mop )
{
  instructions.push_back( mop );
}



void Queue::SetCommandBus( GenericBus *cBus )
{
  cmdBus = cBus;
}

void Queue::SetDataBus( GenericBus *dBus )
{
  dataBus = dBus;
}


void Queue::Cycle( )
{
  currentCycle++;

#if 0
  /* Check if there are operations in the queue to be placed on the bus. */
  if( instructions.size( ) > 0 )
    {
      /* If there is an operation in this cycle and the command bus is ready, send it out. */
      if( instructions[0].GetCycle( ) <= currentCycle && cmdBus->Ready( ) )
	{
	  cmdBus->SetState( BUS_BUSY );
	  
	  /* Delete the command from the queue. */
	  instructions.erase( instructions.begin( ) );
	}
    }
#endif 
}



void Queue::Print() 
{
  std::deque<MemOp>::iterator i;
  char OpTable[5][20] = { "NOP", "READ", "WRITE", "ACTIVATE", "PRECHARGE" };

  for( i = instructions.begin(); i != instructions.end(); ++i ) 
    {
      std::cout << OpTable[ (int)((*i).GetOperation( )) ] << " at "
	        << std::hex << "0x" << (*i).GetAddress( ) << " in cycle "
	        << std::dec << (*i).GetCycle( ) << std::endl;
    }
  std::cout << std::endl;
}

