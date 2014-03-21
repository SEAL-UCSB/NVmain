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
    ranks = NULL;
    configSet = false;
    numRanks = 0;
    syncValue = 0.0f;
}

OnChipBus::~OnChipBus( )
{
    if( numRanks > 0 )
    {
        for( ncounter_t i = 0; i < numRanks; i++ )
        {
            delete ranks[i];
        }

        delete [] ranks;
    }
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
        SetDecoder( incAT );

        ranks = new Rank * [numRanks];
        for( ncounter_t i = 0; i < numRanks; i++ )
        {
            std::stringstream formatter;

            ranks[i] = RankFactory::CreateRankNoWarn( c->GetString( "RankType" ) );

            formatter.str( "" );
            formatter << StatName( ) << ".rank" << i;
            ranks[i]->StatName( formatter.str( ) );

            ranks[i]->SetParent( this );
            AddChild( ranks[i] );

            /* SetConfig recursively */
            ranks[i]->SetConfig( conf, createChildren ); 
            ranks[i]->RegisterStats( );
        }
    }

    SetDebugName( "OnChipBus", c );
}

bool OnChipBus::CanPowerDown( const OpType& pdOp, const ncounter_t& rankId )
{
    return ranks[rankId]->CanPowerDown( pdOp );
}

bool OnChipBus::PowerDown( const OpType& pdOp, const ncounter_t& rankId )
{
    return ranks[rankId]->PowerDown( pdOp );
}

bool OnChipBus::CanPowerUp( const ncounter_t& rankId )
{
    return ranks[rankId]->CanPowerUp( );
}

bool OnChipBus::PowerUp( const ncounter_t& rankId )
{
    return ranks[rankId]->PowerUp( );
}

bool OnChipBus::IsRankIdle( const ncounter_t& rankId )
{
    return ranks[rankId]->Idle( );
}

bool OnChipBus::IssueCommand( NVMainRequest *req )
{
    ncounter_t opRank;
    bool success = false;

    req->address.GetTranslatedAddress( NULL, NULL, NULL, &opRank, NULL, NULL );

    if( ranks[opRank]->IsIssuable( req ) )
    {
        if( req->type == 0 )
        {
            std::cout << "OnChipBus got unknown op.\n";
        }
        
        assert( GetChild( req )->GetTrampoline() == ranks[opRank] );
        success = GetChild( req )->IssueCommand( req );

        /*
         *  To preserve rank-to-rank switching time, we need to notify the
         *  other ranks what the command sent to opRank was.
         */
        if( success )
        {
            for( ncounter_t i = 0; i < numRanks; i++ )
                if( (ncounter_t)(i) != opRank )
                    ranks[i]->Notify( req->type );
        }
    }

    return success;
}

bool OnChipBus::IsIssuable( NVMainRequest *req, FailReason *reason )
{
    ncounter_t opRank;

    req->address.GetTranslatedAddress( NULL, NULL, NULL, &opRank, NULL, NULL );
    
    return ranks[opRank]->IsIssuable( req, reason );
}

ncycle_t OnChipBus::GetNextActivate( ncounter_t rank, ncounter_t bank )
{
    if( rank < numRanks )
        return ranks[rank]->GetNextActivate( bank );

    return 0;
}

ncycle_t OnChipBus::GetNextRead( ncounter_t rank, ncounter_t bank )
{
    if( rank < numRanks )
        return ranks[rank]->GetNextRead( bank );

    return 0;
}

ncycle_t OnChipBus::GetNextWrite( ncounter_t rank, ncounter_t bank )
{
    if( rank < numRanks )
        return ranks[rank]->GetNextWrite( bank );

    return 0;
}

ncycle_t OnChipBus::GetNextPrecharge( ncounter_t rank, ncounter_t bank )
{
  if( rank < numRanks )
      return ranks[rank]->GetNextPrecharge( bank );

  return 0;
}

ncycle_t OnChipBus::GetNextRefresh( ncounter_t rank, ncounter_t bank )
{
    if( rank < numRanks )
        return ranks[rank]->GetNextRefresh( bank );

    return 0;
}

void OnChipBus::CalculateStats( )
{
    for( ncounter_t i = 0; i < numRanks; i++ )
    {
        ranks[i]->CalculateStats( );
    }
}

void OnChipBus::Cycle( ncycle_t steps )
{
    for( unsigned rankIdx = 0; rankIdx < numRanks; rankIdx++ )
        ranks[rankIdx]->Cycle( steps );
}
