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


#ifndef __NVMAIN_UTILS_VISUALIZER_H__
#define __NVMAIN_UTILS_VISUALIZER_H__



#include "src/NVMObject.h"
#include "include/NVMainRequest.h"
#include "include/NVMTypes.h"




namespace NVM {


class Visualizer : public NVMObject
{
 public:
  Visualizer( );
  ~Visualizer( );

  bool IssueCommand( NVMainRequest *req );
  bool IssueAtomic( NVMainRequest *req );

  bool RequestComplete( NVMainRequest *req );

  void Cycle( ncycle_t );

  void Init( );

 private:
  ncounter_t numRanks, numBanks;
  ncycle_t busBurstLength;
  ncycle_t startCycle, endCycle, endCycle2;
  ncounter_t lineLength;

  std::vector<std::string> graphLines;
  std::vector<char> graphSymbol;
};



};



#endif



