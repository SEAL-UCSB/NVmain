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

#include "MemControl/DRAMCache/DRAMCache.h"
#include "MemControl/MemoryControllerFactory.h"
#include "Interconnect/InterconnectFactory.h"
#include "include/NVMHelpers.h"
#include "NVM/nvmain.h"
#include "Decoders/DRCDecoder/DRCDecoder.h"
#include "src/EventQueue.h"

#include <iostream>
#include <sstream>
#include <cassert>
#include <cstdlib>

using namespace NVM;

DRAMCache::DRAMCache( )
{
    //translator->GetTranslationMethod( )->SetOrder( 5, 1, 4, 3, 2, 6 );

    std::cout << "Created a DRAMCache!" << std::endl;

    drcChannels = NULL;

    numChannels = 0;
}

DRAMCache::~DRAMCache( )
{
}

void DRAMCache::SetConfig( Config *conf, bool createChildren )
{
    /* Initialize DRAM Cache channels */
    numChannels = static_cast<ncounter_t>( conf->GetValue( "DRC_CHANNELS" ) );

    if( createChildren )
    {
        /* Initialize off-chip memory */
        std::string configFile;
        Config *mainMemoryConfig;

        configFile  = NVM::GetFilePath( conf->GetFileName( ) );
        configFile += conf->GetString( "MM_CONFIG" );

        mainMemoryConfig = new Config( );
        mainMemoryConfig->Read( configFile );

        mainMemory = new NVMain( );
        EventQueue *mainMemoryEventQueue = new EventQueue( );
        mainMemory->SetParent( this ); 
        mainMemory->SetEventQueue( mainMemoryEventQueue );
        GetGlobalEventQueue( )->AddSystem( mainMemory, mainMemoryConfig );
        mainMemory->SetConfig( mainMemoryConfig, "offChipMemory", createChildren );

        /* Orphan the interconnect created by NVMain */
        std::vector<NVMObject_hook *>& childNodes = GetChildren( );

        childNodes.clear();

        if( !conf->KeyExists( "DRCVariant" ) )
        {
            std::cout << "Error: No DRCVariant specified." << std::endl;
            exit(1);
        }

        drcChannels = new AbstractDRAMCache*[numChannels];
        for( ncounter_t i = 0; i < numChannels; i++ )
        {
            /* Setup the translation method for DRAM cache decoders. */
            int channels, ranks, banks, rows, cols, subarrays, ignoreBits;
            
            if( conf->KeyExists( "MATHeight" ) )
            {
                rows = conf->GetValue( "MATHeight" );
                subarrays = conf->GetValue( "ROWS" ) / conf->GetValue( "MATHeight" );
            }
            else
            {
                rows = conf->GetValue( "ROWS" );
                subarrays = 1;
            }

            cols = conf->GetValue( "COLS" );
            banks = conf->GetValue( "BANKS" );
            ranks = conf->GetValue( "RANKS" );
            channels = conf->GetValue( "DRC_CHANNELS" );

            TranslationMethod *drcMethod = new TranslationMethod();
            drcMethod->SetBitWidths( NVM::mlog2( rows ),
                                     NVM::mlog2( cols ),
                                     NVM::mlog2( banks ),
                                     NVM::mlog2( ranks ),
                                     NVM::mlog2( channels ),
                                     NVM::mlog2( subarrays )
                                     );
            drcMethod->SetCount( rows, cols, banks, ranks, channels, subarrays );
            drcMethod->SetAddressMappingScheme(conf->GetString("AddressMappingScheme"));
            
            /* When selecting a child, use the channel field from a DRC decoder. */
            DRCDecoder *drcDecoder = new DRCDecoder( );
            drcDecoder->SetConfig( config, createChildren );
            drcDecoder->SetTranslationMethod( drcMethod );
            drcDecoder->SetDefaultField( CHANNEL_FIELD );
            /* Set ignore bits for DRC decoder*/
            if( conf->KeyExists( "IgnoreBits" ) )
            {
                ignoreBits = conf->GetValue( "IgnoreBits" );
                drcDecoder->SetIgnoreBits(ignoreBits);
            }

            SetDecoder( drcDecoder );

            /* Initialize a DRAM cache channel. */
            std::stringstream formatter;

            drcChannels[i] = dynamic_cast<AbstractDRAMCache *>( MemoryControllerFactory::CreateNewController(conf->GetString( "DRCVariant" )) );
            drcChannels[i]->SetMainMemory( mainMemory );

            formatter.str( "" );
            formatter << StatName( ) << "." << conf->GetString( "DRCVariant" ) << i;
            drcChannels[i]->SetID( static_cast<int>(i) );
            drcChannels[i]->StatName( formatter.str() ); 

            drcChannels[i]->SetParent( this );
            AddChild( drcChannels[i] );

            drcChannels[i]->SetConfig( conf, createChildren );
            drcChannels[i]->RegisterStats( );
        }
        /* Add mainMemory as the last child */
        AddChild( mainMemory );
    }

    /* DRC Variant will call base SetConfig */
    //MemoryController::SetConfig( conf, createChildren );

    SetDebugName( "DRAMCache", conf );
}

void DRAMCache::RegisterStats( )
{

}

void DRAMCache::Retranslate( NVMainRequest *req )
{
    uint64_t col, row, bank, rank, chan, subarray;

    GetDecoder()->Translate( req->address.GetPhysicalAddress(), &row, &col, &bank, &rank, &chan, &subarray );
    req->address.SetTranslatedAddress( row, col, bank, rank, chan, subarray );
}

bool DRAMCache::IssueAtomic( NVMainRequest *req )
{
    uint64_t chan;

    Retranslate( req );
    req->address.GetTranslatedAddress( NULL, NULL, NULL, NULL, &chan, NULL );
    assert( chan < numChannels );
    assert( GetChild(req)->GetTrampoline() == drcChannels[chan] );

    return drcChannels[chan]->IssueAtomic( req );
}

bool DRAMCache::IsIssuable( NVMainRequest * req, FailReason * /*fail*/ )
{
    uint64_t chan;

    Retranslate( req );
    req->address.GetTranslatedAddress( NULL, NULL, NULL, NULL, &chan, NULL );
    assert( chan < numChannels );
    assert( GetChild(req)->GetTrampoline() == drcChannels[chan] );

    return drcChannels[chan]->IsIssuable( req );
}

bool DRAMCache::IssueCommand( NVMainRequest *req )
{
    uint64_t chan;

    Retranslate( req );
    req->address.GetTranslatedAddress( NULL, NULL, NULL, NULL, &chan, NULL );
    assert( chan < numChannels );
    assert( GetChild(req)->GetTrampoline() == drcChannels[chan] );

    return drcChannels[chan]->IssueCommand( req );
}

bool DRAMCache::IssueFunctional( NVMainRequest *req )
{
    uint64_t chan;

    Retranslate( req );
    req->address.GetTranslatedAddress( NULL, NULL, NULL, NULL, &chan, NULL );

    assert( chan < numChannels );

    return drcChannels[chan]->IssueFunctional( req );
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

            /* Retranslate incase the request was rerouted. */
            Retranslate( req );
            req->address.GetTranslatedAddress( NULL, NULL, NULL, NULL, &chan, NULL );

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

void DRAMCache::CalculateStats( )
{
    uint64_t i;

    for( i = 0; i < numChannels; i++ )
    {
        drcChannels[i]->CalculateStats( );
    }

    mainMemory->CalculateStats( );
}

NVMain *DRAMCache::GetMainMemory( )
{
    return mainMemory;
}
