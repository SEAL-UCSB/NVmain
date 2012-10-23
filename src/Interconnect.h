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


#include "src/NVMObject.h"
#include "src/Config.h"
#include "include/NVMTypes.h"
#include "include/NVMainRequest.h"


namespace NVM {


class Interconnect : public NVMObject
{
 public:
  Interconnect( ) { }
  virtual ~Interconnect( ) { }

  virtual void SetConfig( Config *c ) = 0;

  void StatName( std::string name ) { statName = name; }
  virtual void PrintStats( ) { }
  
  virtual void Cycle( ncycle_t steps ) = 0;

 protected:
  std::string statName;
};


};



#endif


