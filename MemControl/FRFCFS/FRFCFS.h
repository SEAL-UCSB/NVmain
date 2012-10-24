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

#ifndef __FRFCFS_H__
#define __FRFCFS_H__

#include "src/MemoryController.h"

#include <deque>


namespace NVM {


class FRFCFS : public MemoryController
{
 public:
  FRFCFS( Interconnect *memory, AddressTranslator *translator );
  ~FRFCFS( );


  bool IssueCommand( NVMainRequest *req );
  bool RequestComplete( NVMainRequest * request );

  void SetConfig( Config *conf );

  void Cycle( ncycle_t steps );

  bool QueueFull( NVMainRequest *req );

  void PrintStats( );

 private:
  NVMTransactionQueue memQueue;

  /* Cached Configuration Variables*/
  uint64_t queueSize;

  /* Stats */
  uint64_t measuredLatencies, measuredQueueLatencies;
  float averageLatency, averageQueueLatency;
  uint64_t mem_reads, mem_writes;
  uint64_t rb_hits;
  uint64_t rb_miss;
  uint64_t starvation_precharges;
  uint64_t cpu_insts;
};



};


#endif
