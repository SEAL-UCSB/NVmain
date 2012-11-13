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


#ifndef __MEMCONTROL_BASICDRC_H__
#define __MEMCONTROL_BASICDRC_H__



#include "src/MemoryController.h"
#include "Utils/Caches/CacheBank.h"


namespace NVM {


class NVMain;


class BasicDRC : public MemoryController
{
 public:
  BasicDRC( Interconnect *memory, AddressTranslator *translator );
  ~BasicDRC( );


  void SetConfig( Config *conf );
  void SetMainMemory( NVMain *mm );

  bool IssueAtomic( NVMainRequest *req );
  bool IssueCommand( NVMainRequest *req );
  bool RequestComplete( NVMainRequest *req );

  void Cycle( ncycle_t );

  void PrintStats( );

 private:
  NVMainRequest *MakeTagRequest( NVMainRequest *triggerRequest, int tag );
  NVMainRequest *MakeTagWriteRequest( NVMainRequest *triggerRequest );
  NVMainRequest *MakeDRCRequest( NVMainRequest *triggerReqest );

  /* Predicate to determine if a bank is locked. */
  class BankLocked : public SchedulingPredicate
  {
    friend class BasicDRC;

    private:
      BasicDRC &memoryController;

    public:
      BankLocked( BasicDRC &_memoryController ) : memoryController(_memoryController) { }

      bool operator() (uint64_t, uint64_t);
  };

  /* Predicate to determine if fill queue is full. */
  class FillQueueFull : public SchedulingPredicate
  {
    friend class BasicDRC;

    private:
      BasicDRC &memoryController;
      bool draining;

    public:
      FillQueueFull( BasicDRC &_memoryController ) : memoryController(_memoryController), draining(false) { }

      bool operator() (uint64_t, uint64_t);
  };

  /* Predicate to determine if fill queue is full. */
  class NoWriteBuffering : public SchedulingPredicate
  {
    friend class BasicDRC;

    private:
      BasicDRC &memoryController;

    public:
      NoWriteBuffering( BasicDRC &_memoryController ) : memoryController(_memoryController) { }

      bool operator() (uint64_t, uint64_t);
  };

  bool IssueDRCCommands( NVMainRequest *req );
  bool IssueFillCommands( NVMainRequest *req );

  void CalculateLatency( NVMainRequest *req, float *average, uint64_t *measured );
  void CalculateQueueLatency( NVMainRequest *req, float *average, uint64_t *measured );

  NVMTransactionQueue *drcQueue;
  NVMTransactionQueue *fillQueue;

  float averageHitLatency, averageHitQueueLatency;
  float averageMissLatency, averageMissQueueLatency;
  float averageMMLatency, averageMMQueueLatency;
  float averageFillLatency, averageFillQueueLatency;
  
  uint64_t measuredHitLatencies, measuredHitQueueLatencies;
  uint64_t measuredMissLatencies, measuredMissQueueLatencies;
  uint64_t measuredMMLatencies, measuredMMQueueLatencies;
  uint64_t measuredFillLatencies, measuredFillQueueLatencies;

  uint64_t mem_reads, mem_writes;
  uint64_t mm_reqs, mm_reads, fills;
  uint64_t rb_hits, rb_miss;
  uint64_t drcHits, drcMiss;
  uint64_t starvation_precharges;
  uint64_t psInterval;

  uint64_t fillQueueSize, drcQueueSize;

  bool **bankLocked;
  BankLocked locks;
  FillQueueFull FQF;
  NoWriteBuffering NWB;
  bool useWriteBuffer;

  NVMain *mainMemory;

  CacheBank ***functionalCache;

};



};



#endif


