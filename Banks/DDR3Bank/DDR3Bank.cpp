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

#include "Banks/DDR3Bank/DDR3Bank.h"
#include "src/MemoryController.h"
#include "src/EventQueue.h"

#include <signal.h>
#include <cassert>
#include <iostream>
#include <sstream>
#include <limits>

using namespace NVM;

DDR3Bank::DDR3Bank( )
{
    nextActivate = 0;
    nextPrecharge = 0;
    nextRead = 0;
    nextWrite = 0;
    nextRefresh = 0;
    nextRefreshDone = 0;
    nextPowerDown = 0;
    nextPowerDownDone = 0;
    nextPowerUp = 0;
    nextCommand = CMD_NOP;

    dummyStat = 0;

    subArrayNum = 0;
    activeSubArrayQueue.clear();

    /* a MAT is 512x512 by default */
    MATWidth = 512;
    MATHeight = 512;

    state = DDR3BANK_CLOSED;
    lastActivate = 0;
    openRow = 0;

    bankEnergy = 0.0f;
    activeEnergy = 0.0f;
    burstEnergy = 0.0f;
    refreshEnergy = 0.0f;

    bankPower = 0.0f;
    activePower = 0.0f;
    burstPower = 0.0f;
    refreshPower = 0.0f;

    bandwidth = 0.0;
    dataCycles = 0;
    powerCycles = 0;
    utilization = 0.0f;

    activeCycles = 0;
    standbyCycles = 0;
    fastExitActiveCycles = 0;
    fastExitPrechargeCycles = 0;
    slowExitPrechargeCycles = 0;
    writeCycle = false;
    writeMode = WRITE_THROUGH;
    idleTimer = 0;

    reads = 0;
    writes = 0;
    activates = 0;
    precharges = 0;
    refreshes = 0;

    actWaits = 0;
    actWaitTotal = 0;
    actWaitAverage = 0.0;

    averageEndurance = 0;
    worstCaseEndurance = 0;

    bankId = -1;
}

DDR3Bank::~DDR3Bank( )
{
}

void DDR3Bank::SetConfig( Config *config, bool createChildren )
{
    /* customize MAT size */
    if( config->KeyExists( "MATWidth" ) )
        MATWidth = static_cast<ncounter_t>( config->GetValue( "MATWidth" ) );

    Params *params = new Params( );
    params->SetParams( config );
    SetParams( params );

    MATHeight = p->MATHeight;
    subArrayNum = p->ROWS / MATHeight;

    if( createChildren )
    {
        /* When selecting a child, use the subarray field from the decoder. */
        AddressTranslator *bankAT = DecoderFactory::CreateDecoderNoWarn( config->GetString( "Decoder" ) );
        TranslationMethod *method = GetParent()->GetTrampoline()->GetDecoder()->GetTranslationMethod();
        bankAT->SetTranslationMethod( method );
        bankAT->SetDefaultField( SUBARRAY_FIELD );
        bankAT->SetConfig( config, createChildren );
        SetDecoder( bankAT );

        for( ncounter_t i = 0; i < subArrayNum; i++ )
        {
            SubArray *nextSubArray = new SubArray( );

            std::stringstream formatter;

            formatter << i;
            nextSubArray->SetName ( formatter.str() );
            nextSubArray->SetId( i );

            formatter.str( "" );
            formatter<< StatName( ) << ".subarray" << i;
            nextSubArray->StatName (formatter.str( ) );

            nextSubArray->SetParent( this );
            AddChild( nextSubArray );

            nextSubArray->SetConfig( config, createChildren );
            nextSubArray->RegisterStats( );
        }
    }

    if( p->InitPD )
        state = DDR3BANK_PDPF;
}

