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

#ifndef __SIMINTERFACE_H__
#define __SIMINTERFACE_H__


#include <stdint.h>
#include <map>

#include "include/NVMDataBlock.h"


namespace NVM {


class Config;
class SimInterface
{
 public:
  SimInterface( ) { }
  virtual ~SimInterface( ) { }

  virtual unsigned int GetInstructionCount( int ) = 0;
  virtual unsigned int GetCacheMisses( int, int ) = 0;
  virtual unsigned int GetCacheHits( int, int ) = 0;

  virtual bool HasInstructionCount( ) = 0;
  virtual bool HasCacheMisses( ) = 0;
  virtual bool HasCacheHits( ) = 0;

  int  GetDataAtAddress( uint64_t address, NVMDataBlock *data );
  void SetDataAtAddress( uint64_t address, NVMDataBlock data );

  void SetConfig( Config *conf );
  Config *GetConfig( );

 private:
  std::map< uint64_t, NVMDataBlock > memoryData;
  std::map< uint64_t, unsigned int > accessCounts;
  Config *conf;

};


};



#endif

