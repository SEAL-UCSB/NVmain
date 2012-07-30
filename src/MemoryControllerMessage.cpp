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

#include "src/MemoryControllerMessage.h"


using namespace NVM;



MemoryControllerMessage::MemoryControllerMessage( )
{
  /* Latency of -1 implies default latency (not specified by user). */
  latency = -1;
  src = 0;
  dest = 0;
  message = NULL;
}


MemoryControllerMessage::~MemoryControllerMessage( )
{

}


