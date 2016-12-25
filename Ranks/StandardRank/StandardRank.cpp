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
*   Tao Zhang       ( Email: tzz106 at cse dot psu dot edu
*                     Website: http://www.cse.psu.edu/~tzz106 )
*******************************************************************************/

#include "Ranks/StandardRank/StandardRank.h"
#include "src/EventQueue.h"
#include "Banks/BankFactory.h"

#include <iostream>
#include <sstream>
#include <cassert>

using namespace NVM;

std::string GetFilePath( std::string file );

StandardRank::StandardRank( )
{
    activeCycles = 0;
    standbyCycles = 0;
    fastExitActiveCycles = 0;
    fastExitPrechargeCycles = 0;
    slowExitCycles = 0;

    totalEnergy = 0.0;
    backgroundEnergy = 0.0;
    activateEnergy = 0.0;
    burstEnergy = 0.0;
    refreshEnergy = 0.0;

    totalPower = 0.0;
    backgroundPower = 0.0;
    activatePower = 0.0;
    burstPower = 0.0;
    refreshPower = 0.0;

    reads = 0;
    writes = 0;

    actWaits = 0;
    actWaitTotal = 0;
    actWaitAverage = 0.0;

    rrdWaits = 0;
    rrdWaitTotal = 0;
    rrdWaitAverage = 0.0;

    fawWaits = 0;
    fawWaitTotal = 0;
    fawWaitAverage = 0.0;

    lastActivate = NULL;
    RAWindex = 0;

    conf = NULL;

    state = STANDARDRANK_CLOSED;
    backgroundEnergy = 0.0f;

    psInterval = 0;
    lastReset = 0;
}

StandardRank::~StandardRank( )
{
    delete [] lastActivate;
}

void StandardRank::SetConfig( Config *c, bool createChildren )
{
    conf = c;

    Params *params = new Params( );
    params->SetParams( c );
    SetParams( params );

    deviceWidth = p->DeviceWidth;
    busWidth = p->BusWidth;

    banksPerRefresh = p->BanksPerRefresh;

    if( conf->GetValue( "RAW" ) == -1 )
    {
        std::cout << "NVMain Warning: RAW (Row Activation Window) is not "
            << "specified. Has set it to 4 (FAW)" << std::endl;
        rawNum = 4;
    }
    else
        rawNum = p->RAW;

    assert( rawNum != 0 );

    /* Calculate the number of devices needed. */
    deviceCount = busWidth / deviceWidth;
    if( busWidth % deviceWidth != 0 )
    {
        std::cout << "NVMain: device width is not a multiple of the bus width!\n";
        deviceCount++;
    }

    bankCount = p->BANKS;

    if( createChildren )
    {
        /* When selecting a child, use the bank field from the decoder. */
        AddressTranslator *rankAT = DecoderFactory::CreateDecoderNoWarn( conf->GetString( "Decoder" ) );
        rankAT->SetTranslationMethod( GetParent( )->GetTrampoline( )->GetDecoder( )->GetTranslationMethod( ) );
        rankAT->SetDefaultField( BANK_FIELD );
        rankAT->SetConfig( c, createChildren );
        SetDecoder( rankAT );

        std::cout << "Creating " << bankCount << " banks in all " 
            << deviceCount << " devices.\n";

        for( ncounter_t i = 0; i < bankCount; i++ )
        {
            std::stringstream formatter;

            Bank *nextBank = BankFactory::CreateBankNoWarn( conf->GetString( "BankType" ) );

            formatter << i;
            nextBank->SetId( i );
            formatter.str( "" );

            formatter << StatName( ) << ".bank" << i;
            nextBank->StatName( formatter.str( ) );

            nextBank->SetParent( this );
            AddChild( nextBank );

            /* SetConfig recursively. */
            nextBank->SetConfig( c, createChildren );
            nextBank->RegisterStats( );
        }
    }

    /* 
     * Make sure this doesn't cause unnecessary tRRD delays at start 
     * TODO: have Activate check currentCycle < tRRD/tRAW maybe?
     */
    lastActivate = new ncycle_t[rawNum];
    for( ncounter_t i = 0; i < rawNum; i++ )
        lastActivate[i] = 0;

    /* We'll say you can't do anything until the command has time to issue on the bus. */
    nextRead = p->tCMD;
    nextWrite = p->tCMD;
    nextActivate = p->tCMD;
    nextPrecharge = p->tCMD;

    fawWaits = 0;
    rrdWaits = 0;
    actWaits = 0;

    fawWaitTotal = 0;
    rrdWaitTotal = 0;
    actWaitTotal = 0;
}

void StandardRank::RegisterStats( )
{
    if( p->EnergyModel == "current" )
    {
        AddUnitStat(totalEnergy, "mA*t");
        AddUnitStat(backgroundEnergy, "mA*t");
        AddUnitStat(activateEnergy, "mA*t");
        AddUnitStat(burstEnergy, "mA*t");
        AddUnitStat(refreshEnergy, "mA*t");
    }
    else
    {
        AddUnitStat(totalEnergy, "nJ");
        AddUnitStat(backgroundEnergy, "nJ");
        AddUnitStat(activateEnergy, "nJ");
        AddUnitStat(burstEnergy, "nJ");
        AddUnitStat(refreshEnergy, "nJ");
    }

    AddUnitStat(totalPower, "W");
    AddUnitStat(backgroundPower, "W");
    AddUnitStat(activatePower, "W");
    AddUnitStat(burstPower, "W");
    AddUnitStat(refreshPower, "W");

    AddStat(reads);
    AddStat(writes);

    AddStat(activeCycles);
    AddStat(standbyCycles);
    AddStat(fastExitActiveCycles);
    AddStat(fastExitPrechargeCycles);
    AddStat(slowExitCycles);

    AddStat(actWaits);
    AddStat(actWaitTotal); 
    AddStat(actWaitAverage);

    AddStat(rrdWaits);
    AddStat(rrdWaitTotal); 
    AddStat(rrdWaitAverage); 

    AddStat(fawWaits);
    AddStat(fawWaitTotal);
    AddStat(fawWaitAverage);
}

bool StandardRank::Idle( )
{
    bool rankIdle = true;
    for( ncounter_t i = 0; i < bankCount; i++ )
    {
        if( GetChild(i)->Idle( ) == false )
        {
            rankIdle = false;
            break;
        }
    }

    return rankIdle;
}

bool StandardRank::Activate( NVMainRequest *request )
{
    uint64_t activateBank;

    request->address.GetTranslatedAddress( NULL, NULL, &activateBank, NULL, NULL, NULL );

    if( activateBank >= bankCount )
    {
        std::cerr << "Rank: Attempted to activate non-existant bank " << activateBank << std::endl;
        return false;
    }
    
    /*
     *  Ensure that the time since the last bank activation is >= tRRD. This is to limit
     *  power consumption.
     */
    if( nextActivate <= GetEventQueue()->GetCurrentCycle() 
        && lastActivate[( RAWindex + 1 ) % rawNum] + p->tRAW 
            <= GetEventQueue( )->GetCurrentCycle( ) )
    {
        /* issue ACTIVATE to target bank */
        GetChild( request )->IssueCommand( request );

        if( state == STANDARDRANK_CLOSED )
            state = STANDARDRANK_OPEN;

        /* move to the next counter */
        RAWindex = (RAWindex + 1) % rawNum;
        lastActivate[RAWindex] = GetEventQueue()->GetCurrentCycle();
        nextActivate = MAX( nextActivate, 
                            GetEventQueue()->GetCurrentCycle() + p->tRRDR );
    }
    else
    {
        std::cerr << "NVMain Error: Rank Activation FAILED! " 
            << "Did you check IsIssuable?" << std::endl;
    }

    return true;
}

bool StandardRank::Read( NVMainRequest *request )
{
    uint64_t readBank;

    request->address.GetTranslatedAddress( NULL, NULL, &readBank, NULL, NULL, NULL );

    if( readBank >= bankCount )
    {
        std::cerr << "NVMain Error: Rank attempted to read non-existant bank: " 
            << readBank << "!" << std::endl;
        return false;
    }

    if( nextRead > GetEventQueue()->GetCurrentCycle() )
    {
        std::cerr << "NVMain Error: Rank Read violates the timing constraint: " 
            << readBank << "!" << std::endl;
        return false;
    }

    /* issue READ or READ_PRECHARGE to target bank */
    bool success = GetChild( request )->IssueCommand( request );

    /* Even though the command may be READ_PRECHARGE, it still works */
    nextRead = MAX( nextRead, 
                    GetEventQueue()->GetCurrentCycle() 
                    + MAX( p->tBURST, p->tCCD ) * request->burstCount );

    nextWrite = MAX( nextWrite, 
                     GetEventQueue()->GetCurrentCycle() 
                     + MAX( p->tBURST, p->tCCD ) * (request->burstCount - 1)
                     + p->tCAS + p->tBURST + p->tRTRS - p->tCWD ); 

    /* if it has implicit precharge, insert the precharge to close the rank */ 
    if( request->type == READ_PRECHARGE )
    {
        NVMainRequest* dupPRE = new NVMainRequest;
        dupPRE->type = PRECHARGE;
        dupPRE->owner = this;

        GetEventQueue( )->InsertEvent( EventResponse, this, dupPRE, 
            MAX( p->tBURST, p->tCCD ) * (request->burstCount - 1)
            + GetEventQueue( )->GetCurrentCycle( ) + p->tAL + p->tRTP );
    }

    if( success == false )
    {
        std::cerr << "NVMain Error: Rank Read FAILED! Did you check IsIssuable?" 
            << std::endl;
    }

    return success;
}


bool StandardRank::Write( NVMainRequest *request )
{
    uint64_t writeBank;

    request->address.GetTranslatedAddress( NULL, NULL, &writeBank, NULL, NULL, NULL );

    if( writeBank >= bankCount )
    {
        std::cerr << "NVMain Error: Attempted to write non-existant bank: " 
            << writeBank << "!" << std::endl;
        return false;
    }

    if( nextWrite > GetEventQueue()->GetCurrentCycle() )
    {
        std::cerr << "NVMain Error: Rank Write violates the timing constraint: " 
            << writeBank << "!" << std::endl;
        return false;
    }

    /* issue WRITE or WRITE_PRECHARGE to the target bank */
    bool success = GetChild( request )->IssueCommand( request );

    /* Even though the command may be WRITE_PRECHARGE, it still works */
    nextRead = MAX( nextRead, 
                    GetEventQueue()->GetCurrentCycle() 
                    + MAX( p->tBURST, p->tCCD ) * (request->burstCount - 1)
                    + p->tCWD + p->tBURST + p->tWTR );

    nextWrite = MAX( nextWrite, 
                     GetEventQueue()->GetCurrentCycle() 
                     + MAX( p->tBURST, p->tCCD ) * request->burstCount );

    /* if it has implicit precharge, insert the precharge to close the rank */ 
    if( request->type == WRITE_PRECHARGE )
    {
        NVMainRequest* dupPRE = new NVMainRequest;
        dupPRE->type = PRECHARGE;
        dupPRE->owner = this;

        GetEventQueue( )->InsertEvent( EventResponse, this, dupPRE, 
                        GetEventQueue( )->GetCurrentCycle( ) 
                        + MAX( p->tBURST, p->tCCD ) * (request->burstCount - 1)
                        + p->tAL + p->tCWD + p->tBURST + p->tWR );
    }

    if( success == false )
    {
        std::cerr << "NVMain Error: Rank Write FAILED! Did you check IsIssuable?" 
            << std::endl;
    }

    return success;
}

bool StandardRank::Precharge( NVMainRequest *request )
{
    uint64_t preBank;

    request->address.GetTranslatedAddress( NULL, NULL, &preBank, NULL, NULL, NULL );

    if( preBank >= bankCount )
    {
        std::cerr << "NVMain Error: Rank Attempted to precharge non-existant bank: " 
            << preBank << std::endl;
        return false;
    }

    /*
     *  There are no rank-level constraints on precharges. If the bank says timing
     *  was met we can send the command to the bank.
     */
    /* issue PRECHARGE/PRECHARGE_ALL to the target bank */
    bool success = GetChild( request )->IssueCommand( request );

    if( Idle( ) )
        state = STANDARDRANK_CLOSED;

    nextPrecharge = MAX( nextPrecharge, 
                         GetEventQueue()->GetCurrentCycle() + p->tPPD );

    if( success == false )
    {
        std::cerr << "NVMain Error: Rank Precharge FAILED! Did you check IsIssuable?" 
            << std::endl;
    }

    return success;
}

bool StandardRank::CanPowerDown( NVMainRequest *request )
{
    bool issuable = true;

    if( state == STANDARDRANK_REFRESHING )
        return false;

    for( ncounter_t childIdx = 0; childIdx < GetChildren().size(); childIdx++ )
    {
        if( GetChild(childIdx)->IsIssuable( request ) == false )
        {
            issuable = false;
            break;
        }
    }

    return issuable;
}

bool StandardRank::PowerDown( NVMainRequest *request )
{
    /* 
     * PowerDown() should be completed to all banks, partial PowerDown is
     * incorrect. Therefore, call CanPowerDown() first before every PowerDown
     */
    for( ncounter_t childIdx = 0; childIdx < GetChildren().size(); childIdx++ )
    {
#ifndef NDEBUG
        bool rv = GetChild(childIdx)->IssueCommand( request );
        assert( rv == true );
#else
        GetChild(childIdx)->IssueCommand( request );
#endif
    }

    switch( request->type )
    {
        case POWERDOWN_PDA:
            state = STANDARDRANK_PDA;
            break;

        case POWERDOWN_PDPF:
            state = STANDARDRANK_PDPF;
            break;

        case POWERDOWN_PDPS:
            state = STANDARDRANK_PDPS;
            break;

        default:
            std::cerr<< "NVMain Error: Unrecognized PowerDown command " 
                << request->type << " is detected in Rank " << std::endl; 
            break;
    }

    GetEventQueue( )->InsertEvent( EventResponse, this, request, 
            GetEventQueue()->GetCurrentCycle() + p->tPD );
    
    return true;
}

bool StandardRank::CanPowerUp( NVMainRequest *request )
{
    bool issuable = true;
    ncounter_t issuableCount = 0;

    /* Since all banks are powered down, check the first bank is enough */
    for( ncounter_t childIdx = 0; childIdx < GetChildren().size(); childIdx++ )
    {
        if( GetChild(childIdx)->IsIssuable( request ) == false )
        {
            issuable = false;
        }
        else
        {
            issuableCount++;
        }
    }

    assert( issuableCount == 0 || issuableCount == GetChildren().size() );

    return issuable;
}

bool StandardRank::PowerUp( NVMainRequest *request )
{
    ncounter_t puTimer = 1;

    /* 
     * PowerUp() should be completed to all banks, partial PowerUp is
     * incorrect. Therefore, call CanPowerUp() first before every PowerDown
     */
    for( ncounter_t childIdx = 0; childIdx < GetChildren().size(); childIdx++ )
    {
#ifndef NDEBUG
        bool rv = GetChild(childIdx)->IssueCommand( request );
        assert( rv == true );
#else
        GetChild(childIdx)->IssueCommand( request );
#endif
    }

    switch( state )
    {
        case STANDARDRANK_PDA:
            state = STANDARDRANK_OPEN;
            puTimer = p->tXP;
            break;

        case STANDARDRANK_PDPF:
            puTimer = p->tXP;
            state = STANDARDRANK_CLOSED;
            break;

        case STANDARDRANK_PDPS:
            puTimer = p->tXPDLL;
            state = STANDARDRANK_CLOSED;
            break;

        default:
            std::cerr<< "NVMain Error: PowerUp is issued to a Rank that is not " 
                << "PowerDown before. The current rank state is " << state 
                << std::endl; 
            break;
    }
    
    GetEventQueue( )->InsertEvent( EventResponse, this, request, 
            GetEventQueue()->GetCurrentCycle() + puTimer );
    
    return true;
}

/*
 * refresh is issued to those banks that start from the bank specified by the
 * request.  
 */
bool StandardRank::Refresh( NVMainRequest *request )
{
    assert( nextActivate <= ( GetEventQueue()->GetCurrentCycle() ) );
    uint64_t refreshBankGroupHead;
    request->address.GetTranslatedAddress( 
            NULL, NULL, &refreshBankGroupHead, NULL, NULL, NULL );

    assert( (refreshBankGroupHead + banksPerRefresh) <= bankCount );

    for( ncounter_t i = 0; i < banksPerRefresh; i++ )
    {
        NVMainRequest* refReq = new NVMainRequest;
        *refReq = *request;
        GetChild( refreshBankGroupHead+i )->IssueCommand( refReq );
    }

    state = STANDARDRANK_REFRESHING;

    request->owner = this;
    GetEventQueue( )->InsertEvent( EventResponse, this, request, 
        GetEventQueue()->GetCurrentCycle() + p->tRFC );

    /*
     * simply treat the REFRESH as an ACTIVATE. For a finer refresh
     * granularity, the nextActivate does not block the other bank groups
     */
    nextActivate = MAX( nextActivate, GetEventQueue( )->GetCurrentCycle( ) 
                                        + p->tRRDR );
    RAWindex = (RAWindex + 1) % rawNum;
    lastActivate[RAWindex] = GetEventQueue( )->GetCurrentCycle( );

    return true;
}

ncycle_t StandardRank::NextIssuable( NVMainRequest *request )
{
    ncycle_t nextCompare = 0;
    ncounter_t bank;

    request->address.GetTranslatedAddress( NULL, NULL, &bank, NULL, NULL, NULL );

    if( request->type == ACTIVATE || request->type == REFRESH ) nextCompare = MAX( nextActivate, lastActivate[(RAWindex+1)%rawNum] + p->tRAW );
    else if( request->type == READ || request->type == READ_PRECHARGE ) nextCompare = nextRead;
    else if( request->type == WRITE || request->type == WRITE_PRECHARGE ) nextCompare = nextWrite;
    else if( request->type == PRECHARGE || request->type == PRECHARGE_ALL ) nextCompare = nextPrecharge;
    else assert(false);
        
    return MAX(GetChild( request )->NextIssuable( request ), nextCompare );
}

bool StandardRank::IsIssuable( NVMainRequest *req, FailReason *reason )
{
    uint64_t opBank;
    bool rv;
    
    req->address.GetTranslatedAddress( NULL, NULL, &opBank, NULL, NULL, NULL );

    rv = true;

    if( req->type == ACTIVATE )
    {
        if( nextActivate > GetEventQueue( )->GetCurrentCycle( ) 
            || ( lastActivate[(RAWindex + 1) % rawNum] + p->tRAW ) 
                > GetEventQueue()->GetCurrentCycle() )  
        {
            rv = false;

            if( reason ) 
                reason->reason = RANK_TIMING;
        }
        else
        {
            rv = GetChild( req )->IsIssuable( req, reason );
        }

        if( rv == false )
        {
            if( nextActivate > GetEventQueue( )->GetCurrentCycle( ) )
            {
                actWaits++;
                actWaitTotal += nextActivate - GetEventQueue( )->GetCurrentCycle( );
            }

            if( ( lastActivate[RAWindex] + p->tRRDR )
                    > GetEventQueue( )->GetCurrentCycle( ) ) 
            {
                rrdWaits++;
                rrdWaitTotal += ( lastActivate[RAWindex] + 
                        p->tRRDR - (GetEventQueue()->GetCurrentCycle()) );
            }
            if( ( lastActivate[( RAWindex + 1 ) % rawNum] + p->tRAW )
                    > GetEventQueue( )->GetCurrentCycle( ) ) 
            {
                fawWaits++;
                fawWaitTotal += ( lastActivate[( RAWindex + 1 ) % rawNum] + 
                    p->tRAW - GetEventQueue( )->GetCurrentCycle( ) );
            }
        }
    }
    else if( req->type == READ || req->type == READ_PRECHARGE )
    {
        if( nextRead > GetEventQueue( )->GetCurrentCycle( ) )
        {
            rv = false;

            if( reason ) 
                reason->reason = RANK_TIMING;
        }
        else
        {
            rv = GetChild( req )->IsIssuable( req, reason );
        }
    }
    else if( req->type == WRITE || req->type == WRITE_PRECHARGE )
    {
        if( nextWrite > GetEventQueue( )->GetCurrentCycle( ) )
        {
            rv = false;

            if( reason ) 
                reason->reason = RANK_TIMING;
        }
        else
        {
            rv = GetChild( req )->IsIssuable( req, reason );
        }
    }
    else if( req->type == PRECHARGE || req->type == PRECHARGE_ALL )
    {
        if( nextPrecharge > GetEventQueue( )->GetCurrentCycle( ) ) 
        {
            rv = false;

            if( reason ) 
                reason->reason = RANK_TIMING;
        }
        else
        {
            rv = GetChild( req )->IsIssuable( req, reason );
        }
    }
    else if( req->type == POWERDOWN_PDA 
            || req->type == POWERDOWN_PDPF 
            || req->type == POWERDOWN_PDPS )
    {
        rv = CanPowerDown( req );

        if( !rv && reason ) 
            reason->reason = RANK_TIMING;
    }
    else if( req->type == POWERUP )
    {
        rv = CanPowerUp( req );

        if( !rv && reason ) 
            reason->reason = RANK_TIMING;
    }
    else if( req->type == REFRESH )
    {
        /* firstly, check whether REFRESH can be issued to a rank */
        if( nextActivate > GetEventQueue()->GetCurrentCycle() 
            || ( lastActivate[( RAWindex + 1 ) % rawNum] + p->tRAW 
                > GetEventQueue( )->GetCurrentCycle( ) )  )
        {
            rv = false;
            if( reason ) 
                reason->reason = RANK_TIMING;

            return rv;
        }

        /* REFRESH can only be issued when all banks in the group are issuable */ 
        assert( (opBank + banksPerRefresh) <= bankCount );

        for( ncounter_t i = 0; i < banksPerRefresh; i++ )
        {
            rv = GetChild( opBank + i )->IsIssuable( req, reason );
            if( rv == false )
                return rv;
        }
    }
    else
    {
        /* Unknown command -- See if child module can handle it. */
        rv = GetChild( req )->IsIssuable( req, reason );
    }

    return rv;
}

bool StandardRank::IssueCommand( NVMainRequest *req )
{
    bool rv = false;

    if( !IsIssuable( req ) )
    {
        uint64_t bank, rank, channel;
        req->address.GetTranslatedAddress( NULL, NULL, &bank, &rank, &channel, NULL );
        std::cout << "NVMain: Rank: Warning: Command " << req->type 
            << " @ Bank " << bank << " Rank " << rank << " Channel " << channel 
            << " can not be issued!\n" << std::endl;
    }
    else
    {
        rv = true;
        
        switch( req->type )
        {
            case ACTIVATE:
                rv = this->Activate( req );
                break;
            
            case READ:
            case READ_PRECHARGE:
                rv = this->Read( req );
                break;
            
            case WRITE:
            case WRITE_PRECHARGE:
                rv = this->Write( req );
                break;
            
            case PRECHARGE:
            case PRECHARGE_ALL:
                rv = this->Precharge( req );
                break;

            case POWERDOWN_PDA:
            case POWERDOWN_PDPF: 
            case POWERDOWN_PDPS: 
                rv = this->PowerDown( req );
                break;

            case POWERUP:
                rv = this->PowerUp( req );
                break;
        
            case REFRESH:
                rv = this->Refresh( req );
                break;

            default:
                std::cout << "NVMain: Rank: Unknown operation in command queue! " 
                    << req->type << std::endl;
                break;  
        }
    }

    return rv;
}

/* 
 *  Other ranks should notify us when they read/write so we can ensure minimum 
 *  timings are met.
 */
void StandardRank::Notify( NVMainRequest *request )
{
    OpType op = request->type;

    /* We only care if other ranks are reading/writing (to avoid bus contention) */
    if( op == READ || op == READ_PRECHARGE )
    {
        nextRead = MAX( nextRead, GetEventQueue()->GetCurrentCycle() 
                                    + p->tBURST + p->tRTRS );

        nextWrite = MAX( nextWrite, GetEventQueue()->GetCurrentCycle() 
                                    + p->tCAS + p->tBURST + p->tRTRS - p->tCWD);
    }
    else if( op == WRITE || op == WRITE_PRECHARGE )
    {
        nextWrite = MAX( nextWrite, GetEventQueue()->GetCurrentCycle() 
                                    + p->tBURST + p->tOST );

        nextRead = MAX( nextRead, GetEventQueue()->GetCurrentCycle()
                                    + p->tBURST + p->tCWD + p->tRTRS - p->tCAS );
    }
}

bool StandardRank::RequestComplete( NVMainRequest* req )
{
    if( req->owner == this )
    {
        switch( req->type )
        {
            /* 
             * check whether all banks are idle. some banks may be active
             * due to the possible fine-grained structure 
             */
            case PRECHARGE:
            case REFRESH:
                {
                    if( Idle( ) )
                        state = STANDARDRANK_CLOSED;

                    break;
                }

            default:
                break;
        }

        delete req;
        return true;
    }
    else
        return GetParent( )->RequestComplete( req );
}

void StandardRank::Cycle( ncycle_t steps )
{
    for( ncounter_t childIdx = 0; childIdx < GetChildCount( ); childIdx++ )
        GetChild( childIdx )->Cycle( steps );

    /* Count cycle numbers and calculate background energy for each state */
    switch( state )
    {
        /* active powerdown */
        case STANDARDRANK_PDA:
            fastExitActiveCycles += steps;
            if( p->EnergyModel == "current" )
                backgroundEnergy += ( p->EIDD3P * (double)steps ) * (double)deviceCount;  
            else
                backgroundEnergy += ( p->Epda * (double)steps );  
            break;

        /* precharge powerdown fast exit */
        case STANDARDRANK_PDPF:
            fastExitPrechargeCycles += steps;
            if( p->EnergyModel == "current" )
                backgroundEnergy += ( p->EIDD2P1 * (double)steps ) * (double)deviceCount;
            else 
                backgroundEnergy += ( p->Epdpf * (double)steps );  
            break;

        /* precharge powerdown slow exit */
        case STANDARDRANK_PDPS:
            slowExitCycles += steps;
            if( p->EnergyModel == "current" )
                backgroundEnergy += ( p->EIDD2P0 * (double)steps ) * (double)deviceCount;  
            else 
                backgroundEnergy += ( p->Epdps * (double)steps );  
            break;

        /* active standby */
        case STANDARDRANK_REFRESHING:
        case STANDARDRANK_OPEN:
            activeCycles += steps;
            if( p->EnergyModel == "current" )
                backgroundEnergy += ( p->EIDD3N * (double)steps ) * (double)deviceCount;  
            else
                backgroundEnergy += ( p->Eactstdby * (double)steps );  
            break;

        /* precharge standby */
        case STANDARDRANK_CLOSED:
            standbyCycles += steps;
            if( p->EnergyModel == "current" )
                backgroundEnergy += ( p->EIDD2N * (double)steps ) * (double)deviceCount;  
            else
                backgroundEnergy += ( p->Eprestdby * (double)steps );  
            break;

        default:
            if( p->EnergyModel == "current" )
                backgroundEnergy += ( p->EIDD2N * (double)steps ) * (double)deviceCount;  
            else
                backgroundEnergy += ( p->Eprestdby * (double)steps );  
            break;
    }
}

void StandardRank::CalculateStats( )
{
    NVMObject::CalculateStats( );

    totalEnergy = activateEnergy = burstEnergy = refreshEnergy = 0.0;
    totalPower = backgroundPower = activatePower = burstPower = refreshPower = 0.0;
    reads = writes = 0;

    for( ncounter_t i = 0; i < bankCount; i++ )
    {
        StatType bankEstat = GetStat( GetChild(i), "bankEnergy" );
        StatType actEstat =  GetStat( GetChild(i), "activeEnergy" );
        StatType bstEstat =  GetStat( GetChild(i), "burstEnergy" );
        StatType refEstat =  GetStat( GetChild(i), "refreshEnergy" );

        totalEnergy += CastStat( bankEstat, double );
        activateEnergy += CastStat( actEstat, double );
        burstEnergy += CastStat( bstEstat, double );
        refreshEnergy += CastStat( refEstat, double );

        StatType readCount = GetStat( GetChild(i), "reads" );
        StatType writeCount = GetStat( GetChild(i), "writes" );

        reads += CastStat( readCount, ncounter_t );
        writes += CastStat( writeCount, ncounter_t );
    }


    /* Get simulation time in nanoseconds (ns). Since energy is in nJ, energy / ns = W */
    double simulationTime = 1.0;
    
    if( p->EnergyModel == "current" )
    {
        simulationTime = GetEventQueue()->GetCurrentCycle() - lastReset;
    }
    else
    {
        simulationTime = static_cast<double>(GetEventQueue()->GetCurrentCycle() - lastReset) 
                       * (1000.0 / static_cast<double>(p->CLK));
    }

    if( simulationTime != 0 )
    {
        /* power in W */
        if( p->EnergyModel == "current" )
        {
            backgroundPower = ( backgroundEnergy / (double)deviceCount * p->Voltage ) / (double)simulationTime / 1000.0; 
            activatePower = ( activateEnergy * p->Voltage ) / (double)simulationTime / 1000.0; 
            burstPower = ( burstEnergy * p->Voltage ) / (double)simulationTime / 1000.0; 
            refreshPower = ( refreshEnergy * p->Voltage ) / (double)simulationTime / 1000.0; 
        }
        else
        {
            backgroundPower = backgroundEnergy / ((double)simulationTime);
            activatePower = activateEnergy / ((double)simulationTime);
            burstPower = burstEnergy / ((double)simulationTime);
            refreshPower = refreshEnergy / ((double)simulationTime);
        }
    }

    /* Current mode is measured on a per-device basis. */
    if( p->EnergyModel == "current" )
    {
        /* energy breakdown. device is in lockstep within a rank */
        activateEnergy *= (double)deviceCount;
        burstEnergy *= (double)deviceCount;
        refreshEnergy *= (double)deviceCount;

        /* power breakdown. device is in lockstep within a rank */
        activatePower *= (double)deviceCount;
        burstPower *= (double)deviceCount;
        refreshPower *= (double)deviceCount;
    }

    totalEnergy += activateEnergy + burstEnergy + refreshEnergy + backgroundEnergy;
    totalPower += activatePower + burstPower + refreshPower + backgroundPower;

    actWaitAverage = static_cast<double>(actWaitTotal) / static_cast<double>(actWaits);
    rrdWaitAverage = static_cast<double>(rrdWaitTotal) / static_cast<double>(rrdWaits);
    fawWaitAverage = static_cast<double>(fawWaitTotal) / static_cast<double>(fawWaits);
}

void StandardRank::ResetStats( )
{
    lastReset = GetEventQueue()->GetCurrentCycle();
}

