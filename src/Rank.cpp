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

    FAWindex = 0;

    conf = NULL;

    backgroundEnergy = 0.0f;

    psInterval = 0;
}

Rank::~Rank( )
{
    if( devices != NULL )
    {
        for( ncounter_t i = 0; i < deviceCount; i++ )
            for( ncounter_t j = 0; j < bankCount; j++ )
            {
                Bank *bank = devices[i].GetBank( j );

                delete bank;
            }

        delete [] devices;
    }
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

    devices = new Device[deviceCount];
    
    for( ncounter_t i = 0; i < deviceCount; i++ )
    {
        for( ncounter_t j = 0; j < bankCount; j++ )
        {
            Bank *newBank = new Bank( );
            std::stringstream formatter;

            formatter << j;
            newBank->SetName( formatter.str( ) );
            newBank->SetId( (int)i );
            formatter.str( "" );

            devices[i].AddBank( newBank );

            formatter << statName << ".bank" << j;
            newBank->StatName( formatter.str( ) );

            newBank->SetParent( this );
            AddChild( newBank );

            /* SetConfig recursively. */
            newBank->SetConfig( c );

        }
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
        && (ncycles_t)lastActivate[FAWindex] + (ncycles_t)p->tRRDR <= (ncycles_t)GetEventQueue()->GetCurrentCycle()
        && (ncycles_t)lastActivate[(FAWindex + 1)%4] + (ncycles_t)p->tFAW <= (ncycles_t)GetEventQueue()->GetCurrentCycle() )
    {
        assert( GetChild(request)->GetTrampoline() == devices[0].GetBank(activateBank) );
        GetChild( request )->IssueCommand( request );

        /* Broadcast request to remaining banks... this won't call hooks. */
        for( ncounter_t i = 1; i < deviceCount; i++ )
          devices[i].GetBank( activateBank )->Activate( request );

        FAWindex = (FAWindex + 1) % 4;
        lastActivate[FAWindex] = (ncycles_t)GetEventQueue()->GetCurrentCycle();
        nextActivate = MAX( nextActivate, GetEventQueue()->GetCurrentCycle() + p->tRRDR );
    }
    else
    {
        std::cerr << "Rank: Activation FAILED! Did you check IsIssuable?" << std::endl;
    }

    return true;
}

bool Rank::Read( NVMainRequest *request )
{
    uint64_t readRow, readBank;

    request->address.GetTranslatedAddress( &readRow, NULL, &readBank, NULL, NULL );

    if( readBank >= bankCount )
    {
        std::cerr << "Attempted to read non-existant bank: " << readBank << "!" << std::endl;
        return false;
    }

    if( nextRead > GetEventQueue()->GetCurrentCycle() )
    {
        return false;
    }

    /*
     *  We need to check for bank conflicts. If there are no conflicts, we can issue the 
     *  command as long as the bus will be ready and the column can decode that fast. The
     *  bank code ensures the second criteria.
     */
    if( !devices[0].GetBank( readBank )->WouldConflict( readRow ) )
    {
        GetChild( request )->IssueCommand( request );

        /* Broadcast request to remaining banks... this won't call hooks. */
        for( ncounter_t i = 1; i < deviceCount; i++ )
          devices[i].GetBank( readBank )->Read( request );

        nextRead = MAX( nextRead, GetEventQueue()->GetCurrentCycle() + MAX( p->tBURST, p->tCCD ) );
        nextWrite = MAX( nextWrite, GetEventQueue()->GetCurrentCycle() + p->tCAS + p->tBURST +
             p->tRTRS - p->tCWD ); 
    }
    else
    {
        std::cerr << "Rank: Read FAILED! Did you check IsIssuable?" << std::endl;
    }

    return true;
}


bool Rank::Write( NVMainRequest *request )
{
    uint64_t writeRow, writeBank;

    request->address.GetTranslatedAddress( &writeRow, NULL, &writeBank, NULL, NULL );

    if( writeBank >= bankCount )
    {
        std::cerr << "Attempted to write non-existant bank: " << writeBank << "!" << std::endl;
        return false;
    }

    if( nextWrite > GetEventQueue()->GetCurrentCycle() )
    {
        return false;
    }

    if( !devices[0].GetBank( writeBank )->WouldConflict( writeRow ) )
    {
        GetChild( request )->IssueCommand( request );

        /* Broadcast request to remaining banks... this won't call hooks. */
        for( ncounter_t i = 1; i < deviceCount; i++ )
          devices[i].GetBank( writeBank )->Write( request );

        nextRead = MAX( nextRead, GetEventQueue()->GetCurrentCycle() + p->tCWD + p->tBURST
                + p->tWTR );
        nextWrite = MAX( nextWrite, GetEventQueue()->GetCurrentCycle() + MAX( p->tBURST, p->tCCD ) );
    }
    else
    {
        std::cerr << "Rank: Write FAILED! Did you check IsIssuable?" << std::endl;
    }

    return true;
}

