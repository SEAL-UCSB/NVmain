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

#ifndef __DEVICE_H__
#define __DEVICE_H__


#include <vector>
#include <string>


#include "src/NVMObject.h"
#include "src/Bank.h"


namespace NVM { 


/*
 *  The purpose of this class is to group together banks that will be
 *  powered down at the same time. For standard main memory DIMM, this
 *  is a single device containing internal banks. For other times of
 *  memory, this class can be used as a general container for banks.
 */
class Device : public NVMObject
{
 public:
  Device();
  ~Device();

  void AddBank( Bank *newBank );
  Bank *GetBank( uint64_t bankId );
  uint64_t GetBankCount( );

  bool PowerUp( uint64_t whichBank );
  bool PowerDown( bool fastExit );

  bool CanPowerUp( uint64_t whichBank );
  bool CanPowerDown( OpType pdOp );

  void Cycle( ncycle_t steps );

 private:
  uint64_t count;
  std::vector< Bank* > banks;

};


};


#endif

