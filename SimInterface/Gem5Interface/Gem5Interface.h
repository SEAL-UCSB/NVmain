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

#ifndef __NVMAIN_SIMINTERFACE_GEM5_H__
#define __NVMAIN_SIMINTERFACE_GEM5_H__


#include "src/SimInterface.h"



namespace NVM {


class Gem5Interface : public SimInterface
{
public:
  Gem5Interface( );
  ~Gem5Interface( );

  unsigned int GetInstructionCount( int core );
  unsigned int GetCacheMisses( int core, int level );
  unsigned int GetCacheHits( int core, int level );
  unsigned int GetUserMisses( int core );


  bool HasInstructionCount( );
  bool HasCacheMisses( );
  bool HasCacheHits( );


  int  GetDataAtAddress( uint64_t address, NVMDataBlock *data );
  void SetDataAtAddress( uint64_t address, NVMDataBlock data );


private:
};


};


#endif


