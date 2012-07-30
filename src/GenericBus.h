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
 *  ---------------------------------------------------------------------
 *
 */

#ifndef __GENERICBUS_H__
#define __GENERICBUS_H__


#include <string>


#include "src/Cycler.h"
#include "src/Config.h"
#include "include/NVMTypes.h"


namespace NVM {


class GenericBus : public Cycler 
{
 public:
  GenericBus( );
  ~GenericBus( );

  void SetConfig( Config *config );
  void SetBusy( ncycle_t startCycle, ncycle_t endCycle );
  void SetLabel( ncycle_t startCycle, ncycle_t endCycle, char label );
  void SetGraphLabel( std::string label );

  void PrintStats( );

  void Cycle( );

 private:
  Config *config;

  std::string graphLabel;
  char outputGraph[2][50]; 
  unsigned char activeGraph;
  
  ncycle_t currentCycle;
  ncycle_t activeCycles;
  ncycle_t graphStartCycle;

};


};


#endif

