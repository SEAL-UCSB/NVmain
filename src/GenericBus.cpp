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

#include <iostream>
#include <cstring>


#include "src/GenericBus.h"


using namespace NVM;




GenericBus::GenericBus( )
{
  memset( outputGraph[0], '-', 50 );
  memset( outputGraph[1], '-', 50 );

  activeGraph = 0;
  currentCycle = 0;
  activeCycles = 0;
  graphStartCycle = 0;
  config = NULL;
}

GenericBus::~GenericBus( )
{
  
}


void GenericBus::SetConfig( Config *config )
{
  this->config = config;
}


void GenericBus::SetBusy( ncycle_t startCycle, ncycle_t endCycle )
{
  ncycle_t relativeStartCycle, relativeEndCycle;

  if( graphStartCycle > startCycle || graphStartCycle + 100 <= endCycle )
    {
      //std::cout << "GenericBus: Start of end cycle out of bounds for current cycle range!" 
      //	<< " (" << startCycle << ", " << endCycle << ")" << std::endl;
      return;
    }

  if( endCycle < startCycle )
    {
      std::cout << "GenericBus: Busy state ends before it begins!" << std::endl;
      return;
    }

  relativeStartCycle = startCycle - graphStartCycle;
  relativeEndCycle = endCycle - graphStartCycle;

  if( relativeStartCycle >= 50 )
    {
      relativeStartCycle -= 50;
      relativeEndCycle -= 50;

      for( ncycle_t i = relativeStartCycle ; i < relativeEndCycle; i++ )
        {
          outputGraph[!activeGraph][i] = static_cast<char>('X' - '-' + outputGraph[!activeGraph][i]);
        }
    }
  else if( relativeStartCycle < 50 )
    {
      if( relativeEndCycle < 50 )
        {
          for( ncycle_t i = relativeStartCycle; i < relativeEndCycle; i++ )
            {
              outputGraph[activeGraph][i] = static_cast<char>('X' - '-' + outputGraph[activeGraph][i]);
            }
        }
      else
        {
          relativeEndCycle -= 50;

          for( ncycle_t i = relativeStartCycle; i < 50; i++ )
            {
              outputGraph[activeGraph][i] = static_cast<char>('X' - '-' + outputGraph[activeGraph][i]);
            }
          
          for( ncycle_t i = 0; i < relativeEndCycle; i++ )
            {
              outputGraph[!activeGraph][i] = static_cast<char>('X' - '-' + outputGraph[!activeGraph][i]);
            }
        }
    }
}



void GenericBus::PrintStats( )
{
  std::cout << "Bus '" << graphLabel << "' Utilization: " << activeCycles << "/" 
	    << currentCycle << std::endl << std::endl;
}


void GenericBus::SetLabel( ncycle_t startCycle, ncycle_t endCycle, char label )
{
  ncycle_t relativeStartCycle, relativeEndCycle;

  if( graphStartCycle > startCycle || graphStartCycle + 100 <= endCycle )
    {
      //std::cout << "GenericBus: Start of end cycle out of bounds for current cycle range!" 
      //	  << " (" << startCycle << ", " << endCycle << ")" << std::endl;
      return;
    }

  if( endCycle < startCycle )
    {
      std::cout << "GenericBus: Busy state ends before it begins!" << std::endl;
      return;
    }

  relativeStartCycle = startCycle - graphStartCycle;
  relativeEndCycle = endCycle - graphStartCycle;

  if( relativeStartCycle >= 50 )
    {
      relativeStartCycle -= 50;
      relativeEndCycle -= 50;

      for( ncycle_t i = relativeStartCycle ; i < relativeEndCycle; i++ )
        {
          outputGraph[!activeGraph][i] = label;
        }
    }
  else if( relativeStartCycle < 50 )
    {
      if( relativeEndCycle < 50 )
        {
          for( ncycle_t i = relativeStartCycle; i < relativeEndCycle; i++ )
            {
              outputGraph[activeGraph][i] = label;
            }
        }
      else
        {
          relativeEndCycle -= 50;

          for( ncycle_t i = relativeStartCycle; i < 50; i++ )
            {
              outputGraph[activeGraph][i] = label;
            }
          
          for( ncycle_t i = 0; i < relativeEndCycle; i++ )
            {
              outputGraph[!activeGraph][i] = label;
            }
        }
    }
}


void GenericBus::SetGraphLabel( std::string label )
{
  graphLabel = label;
}


void GenericBus::Cycle( )
{
  currentCycle++;

  if( currentCycle % 50 == 0 && config != NULL &&
      config->GetString( "PrintGraphs" ) == "true" )
    {
      std::cout << graphLabel << "     ";

      for( unsigned int i = 0; i < 50; i++ )
        {
          std::cout << outputGraph[activeGraph][i];
          if( outputGraph[activeGraph][i] != '-' )
            activeCycles++;
        }

      std::cout << "     " << graphStartCycle + 50 << std::endl;

      memset( outputGraph[activeGraph], '-', 50 );

      activeGraph = !activeGraph;
      graphStartCycle += 50;
    }
}

