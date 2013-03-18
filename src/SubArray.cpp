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
*   Tao Zhang       ( Email: tzz106 at cse dot psu dot edu
*                     Website: http://www.cse.psu.edu/~tzz106 )
*******************************************************************************/

#include "src/SubArray.h"
#include "src/MemoryController.h"
#include "src/EventQueue.h"
#include "Endurance/EnduranceModelFactory.h"

#include <signal.h>
#include <cassert>
#include <iostream>
#include <limits>

using namespace NVM;

SubArray::SubArray( )
{
    conf = NULL;

    nextActivate = 0;
    nextPrecharge = 0;
    nextRead = 0;
    nextWrite = 0;
    nextCommand = CMD_NOP;

    state = SUBARRAY_CLOSED;
    lastActivate = 0;
    openRow = 0;

    subArrayEnergy = 0.0f;
    backgroundEnergy = 0.0f;
    activeEnergy = 0.0f;
    burstEnergy = 0.0f;
    refreshEnergy = 0.0f;

    subArrayPower = 0.0f;
    backgroundPower = 0.0f;
    activePower = 0.0f;
    burstPower = 0.0f;
    refreshPower = 0.0f;

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

    subArrayId = -1;

    psInterval = 0;
}

SubArray::~SubArray( )
{
}

void SubArray::SetConfig( Config *c )
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

    /* We need to create an endurance model at a sub-array level */
    endrModel = EnduranceModelFactory::CreateEnduranceModel( p->EnduranceModel );
    if( endrModel )
        endrModel->SetConfig( conf );
}

/*
 * Activate() open a row 
 */
bool SubArray::Activate( NVMainRequest *request )
{
    uint64_t activateRow;

    request->address.GetTranslatedAddress( &activateRow, NULL, NULL, NULL, NULL );

    /* TODO: Can we remove this sanity check and totally trust IsIssuable()? */
    /* sanity check */
    if( nextActivate > GetEventQueue()->GetCurrentCycle() )
    {
        std::cerr << "NVMain Error: SubArray violates ACTIVATION timing constraint!"
            << std::endl;
        return false;
    }
    else if( state != SUBARRAY_CLOSED )
    {
        std::cerr << "NVMain Error: try to open a subarray that is not idle!"
            << std::endl;
        return false;
    }

    /* Update timing constraints */
    nextPrecharge = MAX( nextPrecharge, 
                         GetEventQueue()->GetCurrentCycle() 
                             + MAX( p->tRCD, p->tRAS ) );

    nextRead = MAX( nextRead, 
                    GetEventQueue()->GetCurrentCycle() 
                        + p->tRCD - p->tAL );

    nextWrite = MAX( nextWrite, 
                     GetEventQueue()->GetCurrentCycle() 
                         + p->tRCD - p->tAL );

    GetEventQueue( )->InsertEvent( EventResponse, this, request, 
                    GetEventQueue()->GetCurrentCycle() + p->tRCD );

    /* 
     * We simplify the row record here. The absolute row number is record
     * rather than the relative row number within the subarray
     */
    openRow = activateRow;

    state = SUBARRAY_OPEN;
    writeCycle = false;

    lastActivate = GetEventQueue()->GetCurrentCycle();

    /* Add to bank's total energy. */
    if( p->EnergyModel_set && p->EnergyModel == "current" )
    {
        /* DRAM Model */
        uint64_t tRC = p->tRAS + p->tRCD;

        subArrayEnergy += ( (float)((p->EIDD0 * (float)tRC) 
                    - ((p->EIDD3N * (float)p->tRAS)
                    +  (p->EIDD2N * (float)p->tRP))) );

        activeEnergy += ( (float)((p->EIDD0 * (float)tRC) 
                      - ((p->EIDD3N * (float)p->tRAS)
                      +  (p->EIDD2N * (float)p->tRP) )) );
    }
    else
    {
        /* Flat energy model. */
        subArrayEnergy += p->Erd;
        activeEnergy += p->Erd;
    }

    activates++;

    return true;
}

/*
 * Read() fulfills the column read function
 */
