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

#ifndef __NVMAIN_ENDURANCE_FLIPNWRITE_H__
#define __NVMAIN_ENDURANCE_FLIPNWRITE_H__


#include "src/EnduranceModel.h"

#include <set>


namespace NVM {


class FlipNWrite : public EnduranceModel
{
 public:
  FlipNWrite( );
  ~FlipNWrite( );

  bool Write( NVMAddress address, NVMDataBlock oldData, NVMDataBlock newData );

  void PrintStats( );

 private:
  std::set< uint64_t > flippedAddresses;
  
  uint64_t flipBitsWritten;
  uint64_t flipNWriteModify;
  uint64_t bitCompareModify;

  void InvertData( NVMDataBlock &data, uint64_t startBit, uint64_t endBit );

};


};


#endif


