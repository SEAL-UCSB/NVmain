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

#include "src/Bank.h"
#include "src/MemoryController.h"
#include "src/EventQueue.h"
#include "Endurance/EnduranceModelFactory.h"

#include <signal.h>
#include <cassert>
#include <iostream>
#include <sstream>
#include <limits>

using namespace NVM;

Bank::Bank( )
{
    conf = NULL;

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

    subArrays = NULL;
    subArrayNum = 0;
    activeSubArrayQueue.clear();

    /* a MAT is 512x512 by default */
    MATWidth = 512;
    MATHeight = 512;

    state = BANK_CLOSED;
    lastActivate = 0;
    openRow = 0;

    bankEnergy = 0.0f;
    backgroundEnergy = 0.0f;
    activeEnergy = 0.0f;
    burstEnergy = 0.0f;
    refreshEnergy = 0.0f;

    bankPower = 0.0f;
    backgroundPower = 0.0f;
    activePower = 0.0f;
    burstPower = 0.0f;
    refreshPower = 0.0f;

    dataCycles = 0;
    activeCycles = 0;
    standbyCycles = 0;
    feCycles = 0;
    seCycles = 0;
    utilization = 0.0f;
    writeCycle = false;
    writeMode = WRITE_THROUGH;
    idleTimer = 0;

    reads = 0;
    writes = 0;
    activates = 0;
    precharges = 0;
    refreshes = 0;

    actWaits = 0;
    actWaitTime = 0;

    bankId = -1;

    psInterval = 0;
}

Bank::~Bank( )
{
    for( ncounter_t i = 0; i < subArrayNum; i++ )
        delete subArrays[i];

    delete [] subArrays;
}

void Bank::SetConfig( Config *c )
{
    conf = c;
    
    /* customize MAT size */
    if( conf->KeyExists( "MATWidth" ) )
        MATWidth = static_cast<ncounter_t>( conf->GetValue( "MATWidth" ) );

    if( conf->KeyExists( "MATHeight" ) )
        MATHeight = static_cast<ncounter_t>( conf->GetValue( "MATHeight" ) );


    Params *params = new Params( );
    params->SetParams( c );
    SetParams( params );

    subArrayNum = p->ROWS / MATHeight;
    subArrays = new SubArray*[subArrayNum];

    for( ncounter_t i = 0; i < subArrayNum; i++ )
    {
        subArrays[i] = new SubArray;

        std::stringstream formatter;

        formatter << i;
        subArrays[i]->SetName ( formatter.str() );
        subArrays[i]->SetId( i );

        formatter.str( "" );
        formatter<< statName << ".subarray" << i;
        subArrays[i]->StatName (formatter.str( ) );

        subArrays[i]->SetParent( this );
        AddChild( subArrays[i] );

        subArrays[i]->SetConfig( c );
    }

    /* We need to create an endurance model on a bank-by-bank basis */
    endrModel = EnduranceModelFactory::CreateEnduranceModel( p->EnduranceModel );
    if( endrModel )
        endrModel->SetConfig( conf );

    if( p->InitPD )
        state = BANK_PDPF;
}

/*
 * PowerDown() power the bank down along with different modes
 */
bool Bank::PowerDown( OpType pdType )
{
    bool returnValue = false;

    if( nextPowerDown <= GetEventQueue()->GetCurrentCycle() 
        && ( state == BANK_OPEN || state == BANK_CLOSED ) )
    {
        /* Update timing constraints */
        /*
         *  The power down state (pdState) will be determined by the device 
         *  class, which will be checked to see if all banks are idle or not, 
         *  and if fast exit is used.
         */
        nextPowerUp = MAX( nextPowerUp, GetEventQueue()->GetCurrentCycle() 
                                        + p->tPD );
        
        if( state == BANK_OPEN )
        {
            assert( pdType == POWERDOWN_PDA );
            state = BANK_PDA;
        }
        else if( state == BANK_CLOSED )
        {
            switch( pdType )
            {
                case POWERDOWN_PDA:
                case POWERDOWN_PDPF:
                    state = BANK_PDPF;
                    break;

                case POWERDOWN_PDPS:
                    state = BANK_PDPS;
                    break;

                default:
                    state = BANK_PDPF;
                    break;
            }
        }

        returnValue = true;
    }

    return returnValue;
}