bool Rank::Precharge( NVMainRequest *request )
{
    uint64_t prechargeBank;

    request->address.GetTranslatedAddress( NULL, NULL, &prechargeBank, NULL, NULL );

    if( prechargeBank >= bankCount )
    {
        std::cerr << "Rank: Attempted to precharge non-existant bank: " << prechargeBank << std::endl;
        return false;
    }

    /*
     *  There are no rank-level constraints on precharges. If the bank says timing
     *  was met we can send the command to the bank.
     */
    //devices[0].GetBank( prechargeBank )->IssueCommand( request );
    assert( devices[0].GetBank( prechargeBank ) == GetChild( request )->GetTrampoline() );
    GetChild( request )->IssueCommand( request );

    /* Broadcast request to remaining banks... this won't call hooks. */
    for( ncounter_t i = 1; i < deviceCount; i++ )
      devices[i].GetBank( prechargeBank )->Precharge( request );

    return true;
}

bool Rank::PowerUp( NVMainRequest *request )
{
    bool returnValue = false;
    uint64_t puBank;

    request->address.GetTranslatedAddress( NULL, NULL, &puBank, NULL, NULL );

    if( puBank >= bankCount )
    {
        std::cerr << "Rank: Attempted to powerup non-existant bank: " << puBank << std::endl;
        return false;
    }

    returnValue = GetChild( request )->IssueCommand( request );

    if( returnValue )
    {
        /* Broadcast request to remaining banks... this won't call hooks. */
        for( ncounter_t i = 1; i < deviceCount; i++ )
            if( !devices[i].GetBank( puBank )->PowerUp( request ) )
            {
                if( i != 0 )
                    std::cerr << "Rank: Error partial power up failure!" << std::endl;

                returnValue = false;
            }
    }

    return returnValue;
}

/*
 * refresh is issued to those banks that start from the bank specified by the
 * request.  
 */
bool Rank::Refresh( NVMainRequest *request )
{
    assert( nextActivate <= ( GetEventQueue()->GetCurrentCycle() ) );
    uint64_t refreshBankHead, refreshRank;
    request->address.GetTranslatedAddress( NULL, NULL, &refreshBankHead, &refreshRank, NULL );
    assert( (refreshBankHead + banksPerRefresh) <= bankCount );

    for( ncounter_t i = 0; i < deviceCount; i++ )
        for( ncounter_t j = 0; j < banksPerRefresh; j++ )
            devices[i].GetBank( (refreshBankHead + j) )->Refresh( );

    /*
     * simply treat the REFRESH as an ACTIVATE. For a finer refresh
     * granularity, the nextActivate does not block the other bank groups
     */
    nextActivate = MAX( nextActivate, GetEventQueue()->GetCurrentCycle() + p->tRRDR );
    /*
     * since refresh must NOT be returned to memory controller, we can delete
     * it here
     */
    delete request;

    return true;
}

ncycle_t Rank::GetNextActivate( uint64_t bank )
{
    return MAX( 
            MAX( nextActivate, devices[0].GetBank( bank )->GetNextActivate( ) ),
            MAX( lastActivate[FAWindex] + p->tRRDR, 
                lastActivate[(FAWindex + 1)%4] + p->tFAW ) 
           );
}

ncycle_t Rank::GetNextRead( uint64_t bank )
{
    return MAX( nextRead, devices[0].GetBank( bank )->GetNextRead( ) );
}

ncycle_t Rank::GetNextWrite( uint64_t bank )
{
    return MAX( nextWrite, devices[0].GetBank( bank )->GetNextWrite( ) );
}

ncycle_t Rank::GetNextPrecharge( uint64_t bank )
{
    return MAX( nextPrecharge, devices[0].GetBank( bank )->GetNextPrecharge( ) );
}

ncycle_t Rank::GetNextRefresh( uint64_t bank )
{
    return devices[0].GetBank( bank )->GetNextRefresh( );
}