bool SubArray::Read( NVMainRequest *request )
{
    uint64_t readRow, readCol;

    request->address.GetTranslatedAddress( &readRow, &readCol, NULL, NULL, NULL );

    /* TODO: Can we remove this sanity check and totally trust IsIssuable()? */
    /* sanity check */
    if( nextRead > GetEventQueue()->GetCurrentCycle() )
    {
        std::cerr << "NVMain Error: Subarray violates READ timing constraint!"
            << std::endl;
        return false;
    }
    else if( state != SUBARRAY_OPEN )
    {
        std::cerr << "NVMain Error: try to read a subarray that is not active!"
            << std::endl;
        return false;
    }
    else if( readRow != openRow )
    {
        std::cerr << "NVMain Error: try to read a row that is not opened in a subarray!"
            << std::endl;
        return false;
    }

    /* Update timing constraints */
    if( request->type == READ_PRECHARGE )
    {
        nextActivate = MAX( nextActivate, 
                            GetEventQueue()->GetCurrentCycle()
                                + p->tAL + p->tRTP + p->tRP );

        nextPrecharge = MAX( nextPrecharge, nextActivate );
        nextRead = MAX( nextRead, nextActivate );
        nextWrite = MAX( nextWrite, nextActivate );

        /* close the subarray */
        state = SUBARRAY_CLOSED;
        openRow = p->ROWS;

        precharges++;
    }
    else
    {
        nextPrecharge = MAX( nextPrecharge, 
                             GetEventQueue()->GetCurrentCycle() 
                                 + p->tAL + p->tBURST + p->tRTP - p->tCCD );

        nextRead = MAX( nextRead, 
                        GetEventQueue()->GetCurrentCycle() 
                            + MAX( p->tBURST, p->tCCD ) );

        nextWrite = MAX( nextWrite, 
                         GetEventQueue()->GetCurrentCycle() 
                             + p->tCAS + p->tBURST + p->tRTRS - p->tCWD );
    }

    /*
     *  Data is placed on the bus starting from tCAS and is complete after tBURST.
     *  Wakeup owner at the end of this to notify that the whole request is complete.
     *
     *  Note: In critical word first, tBURST can be replaced with 1.
     */
    /* Issue a bus burst request when the burst starts. */
    NVMainRequest *busReq = new NVMainRequest( );
    *busReq = *request;
    busReq->type = BUS_WRITE;
    busReq->owner = this;

    GetEventQueue( )->InsertEvent( EventResponse, this, busReq, 
            GetEventQueue()->GetCurrentCycle() + p->tCAS );

    /* Notify owner of read completion as well */
    GetEventQueue( )->InsertEvent( EventResponse, this, request, 
            GetEventQueue()->GetCurrentCycle() + p->tCAS + p->tBURST );


    /* Calculate energy */
    if( p->EnergyModel_set && p->EnergyModel == "current" )
    {
        /* DRAM Model */
        subArrayEnergy += (float)((p->EIDD4R - p->EIDD3N) * (float)p->tBURST);

        burstEnergy += (float)((p->EIDD4R - p->EIDD3N) * (float)p->tBURST);
    }
    else
    {
        /* Flat Energy Model */
        subArrayEnergy += p->Eopenrd;

        burstEnergy += p->Eopenrd;
    }

    /*
     *  There's no reason to track data if endurance is not modeled.
     */
    if( conf->GetSimInterface( ) != NULL && endrModel != NULL )
    {
        NVMDataBlock dataBlock;

        /*
         *  In a trace-based simulation, or a live simulation where simulation is
         *  started in the middle of execution, some data being read maybe have never
         *  been written to memory. In this case, we will store the value, since the
         *  data is always correct from either the simulator or the value in the trace,
         *  which corresponds to the actual value that was read. 
         *
         *  Since the read data is already in the request, we don't actually need to
         *  copy it there.
         */
        if( !conf->GetSimInterface( )->GetDataAtAddress( 
                    request->address.GetPhysicalAddress( ), &dataBlock ) )
        {
            //std::cout << "Setting data block for 0x" << std::hex << request->address.GetPhysicalAddress( )
            //          << std::dec << " to " << request->data << std::endl;
            conf->GetSimInterface( )->SetDataAtAddress( 
                    request->address.GetPhysicalAddress( ), request->data );
        }
    }

    reads++;
    dataCycles += p->tBURST;
    
    return true;
}

/*
 * Write() fulfills the column write function
 */
