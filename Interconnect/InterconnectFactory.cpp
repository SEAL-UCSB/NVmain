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

#include "Interconnect/InterconnectFactory.h"

#include <iostream>


/* Add your interconnect's include file below. */
#include "Interconnect/OnChipBus/OnChipBus.h"
#include "Interconnect/OffChipBus/OffChipBus.h"


using namespace NVM;



Interconnect *InterconnectFactory::CreateInterconnect( std::string type )
{
  Interconnect *ic = NULL;

  if( type == "" )
    std::cout << "NVMain: Interconnect type is not set!" << std::endl;

  if( type == "OnChipBus" )
    ic = new OnChipBus( );
  else if( type == "OffChipBus" )
    ic = new OffChipBus( );


  else
    std::cout << "NVMain: Could not find interconnect called " << type << "!" << std::endl;

  return ic;
}

