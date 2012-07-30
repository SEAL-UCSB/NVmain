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

#ifndef __NULLINTERFACE_H__
#define __NULLINTERFACE_H__


#include "src/SimInterface.h"


namespace NVM {


class NullInterface : public SimInterface
{
public:
  NullInterface( );
  ~NullInterface( );

  unsigned int GetInstructionCount( int core );
  unsigned int GetCacheMisses( int core, int level );
  unsigned int GetCacheHits( int core, int level );


  bool HasInstructionCount( );
  bool HasCacheMisses( );
  bool HasCacheHits( );

};


};


#endif

