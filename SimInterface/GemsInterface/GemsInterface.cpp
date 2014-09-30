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

#include <stdlib.h>
#include "src/Config.h"
#include "SimInterface/GemsInterface/GemsInterface.h"
#include "system/System.h"
#include "common/Driver.h"
#include "profiler/Profiler.h"
#include "profiler/CacheProfiler.h"
#include "simics/interface.h"

using namespace NVM;

GemsInterface::GemsInterface( )
{
    gems_system_ptr = NULL;
    gems_eventQueue_ptr = NULL;
}

GemsInterface::~GemsInterface( )
{
}

unsigned int GemsInterface::GetInstructionCount( int core )
{
    return gems_system_ptr->getDriver( )->getInstructionCount( core );
}

unsigned int GemsInterface::GetCacheMisses( int core, int level )
{
    /*
     *  For level "0" return the total misses from all caches...
     */
    //if( level == 1 )
    //return (unsigned int)gems_system_ptr->getProfiler( )->getL1DCacheProfiler( )->getProcTotalMisses( );
    //else if( level == 2 )
    return (unsigned int)gems_system_ptr->getProfiler( )->getProcTotalMisses( core );
}

unsigned int GemsInterface::GetUserMisses( int core )
{
    return (unsigned int)gems_system_ptr->getProfiler( )->getProcUserMisses( core );
}

unsigned int GemsInterface::GetCacheHits( int core, int level )
{
    return 0;
}

bool GemsInterface::HasInstructionCount( )
{
    return true;
}

bool GemsInterface::HasCacheMisses( )
{
    return true;
}

bool GemsInterface::HasCacheHits( )
{
    return true;
}

void GemsInterface::SetSystemPtr( System *system_ptr )
{
    gems_system_ptr = system_ptr;
}

void GemsInterface::SetEventQueuePtr( EventQueue *eventQueue_ptr )
{
    gems_eventQueue_ptr = eventQueue_ptr;
}

System *GemsInterface::GetSystemPtr( )
{
    return gems_system_ptr;
}

EventQueue *GemsInterface::GetEventQueuePtr( )
{
    return gems_eventQueue_ptr;
}

void GemsInterface::SetDataAtAddress( uint64_t /*address*/, NVMDataBlock& /*data*/ )
{
    /*
     *  Simics stores the values of memory, so there's no reason
     *  to store it here. (Overloads default function which WILL
     *  store the values).
     */
}

int GemsInterface::GetDataAtAddress( uint64_t address, NVMDataBlock *data )
{
    unsigned int memBlockSize = GetConfig( )->GetValue( "BusWidth" ) / 8;
    char *buffer;

    buffer = new char [ memBlockSize ];

    SIMICS_read_physical_memory_buffer( 0, address, buffer, memBlockSize );

    for( unsigned int i = 0; i < memBlockSize; i++ )
        data->SetByte( i, (uint8_t)buffer[i] );
    
    return true;
}
