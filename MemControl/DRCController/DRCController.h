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

#ifndef __MEMCONTROL_DRCCONTROLLER_H__
#define __MEMCONTROL_DRCCONTROLLER_H__

#include "src/MemoryController.h"
#include "src/Prefetcher.h"
#include "Utils/Caches/CacheBank.h"

#include <deque>
#include <set>


namespace NVM {


class DRCController : public MemoryController
{
 public:
  DRCController( Interconnect *memory, AddressTranslator *translator );
  ~DRCController( );


  int StartCommand( MemOp *mop );
  void RequestComplete( NVMainRequest *request );
  bool QueueFull( NVMainRequest *request );

  void SetConfig( Config *confg );

  void Cycle( );

  void PrintStats( );

 private:
  AddressTranslator *mmTranslator;
  TranslationMethod *mmMethod;
  Config *mmConfig;
  MemoryController **mmController;
  Interconnect **mmMemory;
  CacheBank ***functionalCache;
  CacheBank *installCache;
  CacheBank *missMap;
  Prefetcher *prefetcher;
  unsigned int **lastClose;
  unsigned int numRanks, numBanks;
  unsigned int mmChannels;

  uint64_t watchAddr;

  /* Stats */
  uint64_t app_reads, app_writes;
  uint64_t misses, read_hits, write_hits;
  uint64_t evictions, clean_evictions, dirty_evictions;
  float averageLatency, averageQueueLatency; 
  uint64_t measuredLatencies, measuredQueueLatencies;
  float averageMMLatency, averageMMQueueLatency; 
  uint64_t measuredMMLatencies, measuredMMQueueLatencies;
  uint64_t mmEvicts, cleanMMEvicts, dirtyMMEvicts;
  uint64_t mmForceEvicts;
  uint64_t tag_requeries;
  uint64_t missMapHits, missMapMisses;
  uint64_t prefetchesIssued, prefetchHits, prefetchMisses;
  uint64_t missCold, missConflict;
  uint64_t maxDRCQueue, totalDRCQueue, countDRCQueue;
  uint64_t maxPFWQueue, totalPFWQueue, countPFWQueue;
  float averageDRCQueue, averagePFWQueue;
  uint64_t longAssRequests;
  uint64_t drcRBhits, drcRBmiss;
  uint64_t drcRPdrops, drcUPdrops, drcUDdrops;

  uint64_t icHits;
  uint64_t icInstalledEvicts, icUninstalledEvicts;
  uint64_t icEvicts, icDemandRefEvicts, icDemandUnrefEvicts;
  uint64_t icPrefetchRefEvicts, icPrefetchUnrefEvicts;

  void UpdateAverageLatency( NVMainRequest *timedRequest );
  void UpdateAverageMMLatency( NVMainRequest *timedRequest );
  void MissMapInstall( NVMainRequest *request );
  void MissMapEvict( NVMAddress *victim );
  void InjectPrefetch( MemOp *mop );
  bool PrefetchInProgress( NVMAddress addr );
  bool AddressQueued( NVMAddress addr );

  std::deque<MemOp *> drcQueue;
  std::deque<MemOp *> issuedQueue;
  std::deque<MemOp *> mmQueue;
  std::deque<MemOp *> **bankQueue;
  std::deque<MemOp *> wbQueue;

  std::deque<MemOp *> pfwaitQueue;
  std::deque<MemOp *> pfInProgress;
  std::deque<MemOp *> writeList;
  std::deque<MemOp *> installList;

  std::map<uint64_t, uint64_t> rpRefCount; 
  std::map<uint64_t, uint64_t> upRefCount; 
  std::map<uint64_t, uint64_t> rdRefCount; 
  std::map<uint64_t, uint64_t> udRefCount; 

  std::set<uint64_t> allPrefetches;
  std::set<uint64_t> countedPrefetches;

  uint64_t maxQueueLength;
  bool **bankLocked;
  bool **actQueued;
  uint64_t **actRow;
};


};

#endif