/*
 * PowerUp() force bank to leave powerdown mode and return to either
 * BANK_CLOSE or BANK_OPEN 
 */
bool Bank::PowerUp( )
{
    bool returnValue = false;

    if( nextPowerUp <= GetEventQueue()->GetCurrentCycle() 
        && ( state == BANK_PDPF || state == BANK_PDPS || state == BANK_PDA ) )
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

        if( state == BANK_PDPS )
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
        if( state == BANK_PDA )
            state = BANK_OPEN;
        else
            state = BANK_CLOSED;

        returnValue = true;
    }

    return returnValue;
}

/*
 * Activate() open a row 
 */
bool Bank::Activate( NVMainRequest *request )
{
    /* TODO: Can we remove this sanity check and totally trust IsIssuable()? */
    /* sanity check */
    if( nextActivate > GetEventQueue()->GetCurrentCycle() )
    {
        std::cerr << "NVMain Error: Bank violates ACTIVATION timing constraint!"
            << std::endl;
        return false;
    }
    else if( state != BANK_CLOSED )
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

    ncounter_t activateRow;
    request->address.GetTranslatedAddress( &activateRow, NULL, NULL, NULL, NULL );

    ncounter_t activateSubArray = activateRow / MATHeight;

    /* update the timing constraints */
    nextPowerDown = MAX( nextPowerDown, 
                         GetEventQueue()->GetCurrentCycle() + p->tRCD );

    /* issue ACTIVATE to the target subarray */
    bool success = subArrays[activateSubArray]->IssueCommand( request );

    if( success )
    {
        /* bank-level update */
        openRow = activateRow;
        state = BANK_OPEN;
        activeSubArrayQueue.push_back( activateSubArray );
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
bool Bank::Read( NVMainRequest *request )
{
    /* TODO: Can we remove this sanity check and totally trust IsIssuable()? */
    /* sanity check */
    if( nextRead > GetEventQueue()->GetCurrentCycle() )
    {
        std::cerr << "NVMain Error: Bank violates READ timing constraint!"
            << std::endl;
        return false;
    }
    else if( state != BANK_OPEN )
    {
        std::cerr << "NVMain Error: try to read a bank that is not active!"
            << std::endl;
        return false;
    }

    uint64_t readRow, readCol;
    request->address.GetTranslatedAddress( &readRow, &readCol, NULL, NULL, NULL );

    ncounter_t opSubArray = readRow / MATHeight;

    /* Update timing constraints */
    if( request->type == READ_PRECHARGE )
        nextPowerDown = MAX( nextPowerDown, 
                             GetEventQueue()->GetCurrentCycle() 
                                 + p->tAL + p->tRTP + p->tRP );
    else
        nextPowerDown = MAX( nextPowerDown, 
                             GetEventQueue()->GetCurrentCycle() + p->tRDPDEN );

    nextRead = MAX( nextRead, 
                    GetEventQueue()->GetCurrentCycle() 
                        + MAX( p->tBURST, p->tCCD ) );

    nextWrite = MAX( nextWrite, 
                     GetEventQueue()->GetCurrentCycle() 
                         + p->tCAS + p->tBURST + p->tRTRS - p->tCWD );

    /* issue READ/READ_RECHARGE to the target subarray */
    bool success = subArrays[opSubArray]->IssueCommand( request );

    if( success )
    {
        if( request->type == READ_PRECHARGE )
        {
            precharges++;

            std::deque<ncounter_t>::iterator it;
            for( it = activeSubArrayQueue.begin(); 
                    it != activeSubArrayQueue.end(); ++it )
            {
                if( (*it) == opSubArray )
                {
                    /* delete the item in the active subarray list */
                    activeSubArrayQueue.erase( it );
                    break;
                }
            }

            if( activeSubArrayQueue.empty() )
                state = BANK_CLOSED;
        } // if( request->type == READ_PRECHARGE )

        dataCycles += p->tBURST;
        reads++;
    } // if( succsss )
    else
    {
        std::cerr << "NVMain Error: Bank " << bankId << " failed to "
            << "read the subarray " << opSubArray << std::endl;
    }

    return success;
}

/*
 * Write() fulfills the column write function
 */
bool Bank::Write( NVMainRequest *request )
{
    /* TODO: Can we remove this sanity check and totally trust IsIssuable()? */
    /* sanity check */
    if( nextWrite > GetEventQueue()->GetCurrentCycle() )
    {
        std::cerr << "NVMain Error: Bank violates WRITE timing constraint!"
            << std::endl;
        return false;
    }
    else if( state != BANK_OPEN )
    {
        std::cerr << "NVMain Error: try to read a bank that is not active!"
            << std::endl;
        return false;
    }

    uint64_t writeRow, writeCol;
    request->address.GetTranslatedAddress( &writeRow, &writeCol, NULL, NULL, NULL );
    ncounter_t opSubArray = writeRow / MATHeight;

    /* Update timing constraints */
    /* if implicit precharge is enabled, do the precharge */
    if( request->type == WRITE_PRECHARGE )
        nextPowerDown = MAX( nextActivate, 
                            GetEventQueue()->GetCurrentCycle()
                                + p->tAL + p->tCWD + p->tBURST + p->tWR 
                                + p->tRP );

    /* else, no implicit precharge is enabled, simply update the timing */
    else
        nextPowerDown = MAX( nextPowerDown, 
                             GetEventQueue()->GetCurrentCycle() + p->tWRPDEN );

    nextRead = MAX( nextRead, 
                    GetEventQueue()->GetCurrentCycle() 
                        + p->tCWD + p->tBURST + p->tWTR );

    nextWrite = MAX( nextWrite, 
                     GetEventQueue()->GetCurrentCycle() 
                         + MAX( p->tBURST, p->tCCD ) );

    /* issue WRITE/WRITE_PRECHARGE to the target subarray */
    bool success = subArrays[opSubArray]->IssueCommand( request );

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
                if( (*it) == opSubArray )
                {
                    /* delete the item in the active subarray list */
                    activeSubArrayQueue.erase( it );
                    break;
                }
            }

            if( activeSubArrayQueue.empty( ) )
                state = BANK_CLOSED;
        }
    }
    else
    {
        std::cerr << "NVMain Error: Bank " << bankId << " failed to "
            << "write the subarray " << opSubArray << std::endl;
    }

    return success;
}