void DDR3Bank::RegisterStats( )
{
    if( p->EnergyModel == "current" )
    {
        AddUnitStat(bankEnergy, "mA*t");
        AddUnitStat(activeEnergy, "mA*t");
        AddUnitStat(burstEnergy, "mA*t");
        AddUnitStat(refreshEnergy, "mA*t");
    }
    else
    {
        AddUnitStat(bankEnergy, "nJ");
        AddUnitStat(activeEnergy, "nJ");
        AddUnitStat(burstEnergy, "nJ");
        AddUnitStat(refreshEnergy, "nJ");
    }

    AddUnitStat(bankPower, "W");
    AddUnitStat(activePower, "W");
    AddUnitStat(burstPower, "W");
    AddUnitStat(refreshPower, "W");

    AddUnitStat(bandwidth, "MB/s");
    AddStat(dataCycles); 
    AddStat(powerCycles);
    AddStat(utilization);

    AddStat(reads);
    AddStat(writes);
    AddStat(activates);
    AddStat(precharges);
    AddStat(refreshes);

    AddStat(activeCycles);
    AddStat(standbyCycles);
    AddStat(fastExitActiveCycles);
    AddStat(fastExitPrechargeCycles);
    AddStat(slowExitPrechargeCycles);

    AddStat(actWaits);
    AddStat(actWaitTotal); 
    AddStat(actWaitAverage);

    AddStat(averageEndurance);
    AddStat(worstCaseEndurance);
}

/*
 * PowerDown() power the bank down along with different modes
 */
bool DDR3Bank::PowerDown( NVMainRequest *request )
{
    bool returnValue = false;

    if( nextPowerDown <= GetEventQueue()->GetCurrentCycle() 
        && ( state == DDR3BANK_OPEN || state == DDR3BANK_CLOSED ) )
    {
        /* Update timing constraints */
        /*
         *  The power down state (pdState) will be determined by the device 
         *  class, which will be checked to see if all banks are idle or not, 
         *  and if fast exit is used.
         */
        nextPowerUp = MAX( nextPowerUp, GetEventQueue()->GetCurrentCycle() 
                                        + p->tPD );

        if( state == DDR3BANK_OPEN )
        {
            assert( request->type == POWERDOWN_PDA );
            state = DDR3BANK_PDA;
        }
        else if( state == DDR3BANK_CLOSED )
        {
            switch( request->type )
            {
                case POWERDOWN_PDA:
                case POWERDOWN_PDPF:
                    state = DDR3BANK_PDPF;
                    break;

                case POWERDOWN_PDPS:
                    state = DDR3BANK_PDPS;
                    break;

                default:
                    state = DDR3BANK_PDPF;
                    break;
            }
        }

        returnValue = true;
    }

    return returnValue;
}

/*
 * PowerUp() force bank to leave powerdown mode and return to either
 * DDR3BANK_CLOSE or DDR3BANK_OPEN 
 */
bool DDR3Bank::PowerUp( NVMainRequest * /*request*/ )
{
    bool returnValue = false;

    if( nextPowerUp <= GetEventQueue()->GetCurrentCycle() 
        && ( state == DDR3BANK_PDPF || state == DDR3BANK_PDPS || state == DDR3BANK_PDA ) )
    {
        /* Update timing constraints */
        nextPowerDown = MAX( nextPowerDown, 
                             GetEventQueue()->GetCurrentCycle() + p->tXP );

        nextActivate = MAX( nextActivate, 
                            GetEventQueue()->GetCurrentCycle() + p->tXP );

        nextPrecharge = MAX( nextPrecharge, 
                             GetEventQueue()->GetCurrentCycle() + p->tXP );
        nextWrite = MAX( nextWrite, 
                         GetEventQueue()->GetCurrentCycle() + p->tXP );

        if( state == DDR3BANK_PDPS )
            nextRead = MAX( nextRead, 
                            GetEventQueue()->GetCurrentCycle() + p->tXPDLL );
        else
            nextRead = MAX( nextRead, 
                            GetEventQueue()->GetCurrentCycle() + p->tXP );

        /*
         *  While technically the bank is being "powered up" we will just reset
         *  the previous state. For energy calculations, the bank is still considered
         *  to be consuming background power while powering up/down. Thus, we need
         *  a powerdown wait, but no power up wait.
         */

        if( state == DDR3BANK_PDA )
            state = DDR3BANK_OPEN;
        else
            state = DDR3BANK_CLOSED;

        returnValue = true;
    }

    return returnValue;
}

