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

#ifndef __MEMCONTROL_FCFS_H__
#define __MEMCONTROL_FCFS_H__

#include "src/MemoryController.h"


namespace NVM {


class FCFS : public MemoryController
{
 public:
  FCFS( Interconnect *memory, AddressTranslator *translator );
  ~FCFS( ) { }

  void SetConfig( Config *conf );

  bool IssueCommand( NVMainRequest *req );
  bool RequestComplete( NVMainRequest * request );
  bool QueueFull( NVMainRequest *req );

  void Cycle( ncycle_t );
  void PrintStats( );

 private:
  uint64_t queueSize;

  /* Stats */
  uint64_t measuredLatencies, measuredQueueLatencies;
  float averageLatency, averageQueueLatency;
  uint64_t mem_reads, mem_writes;
  uint64_t rb_hits;
  uint64_t rb_miss;

};


};


#endif
