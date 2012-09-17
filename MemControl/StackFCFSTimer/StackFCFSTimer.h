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

#ifndef __STACKFCFSTIMER_H__
#define __STACKFCFSTIMER_H__

#include "src/MemoryController.h"


namespace NVM {


class StackFCFSTimer : public MemoryController
{
 public:
  StackFCFSTimer( Interconnect *memory, AddressTranslator *translator );
  ~StackFCFSTimer( ) { }


  bool IssueCommand( NVMainRequest *req );

  void PrintStats( );

  void Cycle( );

 private:
  unsigned int **bankTimer, slotTimer;
  unsigned int MLR_value, MLW_value, slotLen;
  unsigned int bankCount, rankCount;
  bool configSet;

  std::map< uint64_t, uint64_t > accessTime;
  double averageAccess;
  uint64_t accessCount;
  std::map< uint64_t, unsigned char > nackList;
  uint64_t nackCount;
  uint64_t nackRequests;


  unsigned int GetMLValue( BulkCommand cmd );

};


};


#endif