/*
 * Activate() open a row 
 */
bool DDR3Bank::Activate( NVMainRequest *request )
{
    /* TODO: Can we remove this sanity check and totally trust IsIssuable()? */
    /* sanity check */
    if( nextActivate > GetEventQueue()->GetCurrentCycle() )
    {
        std::cerr << "NVMain Error: Bank violates ACTIVATION timing constraint!"
            << std::endl;
        return false;
    }
    else if( state != DDR3BANK_CLOSED )
    {
        /*
         * it means no subarray is active when activeSubArrayQueue is empty.
         * therefore, the bank state must be idle rather than active. Actually,
         * there are other conditions that the ACTIVATE cannot be issued. But 
         * we leave the work for subarray so that we don't check here.
         */
        if( activeSubArrayQueue.empty( ) )
        {
            std::cerr << "NVMain Error: try to open a bank that is not idle!"
                << std::endl;
            return false;
        }
    }

    ncounter_t activateRow, activateSubArray;
    request->address.GetTranslatedAddress( &activateRow, NULL, NULL, NULL, NULL, &activateSubArray );

    /* update the timing constraints */
    nextPowerDown = MAX( nextPowerDown, 
                         GetEventQueue()->GetCurrentCycle() + p->tRCD );

    /* issue ACTIVATE to the target subarray */
    bool success = GetChild( request )->IssueCommand( request );

    if( success )
    {
        /* bank-level update */
        openRow = activateRow;
        state = DDR3BANK_OPEN;
        activeSubArrayQueue.push_front( activateSubArray );
        activates++;
    }
    else
    {
        std::cerr << "NVMain Error: Bank " << bankId << " failed to "
            << "activate the subarray " << activateSubArray << std::endl;
    }

    return success;
}

/*
 * Read() fulfills the column read function
 */
bool DDR3Bank::Read( NVMainRequest *request )
{
    /* TODO: Can we remove this sanity check and totally trust IsIssuable()? */
    /* sanity check */
    if( nextRead > GetEventQueue()->GetCurrentCycle() )
    {
        std::cerr << "NVMain Error: Bank violates READ timing constraint!"
            << std::endl;
        return false;
    }
    else if( state != DDR3BANK_OPEN )
    {
        std::cerr << "NVMain Error: try to read a bank that is not active!"
            << std::endl;
        return false;
    }

    uint64_t readRow, readSubArray;
    request->address.GetTranslatedAddress( &readRow, NULL, NULL, NULL, NULL, &readSubArray );

    /* Update timing constraints */
    if( request->type == READ_PRECHARGE )
    {
        nextPowerDown = MAX( nextPowerDown, 
                             GetEventQueue()->GetCurrentCycle() 
                                 + MAX( p->tBURST, p->tCCD ) * (request->burstCount - 1)
                                 + p->tAL + p->tRTP + p->tRP );
    }
    else
    {
        nextPowerDown = MAX( nextPowerDown, 
                             MAX( p->tBURST, p->tCCD ) * (request->burstCount - 1)
                             + GetEventQueue()->GetCurrentCycle() + p->tRDPDEN );
    }

    nextRead = MAX( nextRead, 
                    GetEventQueue()->GetCurrentCycle() 
                        + MAX( p->tBURST, p->tCCD ) * request->burstCount );

    nextWrite = MAX( nextWrite, 
                     GetEventQueue()->GetCurrentCycle()
                         + MAX( p->tBURST, p->tCCD ) * (request->burstCount - 1)
                         + p->tCAS + p->tBURST + p->tRTRS - p->tCWD );

    /* issue READ/READ_RECHARGE to the target subarray */
    bool success = GetChild( request )->IssueCommand( request );

    if( success )
    {
        if( request->type == READ_PRECHARGE )
        {
            precharges++;

            std::deque<ncounter_t>::iterator it;
            for( it = activeSubArrayQueue.begin(); 
                    it != activeSubArrayQueue.end(); ++it )
            {
                if( (*it) == readSubArray )
                {
                    /* delete the item in the active subarray list */
                    activeSubArrayQueue.erase( it );
                    break;
                }
            }

            if( activeSubArrayQueue.empty() )
                state = DDR3BANK_CLOSED;
        } // if( request->type == READ_PRECHARGE )

        dataCycles += p->tBURST;
        reads++;
    } // if( succsss )
    else
    {
        std::cerr << "NVMain Error: Bank " << bankId << " failed to "
            << "read the subarray " << readSubArray << std::endl;
    }

    return success;
}