/*
 * Precharge() close a row and force the bank back to BANK_CLOSED
 */
bool Bank::Precharge( NVMainRequest *request )
{
    /* TODO: Can we remove this sanity check and totally trust IsIssuable()? */
    /* sanity check */
    if( nextPrecharge > GetEventQueue()->GetCurrentCycle() )
    {
        std::cerr << "NVMain Error: Bank violates PRECHARGE timing constraint!"
            << std::endl;
        return false;
    }
    else if( state != BANK_CLOSED && state != BANK_OPEN )
    {
        std::cerr << "NVMain Error: try to precharge a bank that is neither "
            << "idle nor active" << std::endl;
        return false;
    }

    uint64_t preRow;
    request->address.GetTranslatedAddress( &preRow, NULL, NULL, NULL, NULL );

    ncounter_t preSubArray = preRow / MATHeight;

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
        bool success = subArrays[preSubArray]->IssueCommand( request );
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
                bool success = 
                    subArrays[openedSubArray]->IssueCommand( dummyPrecharge ); 

                if( success == false )
                {
                    std::cerr << "NVMain Error: Bank " << bankId << " failed to "
                        << "precharge the subarray " << openedSubArray 
                        << std::endl;
                    return false;
                }

                precharges++;
            }

            ncounter_t openedSubArray = activeSubArrayQueue.front( );
            activeSubArrayQueue.pop_front( );
            bool success = 
                subArrays[openedSubArray]->IssueCommand( request ); 

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
        state = BANK_CLOSED;

    precharges++;

    return true;
}

/* 
 * Refresh() is simply treated as a Activate()
 */
