/*******************************************************************************
* Copyright (c) 2012-2013, The Microsystems Design Labratory (MDL)
* Department of Computer Science and Engineering, The Pennsylvania State University
* All rights reserved.
* 
* This source code is part of NVMain - A cycle accurate timing, bit accurate
* energy simulator for both volatile (e.g., DRAM) and nono-volatile memory
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

#include "MemControl/DRAMCache/DRAMCache.h"
#include "include/NVMHelpers.h"
#include "NVM/nvmain.h"
#include <iostream>
#include <assert.h>

using namespace NVM;

DRAMCache::DRAMCache( Interconnect *memory, AddressTranslator *translator )
{
    translator->GetTranslationMethod( )->SetOrder( 5, 1, 4, 3, 2 );

    SetMemory( memory );
    SetTranslator( translator );

    std::cout << "Created a DRAMCache!" << std::endl;

    drcChannels = NULL;

    numChannels = 0;
}

DRAMCache::~DRAMCache( )
{
}

void DRAMCache::SetConfig( Config *conf )
{
    /* Initialize off-chip memory */
    std::string configFile;
    Config *mainMemoryConfig;

    configFile  = NVM::GetFilePath( conf->GetFileName( ) );
    configFile += conf->GetString( "MM_CONFIG" );

    mainMemoryConfig = new Config( );
    mainMemoryConfig->Read( configFile );

    mainMemory = new NVMain( );
    mainMemory->SetConfig( mainMemoryConfig, "offChipMemory" );
    /* TODO: Somehow this needs to have all the basic DRCs as parents.. */
    mainMemory->SetParent( this ); 

    /* Initialize DRAM Cache channels */
    if( conf->KeyExists( "DRC_CHANNELS" ) )
        numChannels = static_cast<ncounter_t>( conf->GetValue( "DRC_CHANNELS" ) );
    else
        numChannels = 1;


    drcChannels = new LH_Cache*[numChannels];
    for( ncounter_t i = 0; i < numChannels; i++ )
    {
        /* 
         * TODO: Create a factory for other types of DRAM caches and change 
         * this type to a generic memory controller 
         */
        drcChannels[i] = new LH_Cache( GetMemory(), GetTranslator() ); 
        drcChannels[i]->SetMainMemory( mainMemory );

        drcChannels[i]->SetID( static_cast<int>(i) );
        drcChannels[i]->StatName( this->statName ); 

        drcChannels[i]->SetParent( this );
        AddChild( drcChannels[i] );

        drcChannels[i]->SetConfig( conf );
    }

    MemoryController::SetConfig( conf );
}

bool DRAMCache::IssueAtomic( NVMainRequest *req )
{
    uint64_t chan;

    req->address.GetTranslatedAddress( NULL, NULL, NULL, NULL, &chan );

    assert( chan < numChannels );

    return drcChannels[chan]->IssueAtomic( req );
}

bool DRAMCache::IssueCommand( NVMainRequest *req )
{
    uint64_t chan;

    req->address.GetTranslatedAddress( NULL, NULL, NULL, NULL, &chan );

    assert( chan < numChannels );

    return drcChannels[chan]->IssueCommand( req );
}

bool DRAMCache::RequestComplete( NVMainRequest *req )
{
    bool rv = false;

    if( req->type == REFRESH )
        ProcessRefreshPulse( req );
    else if( req->owner == this )
    {
        delete req;
        rv = true;
    }
    else
    {
        /* 
         *  We handle DRC and NVMain source requests. If the request is 
         *  somewhere in the DRC hierarchy, send to the LH_Cache, otherwise
         *  back to NVMain.
         */
        bool drcRequest = false;

        for( ncounter_t i = 0; i < numChannels; i++ )
        {
            if( req->owner == drcChannels[i] )
            {
                drcRequest = true;
                break;
            }
        }

        if( drcRequest )
        {
            uint64_t chan;

            req->address.GetTranslatedAddress( NULL, NULL, NULL, NULL, &chan );

            rv = drcChannels[chan]->RequestComplete( req );
        }
        else
        {
            rv = GetParent( )->RequestComplete( req );
        }
    }

    return rv;
}

void DRAMCache::Cycle( ncycle_t steps )
{
    uint64_t i;

    for( i = 0; i < numChannels; i++ )
    {
        drcChannels[i]->Cycle( steps );
    }

    mainMemory->Cycle( steps );
}

void DRAMCache::PrintStats( )
{
    uint64_t i;

    for( i = 0; i < numChannels; i++ )
    {
        drcChannels[i]->PrintStats( );
    }

    mainMemory->PrintStats( );
}