/*
 * Write() fulfills the column write function
 */
bool DDR3Bank::Write( NVMainRequest *request )
{
    /* TODO: Can we remove this sanity check and totally trust IsIssuable()? */
    /* sanity check */
    if( nextWrite > GetEventQueue()->GetCurrentCycle() )
    {
        std::cerr << "NVMain Error: Bank violates WRITE timing constraint!"
            << std::endl;
        return false;
    }
    else if( state != DDR3BANK_OPEN )
    {
        std::cerr << "NVMain Error: try to read a bank that is not active!"
            << std::endl;
        return false;
    }

    uint64_t writeRow, writeSubArray;
    request->address.GetTranslatedAddress( &writeRow, NULL, NULL, NULL, NULL, &writeSubArray );

    /* Update timing constraints */
    /* if implicit precharge is enabled, do the precharge */
    if( request->type == WRITE_PRECHARGE )
    {
        nextPowerDown = MAX( nextActivate, 
                             GetEventQueue()->GetCurrentCycle()
                             + MAX( p->tBURST, p->tCCD ) * (request->burstCount - 1)
                             + p->tAL + p->tCWD + p->tBURST + p->tWR 
                             + p->tRP );
    }
    /* else, no implicit precharge is enabled, simply update the timing */
    else
    {
        nextPowerDown = MAX( nextPowerDown, 
                             MAX( p->tBURST, p->tCCD ) * (request->burstCount - 1)
                             + GetEventQueue()->GetCurrentCycle() + p->tWRPDEN );
    }

    nextRead = MAX( nextRead, 
                    GetEventQueue()->GetCurrentCycle() 
                    + MAX( p->tBURST, p->tCCD ) * (request->burstCount - 1)
                    + p->tCWD + p->tBURST + p->tWTR );

    nextWrite = MAX( nextWrite, 
                     GetEventQueue()->GetCurrentCycle() 
                     + MAX( p->tBURST, p->tCCD ) * request->burstCount );

    /* issue WRITE/WRITE_PRECHARGE to the target subarray */
    bool success = GetChild( request )->IssueCommand( request );

    if( success )
    {
        dataCycles += p->tBURST;
        writeCycle = true;
        writes++;

        if( request->type == WRITE_PRECHARGE )
        {
            precharges++;

            /* update the activeSubArrayQueue list */
            std::deque<ncounter_t>::iterator it;
            for( it = activeSubArrayQueue.begin( ); 
                    it != activeSubArrayQueue.end( ); ++it )
            {
                if( (*it) == writeSubArray )
                {
                    /* delete the item in the active subarray list */
                    activeSubArrayQueue.erase( it );
                    break;
                }
            }

            if( activeSubArrayQueue.empty( ) )
                state = DDR3BANK_CLOSED;
        }
    }
    else
    {
        std::cerr << "NVMain Error: Bank " << bankId << " failed to "
            << "write the subarray " << writeSubArray << std::endl;
    }

    return success;
}

/*
 * Precharge() close a row and force the bank back to DDR3BANK_CLOSED
 */
