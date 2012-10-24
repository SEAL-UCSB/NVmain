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

#ifndef __ENDURANCEMODEL_H__
#define __ENDURANCEMODEL_H__


#include <string>
#include <map>
#include <stdint.h>

#include "src/Config.h"
#include "src/NVMObject.h"
#include "src/EnduranceDistribution.h"
#include "include/NVMDataBlock.h"
#include "include/NVMAddress.h"
#include "src/FaultModel.h"


namespace NVM {


class FaultModel;

class EnduranceModel : public NVMObject
{
 public:
  EnduranceModel( );
  ~EnduranceModel( ) {}

  virtual bool Write( NVMAddress address, NVMDataBlock oldData, NVMDataBlock newData ) = 0;

  virtual void SetConfig( Config *conf );
  Config *GetConfig( );

  uint64_t GetWorstLife( );
  uint64_t GetAverageLife( );

  virtual void PrintStats( ) { }

  void Cycle( ncycle_t steps );

 protected:
  Config *config;
  EnduranceDistribution *enduranceDist;
  FaultModel *faultModel;
  std::map<uint64_t, uint64_t> life;
  
  bool DecrementLife( uint64_t addr, NVMAddress faultAddr );

  void SetGranularity( uint64_t bits );
  uint64_t GetGranularity( );

 private:
  uint64_t granularity;

};


};


#endif