bool SubArray::Write( NVMainRequest *request )
{
    uint64_t writeRow, writeCol;

    request->address.GetTranslatedAddress( &writeRow, &writeCol, NULL, NULL, NULL );

    /* TODO: Can we remove this sanity check and totally trust IsIssuable()? */
    /* sanity check */
    if( nextWrite > GetEventQueue()->GetCurrentCycle() )
    {
        std::cerr << "NVMain Error: Subarray violates WRITE timing constraint!"
            << std::endl;
        return false;
    }
    else if( state != SUBARRAY_OPEN )
    {
        std::cerr << "NVMain Error: try to write a subarray that is not active!"
            << std::endl;
        return false;
    }
    else if( writeRow != openRow )
    {
        std::cerr << "NVMain Error: try to write a row that is not opened "
            << "in a subarray!" << std::endl;
        return false;
    }

    /* Update timing constraints */
    if( request->type == WRITE_PRECHARGE )
    {
        nextActivate = MAX( nextActivate, 
                            GetEventQueue()->GetCurrentCycle()
                                + p->tAL + p->tCWD + p->tBURST 
                                + p->tWR + p->tRP );

        nextPrecharge = MAX( nextPrecharge, nextActivate );
        nextRead = MAX( nextRead, nextActivate );
        nextWrite = MAX( nextWrite, nextActivate );

        /* close the subarray */
        state = SUBARRAY_CLOSED;
        openRow = p->ROWS;

        precharges++;
    }
    else
    {
        nextPrecharge = MAX( nextPrecharge, 
                             GetEventQueue()->GetCurrentCycle() 
                                 + p->tAL + p->tCWD + p->tBURST + p->tWR );

        nextRead = MAX( nextRead, 
                        GetEventQueue()->GetCurrentCycle() 
                            + p->tCWD + p->tBURST + p->tWTR );

        nextWrite = MAX( nextWrite, 
                         GetEventQueue()->GetCurrentCycle() 
                             + MAX( p->tBURST, p->tCCD ) );
    }

    /* Issue a bus burst request when the burst starts. */
    NVMainRequest *busReq = new NVMainRequest( );
    *busReq = *request;
    busReq->type = BUS_READ;
    busReq->owner = this;

    GetEventQueue( )->InsertEvent( EventResponse, this, busReq, 
            GetEventQueue()->GetCurrentCycle() + p->tCAS );

    /* Notify owner of write completion as well */
    GetEventQueue( )->InsertEvent( EventResponse, this, request, 
            GetEventQueue()->GetCurrentCycle() + p->tCWD + p->tBURST );

    /* Calculate energy. */
    if( p->EnergyModel_set && p->EnergyModel == "current" )
    {
        /* DRAM Model. */
        subArrayEnergy += (float)((p->EIDD4W - p->EIDD3N) * (float)p->tBURST);

        burstEnergy += (float)((p->EIDD4W - p->EIDD3N) * (float)p->tBURST);
    }
    else
    {
        /* Flat energy model. */
        subArrayEnergy += p->Ewr; // - p->Ewrpb * numUnchangedBits;

        burstEnergy += p->Ewr; // - p->Ewrpb * numUnchangedBits;
    }

    writeCycle = true;

    writes++;
    dataCycles += p->tBURST;
    
    if( endrModel )
    {
        NVMDataBlock oldData;

        if( conf->GetSimInterface( ) != NULL )
        {
            /* If the old data is not there, we will assume the data is 0.*/
            uint64_t wordSize;
            uint64_t blockMask;
            uint64_t blockAddr;
            bool hardError;

            wordSize = p->BusWidth;
            wordSize *= p->tBURST * p->RATE;
            wordSize /= 8;

            blockMask = ~(wordSize - 1);

            blockAddr = request->address.GetPhysicalAddress( ) & blockMask;

            if( !conf->GetSimInterface( )-> GetDataAtAddress( 
                        request->address.GetPhysicalAddress( ), &oldData ) )
            {
                for( int i = 0; i < (int)(p->BusWidth / 8); i++ )
                  oldData.SetByte( i, 0 );
            }
        
            /* Write the new data... */
            conf->GetSimInterface( )->SetDataAtAddress( 
                    request->address.GetPhysicalAddress( ), request->data );
    
            /* Model the endurance */
            hardError = !endrModel->Write( request->address, oldData, 
                                            request->data );

            if( hardError )
            {
                std::cout << "WARNING: Write to 0x" << std::hex 
                    << request->address.GetPhysicalAddress( )
                    << std::dec << " resulted in a hard error! " << std::endl;
            }
        }
        else
        {
            std::cerr << "NVMain Error: Endurance modeled without simulator "
                << "interface for data tracking!" << std::endl;
        }
    }

    return true;
}