bool Bank::Refresh( NVMainRequest *request )
{
    /* TODO: Can we remove this sanity check and totally trust IsIssuable()? */
    /* sanity check */
    if( nextActivate > GetEventQueue()->GetCurrentCycle() )
    {
        std::cerr << "NVMain Error: Bank violates REFRESH timing constraint!"
            << std::endl;
        return false;
    }

    uint64_t refRow;

    request->address.GetTranslatedAddress( &refRow, NULL, NULL, NULL, NULL );

    ncounter_t refSubArray = refRow / MATHeight;

    /* Update timing constraints */
    /* 
     * when one sub-array is under refresh, powerdown can only be issued after
     * tRFC
     */
    nextPowerDown = MAX( nextPowerDown, 
                         GetEventQueue()->GetCurrentCycle() + p->tRFC );

    /* TODO: implement sub-array-level refresh */

    bool success = subArrays[refSubArray]->IssueCommand( request );
    
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

/*
 * IsIssuable() tells whether one request satisfies the timing constraints
 */
bool Bank::IsIssuable( NVMainRequest *req, FailReason *reason )
{
    uint64_t opRank;
    uint64_t opBank;
    uint64_t opRow;
    uint64_t opCol;
    bool rv = true;

    req->address.GetTranslatedAddress( &opRow, &opCol, &opBank, &opRank, NULL );
    uint64_t opSubArray = opRow / MATHeight;

    if( nextCommand != CMD_NOP )
        return false;
      
    if( req->type == ACTIVATE )
    {
        /* if the bank-level nextActive is not satisfied, cannot issue */
        if( nextActivate > (GetEventQueue()->GetCurrentCycle()) )
        {
            rv = false;
            if( reason ) 
                reason->reason = BANK_TIMING;

            actWaits++;
            actWaitTime += nextActivate - (GetEventQueue()->GetCurrentCycle());
        }
        else
            rv = subArrays[opSubArray]->IsIssuable( req, reason );
    }
    else if( req->type == READ || req->type == READ_PRECHARGE )
    {
        if( nextRead > (GetEventQueue()->GetCurrentCycle()) 
            || state != BANK_OPEN  )
        {
            rv = false;
            if( reason ) 
                reason->reason = BANK_TIMING;
        }
        else
            rv = subArrays[opSubArray]->IsIssuable( req, reason );
    }
    else if( req->type == WRITE || req->type == WRITE_PRECHARGE )
    {
        if( nextWrite > (GetEventQueue()->GetCurrentCycle()) 
            || state != BANK_OPEN )
        {
            rv = false;
            if( reason ) 
                reason->reason = BANK_TIMING;
        }
        else
            rv = subArrays[opSubArray]->IsIssuable( req, reason );
    }
    else if( req->type == PRECHARGE || req->type == PRECHARGE_ALL )
    {
        if( nextPrecharge > (GetEventQueue()->GetCurrentCycle()) 
            || ( state != BANK_CLOSED && state != BANK_OPEN ) )
        {
            rv = false;
            if( reason ) 
                reason->reason = BANK_TIMING;
        }
        else
            if( req->type == PRECHARGE_ALL )
            {
                std::deque<ncounter_t>::iterator it;

                for( it = activeSubArrayQueue.begin(); 
                        it != activeSubArrayQueue.end(); ++it )
                {
                    rv = subArrays[(*it)]->IsIssuable( req, reason );

                    if( rv == false )
                        break;
                }
            }
            else
                rv = subArrays[opSubArray]->IsIssuable( req, reason );
    }
    else if( req->type == POWERDOWN_PDA 
             || req->type == POWERDOWN_PDPF 
             || req->type == POWERDOWN_PDPS )
    {
        if( nextPowerDown > (GetEventQueue()->GetCurrentCycle()) 
            || ( state != BANK_CLOSED && state != BANK_OPEN ) 
            || ( ( req->type == POWERDOWN_PDPF || req->type == POWERDOWN_PDPS ) 
                && state == BANK_OPEN ) )
        {
            rv = false;
            if( reason ) 
                reason->reason = BANK_TIMING;
        }
    }
    else if( req->type == POWERUP )
    {
        if( nextPowerUp > (GetEventQueue()->GetCurrentCycle()) 
            || ( state != BANK_PDPF && state != BANK_PDPS && state != BANK_PDA ) )
        {
            rv = false;
            if( reason ) 
                reason->reason = BANK_TIMING;
        }
    }
    else if( req->type == REFRESH )
    {
        if( nextActivate > ( GetEventQueue()->GetCurrentCycle() ) 
            || ( state != BANK_CLOSED && state != BANK_OPEN ) )
        {
            rv = false;
            if( reason )
              reason->reason = BANK_TIMING;
        }
        else
            rv = subArrays[opSubArray]->IsIssuable( req, reason );
    }
    else
    {
        std::cout << "Bank: IsIssuable: Unknown operation: " << req->type 
            << std::endl;
        rv = false;
        if( reason ) 
            reason->reason = UNKNOWN_FAILURE;
    }

    return rv;
}

/*
 * IssueCommand() issue the command so that bank status will be updated
 */
bool Bank::IssueCommand( NVMainRequest *req )
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

            default:
                std::cerr << "NVMain Error: Bank detects unknown operation! " 
                    << req->type << std::endl;
                break;  
        }
    }

    return rv;
}