bool DDR3Bank::Precharge( NVMainRequest *request )
{
    /* TODO: Can we remove this sanity check and totally trust IsIssuable()? */
    /* sanity check */
    if( nextPrecharge > GetEventQueue()->GetCurrentCycle() )
    {
        std::cerr << "NVMain Error: Bank violates PRECHARGE timing constraint!"
            << std::endl;
        return false;
    }
    else if( state != DDR3BANK_CLOSED && state != DDR3BANK_OPEN )
    {
        std::cerr << "NVMain Error: try to precharge a bank that is neither "
            << "idle nor active" << std::endl;
        return false;
    }

    uint64_t preRow, preSubArray;
    request->address.GetTranslatedAddress( &preRow, NULL, NULL, NULL, NULL, &preSubArray );

    /* Update timing constraints */
    /* 
     * even though tPRPDEN = 1, the IDD spec in powerdown mode is only applied 
     * after the completion of precharge
     */
    nextPowerDown = MAX( nextPowerDown, 
                         GetEventQueue()->GetCurrentCycle() + p->tRP );

    if( request->type == PRECHARGE ) 
    {
        /* issue PRECHARGE/PRECHARGE_ALL to the subarray */
        bool success = GetChild( request )->IssueCommand( request );
        if( success )
        {
            /* update the activeSubArrayQueue list */
            std::deque<ncounter_t>::iterator it;
            for( it = activeSubArrayQueue.begin( ); 
                    it != activeSubArrayQueue.end( ); ++it )
            {
                if( (*it) == preSubArray )
                {
                    /* delete the item in the active subarray list */
                    activeSubArrayQueue.erase( it );
                    break;
                }
            }
        }
        else
        {
            std::cerr << "NVMain Error: Bank " << bankId << " failed to "
                << "precharge the subarray " << preSubArray << std::endl;
            return false;
        }
    } // if( request->type == PRECHARGE )
    else if( request->type == PRECHARGE_ALL )
    {
        if( activeSubArrayQueue.empty( ) == false )
        {
            /* close all subarrays */
            while( activeSubArrayQueue.size( ) > 1 )
            {
                ncounter_t openedSubArray = activeSubArrayQueue.front( );
                activeSubArrayQueue.pop_front( );

                NVMainRequest* dummyPrecharge = new NVMainRequest;
                (*dummyPrecharge) = (*request);
                dummyPrecharge->owner = this;
                bool success = GetChild( openedSubArray )->IssueCommand( dummyPrecharge );

                if( success == false )
                {
                    std::cerr << "NVMain Error: Bank " << bankId << " failed to "
                        << "precharge the subarray " << openedSubArray 
                        << std::endl;
                    return false;
                }

                //precharges++;
            }

            ncounter_t openedSubArray = activeSubArrayQueue.front( );
            activeSubArrayQueue.pop_front( );
            bool success = GetChild( openedSubArray )->IssueCommand( request );

            if( success == false )
            {
                std::cerr << "NVMain Error: Bank " << bankId << " failed to "
                    << "issue " << request->type << " to subarray" 
                    << openedSubArray << std::endl;
                return false;
            }

            assert( activeSubArrayQueue.empty( ) );
        } // if( activeSubArrayQueue.empty( ) == false )
    } // if( request->type == PRECHARGE_ALL )
    else
    {
        std::cerr << "NVMain Error: Bank " << bankId 
            << " has unrecognized command "
            << request->type << std::endl;
        return false;
    } 

    if( activeSubArrayQueue.empty( ) )
        state = DDR3BANK_CLOSED;

    precharges++;

    return true;
}

/* 
 * Refresh() is simply treated as a Activate()
 */
