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

#ifndef __RANK_H__
#define __RANK_H__


#include <stdint.h>
#include <list>


#include "src/Bank.h"
#include "src/Device.h"
#include "src/Params.h"
#include "src/NVMObject.h"


#include <iostream>


namespace NVM {


class Rank : public NVMObject
{
public:
  Rank( );
  ~Rank( );

  void SetConfig( Config *c );
  void SetParams( Params *params ) { p = params; }

  bool IssueCommand( NVMainRequest *mop );
  bool IsIssuable( NVMainRequest *mop, FailReason *reason = NULL );
  void Notify( OpType op );

  void SetName( std::string name );
  void PrintStats( );
  void StatName( std::string name ) { statName = name; }

  ncycle_t GetNextActivate( uint64_t bank );
  ncycle_t GetNextRead( uint64_t bank );
  ncycle_t GetNextWrite( uint64_t bank );
  ncycle_t GetNextPrecharge( uint64_t bank );
  ncycle_t GetNextRefresh( uint64_t bank );

  void Cycle( ncycle_t steps );

  Device *GetDevice( uint64_t device ) { return &(devices[device]); }

private:
  Config *conf;
  ncounter_t stateTimeout;
  std::string statName;
  uint64_t psInterval;

  Device *devices;
  ncounter_t bankCount;
  ncounter_t deviceWidth;
  ncounter_t deviceCount;
  ncounter_t busWidth;
  ncycles_t lastActivate[4];
  ncounter_t FAWindex;

  ncycle_t nextRead;
  ncycle_t nextWrite;
  ncycle_t nextActivate;
  ncycle_t nextPrecharge;

  ncounter_t rrdWaits;
  ncounter_t rrdWaitTime;
  ncounter_t fawWaits;
  ncounter_t fawWaitTime;
  ncounter_t actWaits;
  ncounter_t actWaitTime;

  float backgroundEnergy;

  bool Activate( NVMainRequest *request );
  bool Read( NVMainRequest *request );
  bool Write( NVMainRequest *request );
  bool Precharge( NVMainRequest *request );
  bool Refresh( NVMainRequest *request );

  bool PowerUp( NVMainRequest *request );

  Params *p;

};


};


#endif

