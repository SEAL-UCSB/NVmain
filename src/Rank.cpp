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
    /* 
     * Make sure this doesn't cause unnecessary tRRD delays at start 
     * TODO: have Activate check currentCycle < tRRD/tFAW maybe?
     */
    lastActivate[0] = -10000;
    lastActivate[1] = -10000;
    lastActivate[2] = -10000;
    lastActivate[3] = -10000;

    activeCycles = 0;
    standbyCycles = 0;
    feCycles = 0;
    seCycles = 0;

    FAWindex = 0;

    conf = NULL;

    backgroundEnergy = 0.0f;

    psInterval = 0;
}

Rank::~Rank( )
{
    for( ncounter_t i = 0; i < bankCount; i++ )
        delete banks[i];

    delete [] banks;
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

    /* Calculate the number of devices needed. */
    deviceCount = busWidth / deviceWidth;
    if( busWidth % deviceWidth != 0 )
    {
        std::cout << "NVMain: device width is not a multiple of the bus width!\n";
        deviceCount++;
    }

    std::cout << "Creating " << bankCount << " banks in all " << deviceCount << " devices.\n";

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
    }

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

    fawWaitTime = 0;
    rrdWaitTime = 0;
    actWaitTime = 0;
}

bool Rank::Activate( NVMainRequest *request )
{
    uint64_t activateBank;

    request->address.GetTranslatedAddress( NULL, NULL, &activateBank, NULL, NULL );

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
        && (ncycles_t)lastActivate[FAWindex] + (ncycles_t)p->tRRDR 
            <= (ncycles_t)GetEventQueue()->GetCurrentCycle()
        && (ncycles_t)lastActivate[(FAWindex + 1)%4] + (ncycles_t)p->tFAW 
            <= (ncycles_t)GetEventQueue()->GetCurrentCycle() )
    {
        /* issue ACTIVATE to target bank */
        GetChild( request )->IssueCommand( request );

        state = RANK_OPEN;

        FAWindex = (FAWindex + 1) % 4;
        lastActivate[FAWindex] = (ncycles_t)GetEventQueue()->GetCurrentCycle();
        nextActivate = MAX( nextActivate, GetEventQueue()->GetCurrentCycle() 
                                            + p->tRRDR );
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
    uint64_t readRow, readBank;

    request->address.GetTranslatedAddress( &readRow, NULL, &readBank, NULL, NULL );

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

    /*
     *  We need to check for bank conflicts. If there are no conflicts, we can issue the 
     *  command as long as the bus will be ready and the column can decode that fast. The
     *  bank code ensures the second criteria.
     */
    if( !banks[readBank]->WouldConflict( readRow ) )
    {
        /* issue READ or READ_PRECHARGE to target bank */
        GetChild( request )->IssueCommand( request );

        /* Even though the command may be READ_PRECHARGE, it still works */
        nextRead = MAX( nextRead, GetEventQueue()->GetCurrentCycle() 
                                    + MAX( p->tBURST, p->tCCD ) );

        nextWrite = MAX( nextWrite, GetEventQueue()->GetCurrentCycle() 
                                    + p->tCAS + p->tBURST + p->tRTRS - p->tCWD ); 
    }
    else
    {
        std::cerr << "NVMain Error: Rank Read FAILED! Did you check IsIssuable?" << std::endl;
    }

    return true;
}


bool Rank::Write( NVMainRequest *request )
{
    uint64_t writeRow, writeBank;

    request->address.GetTranslatedAddress( &writeRow, NULL, &writeBank, NULL, NULL );

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

    if( !banks[writeBank]->WouldConflict( writeRow ) )
    {
        /* issue WRITE or WRITE_PRECHARGE to the target bank */
        GetChild( request )->IssueCommand( request );

        /* Even though the command may be WRITE_PRECHARGE, it still works */
        nextRead = MAX( nextRead, GetEventQueue()->GetCurrentCycle() 
                                    + p->tCWD + p->tBURST + p->tWTR );

        nextWrite = MAX( nextWrite, GetEventQueue()->GetCurrentCycle() 
                                    + MAX( p->tBURST, p->tCCD ) );
    }
    else
    {
        std::cerr << "NVMain Error: Rank Write FAILED! Did you check IsIssuable?" 
            << std::endl;
    }

    return true;
}

