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

#ifndef __UTILS_MISSMAP_H__
#define __UTILS_MISSMAP_H__


#include "Utils/Caches/CacheBank.h"
#include "src/Config.h"
#include "src/MemoryController.h"
#include "MemControl/MemoryControllerFactory.h"

#include <queue>


namespace NVM {



#define MISSMAP_READ 50
#define MISSMAP_WRITE 51
#define MISSMAP_MEMREAD 52
#define MISSMAP_FORCE_EVICT 53

class NVMain;
class LH_Cache;


class MissMap : public MemoryController
{
 public:
  MissMap( );
  ~MissMap( );

  void SetConfig( Config *conf );

  bool QueueFull( NVMainRequest *request );

  bool IssueAtomic( NVMainRequest *req );
  bool IssueCommand( NVMainRequest *req );
  bool RequestComplete( NVMainRequest *req );

  void Cycle( ncycle_t );

  void PrintStats( );

 private:
  CacheBank *missMap;
  std::queue<NVMainRequest *> missMapQueue;
  std::queue<NVMainRequest *> missMapFillQueue;
  uint64_t missMapQueueSize;

  NVMain *mainMemory;
  LH_Cache **drcChannels;
  ncounter_t numChannels;

  /* Stats. */
  uint64_t missMapAllocations, missMapWrites;
  uint64_t missMapHits, missMapMisses;
  uint64_t missMapForceEvicts;
  uint64_t missMapMemReads;

};


};


#endif