bool DDR3Bank::Refresh( NVMainRequest *request )
{
    /* TODO: Can we remove this sanity check and totally trust IsIssuable()? */
    /* sanity check */
    if( nextActivate > GetEventQueue()->GetCurrentCycle() )
    {
        std::cerr << "NVMain Error: Bank violates REFRESH timing constraint!"
            << std::endl;
        return false;
    }

    uint64_t refRow, refSubArray;

    request->address.GetTranslatedAddress( &refRow, NULL, NULL, NULL, NULL, &refSubArray );

    /* Update timing constraints */
    /* 
     * when one sub-array is under refresh, powerdown can only be issued after
     * tRFC
     */
    nextPowerDown = MAX( nextPowerDown, 
                         GetEventQueue()->GetCurrentCycle() + p->tRFC );

    /* TODO: implement sub-array-level refresh */

    bool success = GetChild( request )->IssueCommand( request );
    
    if( success )
    {
        refreshes++;
    }
    else
    {
        std::cerr << "NVMain Error: Bank " << bankId << " failed to "
            << "refresh the subarray " << refSubArray 
            << " by command " << request->type << std::endl;
        return false;
    }

    return true;
}

ncycle_t DDR3Bank::NextIssuable( NVMainRequest *request )
{
    ncycle_t nextCompare = 0;

    if( request->type == ACTIVATE || request->type == REFRESH ) nextCompare = nextActivate;
    else if( request->type == READ || request->type == READ_PRECHARGE ) nextCompare = nextRead;
    else if( request->type == WRITE || request->type == WRITE_PRECHARGE ) nextCompare = nextWrite;
    else if( request->type == PRECHARGE || request->type == PRECHARGE_ALL ) nextCompare = nextPrecharge;
        
    return MAX(GetChild( request )->NextIssuable( request ), nextCompare );
}

/*
 * IsIssuable() tells whether one request satisfies the timing constraints
 */
bool DDR3Bank::IsIssuable( NVMainRequest *req, FailReason *reason )
{
    bool rv = true;

    uint64_t opRank, opBank, opRow, opSubArray;
    req->address.GetTranslatedAddress( &opRow, NULL, &opBank, &opRank, NULL, &opSubArray );

    if( nextCommand != CMD_NOP )
        return false;
      
    if( req->type == ACTIVATE )
    {
        /* if the bank-level nextActive is not satisfied, cannot issue */
        if( nextActivate > ( GetEventQueue()->GetCurrentCycle() ) 
            || state == DDR3BANK_PDPF || state == DDR3BANK_PDPS || state == DDR3BANK_PDA )

        {
            rv = false;
            if( reason ) 
                reason->reason = BANK_TIMING;

            actWaits++;
            actWaitTotal += nextActivate - (GetEventQueue()->GetCurrentCycle());
        }
        else
        {
            rv = GetChild( req )->IsIssuable( req, reason );
        }
    }
    else if( req->type == READ || req->type == READ_PRECHARGE )
    {
        if( nextRead > (GetEventQueue()->GetCurrentCycle()) 
            || state != DDR3BANK_OPEN  )
        {
            rv = false;
            if( reason ) 
                reason->reason = BANK_TIMING;
        }
        else
        {
            rv = GetChild( req )->IsIssuable( req, reason );
        }
    }
    else if( req->type == WRITE || req->type == WRITE_PRECHARGE )
    {
        if( nextWrite > (GetEventQueue()->GetCurrentCycle()) 
            || state != DDR3BANK_OPEN )
        {
            rv = false;
            if( reason ) 
                reason->reason = BANK_TIMING;
        }
        else
        {
            rv = GetChild( req )->IsIssuable( req, reason );
        }
    }
    else if( req->type == PRECHARGE || req->type == PRECHARGE_ALL )
    {
        if( nextPrecharge > (GetEventQueue()->GetCurrentCycle()) 
            || ( state != DDR3BANK_CLOSED && state != DDR3BANK_OPEN ) )
        {
            rv = false;
            if( reason ) 
                reason->reason = BANK_TIMING;
        }
        else
        {
            if( req->type == PRECHARGE_ALL )
            {
                std::deque<ncounter_t>::iterator it;

                for( it = activeSubArrayQueue.begin(); 
                        it != activeSubArrayQueue.end(); ++it )
                {
                    rv = GetChild( (*it) )->IsIssuable( req, reason );

                    if( rv == false )
                        break;
                }
            }
            else
            {
                rv = GetChild( req )->IsIssuable( req, reason );
            }
        }
    }
    else if( req->type == POWERDOWN_PDA 
             || req->type == POWERDOWN_PDPF 
             || req->type == POWERDOWN_PDPS )
    {
        if( nextPowerDown > (GetEventQueue()->GetCurrentCycle()) 
            || ( state != DDR3BANK_CLOSED && state != DDR3BANK_OPEN ) 
            || ( ( req->type == POWERDOWN_PDPF || req->type == POWERDOWN_PDPS ) 
                && state == DDR3BANK_OPEN ) )
        {
            rv = false;
            if( reason ) 
                reason->reason = BANK_TIMING;
        }

        for( ncounter_t saIdx = 0; saIdx < subArrayNum; saIdx++ )
        {
            if( !GetChild(saIdx)->IsIssuable( req ) )
            {
                rv = false;
                break;
            }
        }
    }
    else if( req->type == POWERUP )
    {
        if( nextPowerUp > (GetEventQueue()->GetCurrentCycle()) 
            || ( state != DDR3BANK_PDPF && state != DDR3BANK_PDPS && state != DDR3BANK_PDA ) )
        {
            rv = false;
            if( reason ) 
                reason->reason = BANK_TIMING;
        }

        for( ncounter_t saIdx = 0; saIdx < subArrayNum; saIdx++ )
        {
            if( !GetChild(saIdx)->IsIssuable( req ) )
            {
                rv = false;
                break;
            }
        }
    }
    else if( req->type == REFRESH )
    {
        if( nextActivate > ( GetEventQueue()->GetCurrentCycle() ) 
            || ( state != DDR3BANK_CLOSED && state != DDR3BANK_OPEN ) )
        {
            rv = false;
            if( reason )
              reason->reason = BANK_TIMING;
        }
        else
        {
            rv = GetChild( req )->IsIssuable( req, reason );
        }
    }
    else
    {
        /* Unknown command, just ask child modules. */
        rv = GetChild( req )->IsIssuable( req, reason );
    }

    return rv;
}

