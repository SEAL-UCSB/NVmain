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

#ifndef __TRACELINE_H__
#define __TRACELINE_H__


#include <stdint.h>

#include "include/NVMainRequest.h"


namespace NVM {


class TraceLine
{
 public:
  TraceLine();
  ~TraceLine();
  
  void SetLine( uint64_t addr, OpType op, unsigned int cycle, NVMDataBlock data, unsigned int threadId );

  uint64_t GetAddress( );
  OpType GetOperation( );
  unsigned int GetCycle( );
  NVMDataBlock GetData( );
  unsigned int GetThreadId( );

 private:
  uint64_t address;
  OpType operation;
  unsigned int cycle;
  NVMDataBlock data;
  unsigned int threadId;

};


};


#endif 

