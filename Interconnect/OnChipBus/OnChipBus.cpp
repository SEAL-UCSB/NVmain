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

#include "Interconnect/OnChipBus/OnChipBus.h"
#include "Ranks/RankFactory.h"
#include "src/EventQueue.h"

#include <sstream>
#include <iostream>
#include <cassert>

using namespace NVM;

OnChipBus::OnChipBus( )
{
    conf = NULL;
    configSet = false;
    numRanks = 0;
    syncValue = 0.0f;
}

OnChipBus::~OnChipBus( )
{
}

void OnChipBus::SetConfig( Config *c, bool createChildren )
{
    Params *params = new Params( );
    params->SetParams( c );
    SetParams( params );

    conf = c;
    configSet = true;

    numRanks = p->RANKS;

    if( createChildren )
    {
        /* When selecting a child, use the rank field from the decoder. */
        AddressTranslator *incAT = DecoderFactory::CreateDecoderNoWarn( c->GetString( "Decoder" ) );
        TranslationMethod *method = GetParent()->GetTrampoline()->GetDecoder()->GetTranslationMethod();
        incAT->SetTranslationMethod( method );
        incAT->SetDefaultField( RANK_FIELD );
        incAT->SetConfig( c, createChildren );
        SetDecoder( incAT );

        for( ncounter_t i = 0; i < numRanks; i++ )
        {
            std::stringstream formatter;

            Rank *nextRank = RankFactory::CreateRankNoWarn( c->GetString( "RankType" ) );

            formatter.str( "" );
            formatter << StatName( ) << ".rank" << i;
            nextRank->StatName( formatter.str( ) );

            nextRank->SetParent( this );
            AddChild( nextRank );

            /* SetConfig recursively */
            nextRank->SetConfig( conf, createChildren ); 
            nextRank->RegisterStats( );
        }
    }

    SetDebugName( "OnChipBus", c );
}

bool OnChipBus::IssueCommand( NVMainRequest *req )
{
    ncounter_t opRank;
    bool success = false;

    req->address.GetTranslatedAddress( NULL, NULL, NULL, &opRank, NULL, NULL );

    assert( GetChild( req )->IsIssuable( req ) );
        
    success = GetChild( req )->IssueCommand( req );

    /*
     *  To preserve rank-to-rank switching time, we need to notify the
     *  other ranks what the command sent to opRank was.
     */
    if( success )
    {
        for( ncounter_t childIdx = 0; childIdx < GetChildCount( ); childIdx++ )
          if( GetChild( req ) != GetChild( childIdx ) )
            GetChild( childIdx )->Notify( req );
    }

    return success;
}

bool OnChipBus::IsIssuable( NVMainRequest *req, FailReason *reason )
{
    return GetChild( req )->IsIssuable( req, reason );
}

void OnChipBus::CalculateStats( )
{
    for( ncounter_t childIdx = 0; childIdx < GetChildren().size(); childIdx++ )
    {
        GetChild(childIdx)->CalculateStats( );
    }
}

void OnChipBus::Cycle( ncycle_t steps )
{
    for( ncounter_t childIdx = 0; childIdx < GetChildren().size(); childIdx++ )
    {
        GetChild(childIdx)->Cycle( steps );
    }
}
