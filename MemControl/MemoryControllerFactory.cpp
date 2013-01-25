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

#include "MemControl/MemoryControllerFactory.h"

#include "MemControl/FCFS/FCFS.h"
#include "MemControl/FRFCFS/FRFCFS.h"
#include "MemControl/FRFCFS-WQF/FRFCFS-WQF.h"
#include "MemControl/PerfectMemory/PerfectMemory.h"
#include "MemControl/DRAMCache/DRAMCache.h"

#include <iostream>


using namespace NVM;



MemoryController *MemoryControllerFactory::CreateNewController( std::string controller, Interconnect *memory, AddressTranslator *translator )
{
  MemoryController *memoryController = NULL;

  if( controller == "" )
    std::cout << "NVMain: MEM_CTL is not set in configuration file!" << std::endl;

  if( controller == "FCFS" )
    memoryController = new FCFS( memory, translator );
  else if( controller == "FRFCFS" )
    memoryController = new FRFCFS( memory, translator );
  else if( controller == "FRFCFS-WQF" || controller == "FRFCFS_WQF" )
    memoryController = new FRFCFS_WQF( memory, translator );
  else if( controller == "PerfectMemory" )
    memoryController = new PerfectMemory( memory, translator );
  else if( controller == "DRC" )
    memoryController = new DRAMCache( memory, translator );

  if( memoryController == NULL )
    std::cout << "NVMain: Unknown memory controller `" << controller << "'." << std::endl;

  return memoryController;
}