bool Bank::WouldConflict( uint64_t checkRow )
{
    uint64_t opSubArray = checkRow / MATHeight;

    return subArrays[opSubArray]->WouldConflict( checkRow );
}

BankState Bank::GetState( ) 
{
    return state;
}

void Bank::CalculatePower( )
{
    float saPower, bgPower, actPower, bstPower, refPower;
    float saEnergy, bgEnergy, actEnergy, bstEnergy, refEnergy;
    
    /* first reset all counters so that it does not cumulate */
    bankEnergy = backgroundEnergy = activeEnergy = burstEnergy = refreshEnergy 
               = 0.0f;

    bankPower = backgroundPower = activePower = burstPower = refreshPower 
              = 0.0f;

    for( unsigned saIdx = 0; saIdx < subArrayNum; saIdx++ )
    {
        subArrays[saIdx]->GetEnergy( saEnergy, bgEnergy, actEnergy, bstEnergy,
                                     refEnergy );

        bankEnergy += saEnergy;
        backgroundEnergy += bgEnergy;
        activeEnergy += actEnergy;
        burstEnergy += bstEnergy;
        refreshEnergy += refEnergy;

        subArrays[saIdx]->GetPower( saPower, bgPower, actPower, bstPower, 
                                    refPower );

        bankPower += saPower;
        backgroundPower += bgPower;
        activePower += actPower;
        burstPower += bstPower;
        refreshPower += refreshPower;
    }
}

float Bank::GetPower( )
{
    CalculatePower( );

    return bankPower;
}

float Bank::GetEnergy( )
{
    CalculatePower( );

    return bankEnergy;
}

void Bank::SetName( std::string )
{
}

/* 
 * Corresponds to physical bank id 
 * if this bank logically spans multiple devices, the id corresponds to the device, 
 * NOT the logical bank id within a single device.
 */
void Bank::SetId( ncounter_t id )
{
    bankId = id;
}

std::string Bank::GetName( )
{
    return "";
}


ncounter_t Bank::GetId( )
{
    return bankId;
}

