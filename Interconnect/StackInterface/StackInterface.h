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

#ifndef __STACKINTERFACE_H__
#define __STACKINTERFACE_H__


#include "src/Rank.h"
#include "src/Interconnect.h"

#include <deque>


namespace NVM {


enum AckType { ACK_ACK, ACK_NACK, ACK_WRITEFAIL };

/*
 *  The main linked list of requests.
 */
struct StackRequest
{
  unsigned int slot;
  AckType status;
  NVMainRequest *memReq;
};



class StackInterface : public Interconnect
{
 public:
  StackInterface( );
  ~StackInterface( );

  void SetConfig( Config *c );
  void SetMLRValue( unsigned int mlr );
  void SetMLWValue( unsigned int mlw );

  bool IssueCommand( NVMainRequest *req );
  bool IsIssuable( NVMainRequest *req, ncycle_t delay = 0 );

  bool GetCompletedRequest( StackRequest **req );

  void PrintStats( );

  void Cycle( );

 private:
  bool configSet;
  ncounter_t numRanks;
  ncycle_t currentCycle;
  float syncValue;
  unsigned int MLR_value;
  unsigned int MLW_value;

  Config *conf;
  Rank **ranks;

  ncounter_t firstTry, secondTry;
  ncounter_t issuedReqs, completedReqs;
  std::deque<StackRequest *> stackRequests;
  std::deque<StackRequest *> completedRequests;


  unsigned int GetMLValue( BulkCommand cmd );

};


};


#endif

