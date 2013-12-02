/*******************************************************************************
* Copyright (c) 2012-2013, The Microsystems Design Labratory (MDL)
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

#include "src/Rank.h"
#include "src/EventQueue.h"

#include <iostream>
#include <sstream>
#include <cassert>

using namespace NVM;

std::string GetFilePath( std::string file );

Rank::Rank( )
{
    activeCycles = 0;
    standbyCycles = 0;
    fastExitCycles = 0;
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

    state = RANK_CLOSED;
    backgroundEnergy = 0.0f;

    psInterval = 0;
}

Rank::~Rank( )
{
    for( ncounter_t i = 0; i < bankCount; i++ )
        delete banks[i];

    delete [] banks;

    delete [] lastActivate;
}

void Rank::SetConfig( Config *c )
{
    conf = c;

    Params *params = new Params( );
    params->SetParams( c );
    SetParams( params );

    bankCount = p->BANKS;
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

    std::cout << "Creating " << bankCount << " banks in all " 
        << deviceCount << " devices.\n";

    banks = new Bank*[bankCount];
    
    for( ncounter_t i = 0; i < bankCount; i++ )
    {
        std::stringstream formatter;

        banks[i] = new Bank;

        formatter << i;
        banks[i]->SetName( formatter.str( ) );
        banks[i]->SetId( i );
        formatter.str( "" );

        formatter << statName << ".bank" << i;
        banks[i]->StatName( formatter.str( ) );

        banks[i]->SetParent( this );
        AddChild( banks[i] );

        /* SetConfig recursively. */
        banks[i]->SetConfig( c );
        banks[i]->RegisterStats( );
    }

    /* 
     * Make sure this doesn't cause unnecessary tRRD delays at start 
     * TODO: have Activate check currentCycle < tRRD/tRAW maybe?
     */
    lastActivate = new ncycle_t[rawNum];
    for( ncounter_t i = 0; i < rawNum; i++ )
        lastActivate[i] = 0;

    /* When selecting a child, use the rank field from the decoder. */
    AddressTranslator *rankAT = DecoderFactory::CreateDecoderNoWarn( conf->GetString( "Decoder" ) );
    rankAT->SetTranslationMethod( GetParent( )->GetTrampoline( )->GetDecoder( )->GetTranslationMethod( ) );
    rankAT->SetDefaultField( BANK_FIELD );
    SetDecoder( rankAT );

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

void Rank::RegisterStats( )
{
    if( p->EnergyModel_set && p->EnergyModel == "current" )
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
    AddStat(fastExitCycles);
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

bool Rank::Idle( )
{
    bool rankIdle = true;
    for( ncounter_t i = 0; i < bankCount; i++ )
    {
        if( banks[i]->Idle( ) == false )
        {
            rankIdle = false;
            break;
        }
    }

    return rankIdle;
}

bool Rank::Activate( NVMainRequest *request )
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

        if( state == RANK_CLOSED )
            state = RANK_OPEN;

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

bool Rank::Read( NVMainRequest *request )
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
                        + MAX( p->tBURST, p->tCCD ) );

    nextWrite = MAX( nextWrite, 
                     GetEventQueue()->GetCurrentCycle() 
                         + p->tCAS + p->tBURST + p->tRTRS - p->tCWD ); 

    /* if it has implicit precharge, insert the precharge to close the rank */ 
    if( request->type == READ_PRECHARGE )
    {
        NVMainRequest* dupPRE = new NVMainRequest;
        dupPRE->type = PRECHARGE;
        dupPRE->owner = this;

        GetEventQueue( )->InsertEvent( EventResponse, this, dupPRE, 
            GetEventQueue( )->GetCurrentCycle( ) + p->tAL + p->tRTP );
    }

    if( success == false )
    {
        std::cerr << "NVMain Error: Rank Read FAILED! Did you check IsIssuable?" 
            << std::endl;
    }

    return success;
}


