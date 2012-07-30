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

#include "Endurance/EnduranceDistributionFactory.h"

#include <iostream>


using namespace NVM;



EnduranceDistribution *EnduranceDistributionFactory::CreateEnduranceDistribution( std::string distName, Config *conf )
{
  EnduranceDistribution *enduranceDist = NULL;

  if( distName == "" )
    std::cout << "NVMain: EnduranceDist is not set in configuration file!\n";

  
  if( distName == "Normal" ) enduranceDist = new NormalDistribution( conf );
  /*
   *  Add your custom endurance distribution here, for example:
   *
   *  else if( distName == "MyDist" ) enduranceDist = new MyDist( );
   */
  if( distName == "Uniform" ) enduranceDist = new UniformDistribution( conf );


  if( enduranceDist == NULL )
    std::cout << "NVMain: Endurance distribution '" << distName << "' not found in factory!\n";

  return enduranceDist;
}
