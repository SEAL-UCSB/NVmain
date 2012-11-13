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


#ifndef __MEMCONTROL_DRAMCACHE_H__
#define __MEMCONTROL_DRAMCACHE_H__



#include "src/MemoryController.h"
#include "Utils/Caches/CacheBank.h"
#include "MemControl/BasicDRC/BasicDRC.h"


namespace NVM {


class NVMain;


class DRAMCache : public MemoryController
{
 public:
  DRAMCache( Interconnect *memory, AddressTranslator *translator );
  ~DRAMCache( );


  void SetConfig( Config *conf );

  bool IssueAtomic( NVMainRequest *req );
  bool IssueCommand( NVMainRequest *req );
  bool RequestComplete( NVMainRequest *req );

  void Cycle( ncycle_t );

  void PrintStats( );

 private:
  NVMain *mainMemory;
  BasicDRC **drcChannels;
  ncounter_t numChannels;

};



};



#endif


