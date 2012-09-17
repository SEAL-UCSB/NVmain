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

#include "Prefetchers/NaivePrefetcher/NaivePrefetcher.h"


using namespace NVM;


bool NaivePrefetcher::DoPrefetch( NVMainRequest *triggerOp, std::vector<NVMAddress>& prefetchList )
{
  NVMAddress pfAddr;
  
  /*
   *  When an address is requested, we just get the next 3 to make a bundle of 4.
   */
  for( int i = 1; i < 4; i++ )
    {
      pfAddr = triggerOp->address;
      pfAddr.SetPhysicalAddress( triggerOp->address.GetPhysicalAddress( ) + 64*i );

      prefetchList.push_back( pfAddr );
    }

  return true;
}


