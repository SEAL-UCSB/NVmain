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

#ifndef __NVMAIN_H__
#define __NVMAIN_H__

#include <iostream>
#include <fstream>
#include <stdint.h>


#include "src/Params.h"
#include "src/NVMObject.h"
#include "include/NVMainRequest.h"


namespace NVM {


class Config;
class MemoryController;
class MemoryControllerManager;
class Interconnect;
class AddressTranslator;
class SimInterface;
class NVMainRequest;

class NVMain : public NVMObject
{
 public:
  NVMain( );
  ~NVMain( );

  void SetConfig( Config *conf, std::string memoryName = "defaultMemory" );
  void SetParams( Params *params ) { p = params; } 

  int  NewRequest( NVMainRequest *request );
  int  AtomicRequest( NVMainRequest *request );
  bool CanIssue( NVMainRequest *request );

  void PrintStats( );

  void Cycle( ncycle_t steps );

 private:
  Config *config;
  Config **channelConfig;
  MemoryController **memoryControllers;
  Interconnect **memory;
  AddressTranslator *translator;
  SimInterface *simInterface;
  EventQueue *mainEventQueue;

  unsigned int numChannels;
  uint64_t currentCycle;
  std::ofstream pretraceOutput;

  void PrintPreTrace( NVMainRequest *request );

  Params *p;

};


};


#endif

