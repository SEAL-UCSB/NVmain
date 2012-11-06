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

#ifndef __INTERCONNECT_ONCHIPBUS_H__
#define __INTERCONNECT_ONCHIPBUS_H__


#include "src/Rank.h"
#include "src/Interconnect.h"
#include "src/Params.h"


#include <iostream>


namespace NVM {


class OnChipBus : public Interconnect
{
 public:
  OnChipBus( );
  ~OnChipBus( );

  void SetConfig( Config *c );
  void SetParams( Params *params ) { p = params; }

  bool IssueCommand( NVMainRequest *mop );
  bool IsIssuable( NVMainRequest *mop, FailReason *reason = NULL );

  ncycle_t GetNextActivate( uint64_t rank, uint64_t bank );
  ncycle_t GetNextRead( uint64_t rank, uint64_t bank );
  ncycle_t GetNextWrite( uint64_t rank, uint64_t bank );
  ncycle_t GetNextPrecharge( uint64_t rank, uint64_t bank );
  ncycle_t GetNextRefresh( uint64_t rank, uint64_t bank );

  void PrintStats( );

  void Cycle( ncycle_t steps );

  Rank *GetRank( uint64_t rank ) { return ranks[rank]; }
  
 private:
  bool configSet;
  ncounter_t numRanks;
  float syncValue;

  Config *conf;
  Rank **ranks;

  Params *p;

};


};


#endif