bool Rank::Write( NVMainRequest *request )
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
                        + p->tCWD + p->tBURST + p->tWTR );

    nextWrite = MAX( nextWrite, 
                     GetEventQueue()->GetCurrentCycle() 
                         + MAX( p->tBURST, p->tCCD ) );

    /* if it has implicit precharge, insert the precharge to close the rank */ 
    if( request->type == WRITE_PRECHARGE )
    {
        NVMainRequest* dupPRE = new NVMainRequest;
        dupPRE->type = PRECHARGE;
        dupPRE->owner = this;

        GetEventQueue( )->InsertEvent( EventResponse, this, dupPRE, 
            GetEventQueue( )->GetCurrentCycle( ) 
                + p->tAL + p->tCWD + p->tBURST + p->tWR );
    }

    if( success == false )
    {
        std::cerr << "NVMain Error: Rank Write FAILED! Did you check IsIssuable?" 
            << std::endl;
    }

    return success;
}

bool Rank::Precharge( NVMainRequest *request )
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
        state = RANK_CLOSED;

    if( success == false )
    {
        std::cerr << "NVMain Error: Rank Precharge FAILED! Did you check IsIssuable?" 
            << std::endl;
    }

    return success;
}

bool Rank::CanPowerDown( const OpType& pdOp )
{
    bool issuable = true;
    NVMainRequest req;
    NVMAddress address;

    if( state == RANK_REFRESHING )
        return false;

    /* Create a dummy operation to determine if we can issue */
    req.type = pdOp;
    req.address.SetTranslatedAddress( 0, 0, 0, 0, 0, 0 );
    req.address.SetPhysicalAddress( 0 );

    for( ncounter_t i = 0; i < bankCount; i++ )
    {
        if( banks[i]->IsIssuable( &req ) == false )
        {
            issuable = false;
            break;
        }
    }

    return issuable;
}

bool Rank::PowerDown( const OpType& pdOp )
{
    /* TODO: use hooker to issue POWERDOWN?? */
    /* 
     * PowerDown() should be completed to all banks, partial PowerDown is
     * incorrect. Therefore, call CanPowerDown() first before every PowerDown
     */
    for( ncounter_t i = 0; i < bankCount; i++ )
       banks[i]->PowerDown( pdOp );

    switch( pdOp )
    {
        case POWERDOWN_PDA:
            state = RANK_PDA;
            break;

        case POWERDOWN_PDPF:
            state = RANK_PDPF;
            break;

        case POWERDOWN_PDPS:
            state = RANK_PDPS;
            break;

        default:
            std::cerr<< "NVMain Error: Unrecognized PowerDown command " 
                << pdOp << " is detected in Rank " << std::endl; 
            break;
    }
    
    return true;
}

bool Rank::CanPowerUp()
{
    bool issuable = true;
    NVMainRequest req;
    NVMAddress address;

    /* Create a dummy operation to determine if we can issue */
    req.type = POWERUP;
    req.address.SetTranslatedAddress( 0, 0, 0, 0, 0, 0 );
    req.address.SetPhysicalAddress( 0 );

    /* since all banks are powered down, check the first bank is enough */
    if( banks[0]->IsIssuable( &req ) == false )
        issuable = false;

    return issuable;
}

bool Rank::PowerUp( )
{
    /* TODO: use hooker to issue POWERDOWN?? */
    /* 
     * PowerUp() should be completed to all banks, partial PowerUp is
     * incorrect. Therefore, call CanPowerUp() first before every PowerDown
     */
    for( ncounter_t i = 0; i < bankCount; i++ )
       banks[i]->PowerUp( );

    switch( state )
    {
        case RANK_PDA:
            state = RANK_OPEN;
            break;

        case RANK_PDPF:
        case RANK_PDPS:
            state = RANK_CLOSED;
            break;

        default:
            std::cerr<< "NVMain Error: PowerUp is issued to a Rank that is not " 
                << "PowerDown before. The current rank state is " << state 
                << std::endl; 
            break;
    }
    
    return true;
}