bool Rank::Precharge( NVMainRequest *request )
{
    uint64_t preBank;

    request->address.GetTranslatedAddress( NULL, NULL, &preBank, NULL, NULL );

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
    GetChild( request )->IssueCommand( request );

    bool rankIdle = true;
    for( ncounter_t i = 0; i < bankCount; i++ )
    {
        if( banks[i]->Idle( ) == false )
        {
            rankIdle = false;
            break;
        }
    }

    if( rankIdle )
        state = RANK_CLOSED;

    return true;
}

bool Rank::CanPowerDown( OpType pdOp )
{
    bool issuable = true;
    NVMainRequest req;
    NVMAddress address;

    /* Create a dummy operation to determine if we can issue */
    req.type = pdOp;
    req.address.SetTranslatedAddress( 0, 0, 0, 0, 0 );
    req.address.SetPhysicalAddress( 0 );

    for( ncounter_t i = 0; i < bankCount; i++ )
        if( !banks[i]->IsIssuable( &req ) )
        {
            issuable = false;
            break;
        }

    return issuable;
}

bool Rank::PowerDown( NVMainRequest *request )
{
    /* TODO: use hooker to issue POWERDOWN?? */
    /* 
     * PowerDown() should be completed to all banks, partial PowerDown is
     * incorrect. Therefore, call CanPowerDown() first before every PowerDown
     */
    for( ncounter_t i = 0; i < bankCount; i++ )
       banks[i]->PowerDown( request->type );

    switch( request->type )
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
                << request->type << " is detected in Rank " << std::endl; 
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
    req.address.SetTranslatedAddress( 0, 0, 0, 0, 0 );
    req.address.SetPhysicalAddress( 0 );

    /* since all banks are powered down, check the first bank is enough */
    if( !banks[0]->IsIssuable( &req ) )
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
                << "PowerDown before in Rank " << std::endl; 
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
            NULL, NULL, &refreshBankGroupHead, NULL, NULL );

    assert( (refreshBankGroupHead + banksPerRefresh) <= bankCount );

    /* TODO: use hooker to issue REFRESH?? */
    for( ncounter_t i = 0; i < banksPerRefresh; i++ )
    {
        NVMainRequest* refReq = new NVMainRequest;
        *refReq = *request;
        banks[refreshBankGroupHead + i]->IssueCommand( refReq );
    }

    /* rank is tagged open since IDD3N should be used for background energy */
    state = RANK_OPEN;

    request->owner = this;
    GetEventQueue( )->InsertEvent( EventResponse, this, request, 
        GetEventQueue()->GetCurrentCycle() + p->tRFC );

    /*
     * simply treat the REFRESH as an ACTIVATE. For a finer refresh
     * granularity, the nextActivate does not block the other bank groups
     */
    nextActivate = MAX( nextActivate, GetEventQueue( )->GetCurrentCycle( ) 
                                        + p->tRRDR );
    FAWindex = (FAWindex + 1) % 4;
    lastActivate[FAWindex] = (ncycles_t)GetEventQueue( )->GetCurrentCycle( );

    return true;
}

