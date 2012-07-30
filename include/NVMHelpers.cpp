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


#include "include/NVMHelpers.h"


namespace NVM {


int mlog2( int num )
{
  int retVal = -1;
  int newNum = num;

  if( num < 2 )
    return 0;

  while( newNum != 0 )
    {
      retVal++;
      newNum >>= 1;
    }

  return retVal;
}



std::string GetFilePath( std::string file )
{
  size_t last_sep;

  last_sep = file.find_last_of( "/\\" );
  
  return file.substr( 0, last_sep+1 );
} 


};