/*
 * refresh is issued to those banks that start from the bank specified by the
 * request.  
 */
bool Rank::Refresh( NVMainRequest *request )
{
    assert( nextActivate <= ( GetEventQueue()->GetCurrentCycle() ) );
    uint64_t refreshBankGroupHead;
    request->address.GetTranslatedAddress( 
            NULL, NULL, &refreshBankGroupHead, NULL, NULL, NULL );

    assert( (refreshBankGroupHead + banksPerRefresh) <= bankCount );

    /* TODO: use hooker to issue REFRESH?? */
    for( ncounter_t i = 0; i < banksPerRefresh; i++ )
    {
        NVMainRequest* refReq = new NVMainRequest;
        *refReq = *request;
        banks[refreshBankGroupHead + i]->IssueCommand( refReq );
    }

    state = RANK_REFRESHING;

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

ncycle_t Rank::GetNextActivate( uint64_t bank )
{
    return MAX( 
                MAX( nextActivate, banks[bank]->GetNextActivate( ) ),
                lastActivate[( RAWindex + 1 ) % rawNum] + p->tRAW 
              );
}

ncycle_t Rank::GetNextRead( uint64_t bank )
{
    return MAX( nextRead, banks[bank]->GetNextRead( ) );
}

ncycle_t Rank::GetNextWrite( uint64_t bank )
{
    return MAX( nextWrite, banks[bank]->GetNextWrite( ) );
}

ncycle_t Rank::GetNextPrecharge( uint64_t bank )
{
    return MAX( nextPrecharge, banks[bank]->GetNextPrecharge( ) );
}

ncycle_t Rank::GetNextRefresh( uint64_t bank )
{
    return banks[bank]->GetNextRefresh( );
}

bool Rank::IsIssuable( NVMainRequest *req, FailReason *reason )
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
            rv = banks[opBank]->IsIssuable( req, reason );

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
            rv = banks[opBank]->IsIssuable( req, reason );
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
            rv = banks[opBank]->IsIssuable( req, reason );
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
            rv = banks[opBank]->IsIssuable( req, reason );
    }
    else if( req->type == POWERDOWN_PDA 
            || req->type == POWERDOWN_PDPF 
            || req->type == POWERDOWN_PDPS )
    {
        rv = CanPowerDown( req->type );

        if( reason ) 
            reason->reason = RANK_TIMING;
    }
    else if( req->type == POWERUP )
    {
        rv = CanPowerUp( );

        if( reason ) 
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
            rv = banks[opBank + i]->IsIssuable( req, reason );
            if( rv == false )
                return rv;
        }
    }
    /*  Can't issue unknown operations. */
    else
    {
        rv = false;

        if( reason ) 
            reason->reason = UNKNOWN_FAILURE;
    }

    return rv;
}

