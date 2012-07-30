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

#ifndef __INTERCONNECT_H__
#define __INTERCONNECT_H__


#include "src/Cycler.h"
#include "src/MemOp.h"
#include "src/Config.h"
#include "src/NVMNet.h"
#include "include/NVMTypes.h"


namespace NVM {


class Interconnect : public Cycler, public NVMNet
{
 public:
  Interconnect( ) { }
  virtual ~Interconnect( ) { }

  virtual void SetConfig( Config *c ) = 0;

  virtual bool IssueCommand( MemOp *mop ) = 0;
  virtual bool IsIssuable( MemOp *mop, ncycle_t delay = 0 ) = 0;

  virtual void RequestComplete( NVMainRequest *request );

  void StatName( std::string name ) { statName = name; }
  virtual void PrintStats( ) { }
  
  virtual void Cycle( ) = 0;

 protected:
  std::string statName;
};


};



#endif


