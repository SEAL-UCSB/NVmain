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

#ifndef __NVMDATABLOCK_H__
#define __NVMDATABLOCK_H__


#include <vector>
#include <stdint.h>
#include <ostream>


namespace NVM {


class NVMDataBlock
{
 public:
  NVMDataBlock( );
  ~NVMDataBlock( );
  
  uint8_t GetByte( uint64_t byte );
  void SetByte( uint64_t byte, uint8_t value );

  void Print( std::ostream& out ) const;
  
  NVMDataBlock operator=( NVMDataBlock m );

  /*
   *  Normally the Get/SetByte should be used, but
   *  if the data block will store non-standard data
   *  (e.g., something other than cache lines) this
   *  construct may be more convenient.
   */
  void *rawData;
  
 private:
  std::vector< uint8_t > data;
};


};


std::ostream& operator<<( std::ostream& out, const NVM::NVMDataBlock& obj );



#endif

