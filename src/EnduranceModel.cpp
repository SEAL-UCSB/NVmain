/*******************************************************************************
* Copyright (c) 2012-2014, The Microsystems Design Labratory (MDL)
* Department of Computer Science and Engineering, The Pennsylvania State University
* All rights reserved.
* 
* This source code is part of NVMain - A cycle accurate timing, bit accurate
* energy simulator for both volatile (e.g., DRAM) and non-volatile memory
* (e.g., PCRAM). The source code is free and you can redistribute and/or
* modify it by providing that the following conditions are met:
* 
*  1) Redistributions of source code must retain the above copyright notice,
*     this list of conditions and the following disclaimer.
* 
*  2) Redistributions in binary form must reproduce the above copyright notice,
*     this list of conditions and the following disclaimer in the documentation
*     and/or other materials provided with the distribution.
* 
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
* 
* Author list: 
*   Matt Poremba    ( Email: mrp5060 at psu dot edu 
*                     Website: http://www.cse.psu.edu/~poremba/ )
*******************************************************************************/

#include "src/EnduranceModel.h"
#include "Endurance/EnduranceDistributionFactory.h"
#include "src/FaultModel.h"
#include <iostream>
#include <limits>

using namespace NVM;

EnduranceModel::EnduranceModel( )
{
    life.clear( );

    granularity = 0;
}

void EnduranceModel::SetConfig( Config *config, bool /*createChildren*/ )
{
    enduranceDist = EnduranceDistributionFactory::CreateEnduranceDistribution( 
            config->GetString( "EnduranceDist" ), config );
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
        total += i->second;

    if( life.size( ) != 0 )
        average = total / life.size( );
    else
        average = 0;

    return average;
}

bool EnduranceModel::DecrementLife( uint64_t addr )
{
    std::map<uint64_t, uint64_t>::iterator i = life.find( addr );
    bool rv = true;

    if( i == life.end( ) )
    {
          /* Generate a random number using the specified distribution */
          life.insert( std::pair<uint64_t, uint64_t>( addr, enduranceDist->GetEndurance( ) ) );
    }
    else
    {
        /* If the life is 0, leave it at that.  */
        if( i->second != 0 )
        {
            i->second = i->second - 1;
        }
        else
        {
            rv = false;
        }
    }

    return rv;
}

bool EnduranceModel::IsDead( uint64_t addr )
{
    std::map<uint64_t, uint64_t>::iterator i = life.find( addr );
    bool rv = false;

    if( i != life.end( ) && i->second == 0 )
    {
        rv = true;
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