void Bank::PrintStats( )
{
    float idealBandwidth;

    idealBandwidth = (float)(p->CLK * p->MULT * p->RATE * p->BPC);

    if( activeCycles != 0 )
        utilization = (float)((float)dataCycles / (float)(activeCycles + standbyCycles) );
    else
        utilization = 0.0f;

    CalculatePower( );

    if( p->EnergyModel_set && p->EnergyModel == "current" )
    {
        std::cout << "i" << psInterval << "." << statName 
            << ".current " << bankEnergy << "\t; mA" << std::endl;
        std::cout << "i" << psInterval << "." << statName 
            << ".current.background " << backgroundEnergy << "\t; mA" << std::endl;
        std::cout << "i" << psInterval << "." << statName 
            << ".current.active " << activeEnergy << "\t; mA" << std::endl;
        std::cout << "i" << psInterval << "." << statName 
            << ".current.burst " << burstEnergy << "\t; mA" << std::endl;
        std::cout << "i" << psInterval << "." << statName 
            << ".current.refresh " << refreshEnergy << "\t; mA" << std::endl;
    }
    else
    {
        std::cout << "i" << psInterval << "." << statName 
            << ".energy " << bankEnergy << "\t; nJ" << std::endl; 
        std::cout << "i" << psInterval << "." << statName 
            << ".energy.background " << backgroundEnergy << "\t; nJ" << std::endl;
        std::cout << "i" << psInterval << "." << statName 
            << ".energy.active " << activeEnergy << "\t; nJ" << std::endl;
        std::cout << "i" << psInterval << "." << statName 
            << ".energy.burst " << burstEnergy << "\t; nJ" << std::endl;
        std::cout << "i" << psInterval << "." << statName 
            << ".energy.refresh " << refreshEnergy << "\t; nJ" << std::endl;
    }
    
    std::cout << "i" << psInterval << "." << statName << ".power " 
              << bankPower << "\t; W per bank per device" << std::endl
              << "i" << psInterval << "." << statName << ".power.background " 
              << backgroundPower << "\t; W per bank per device" << std::endl
              << "i" << psInterval << "." << statName << ".power.active " 
              << activePower << "\t; W per bank per device" << std::endl
              << "i" << psInterval << "." << statName << ".power.burst " 
              << burstPower << "\t; W per bank per device" << std::endl
              << "i" << psInterval << "." << statName << ".power.refresh " 
              << refreshPower << "\t; W per bank per device" << std::endl;

    std::cout << "i" << psInterval << "." << statName << ".bandwidth " 
              << (utilization * idealBandwidth) << "\t; MB/s " << std::endl
              << "i" << psInterval << "." << statName << "(" << dataCycles << " data cycles in " 
              << (activeCycles + standbyCycles) << " cycles)" << std::endl
              << "i" << psInterval << "." << statName << ".utilization " 
              << utilization << std::endl;

    std::cout << "i" << psInterval << "." << statName 
              << ".reads " << reads << std::endl
              << "i" << psInterval << "." << statName 
              << ".writes " << writes << std::endl
              << "i" << psInterval << "." << statName 
              << ".activates " << activates << std::endl
              << "i" << psInterval << "." << statName 
              << ".precharges " << precharges << std::endl
              << "i" << psInterval << "." << statName 
              << ".refreshes " << refreshes << std::endl;

    std::cout << "i" << psInterval << "." << statName 
              << ".activeCycles " << activeCycles << std::endl
              << "i" << psInterval << "." << statName 
              << ".standbyCycles " << standbyCycles << std::endl
              << "i" << psInterval << "." << statName 
              << ".fastExitCycles " << feCycles << std::endl
              << "i" << psInterval << "." << statName 
              << ".slowExitCycles " << seCycles << std::endl;

    if( endrModel )
    {
        if( endrModel->GetWorstLife( ) == std::numeric_limits< uint64_t >::max( ) )
          std::cout << "i" << psInterval << "." << statName 
                    << ".worstCaseEndurance N/A" << std::endl
                    << "i" << psInterval << "." << statName 
                    << ".averageEndurance N/A" << std::endl;
        else
          std::cout << "i" << psInterval << "." << statName << ".worstCaseEndurance " 
                    << (endrModel->GetWorstLife( )) << std::endl
                    << "i" << psInterval << "." << statName << ".averageEndurance " 
                    << endrModel->GetAverageLife( ) << std::endl;

        endrModel->PrintStats( );
    }

    std::cout << "i" << psInterval << "." << statName 
              << ".actWaits " << actWaits << std::endl
              << "i" << psInterval << "." << statName 
              << ".actWaits.totalTime " << actWaitTime << std::endl
              << "i" << psInterval << "." << statName << ".actWaits.averageTime " 
              << (double)((double)actWaitTime / (double)actWaits) << std::endl;

    psInterval++;
}


bool Bank::Idle( )
{
    if( nextPrecharge <= GetEventQueue()->GetCurrentCycle() 
            && nextActivate <= GetEventQueue()->GetCurrentCycle()
            && nextRead <= GetEventQueue()->GetCurrentCycle() 
            && nextWrite <= GetEventQueue()->GetCurrentCycle()
            && ( state == BANK_CLOSED || state == BANK_OPEN ) )
    {
        return true;
    }

    return false;
}

void Bank::Cycle( ncycle_t steps )
{
    /* Count cycle numbers for each state */
    if( state == BANK_PDPF || state == BANK_PDA )
        feCycles += steps;
    else if( state == BANK_PDPS )
        seCycles += steps;
    else if( state == BANK_OPEN )
        activeCycles += steps;
    else if( state == BANK_CLOSED )
        standbyCycles += steps;
}
