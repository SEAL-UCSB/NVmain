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

#include "src/Cycler.h"
#include "src/Config.h"
#include "src/Interconnect.h"
#include "src/AddressTranslator.h"
#include "src/MemoryControllerMessage.h"
#include "src/MemoryControllerManager.h"
#include "src/NVMNet.h"

#include <vector>
#include <iostream>


namespace NVM {



enum ProcessorOp { LOAD, STORE };
enum MCEndMode { ENDMODE_NORMAL,              // End command when all data is received.
                 ENDMODE_CRITICAL_WORD_FIRST, // End command after first data cycle.
                 ENDMODE_IMMEDIATE,           // End command right now (if you do your own timing)
                 ENDMODE_CUSTOM,              // End after a specific number of cycles.
                 ENDMODE_COUNT
};


class MemoryController : public Cycler, public NVMNet 
{
 public:
  MemoryController( );
  MemoryController( Interconnect *memory, AddressTranslator *translator );
  ~MemoryController( ) { }


  void InitQueues( unsigned int numQueues );
  void Access( ProcessorOp op, uint64_t address );
  virtual int StartCommand( MemOp *mop ) = 0;
  virtual void RequestComplete( NVMainRequest *request );
  virtual void EndCommand( MemOp *mop, MCEndMode endMode = ENDMODE_NORMAL, unsigned int customTime = 0 );
  virtual bool QueueFull( NVMainRequest *request );

  void SetMemory( Interconnect *mem );
  Interconnect *GetMemory( );

  void SetTranslator( AddressTranslator *trans );
  AddressTranslator *GetTranslator( );

  AddressTranslator *GetAddressTranslator( );

  void StatName( std::string name ) { statName = name; }
  virtual void PrintStats( );

  virtual void Cycle( ); 

  virtual void SetConfig( Config *conf );
  Config *GetConfig( );

  void SendMessage( unsigned int dest, void *message, int latency = -1 );
  void RecvMessages( );
  void ProcessMessage( MemoryControllerMessage *msg );

  void SetSendCallback( MemoryControllerManager *manager, void (MemoryControllerManager::*sendCallback)( MemoryControllerMessage * ) );
  void SetRecvCallback( MemoryControllerManager *manager, int  (MemoryControllerManager::*recvCallback)( MemoryControllerMessage * ) );

  void SetID( unsigned int id );

  void RecvMessage( NVMNetMessage * ) { }

  void FlushCompleted( );

 protected:
  Interconnect *memory;
  AddressTranslator *translator;
  Config *config;
  std::string statName;
  uint64_t psInterval;

  std::vector<MemOp *> *commandQueue;
  uint64_t currentCycle;

  unsigned int id;
  MemoryControllerManager *manager;
  void (MemoryControllerManager::*SendCallback)( MemoryControllerMessage * );
  int  (MemoryControllerManager::*RecvCallback)( MemoryControllerMessage * );

  bool refreshUsed;
  std::vector<MemOp *> refreshWaitQueue;
  bool **refreshNeeded;
  MemOp *BuildRefreshRequest( int rank, int bank );

  std::map<MemOp *, unsigned int> completedCommands;

};


};

#endif