ncycle_t Rank::GetNextActivate( uint64_t bank )
{
    return MAX( 
            MAX( nextActivate, banks[bank]->GetNextActivate( ) ),
            MAX( lastActivate[FAWindex] + p->tRRDR, 
                lastActivate[(FAWindex + 1)%4] + p->tFAW ) 
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
    uint64_t opRow;
    uint64_t opCol;
    bool rv;
    
    req->address.GetTranslatedAddress( &opRow, &opCol, &opBank, NULL, NULL );

    rv = true;

    if( req->type == ACTIVATE )
    {
        if( nextActivate > (GetEventQueue()->GetCurrentCycle()) 
            || ( lastActivate[FAWindex] + static_cast<ncycles_t>(p->tRRDR) 
                > static_cast<ncycles_t>(GetEventQueue()->GetCurrentCycle()) )
            || ( lastActivate[(FAWindex + 1)%4] + static_cast<ncycles_t>(p->tFAW) 
                > static_cast<ncycles_t>(GetEventQueue()->GetCurrentCycle()) ) )
        {
            rv = false;

            if( reason ) 
                reason->reason = RANK_TIMING;
        }
        else
            rv = banks[opBank]->IsIssuable( req, reason );

        if( rv == false )
        {
            if( nextActivate > (GetEventQueue()->GetCurrentCycle()) )
            {
                actWaits++;
                actWaitTime += nextActivate - (GetEventQueue()->GetCurrentCycle());
            }

            if( lastActivate[FAWindex] + static_cast<ncycles_t>(p->tRRDR) 
                    > static_cast<ncycles_t>(GetEventQueue()->GetCurrentCycle()) )
            {
                rrdWaits++;
                rrdWaitTime += ( lastActivate[FAWindex] + 
                        p->tRRDR - (GetEventQueue()->GetCurrentCycle()) );
            }
            if( lastActivate[(FAWindex + 1)%4] + static_cast<ncycles_t>(p->tFAW) 
                    > static_cast<ncycles_t>(GetEventQueue()->GetCurrentCycle()) )
            {
                fawWaits++;
                fawWaitTime += lastActivate[(FAWindex +1)%4] + 
                    p->tFAW - (GetEventQueue()->GetCurrentCycle());
            }
        }
    }
    else if( req->type == READ || req->type == READ_PRECHARGE )
    {
        if( nextRead > (GetEventQueue()->GetCurrentCycle()) 
                || banks[opBank]->WouldConflict( opRow ) )
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
        if( nextWrite > (GetEventQueue()->GetCurrentCycle()) 
                || banks[opBank]->WouldConflict( opRow ) )
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
        if( nextPrecharge > (GetEventQueue()->GetCurrentCycle()) )
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
        rv = CanPowerUp();

        if( reason ) 
            reason->reason = RANK_TIMING;
    }
    else if( req->type == REFRESH )
    {
        /* firstly, check whether REFRESH can be issued to a rank */
        if( nextActivate > GetEventQueue()->GetCurrentCycle() 
            || ( lastActivate[FAWindex] + static_cast<ncycles_t>(p->tRRDR) 
                > static_cast<ncycles_t>(GetEventQueue()->GetCurrentCycle()) )
            || ( lastActivate[(FAWindex + 1)%4] + static_cast<ncycles_t>(p->tFAW) 
                > static_cast<ncycles_t>(GetEventQueue()->GetCurrentCycle()) ) )
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
        req->address.GetTranslatedAddress( NULL, NULL, &bank, &rank, &channel );
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
        if( req->type == REFRESH )
        {
            /* 
             * check whether all banks are idle. some banks may be active
             * due to the possible fine-grained refresh 
             */
            bool rankIdle = true;
            for( ncounter_t i = 0; i < bankCount; i++ )
            {
                if( banks[i]->Idle( ) == false )
                {
                    rankIdle = false;
                    break;
                }
            }

            if( rankIdle )
                state = RANK_CLOSED;
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
            feCycles += steps;
            if( p->EnergyModel_set && p->EnergyModel == "current" )
                backgroundEnergy += ( p->EIDD3P * (float)steps );  
            else
                backgroundEnergy += ( p->Epda * (float)steps );  
            break;

        /* precharge powerdown fast exit */
        case RANK_PDPF:
            feCycles += steps;
            if( p->EnergyModel_set && p->EnergyModel == "current" )
                backgroundEnergy += ( p->EIDD2P1 * (float)steps );  
            else 
                backgroundEnergy += ( p->Epdpf * (float)steps );  
            break;

        /* precharge powerdown slow exit */
        case RANK_PDPS:
            seCycles += steps;
            if( p->EnergyModel_set && p->EnergyModel == "current" )
                backgroundEnergy += ( p->EIDD2P0 * (float)steps );  
            else 
                backgroundEnergy += ( p->Epdps * (float)steps );  
            break;

        /* active standby */
        case RANK_OPEN:
            activeCycles += steps;
            if( p->EnergyModel_set && p->EnergyModel == "current" )
                backgroundEnergy += ( p->EIDD3N * (float)steps );  
            else
                backgroundEnergy += ( p->Eleak * (float)steps );  
            break;

        /* precharge standby */
        case RANK_CLOSED:
            standbyCycles += steps;
            if( p->EnergyModel_set && p->EnergyModel == "current" )
                backgroundEnergy += ( p->EIDD2N * (float)steps );  
            else
                backgroundEnergy += ( p->Eleak * (float)steps );  
            break;

        default:
            if( p->EnergyModel_set && p->EnergyModel == "current" )
                backgroundEnergy += ( p->EIDD2N * (float)steps );  
            else
                backgroundEnergy += ( p->Eleak * (float)steps );  
            break;
    }
}

/*
 *  Assign a name to this rank (used in graph outputs)
 */
void Rank::SetName( std::string )
{
}

void Rank::PrintStats( )
{
    float totalPower = 0.0f;
    float totalEnergy = 0.0f;
    float bankE, actE, bstE, refE;
    float bkgEnergy, actEnergy, bstEnergy, refEnergy;
    float bkgPower, actPower, bstPower, refPower;
    ncounter_t reads, writes;

    bkgEnergy = actEnergy = bstEnergy = refEnergy = 0.0f;
    bkgPower = actPower = bstPower = refPower = 0.0f;
    reads = writes = 0;

    for( ncounter_t i = 0; i < bankCount; i++ )
    {
        banks[i]->GetEnergy( bankE, actE, bstE, refE );

        totalEnergy += bankE;
        actEnergy += actE;
        bstEnergy += bstE;
        refEnergy += refE;

        reads += banks[i]->GetReads( );
        writes += banks[i]->GetWrites( );
    }

    /* add up the background energy */
    totalEnergy += backgroundEnergy;
    bkgEnergy = backgroundEnergy;

    float simulationTime = (float)GetEventQueue()->GetCurrentCycle();

    if( simulationTime != 0.0f )
    {
        /* power in mW */
        totalPower = ( totalEnergy * p->Voltage ) / simulationTime / 1000.0f;
        bkgPower = ( bkgEnergy * p->Voltage ) / simulationTime / 1000.0f; 
        actPower = ( actEnergy * p->Voltage ) / simulationTime / 1000.0f; 
        bstPower = ( bstEnergy * p->Voltage ) / simulationTime / 1000.0f; 
        refPower = ( refEnergy * p->Voltage ) / simulationTime / 1000.0f; 
    }

    /* energy breakdown. device is in lockstep within a rank */
    totalEnergy *= (float)deviceCount;
    bkgEnergy *= (float)deviceCount;
    actEnergy *= (float)deviceCount;
    bstEnergy *= (float)deviceCount;
    refEnergy *= (float)deviceCount;

    /* power breakdown. device is in lockstep within a rank */
    totalPower *= (float)deviceCount;
    bkgPower *= (float)deviceCount;
    actPower *= (float)deviceCount;
    bstPower *= (float)deviceCount;
    refPower *= (float)deviceCount;

    if( p->EnergyModel_set && p->EnergyModel == "current" )
    {
        std::cout << "i" << psInterval << "." << statName 
            << ".current " << totalEnergy << "\t; mA" << std::endl;
        std::cout << "i" << psInterval << "." << statName 
            << ".current.background " << bkgEnergy << "\t; mA" << std::endl;
        std::cout << "i" << psInterval << "." << statName 
            << ".current.active " << actEnergy << "\t; mA" << std::endl;
        std::cout << "i" << psInterval << "." << statName 
            << ".current.burst " << bstEnergy << "\t; mA" << std::endl;
        std::cout << "i" << psInterval << "." << statName 
            << ".current.refresh " << refEnergy << "\t; mA" << std::endl;
    }
    else
    {
        std::cout << "i" << psInterval << "." << statName 
            << ".energy " << totalEnergy << "\t; nJ" << std::endl; 
        std::cout << "i" << psInterval << "." << statName 
            << ".energy.background " << bkgEnergy << "\t; nJ" << std::endl;
        std::cout << "i" << psInterval << "." << statName 
            << ".energy.active " << actEnergy << "\t; nJ" << std::endl;
        std::cout << "i" << psInterval << "." << statName 
            << ".energy.burst " << bstEnergy << "\t; nJ" << std::endl;
        std::cout << "i" << psInterval << "." << statName 
            << ".energy.refresh " << refEnergy << "\t; nJ" << std::endl;
    }
    
    std::cout << "i" << psInterval << "." << statName << ".power " 
              << totalPower << "\t; W " << std::endl
              << "i" << psInterval << "." << statName << ".power.background " 
              << bkgPower << "\t; W " << std::endl
              << "i" << psInterval << "." << statName << ".power.active " 
              << actPower << "\t; W " << std::endl
              << "i" << psInterval << "." << statName << ".power.burst " 
              << bstPower << "\t; W " << std::endl
              << "i" << psInterval << "." << statName << ".power.refresh " 
              << refPower << "\t; W " << std::endl;

    std::cout << "i" << psInterval << "." << statName << ".reads " << reads << std::endl;
    std::cout << "i" << psInterval << "." << statName << ".writes " << writes << std::endl;

    std::cout << "i" << psInterval << "." << statName 
              << ".activeCycles " << activeCycles << std::endl
              << "i" << psInterval << "." << statName 
              << ".standbyCycles " << standbyCycles << std::endl
              << "i" << psInterval << "." << statName 
              << ".fastExitCycles " << feCycles << std::endl
              << "i" << psInterval << "." << statName 
              << ".slowExitCycles " << seCycles << std::endl;

    std::cout << "i" << psInterval << "." << statName << ".actWaits " << actWaits << std::endl;
    std::cout << "i" << psInterval << "." << statName << ".actWaits.totalTime " << actWaitTime << std::endl;
    std::cout << "i" << psInterval << "." << statName << ".actWaits.averageTime " << (double)((double)actWaitTime / (double)actWaits) << std::endl;

    std::cout << "i" << psInterval << "." << statName << ".rrdWaits " << rrdWaits << std::endl;
    std::cout << "i" << psInterval << "." << statName << ".rrdWaits.totalTime " << rrdWaitTime << std::endl;
    std::cout << "i" << psInterval << "." << statName << ".rrdWaits.averageTime " << (double)((double)rrdWaitTime / (double)rrdWaits) << std::endl;

    std::cout << "i" << psInterval << "." << statName << ".fawWaits " << fawWaits << std::endl;
    std::cout << "i" << psInterval << "." << statName << ".fawWaits.totalTime " << fawWaitTime << std::endl;
    std::cout << "i" << psInterval << "." << statName << ".fawWaits.averageTime " << (double)((double)fawWaitTime / (double)fawWaits) << std::endl;


    for( ncounter_t i = 0; i < bankCount; i++ )
    {
        banks[i]->PrintStats( );
    }

    psInterval++;
}
