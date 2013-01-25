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

#ifndef __FRFCFS_WQF_WQF_H__
#define __FRFCFS_WQF_WQF_H__

#include "src/MemoryController.h"

#include <deque>


namespace NVM {


class FRFCFS_WQF : public MemoryController
{
 public:
  FRFCFS_WQF( Interconnect *memory, AddressTranslator *translator );
  ~FRFCFS_WQF( );


  bool IssueCommand( NVMainRequest *req );
  bool RequestComplete( NVMainRequest * request );

  void SetConfig( Config *conf );

  void Cycle( ncycle_t steps );

  bool QueueFull( NVMainRequest *req );

  void PrintStats( );

 private:
  NVMTransactionQueue readQueue;
  NVMTransactionQueue writeQueue;

  /* Scheduling predicates. */
  class WriteQueueFull : public SchedulingPredicate
  {
    friend class FRFCFS_WQF; // Need to access private members

    FRFCFS_WQF &memoryController;
    bool draining;

    public:
      WriteQueueFull( FRFCFS_WQF &_memoryController ) : memoryController(_memoryController), draining(false) { }

      bool operator() (uint64_t, uint64_t);
  };

  /* Cached Configuration Variables*/
  uint64_t writeQueueSize;
  uint64_t readQueueSize;
  WriteQueueFull WQF;
  ComplementPredicate WQFc;

  /* added by Tao @ 01/25/2013, the write drain high and low watermark */
  uint64_t HighWaterMark;
  uint64_t LowWaterMark;

  /* Stats */
  uint64_t measuredLatencies, measuredQueueLatencies;
  float averageLatency, averageQueueLatency;
  uint64_t mem_reads, mem_writes;
  uint64_t rb_hits;
  uint64_t rb_miss;
  uint64_t starvation_precharges;
};



};


#endif
