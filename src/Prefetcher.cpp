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
&dummy */


#include "src/Prefetcher.h"


using namespace NVM;



bool Prefetcher::NotifyAccess( NVMainRequest * /*accessOp*/, std::vector<NVMAddress>& /*prefetchList*/ )
{
  return false;
}


/*
 *  Default prefetcher - Don't prefetch anything.
 */
bool Prefetcher::DoPrefetch( NVMainRequest * /*triggerOp*/, std::vector<NVMAddress>& /*prefetchList*/ )
{
  return false;
}


