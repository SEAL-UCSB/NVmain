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

#include "Endurance/Distributions/Uniform.h"

#include <math.h>
#include <iostream>


using namespace NVM;



UniformDistribution::UniformDistribution( Config *conf )
{
  config = conf;


  if( conf->GetValue( "EnduranceDistMean" ) == -1 )
    {
      std::cout << "EnduranceDistMean parameter not found for normal distribution!!\n";
      mean = 1000000;
    }
  else
    {
      mean = conf->GetValue( "EnduranceDistMean" );
    }
}



uint64_t UniformDistribution::GetEndurance( )
{
  return mean;
}