bool Rank::IsIssuable( NVMainRequest *req, FailReason *reason )
{
    uint64_t opRank;
    uint64_t opBank;
    uint64_t opRow;
    uint64_t opCol;
    bool rv;
    
    req->address.GetTranslatedAddress( &opRow, &opCol, &opBank, &opRank, NULL );

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
            rv = devices[0].GetBank( opBank )->IsIssuable( req, reason );

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
    else if( req->type == READ )
    {
        if( nextRead > (GetEventQueue()->GetCurrentCycle()) 
                || devices[0].GetBank( opBank )->WouldConflict( opRow ) )
        {
            rv = false;

            if( reason ) 
                reason->reason = RANK_TIMING;
        }
        else
            rv = devices[0].GetBank( opBank )->IsIssuable( req, reason );
    }
    else if( req->type == WRITE )
    {
        if( nextWrite > (GetEventQueue()->GetCurrentCycle()) 
                || devices[0].GetBank( opBank )->WouldConflict( opRow ) )
        {
            rv = false;

            if( reason ) 
                reason->reason = RANK_TIMING;
        }
        else
            rv = devices[0].GetBank( opBank )->IsIssuable( req, reason );
    }
    else if( req->type == PRECHARGE )
    {
        if( nextPrecharge > (GetEventQueue()->GetCurrentCycle()) )
        {
            rv = false;

            if( reason ) 
                reason->reason = RANK_TIMING;
        }
        else
            rv = devices[0].GetBank( opBank )->IsIssuable( req, reason );
    }
    else if( req->type == POWERDOWN_PDA 
            || req->type == POWERDOWN_PDPF 
            || req->type == POWERDOWN_PDPS )
    {
        rv = devices[0].CanPowerDown( req->type );
    }
    else if( req->type == POWERUP )
    {
        rv = devices[0].CanPowerUp( opBank );
    }
    else if( req->type == REFRESH )
    {
        /* firstly, check whether REFRESH can be issued to a rank */
        if( nextActivate > GetEventQueue()->GetCurrentCycle() )
        {
            rv = false;
            if( reason ) reason->reason = RANK_TIMING;

            return rv;
        }

        /* REFRESH can only be issued when all banks in the group are issuable */ 
        uint64_t refreshBankHead, refreshRank;
        req->address.GetTranslatedAddress( NULL, NULL, &refreshBankHead, &refreshRank, NULL );
        assert( (refreshBankHead + banksPerRefresh) <= bankCount );

        for( ncounter_t i = 0; i < banksPerRefresh; i++ )
        {
            rv = devices[0].GetBank( (refreshBankHead + i) )->IsIssuable( req, reason );
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
                rv = this->Read( req );
                break;
            
            case WRITE:
                rv = this->Write( req );
                break;
            
            case PRECHARGE:
                rv = this->Precharge( req );
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
void Rank::Notify( OpType op )
{
    /* We only care if other ranks are reading/writing (to avoid bus contention) */
    if( op == READ )
    {
        nextRead = MAX( nextRead, 
                GetEventQueue()->GetCurrentCycle() + p->tBURST + p->tRTRS );

        nextWrite = MAX( nextWrite, 
                GetEventQueue()->GetCurrentCycle() + p->tCAS + p->tBURST + p->tRTRS - p->tCWD);
    }
    else if( op == WRITE )
    {
        nextWrite = MAX( nextWrite, 
                GetEventQueue()->GetCurrentCycle() + p->tBURST + p->tOST );

        nextRead = MAX( nextRead, 
                GetEventQueue()->GetCurrentCycle() + p->tBURST + p->tCWD + p->tRTRS - p->tCAS );
    }
}

void Rank::Cycle( ncycle_t )
{
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
    ncounter_t reads, writes;

    reads = 0;
    writes = 0;
    for( ncounter_t i = 0; i < bankCount; i++ )
    {
        totalEnergy += devices[0].GetBank( i )->GetEnergy( );
        totalPower += devices[0].GetBank( i )->GetPower( );

        reads += devices[0].GetBank( i )->GetReads( );
        writes += devices[0].GetBank( i )->GetWrites( );
    }

    totalEnergy *= (float)deviceCount;
    totalPower *= (float)deviceCount;

    totalEnergy += backgroundEnergy;
    totalPower += (((backgroundEnergy / (float)GetEventQueue()->GetCurrentCycle()) * p->Voltage) / 1000.0f) * (float)deviceCount;

    std::cout << "i" << psInterval << "." << statName << ".power " << totalPower << std::endl;
    std::cout << "i" << psInterval << "." << statName << ".energy " << totalEnergy << std::endl;
    std::cout << "i" << psInterval << "." << statName << ".backgroundEnergy " << backgroundEnergy << std::endl;
    std::cout << "i" << psInterval << "." << statName << ".reads " << reads << std::endl;
    std::cout << "i" << psInterval << "." << statName << ".writes " << writes << std::endl;

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
        devices[0].GetBank( i )->PrintStats( );
    }

    psInterval++;
}
