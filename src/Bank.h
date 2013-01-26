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

#ifndef __BANK_H__
#define __BANK_H__


#include <stdint.h>
#include <map>

#include "src/NVMObject.h"
#include "src/Config.h"
#include "src/EnduranceModel.h"
#include "include/NVMAddress.h"
#include "include/NVMainRequest.h"
#include "src/Params.h"

#include <iostream>


namespace NVM {

/*
 *  We only use three bank states because our timing and energy parameters
 *  only tell us the delay of the entire read/write cycle to one bank.
 *
 *  In the case of non-volatile memory, consecutive reads and writes do
 *  not need to consider the case when reads occur before tRAS, since
 *  data is not destroyed during read, and thus does not need to be
 *  written back to the row.
 */
enum BankState { BANK_UNKNOWN,  /***< Unknown state. Uh oh. */
                 BANK_OPEN,     /***< Bank has a row open */
                 BANK_CLOSED,   /***< Bank is idle. */
                 BANK_PDPF,     /***< Bank is in precharge powered down, fast exit mode */
                 BANK_PDA,      /***< Bank is in active powered down mode */
                 BANK_PDPS,     /***< Bank is in precharge powered down, slow exit mode */
                 BANK_REFRESHING/***< Bank is refreshing and will return to BANK_CLOSED state */
};
enum WriteMode { WRITE_BACK, WRITE_THROUGH, DELAYED_WRITE };


class Bank : public NVMObject
{
 public:
  Bank( );
  ~Bank( );

  bool Activate( NVMainRequest *request );
  bool Read( NVMainRequest *request );
  bool Write( NVMainRequest *request );
  bool Precharge( NVMainRequest *request );
  bool Refresh( );
  bool PowerUp( NVMainRequest *request );
  bool PowerDown( BankState pdState );

  bool WouldConflict( uint64_t checkRow );
  bool IsIssuable( NVMainRequest *req, FailReason *reason = NULL );
  bool NeedsRefresh( );

  bool IssueCommand( NVMainRequest *req );

  void SetConfig( Config *c );
  void SetParams( Params *params ) { p = params; }

  BankState GetState( );

  bool Idle( );
  ncycle_t GetDataCycles( ) { return dataCycles; }
  float GetPower( );
  float GetEnergy( ) { return bankEnergy; }
  ncounter_t GetReads( ) { return reads; }
  ncounter_t GetWrites( ) { return writes; }

  void SetNextRefresh( ncycle_t nextREF ); // Should ONLY be used to stagger refreshes initially.
  void SetRefreshRows( ncounter_t numRows ) { refreshRows = numRows; }

  ncycle_t GetNextActivate( ) { return nextActivate; }
  ncycle_t GetNextRead( ) { return nextRead; }
  ncycle_t GetNextWrite( ) { return nextWrite; }
  ncycle_t GetNextPrecharge( ) { return nextPrecharge; }
  ncycle_t GetNextRefresh( ) { return nextRefresh; }
  ncycle_t GetNextPowerDown( ) { return nextPowerDown; }
  uint64_t GetOpenRow( ) { return openRow; }

  void SetName( std::string );
  void SetId( int );
  void PrintStats( );
  void StatName( std::string name ) { statName = name; }

  int GetId( );
  std::string GetName( );

  void Cycle( ncycle_t steps );

  /* added by Tao @ 01/25/2013 */
  bool RequestComplete( NVMainRequest* );

 private:
  Config *conf;
  std::string statName;
  uint64_t psInterval;

  BankState state;
  BulkCommand nextCommand;
  NVMainRequest lastOperation;
  bool refreshUsed;
  ncounter_t refreshRows;
  ncounter_t refreshRowIndex;

  ncycle_t powerCycles;
  ncycle_t feCycles;
  ncycle_t seCycles;

  ncycle_t lastActivate;
  ncycle_t nextActivate;
  ncycle_t nextPrecharge;
  ncycle_t nextRead;
  ncycle_t nextWrite;
  ncycle_t nextRefresh;
  ncycle_t nextRefreshDone;
  ncycle_t nextPowerDown;
  ncycle_t nextPowerDownDone;
  ncycle_t nextPowerUp;
  bool writeCycle;
  bool needsRefresh;
  WriteMode writeMode;

  ncounter_t actWaits;
  ncounter_t actWaitTime;

  float bankEnergy;
  float backgroundEnergy;
  float activeEnergy;
  float burstEnergy;
  float refreshEnergy;
  float utilization;
  ncycle_t activeCycles;
  ncycle_t dataCycles;
  ncounter_t reads, writes, activates, precharges, refreshes;
  ncounter_t idleTimer;

  uint64_t openRow;

  EnduranceModel *endrModel;

  int bankId;
 
  Params *p;

  void IssueImplicit( );


};


};


#endif
