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

#ifndef __MEMCONTROL_DRCDECODER_H__
#define __MEMCONTROL_DRCDECODER_H__



#include "src/AddressTranslator.h"


namespace NVM {


class DRCDecoder : public AddressTranslator
{
 public:
  DRCDecoder( );
  ~DRCDecoder( ) { }

  void SetIgnoreBits( uint64_t numIgnore );
  void SetCachelineSize( uint64_t lineSize );

  void Translate( uint64_t address, uint64_t *row, uint64_t *col, uint64_t *bank, uint64_t *rank, uint64_t *channel );

  void PrintStats( ) { }

 private:
  uint64_t ignoreBits;
  uint64_t cachelineSize;

};


};


#endif


