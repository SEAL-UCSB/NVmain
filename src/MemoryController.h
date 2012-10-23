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

#ifndef __MEMORYCONTROLLER_H__
#define __MEMORYCONTROLLER_H__


#include <string>
#include <vector>

#include "src/NVMObject.h"
#include "src/Config.h"
#include "src/Interconnect.h"
#include "src/AddressTranslator.h"
#include "src/Params.h"
#include "include/NVMainRequest.h"

#include <deque>
#include <iostream>
#include <list>


namespace NVM {



enum ProcessorOp { LOAD, STORE };
enum MCEndMode { ENDMODE_NORMAL,              // End command when all data is received.
                 ENDMODE_CRITICAL_WORD_FIRST, // End command after first data cycle.
                 ENDMODE_IMMEDIATE,           // End command right now (if you do your own timing)
                 ENDMODE_CUSTOM,              // End after a specific number of cycles.
                 ENDMODE_COUNT
};

class SchedulingPredicate
{
 public:
  SchedulingPredicate( ) { }
  ~SchedulingPredicate( ) { }

  virtual bool operator() (uint64_t, uint64_t) { return true; }
};



class MemoryController : public NVMObject 
{
 public:
  MemoryController( );
  MemoryController( Interconnect *memory, AddressTranslator *translator );
  ~MemoryController( ) { }


  void InitQueues( unsigned int numQueues );
  void InitBankQueues( unsigned int numQueues );

  virtual bool RequestComplete( NVMainRequest *request );
  virtual void EndCommand( NVMainRequest *req, MCEndMode endMode = ENDMODE_NORMAL, ncycle_t customTime = 0 );
  virtual bool QueueFull( NVMainRequest *request );

  void SetMemory( Interconnect *mem );
  Interconnect *GetMemory( );

  void SetTranslator( AddressTranslator *trans );
  AddressTranslator *GetTranslator( );

  AddressTranslator *GetAddressTranslator( );

  void StatName( std::string name ) { statName = name; }
  virtual void PrintStats( );

  virtual void Cycle( ncycle_t steps ); 

  virtual void SetConfig( Config *conf );
  void SetParams( Params *params ) { p = params; }
  Config *GetConfig( );

  void SetID( unsigned int id );

  void FlushCompleted( );

 protected:
  Interconnect *memory;
  AddressTranslator *translator;
  Config *config;
  std::string statName;
  uint64_t psInterval;

  std::list<NVMainRequest *> *transactionQueues;
  std::deque<NVMainRequest *> **bankQueues;

  bool **activateQueued;
  uint64_t **effectiveRow;
  unsigned int **starvationCounter;
  unsigned int starvationThreshold;

  NVMainRequest *MakeActivateRequest( NVMainRequest *triggerRequest );
  NVMainRequest *MakePrechargeRequest( NVMainRequest *triggerRequest );
  bool FindStarvedRequest( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **starvedRequest );
  bool FindRowBufferHit( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **hitRequest );
  bool FindOldestReadyRequest( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **oldestRequest );
  bool FindClosedBankRequest( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **closedRequest );
  bool IssueMemoryCommands( NVMainRequest *req );
  void CycleCommandQueues( );

  bool FindStarvedRequest( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **starvedRequest, NVM::SchedulingPredicate& p );
  bool FindRowBufferHit( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **hitRequest, NVM::SchedulingPredicate& p );
  bool FindOldestReadyRequest( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **oldestRequest, NVM::SchedulingPredicate& p );
  bool FindClosedBankRequest( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **closedRequest, NVM::SchedulingPredicate& p );
  
  class DummyPredicate : public SchedulingPredicate
  {
   public:
    bool operator() (uint64_t, uint64_t);
  };

  unsigned int id;

  bool refreshUsed;
  std::vector<NVMainRequest *> refreshWaitQueue;
  bool **refreshNeeded;
  NVMainRequest *BuildRefreshRequest( int rank, int bank );

  std::map<NVMainRequest *, ncycle_t> completedCommands;

  Params *p;

};


};

#endif

