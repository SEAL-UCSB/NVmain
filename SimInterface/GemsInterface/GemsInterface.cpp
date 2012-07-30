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

#include <stdlib.h>

#include "src/Config.h"
#include "SimInterface/GemsInterface/GemsInterface.h"
#include "system/System.h"
#include "common/Driver.h"
#include "profiler/Profiler.h"
#include "profiler/CacheProfiler.h"
#include "simics/interface.h"


using namespace NVM;



GemsInterface::GemsInterface( )
{
  gems_system_ptr = NULL;
  gems_eventQueue_ptr = NULL;
}


GemsInterface::~GemsInterface( )
{
}



unsigned int GemsInterface::GetInstructionCount( int core )
{
  return gems_system_ptr->getDriver( )->getInstructionCount( core );
}


unsigned int GemsInterface::GetCacheMisses( int core, int level )
{
  /*
   *  For level "0" return the total misses from all caches...
   */
  //if( level == 1 )
  //return (unsigned int)gems_system_ptr->getProfiler( )->getL1DCacheProfiler( )->getProcTotalMisses( );
  //else if( level == 2 )
  return (unsigned int)gems_system_ptr->getProfiler( )->getProcTotalMisses( core );

  //return 0;
}


unsigned int GemsInterface::GetUserMisses( int core )
{
  return (unsigned int)gems_system_ptr->getProfiler( )->getProcUserMisses( core );
}


unsigned int GemsInterface::GetCacheHits( int core, int level )
{
  return 0;
}


bool GemsInterface::HasInstructionCount( )
{
  return true;
}


bool GemsInterface::HasCacheMisses( )
{
  return true;
}


bool GemsInterface::HasCacheHits( )
{
  return true;
}


void GemsInterface::SetSystemPtr( System *system_ptr )
{
  gems_system_ptr = system_ptr;
}


void GemsInterface::SetEventQueuePtr( EventQueue *eventQueue_ptr )
{
  gems_eventQueue_ptr = eventQueue_ptr;
}


System *GemsInterface::GetSystemPtr( )
{
  return gems_system_ptr;
}


EventQueue *GemsInterface::GetEventQueuePtr( )
{
  return gems_eventQueue_ptr;
}


void GemsInterface::SetDataAtAddress( uint64_t /*address*/, NVMDataBlock /*data*/ )
{
  /*
   *  Simics stores the values of memory, so there's no reason
   *  to store it here. (Overloads default function which WILL
   *  store the values).
   */
}


int GemsInterface::GetDataAtAddress( uint64_t address, NVMDataBlock *data )
{
  unsigned int memBlockSize = GetConfig( )->GetValue( "BusWidth" ) / 8;
  char *buffer;

  buffer = new char [ memBlockSize ];

  SIMICS_read_physical_memory_buffer( 0, address, buffer, memBlockSize );

  for( unsigned int i = 0; i < memBlockSize; i++ )
    data->SetByte( i, (uint8_t)buffer[i] );
  
  return true;
}

