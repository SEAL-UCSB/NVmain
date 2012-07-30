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

#ifndef __STATCONTROLLER_H__
#define __STATCONTROLLER_H__

#include "src/MemoryController.h"

#include <map>

namespace NVM {


class StatController : public MemoryController
{
 public:
  StatController( Interconnect *memory, AddressTranslator *translator );
  ~StatController( ) { }


  int StartCommand( MemOp *mop );

  void PrintStats( );

 private:
  std::map< uint64_t, uint64_t > addressMap;
  uint64_t maxAccesses;
  uint64_t numWrites;
  uint64_t numReads;

};



};


#endif