/*
 * IssueCommand() issue the command so that bank status will be updated
 */
bool DDR3Bank::IssueCommand( NVMainRequest *req )
{
    bool rv = false;

    if( !IsIssuable( req ) )
    {
        std::cout << "NVMain: Bank: Warning: Command can not be issued!\n";
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

            case REFRESH:
                rv = this->Refresh( req );
                break;

            case POWERDOWN_PDA:
            case POWERDOWN_PDPF:
            case POWERDOWN_PDPS:
                rv = this->PowerDown( req );
                break;

            case POWERUP:
                rv = this->PowerUp( req );
                break;

            default:
                rv = GetChild( req )->IssueCommand( req );
                break;  
        }
    }

    return rv;
}

DDR3BankState DDR3Bank::GetState( ) 
{
    return state;
}

void DDR3Bank::CalculatePower( )
{
    ncycle_t simulationTime = GetEventQueue()->GetCurrentCycle();

    if( simulationTime == 0 )
    {
        bankPower 
            = activePower 
            = burstPower 
            = refreshPower = 0.0f;
        return;
    }

    if( p->EnergyModel == "current" )
    {
        bankPower = ( bankEnergy * p->Voltage ) / (double)simulationTime / 1000.0f; 
        activePower = ( activeEnergy * p->Voltage ) / (double)simulationTime / 1000.0f; 
        burstPower = ( burstEnergy * p->Voltage ) / (double)simulationTime / 1000.0f; 
        refreshPower = ( refreshEnergy * p->Voltage ) / (double)simulationTime / 1000.0f; 
    }
    else
    {
        bankPower = bankEnergy / ((double)simulationTime / 1000000000.0);
        activePower = activeEnergy / ((double)simulationTime / 1000000000.0); 
        burstPower = burstEnergy / ((double)simulationTime / 1000000000.0);
        refreshPower = refreshEnergy / ((double)simulationTime / 1000000000.0);
    }
}

