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

#ifndef __NVMADDRESS_H__
#define __NVMADDRESS_H__


#include <stdint.h>


namespace NVM {


class NVMAddress
{
 public:
  NVMAddress( );
  ~NVMAddress( );
  
  void SetTranslatedAddress( uint64_t addrRow, uint64_t addrCol, uint64_t addrBank, uint64_t addrRank, uint64_t addrChannel );
  void SetPhysicalAddress( uint64_t physicalAddress );
  void SetBitAddress( uint8_t bitAddr );
  
  void GetTranslatedAddress( uint64_t *addrRow, uint64_t *addrCol, uint64_t *addrBank, uint64_t *addrRank, uint64_t *addrChannel );
  uint64_t GetPhysicalAddress( );
  uint8_t GetBitAddress( );

  NVMAddress& operator=( const NVMAddress& m );
  
 private:
  uint64_t physicalAddress;
  uint64_t row;
  uint64_t col;
  uint64_t bank;
  uint64_t rank;
  uint64_t channel;
  uint64_t bit;
  
};


};


#endif