/*
 * Precharge() close a row and force the bank back to SUBARRAY_CLOSED
 */
bool SubArray::Precharge( NVMainRequest *request )
{
    /* TODO: Can we remove this sanity check and totally trust IsIssuable()? */
    /* sanity check */
    if( nextPrecharge > GetEventQueue()->GetCurrentCycle() )
    {
        std::cerr << "NVMain Error: SubArray violates PRECHARGE timing constraint!"
            << std::endl;
        return false;
    }
    else if( state != SUBARRAY_CLOSED && state != SUBARRAY_OPEN )
    {
        std::cerr << "NVMain Error: try to precharge a subarray that is neither " 
            << "idle nor active" << std::endl;
        return false;
    }

    /* Update timing constraints */
    nextActivate = MAX( nextActivate, 
                        GetEventQueue()->GetCurrentCycle() + p->tRP );

    GetEventQueue( )->InsertEvent( EventResponse, this, request, 
              GetEventQueue()->GetCurrentCycle() + p->tRP );

    /* close the subarray */
    state = SUBARRAY_CLOSED;
    openRow = p->ROWS;

    precharges++;

    return true;
}

/* 
 * Refresh() is treated as an ACT and can only be issued when the subarray 
 * is idle 
 */
bool SubArray::Refresh( )
{
    /* TODO: Can we remove this sanity check and totally trust IsIssuable()? */
    /* sanity check */
    if( nextActivate > GetEventQueue()->GetCurrentCycle() )
    {
        std::cerr << "NVMain Error: SubArray violates REFRESH timing constraint!"
            << std::endl;
        return false;
    }
    else if( state != SUBARRAY_CLOSED )
    {
        std::cerr << "NVMain Error: try to refresh a subarray that is not idle " 
            << std::endl;
        return false;
    }

    /* Update timing constraints */
    nextActivate = MAX( nextActivate, 
                        GetEventQueue()->GetCurrentCycle() + p->tRFC );

    /* the subarray is still idle */
    state = SUBARRAY_CLOSED;
    openRow = p->ROWS;

    if( p->EnergyModel_set && p->EnergyModel == "current" )
    {
        subArrayEnergy += (float)((p->EIDD5B - p->EIDD3N) * (float)p->tRFC ); 

        refreshEnergy += (float)((p->EIDD5B - p->EIDD3N) * (float)p->tRFC ); 
    }
    else
    {
        subArrayEnergy += p->Eref;

        refreshEnergy += p->Eref;
    }

    refreshes++;

    return true;
}

/*
 * IsIssuable() tells whether one request satisfies the timing constraints
 */