double DDR3Bank::GetPower( )
{
    CalculatePower( );

    return bankPower;
}

void DDR3Bank::SetName( std::string )
{
}

/* 
 * Corresponds to physical bank id 
 * if this bank logically spans multiple devices, the id corresponds to the device, 
 * NOT the logical bank id within a single device.
 */
void DDR3Bank::SetId( ncounter_t id )
{
    bankId = id;
}

std::string DDR3Bank::GetName( )
{
    return "";
}


ncounter_t DDR3Bank::GetId( )
{
    return bankId;
}

void DDR3Bank::CalculateStats( )
{
    NVMObject::CalculateStats( );

    double idealBandwidth;

    idealBandwidth = (double)(p->CLK * p->RATE * p->BusWidth);

    if( activeCycles != 0 )
        utilization = (double)((double)dataCycles / (double)(activeCycles + standbyCycles) );
    else
        utilization = 0.0f;

    bankEnergy = activeEnergy = burstEnergy = refreshEnergy 
               = 0.0f;

    for( unsigned saIdx = 0; saIdx < subArrayNum; saIdx++ )
    {
        StatType saEstat  = GetStat( GetChild(saIdx), "subArrayEnergy" );
        StatType actEstat = GetStat( GetChild(saIdx), "activeEnergy" );
        StatType bstEstat = GetStat( GetChild(saIdx), "burstEnergy" );
        StatType refEstat = GetStat( GetChild(saIdx), "refreshEnergy" );

        bankEnergy += CastStat( saEstat, double );
        activeEnergy += CastStat( actEstat, double );
        burstEnergy += CastStat( bstEstat, double );
        refreshEnergy += CastStat( refEstat, double );
    }

    CalculatePower( );

    bandwidth = (utilization * idealBandwidth);
    powerCycles = activeCycles + standbyCycles;

    actWaitAverage = static_cast<double>(actWaitTotal) / static_cast<double>(actWaits);

    worstCaseEndurance = std::numeric_limits<uint64_t>::max( );
    averageEndurance = 0;
    for( ncounter_t i = 0; i < GetChildCount( ); i++ )
    {
        StatType subArrayWorstEndr = GetStat( GetChild(i), "worstCaseEndurance" );
        StatType subArrayAverageEndr = GetStat( GetChild(i), "averageEndurance" );

        uint64_t subArrayEndurance = CastStat( subArrayWorstEndr, uint64_t );
        worstCaseEndurance = (subArrayEndurance < worstCaseEndurance) ? subArrayEndurance : worstCaseEndurance;
        averageEndurance += CastStat( subArrayAverageEndr, uint64_t );
    }
    averageEndurance /= GetChildCount( );
}


bool DDR3Bank::Idle( )
{
    bool bankIdle = true;

    for( ncounter_t i = 0; i < subArrayNum; i++ )
    {
        if( GetChild(i)->Idle( ) == false )
        {
            bankIdle = false;
            break;
        }
    }
    return bankIdle;
}

void DDR3Bank::Cycle( ncycle_t steps )
{
    /* Count cycle numbers for each state */
    /* Number of fast exit prechage standbys */
    if( state == DDR3BANK_PDPF )
        fastExitPrechargeCycles += steps;
    else if( state == DDR3BANK_PDA )
        fastExitActiveCycles += steps;
    /* precharge powerdown slow exit */
    else if( state == DDR3BANK_PDPS )
        slowExitPrechargeCycles += steps;
    /* active standby */
    else if( state == DDR3BANK_OPEN )
        activeCycles += steps;
    /* precharge standby */
    else if( state == DDR3BANK_CLOSED )
        standbyCycles += steps;
}
