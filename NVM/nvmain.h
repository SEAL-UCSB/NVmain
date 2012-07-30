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


namespace NVM {


class Config;
class MemoryController;
class MemoryControllerManager;
class Interconnect;
class AddressTranslator;
class SimInterface;
class NVMainRequest;

class NVMain //: public Cycler
{
 public:
  NVMain( );
  ~NVMain( );

  void SetConfig( Config *conf );

  int  NewRequest( NVMainRequest *request );
  bool CanIssue( NVMainRequest *request );

  void Cycle( );

 private:
  Config *config;
  Config **channelConfig;
  MemoryController **memoryControllers;
  MemoryControllerManager *memoryControllerManager;
  Interconnect **memory;
  AddressTranslator *translator;
  SimInterface *simInterface;

  unsigned int numChannels;
  uint64_t currentCycle;
  std::ofstream pretraceOutput;

};


};


#endif

