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

#ifndef __QUEUE_H__
#define __QUEUE_H__

#include <iostream> 
#include <string> 
#include <deque> 


#include "src/MemOp.h"
#include "src/Cycler.h"
#include "src/GenericBus.h"


namespace NVM {


class Queue : public Cycler
{
 public:
  
  Queue( );
  ~Queue( );
	
  void PushFront( MemOp mop );
  void Enqueue( MemOp mop );

  void SetCommandBus( GenericBus *cBus );
  void SetDataBus( GenericBus *dBus );

  void Cycle( );

  void Print( );

 private:
  unsigned int currentCycle;
  GenericBus *cmdBus;
  GenericBus *dataBus;

  std::deque<MemOp> instructions;
};


};


#endif


