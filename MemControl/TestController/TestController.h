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

#ifndef __TESTCONTROLLER_H__
#define __TESTCONTROLLER_H__

#include "src/MemoryController.h"


namespace NVM {


class TestController : public MemoryController
{
 public:
  TestController( Interconnect *memory, AddressTranslator *translator );
  ~TestController( ) { }


  int StartCommand( MemOp *mop );

  void Cycle( );

 private:
  unsigned int sourceThread[4];
  unsigned int readCount[4];

};


};


#endif