bool Rank::IssueCommand( NVMainRequest *req )
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
                rv = this->PowerDown( req->type );
                break;

            case POWERUP:
                rv = this->PowerUp( );
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
void Rank::Notify( OpType op )
{
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

bool Rank::RequestComplete( NVMainRequest* req )
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
                        state = RANK_CLOSED;

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

void Rank::Cycle( ncycle_t steps )
{
    for( unsigned bankIdx = 0; bankIdx < bankCount; bankIdx++ )
        banks[bankIdx]->Cycle( steps );

    /* Count cycle numbers and calculate background energy for each state */
    switch( state )
    {
        /* active powerdown */
        case RANK_PDA:
            fastExitCycles += steps;
            if( p->EnergyModel_set && p->EnergyModel == "current" )
                backgroundEnergy += ( p->EIDD3P * (double)steps );  
            else
                backgroundEnergy += ( p->Epda * (double)steps );  
            break;

        /* precharge powerdown fast exit */
        case RANK_PDPF:
            fastExitCycles += steps;
            if( p->EnergyModel_set && p->EnergyModel == "current" )
                backgroundEnergy += ( p->EIDD2P1 * (double)steps );  
            else 
                backgroundEnergy += ( p->Epdpf * (double)steps );  
            break;

        /* precharge powerdown slow exit */
        case RANK_PDPS:
            slowExitCycles += steps;
            if( p->EnergyModel_set && p->EnergyModel == "current" )
                backgroundEnergy += ( p->EIDD2P0 * (double)steps );  
            else 
                backgroundEnergy += ( p->Epdps * (double)steps );  
            break;

        /* active standby */
        case RANK_REFRESHING:
        case RANK_OPEN:
            activeCycles += steps;
            if( p->EnergyModel_set && p->EnergyModel == "current" )
                backgroundEnergy += ( p->EIDD3N * (double)steps );  
            else
                backgroundEnergy += ( p->Eleak * (double)steps );  
            break;

        /* precharge standby */
        case RANK_CLOSED:
            standbyCycles += steps;
            if( p->EnergyModel_set && p->EnergyModel == "current" )
                backgroundEnergy += ( p->EIDD2N * (double)steps );  
            else
                backgroundEnergy += ( p->Eleak * (double)steps );  
            break;

        default:
            if( p->EnergyModel_set && p->EnergyModel == "current" )
                backgroundEnergy += ( p->EIDD2N * (double)steps );  
            else
                backgroundEnergy += ( p->Eleak * (double)steps );  
            break;
    }
}

/*
 *  Assign a name to this rank (used in graph outputs)
 */
void Rank::SetName( std::string )
{
}

void Rank::CalculateStats( )
{
    double bankE, actE, bstE, refE;

    totalEnergy = activateEnergy = burstEnergy = refreshEnergy = 0.0;
    totalPower = backgroundPower = activatePower = burstPower = refreshPower = 0.0;
    reads = writes = 0;

    for( ncounter_t i = 0; i < bankCount; i++ )
    {
        banks[i]->GetEnergy( bankE, actE, bstE, refE );

        totalEnergy += bankE;
        activateEnergy += actE;
        burstEnergy += bstE;
        refreshEnergy += refE;

        reads += banks[i]->GetReads( );
        writes += banks[i]->GetWrites( );
    }

    /* add up the background energy */
    totalEnergy += backgroundEnergy;
    backgroundEnergy = backgroundEnergy;

    ncycle_t simulationTime = GetEventQueue()->GetCurrentCycle();

    if( simulationTime != 0 )
    {
        /* power in W */
        totalPower = ( totalEnergy * p->Voltage ) / (double)simulationTime / 1000.0;
        backgroundPower = ( backgroundEnergy * p->Voltage ) / (double)simulationTime / 1000.0; 
        activatePower = ( activateEnergy * p->Voltage ) / (double)simulationTime / 1000.0; 
        burstPower = ( burstEnergy * p->Voltage ) / (double)simulationTime / 1000.0; 
        refreshPower = ( refreshEnergy * p->Voltage ) / (double)simulationTime / 1000.0; 
    }

    /* energy breakdown. device is in lockstep within a rank */
    totalEnergy *= (double)deviceCount;
    backgroundEnergy *= (double)deviceCount;
    activateEnergy *= (double)deviceCount;
    burstEnergy *= (double)deviceCount;
    refreshEnergy *= (double)deviceCount;

    /* power breakdown. device is in lockstep within a rank */
    totalPower *= (double)deviceCount;
    backgroundPower *= (double)deviceCount;
    activatePower *= (double)deviceCount;
    burstPower *= (double)deviceCount;
    refreshPower *= (double)deviceCount;

    actWaitAverage = static_cast<double>(actWaitTotal) / static_cast<double>(actWaits);
    rrdWaitAverage = static_cast<double>(rrdWaitTotal) / static_cast<double>(rrdWaits);
    fawWaitAverage = static_cast<double>(fawWaitTotal) / static_cast<double>(fawWaits);

    NVMObject::CalculateStats( );
}
