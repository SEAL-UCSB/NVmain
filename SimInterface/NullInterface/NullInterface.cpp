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

#include "SimInterface/NullInterface/NullInterface.h"


using namespace NVM;




NullInterface::NullInterface( )
{

}


NullInterface::~NullInterface( )
{

}



unsigned int NullInterface::GetInstructionCount( int /*core*/ )
{
  return 0;
}


unsigned int NullInterface::GetCacheMisses( int /*core*/, int /*level*/ )
{
  return 0;
}


unsigned int NullInterface::GetCacheHits( int /*core*/, int /*level*/ )
{
  return 0;
}


bool NullInterface::HasInstructionCount( )
{
  return false;
}


bool NullInterface::HasCacheMisses( )
{
  return false;
}


bool NullInterface::HasCacheHits( )
{
  return false;
}