bool SubArray::IsIssuable( NVMainRequest *req, FailReason *reason )
{
    uint64_t opRow;
    bool rv = true;

    req->address.GetTranslatedAddress( &opRow, NULL, NULL, NULL, NULL );

    if( nextCommand != CMD_NOP )
        return false;
      

    if( req->type == ACTIVATE )
    {
        if( nextActivate > (GetEventQueue()->GetCurrentCycle()) /* if it is too early to open */
            || state != SUBARRAY_CLOSED )   /* or, the subarray is not idle */
        {
            rv = false;
            if( reason ) 
                reason->reason = SUBARRAY_TIMING;
        }

        if( rv == false )
        {
            /* if it is too early to open the subarray */
            if( nextActivate > (GetEventQueue()->GetCurrentCycle()) )
            {
                actWaits++;
                actWaitTime += nextActivate - (GetEventQueue()->GetCurrentCycle() );
            }
        }
    }
    else if( req->type == READ || req->type == READ_PRECHARGE )
    {
        if( nextRead > (GetEventQueue()->GetCurrentCycle()) /* if it is too early to read */
            || state != SUBARRAY_OPEN  /* or, the subarray is not active */
            || opRow != openRow )      /* or, the target row is not the open row */
        {
            rv = false;
            if( reason ) 
                reason->reason = SUBARRAY_TIMING;
        }
    }
    else if( req->type == WRITE || req->type == WRITE_PRECHARGE )
    {
        if( nextWrite > (GetEventQueue()->GetCurrentCycle()) /* if it is too early to write */
            || state != SUBARRAY_OPEN  /* or, the subarray is not active */          
            || opRow != openRow )      /* or, the target row is not the open row */
        {
            rv = false;
            if( reason ) 
                reason->reason = SUBARRAY_TIMING;
        }
    }
    else if( req->type == PRECHARGE || req->type == PRECHARGE_ALL )
    {
        if( nextPrecharge > (GetEventQueue()->GetCurrentCycle()) /* if it is too early to precharge */ 
            || ( state != SUBARRAY_OPEN           /* or the subbary is neither active nor idle */
                 && state != SUBARRAY_CLOSED ) )
        {
            rv = false;
            if( reason ) 
                reason->reason = SUBARRAY_TIMING;
        }
    }
    else if( req->type == REFRESH )
    {
        if( nextActivate > ( GetEventQueue()->GetCurrentCycle() ) /* if it is too early to refresh */ 
            || state != SUBARRAY_CLOSED ) /* or, the subarray is not idle */
        {
            rv = false;
            if( reason )
              reason->reason = SUBARRAY_TIMING;
        }
    }
    else
    {
        std::cout << "SubArray: IsIssuable: Unknown operation: " << req->type 
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
bool SubArray::IssueCommand( NVMainRequest *req )
{
    bool rv = false;

    if( !IsIssuable( req ) )
    {
        std::cerr << "NVMain Error: Command " << req->type << " can not be " 
            << "issued in the subarray!" << std::endl;
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

            default:
                std::cerr << "NVMain Error : subarray detects unknown operation "
                    << "in command queue! " << req->type << std::endl;
                break;  
        }
    }

    return rv;
}

bool SubArray::WouldConflict( uint64_t checkRow )
{
    bool returnValue = true;

    if( state == SUBARRAY_OPEN && checkRow == openRow )
        returnValue = false;

    return returnValue;
}

SubArrayState SubArray::GetState( ) 
{
    return state;
}

void SubArray::GetEnergy( float& total, float& background, float& active, 
                          float& burst, float& refresh )
{
    total = subArrayEnergy;
    background = backgroundEnergy;
    active = activeEnergy;
    burst = burstEnergy;
    refresh = refreshEnergy;
}


void SubArray::GetPower( float& total, float& background, float& active, 
                          float& burst, float& refresh )
{
    CalculatePower( );

    total = subArrayPower;
    background = backgroundPower;
    active = activePower;
    burst = burstPower;
    refresh = refreshPower;
}

void SubArray::CalculatePower( )
{
    float simulationTime = (float)((float)GetEventQueue()->GetCurrentCycle() 
                                / ((float)p->CLK * 1000000.0f));

    if( simulationTime == 0.0f )
    {
        subArrayPower 
            = backgroundPower 
            = activePower 
            = burstPower 
            = refreshPower = 0.0f;
        return;
    }

    // TODO: Move this somewhere else in case we need other variables
    backgroundEnergy = p->Eleak * simulationTime;

    backgroundPower = 
        ((backgroundEnergy / (float)GetEventQueue()->GetCurrentCycle()) 
        * p->Voltage) / 1000.0f;
    
    activePower = 
        ((activeEnergy / (float)GetEventQueue()->GetCurrentCycle()) 
        * p->Voltage) / 1000.0f;

    burstPower = 
        ((burstEnergy / (float)GetEventQueue()->GetCurrentCycle()) 
        * p->Voltage) / 1000.0f;
    
    refreshPower = 
        ((refreshEnergy / (float)GetEventQueue()->GetCurrentCycle()) 
        * p->Voltage) / 1000.0f;

    subArrayPower = ( backgroundPower + activePower + burstPower + refreshPower );
}

void SubArray::SetName( std::string )
{
}

/* 
 * Corresponds to physical subarray id 
 */
void SubArray::SetId( ncounter_t id )
{
    subArrayId = id;
}

std::string SubArray::GetName( )
{
    return "";
}


ncounter_t SubArray::GetId( )
{
    return subArrayId;
}

void SubArray::PrintStats( )
{
    float idealBandwidth;

    idealBandwidth = (float)(p->CLK * p->MULT * p->RATE * p->BPC);

    CalculatePower( );

    if( p->EnergyModel_set && p->EnergyModel == "current" )
    {
        std::cout << "i" << psInterval << "." << statName << ".current " << subArrayEnergy << "\t; mA" << std::endl;
        std::cout << "i" << psInterval << "." << statName << ".current.background " << backgroundEnergy << "\t; mA" << std::endl;
        std::cout << "i" << psInterval << "." << statName << ".current.active " << activeEnergy << "\t; mA" << std::endl;
        std::cout << "i" << psInterval << "." << statName << ".current.burst " << burstEnergy << "\t; mA" << std::endl;
        std::cout << "i" << psInterval << "." << statName << ".current.refresh " << refreshEnergy << "\t; mA" << std::endl;
    }
    else
    {
        std::cout << "i" << psInterval << "." << statName << ".energy " << subArrayEnergy << "\t; nJ" << std::endl; 
        std::cout << "i" << psInterval << "." << statName << ".energy.background " << backgroundEnergy << "\t; nJ" << std::endl;
        std::cout << "i" << psInterval << "." << statName << ".energy.active " << activeEnergy << "\t; nJ" << std::endl;
        std::cout << "i" << psInterval << "." << statName << ".energy.burst " << burstEnergy << "\t; nJ" << std::endl;
        std::cout << "i" << psInterval << "." << statName << ".energy.refresh " << refreshEnergy << "\t; nJ" << std::endl;
    }
    
    std::cout << "i" << psInterval << "." << statName << ".power " << subArrayPower << "\t; W " << std::endl
              << "i" << psInterval << "." << statName << ".power.background " << backgroundPower << "\t; nJ" << std::endl
              << "i" << psInterval << "." << statName << ".power.active " << activePower << "\t; nJ" << std::endl
              << "i" << psInterval << "." << statName << ".power.burst " << burstPower << "\t; nJ" << std::endl
              << "i" << psInterval << "." << statName << ".power.refresh " << refreshPower << "\t; nJ" << std::endl;

    std::cout << "i" << psInterval << "." << statName << ".reads " << reads << std::endl
              << "i" << psInterval << "." << statName << ".writes " << writes << std::endl
              << "i" << psInterval << "." << statName << ".activates " << activates << std::endl
              << "i" << psInterval << "." << statName << ".precharges " << precharges << std::endl
              << "i" << psInterval << "." << statName << ".refreshes " << refreshes << std::endl;

    if( endrModel )
    {
        if( endrModel->GetWorstLife( ) == std::numeric_limits< uint64_t >::max( ) )
          std::cout << "i" << psInterval << "." << statName << ".worstCaseEndurance N/A" << std::endl
                    << "i" << psInterval << "." << statName << ".averageEndurance N/A" << std::endl;
        else
          std::cout << "i" << psInterval << "." << statName << ".worstCaseEndurance " 
                    << (endrModel->GetWorstLife( )) << std::endl
                    << "i" << psInterval << "." << statName << ".averageEndurance " 
                    << endrModel->GetAverageLife( ) << std::endl;

        endrModel->PrintStats( );
    }

    std::cout << "i" << psInterval << "." << statName << ".actWaits " << actWaits << std::endl
              << "i" << psInterval << "." << statName << ".actWaits.totalTime " << actWaitTime << std::endl
              << "i" << psInterval << "." << statName << ".actWaits.averageTime " 
              << (double)((double)actWaitTime / (double)actWaits) << std::endl;

    psInterval++;
}

bool SubArray::Idle( )
{
    if( nextPrecharge <= GetEventQueue()->GetCurrentCycle() 
            && nextActivate <= GetEventQueue()->GetCurrentCycle()
            && nextRead <= GetEventQueue()->GetCurrentCycle() 
            && nextWrite <= GetEventQueue()->GetCurrentCycle()
            && ( state == SUBARRAY_CLOSED || state == SUBARRAY_OPEN ) )
    {
        return true;
    }

    return false;
}

void SubArray::Cycle( ncycle_t )
{
}
