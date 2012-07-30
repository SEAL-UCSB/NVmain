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

#ifndef __CYCLER_H__
#define __CYCLER_H__


#include "include/NVMTypes.h"


namespace NVM {


/*
 *  Generic base class for all simulator classes. The cycle function is called
 *  at each simulation step (at each clock cycle).
 */
class Cycler
{
 public:
  Cycler( ) { }
  virtual ~Cycler( ) { }

  virtual void Cycle( ) = 0;
  
};


};



#endif

