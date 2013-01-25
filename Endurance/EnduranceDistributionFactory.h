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

#ifndef __ENDURANCEDISTRIBUTIONFACTORY_H__
#define __ENDURANCEDISTRIBUTIONFACTORY_H__



#include "src/EnduranceDistribution.h"
#include "src/Config.h"


namespace NVM {


class EnduranceDistributionFactory
{
 public:
  EnduranceDistributionFactory( ) { }
  ~EnduranceDistributionFactory( ) { }

  static EnduranceDistribution *CreateEnduranceDistribution( std::string distName, Config *conf );
};


};


#endif

