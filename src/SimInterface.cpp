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


#include "src/SimInterface.h"
#include "src/Config.h"

#include <iostream>


using namespace NVM;



int SimInterface::GetDataAtAddress( uint64_t address, NVMDataBlock *data )
{
  int retval;

  if( !memoryData.count( address ) )
    {
      retval = 0;
    }
  else
    {
      *data = memoryData[ address ];
      retval = 1;
    }

  return retval;
}


void SimInterface::SetDataAtAddress( uint64_t address, NVMDataBlock data )
{
  memoryData[ address ] = data;

  if( !accessCounts.count( address ) )
    accessCounts[ address ] = 0;
  else
    {
      accessCounts[ address ]++;

      //if( accessCounts[ address ] > 2 )
      //  std::cout << "0x" << std::hex << address << std::dec << " was accessed "
      //            << accessCounts[ address ] << " times" << std::endl;
    }

}


void SimInterface::SetConfig( Config *config )
{
  conf = config;
}


Config *SimInterface::GetConfig( )
{
  return conf;
}
