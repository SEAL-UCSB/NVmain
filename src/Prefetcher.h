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


#ifndef __NVMAIN_PREFETCHER_H__
#define __NVMAIN_PREFETCHER_H__



#include "include/NVMAddress.h"
#include "include/NVMainRequest.h"

#include <vector>



namespace NVM {


class Prefetcher
{
 public:
  Prefetcher( ) { }
  virtual ~Prefetcher( ) { }

  virtual bool NotifyAccess( NVMainRequest *accessOp, std::vector<NVMAddress>& prefetchList );
  virtual bool DoPrefetch( NVMainRequest *triggerOp, std::vector<NVMAddress>& prefetchList );

};


};



#endif

