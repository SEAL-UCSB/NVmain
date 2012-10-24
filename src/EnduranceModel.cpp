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

#include "src/EnduranceModel.h"
#include "Endurance/EnduranceDistributionFactory.h"
#include "src/FaultModel.h"


#include <iostream>
#include <limits>


using namespace NVM;



EnduranceModel::EnduranceModel( ) : config( NULL )
{
  life.clear( );

  granularity = 0;
}



void EnduranceModel::SetConfig( Config *conf )
{
  config = conf;

  enduranceDist = EnduranceDistributionFactory::CreateEnduranceDistribution( config->GetString( "EnduranceDist" ), conf );
}


Config *EnduranceModel::GetConfig( )
{
  return config;
}


/*
 *  Finds the worst life in the life map. If you do not use the life
 *  map, you will need to overload this function to return the worst
 *  case life for statistics reporting.
 */
uint64_t EnduranceModel::GetWorstLife( )
{
  std::map<uint64_t, uint64_t>::iterator i;
  uint64_t min = std::numeric_limits< uint64_t >::max( );

  for( i = life.begin( ); i != life.end( ); i++ )
    {
      if( i->second < min )
        min = i->second;
    }

  return min;
}


/*
 *  Finds the worst life in the life map. If you do not use the life
 *  map, you will need to overload this function to return the worst
 *  case life for statistics reporting.
 */
uint64_t EnduranceModel::GetAverageLife( )
{
  std::map<uint64_t, uint64_t>::iterator i;
  uint64_t total = 0;
  uint64_t average = 0;

  for( i = life.begin( ); i != life.end( ); i++ )
    {
      total += i->second;
    }

  if( life.size( ) != 0 )
    average = total / life.size( );
  else
    average = 0;

  return average;
}


bool EnduranceModel::DecrementLife( uint64_t addr, NVMAddress faultAddr )
{
  uint64_t newLife;
  std::map<uint64_t, uint64_t>::iterator i = life.find( addr );
  bool rv = true;

  if( i == life.end( ) )
    {
      /*
       *  Generate a random number using the specified distribution
       */
      life.insert( std::pair<uint64_t, uint64_t>( addr, enduranceDist->GetEndurance( ) ) );
    }
  else
    {
      /*
       *  If the life is 0, leave it at that.
       */
      if( i->second != 0 )
	    {
	      newLife = i->second - 1;
      
	      life.erase( i );
	      life.insert( std::pair<uint64_t, uint64_t>( addr, newLife ) );
	    }
      else
        {
          /* 
           *  Send to hard-error fault modeller to see if 
           *  the fault can be fixed.
           */
          rv = faultModel->Fault( faultAddr );
        }
    }

  return rv;
}


void EnduranceModel::SetGranularity( uint64_t bits )
{
  granularity = bits;
}


uint64_t EnduranceModel::GetGranularity( )
{
  return granularity;
}


void EnduranceModel::Cycle( ncycle_t )
{
  
}
