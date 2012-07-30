/*
 *  this file is part of nvmain- a cycle accurate timing, bit-accurate
 *  energy simulator for non-volatile memory. originally developed by 
 *  matt poremba at the pennsylvania state university.
 *
 *  website: http://www.cse.psu.edu/~poremba/nvmain/
 *  email: mrp5060@psu.edu
 *
 *  ---------------------------------------------------------------------
 *
 *  if you use this software for publishable research, please include 
 *  the original nvmain paper in the citation list and mention the use 
 *  of nvmain.
 *
 */

#ifndef __INTERCONNECT_OFFCHIPBUS_H__
#define __INTERCONNECT_OFFCHIPBUS_H__


#include "src/Rank.h"
#include "src/Cycler.h"
#include "src/Interconnect.h"
#include "src/NVMNet.h"

#include <list>


namespace NVM {


class OffChipBus : public Interconnect
{
 public:
  OffChipBus( );
  ~OffChipBus( );

  void SetConfig( Config *c );

  bool IssueCommand( MemOp *mop );
  bool IsIssuable( MemOp *mop, ncycle_t delay = 0 );

  void RequestComplete( NVMainRequest *request );

  ncycle_t GetNextActivate( uint64_t rank, uint64_t bank );
  ncycle_t GetNextRead( uint64_t rank, uint64_t bank );
  ncycle_t GetNextWrite( uint64_t rank, uint64_t bank );
  ncycle_t GetNextPrecharge( uint64_t rank, uint64_t bank );
  ncycle_t GetNextRefresh( uint64_t rank, uint64_t bank );

  void PrintStats( );

  void Cycle( );

  void RecvMessage( NVMNetMessage * ) { }

  Rank *GetRank( uint64_t rank ) { return ranks[rank]; }

 private:
  struct DelayedReq
  {
    NVMainRequest *req;
    ncounter_t delay;
  };

  class zero_delay
  {
   public:
    bool operator() ( DelayedReq *dreq ) 
      {
        //hack to prevent memory leaks
        bool done = (dreq->delay == 0);

        if( done ) delete dreq;

        return done; 
      }
  };

  bool configSet;
  ncounter_t numRanks;
  ncycle_t currentCycle;
  ncycle_t offChipDelay;
  float syncValue;

  Config *conf;
  Rank **ranks;
  MemOp *nextOp;
  std::list<DelayedReq *> delayQueue;
};

};



#endif

