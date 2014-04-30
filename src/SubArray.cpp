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
*   Tao Zhang       ( Email: tzz106 at cse dot psu dot edu
*                     Website: http://www.cse.psu.edu/~tzz106 )
*******************************************************************************/

#include "src/SubArray.h"
#include "src/Bank.h"
#include "src/MemoryController.h"
#include "src/EventQueue.h"
#include "include/NVMHelpers.h"
#include "Endurance/EnduranceModelFactory.h"
#include "Endurance/Distributions/Normal.h"

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
    nextPowerDown = 0;
    nextCommand = CMD_NOP;

    state = SUBARRAY_CLOSED;
    lastActivate = 0;
    openRow = 0;

    nextActivate = 0;
    nextPrecharge = 0;
    nextRead = 0;
    nextWrite = 0;
    
    dataCycles = 0;
    worstCaseWrite = 0;

    subArrayEnergy = 0.0f;
    activeEnergy = 0.0f;
    burstEnergy = 0.0f;
    writeEnergy = 0.0f;
    refreshEnergy = 0.0f;

    writeCycle = false;
    writeMode = WRITE_THROUGH;
    isWriting = false;
    writeEnd = 0;
    writeStart = 0;
    writeEventTime = 0;
    writeEvent = NULL;
    writeRequest = NULL;
    nextActivatePreWrite = 0;
    nextPrechargePreWrite = 0;
    nextReadPreWrite = 0;
    nextWritePreWrite = 0;
    nextPowerDownPreWrite = 0;
    idleTimer = 0;

    cancelledWrites = 0;
    cancelledWriteTime = 0;
    pausedWrites = 0;

    averagePausesPerRequest = 0.0;
    measuredPauses = 0;

    averagePausedRequestProgress = 0.0;
    measuredProgresses = 0;

    reads = 0;
    writes = 0;
    activates = 0;
    precharges = 0;
    refreshes = 0;

    actWaits = 0;
    actWaitTotal = 0;
    actWaitAverage = 0.0;

    num00Writes = 0;
    num01Writes = 0;
    num10Writes = 0;
    num11Writes = 0;
    mlcTimingHisto = "";
    cancelCountHisto = "";
    wpPauseHisto = "";
    wpCancelHisto = "";
    averageWriteTime = 0.0;
    measuredWriteTimes = 0;
    averageWriteIterations = 1;

    subArrayId = -1;

    psInterval = 0;
}

SubArray::~SubArray( )
{
}

void SubArray::SetConfig( Config *c, bool createChildren )
{
    conf = c;

    Params *params = new Params( );
    params->SetParams( c );
    SetParams( params );

    MATHeight = p->MATHeight;
    /* customize MAT size */
    if( conf->KeyExists( "MATWidth" ) )
        MATWidth = static_cast<ncounter_t>( conf->GetValue( "MATWidth" ) );

    /* Configure the write mode. */
    if( conf->KeyExists( "WriteMode" ) )
    {
        if( conf->GetString( "WriteMode" ) == "WriteThrough" )
        {
            writeMode = WRITE_THROUGH;
        }
        else if( conf->GetString( "WriteMode" ) == "WriteBack" )
        {
            writeMode = WRITE_BACK;
        }
        else
        {
            std::cout << "NVMain Warning: Unknown write mode `"
                      << conf->GetString( "WriteMode" )
                      << "'. Defaulting to WriteThrough" << std::endl;
            writeMode = WRITE_THROUGH;
        }
    }

    ncounter_t totalWritePulses = p->nWP00 + p->nWP01 + p->nWP10 + p->nWP11;
    averageWriteIterations = static_cast<ncounter_t>( (totalWritePulses+2)/4 );

    if( createChildren )
    {
        /* We need to create an endurance model at a sub-array level */
        endrModel = EnduranceModelFactory::CreateEnduranceModel( p->EnduranceModel );
        if( endrModel )
            endrModel->SetConfig( conf, createChildren );
    }
}

void SubArray::RegisterStats( )
{
    if( p->EnergyModel == "current" )
    {
        AddUnitStat(subArrayEnergy, "mA*t");
        AddUnitStat(activeEnergy, "mA*t");
        AddUnitStat(burstEnergy, "mA*t");
        AddUnitStat(refreshEnergy, "mA*t");
    }
    else
    {
        AddUnitStat(subArrayEnergy, "nJ");
        AddUnitStat(activeEnergy, "nJ");
        AddUnitStat(burstEnergy, "nJ");
        AddUnitStat(writeEnergy, "nJ");
        AddUnitStat(refreshEnergy, "nJ");
    }

    AddStat(cancelledWrites);
    AddStat(cancelledWriteTime);
    AddStat(pausedWrites);

    AddStat(averagePausesPerRequest);
    AddStat(measuredPauses);

    AddStat(averagePausedRequestProgress);
    AddStat(measuredProgresses);

    AddStat(reads);
    AddStat(writes);
    AddStat(activates);
    AddStat(precharges);
    AddStat(refreshes);

    if( endrModel )
    {
        AddStat(worstCaseEndurance);
        AddStat(averageEndurance);
    }

    AddStat(actWaits);
    AddStat(actWaitTotal);
    AddStat(actWaitAverage);

    AddStat(worstCaseWrite);
    AddStat(num00Writes);
    AddStat(num01Writes);
    AddStat(num10Writes);
    AddStat(num11Writes);
    AddStat(averageWriteTime);
    AddStat(measuredWriteTimes);

    AddStat(mlcTimingHisto);
    AddStat(cancelCountHisto);
    AddStat(wpPauseHisto);
    AddStat(wpCancelHisto);
}

/*
 * Activate() open a row 
 */
bool SubArray::Activate( NVMainRequest *request )
{
    uint64_t activateRow;

    request->address.GetTranslatedAddress( &activateRow, NULL, NULL, NULL, NULL, NULL );

    /* Check if we need to cancel or pause a write to service this request. */
    CheckWritePausing( );

    /* TODO: Can we remove this sanity check and totally trust IsIssuable()? */
    /* sanity check */
    if( nextActivate > GetEventQueue()->GetCurrentCycle() )
    {
        std::cerr << "NVMain Error: SubArray violates ACTIVATION timing constraint!"
            << std::endl;
        return false;
    }
    else if( p->UsePrecharge && state != SUBARRAY_CLOSED )
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

    nextPowerDown = MAX( nextPowerDown, 
                         GetEventQueue()->GetCurrentCycle() 
                             + MAX( p->tRCD, p->tRAS ) );

    /* the request is deleted by RequestComplete() */
    request->owner = this;
    GetEventQueue( )->InsertEvent( EventResponse, this, request, 
                    GetEventQueue()->GetCurrentCycle() + MAX( p->tRCD, p->tRAS ) );

    /* 
     * The relative row number is record rather than the absolute row number 
     * within the subarray
     */
    openRow = activateRow;

    state = SUBARRAY_OPEN;
    writeCycle = false;

    lastActivate = GetEventQueue()->GetCurrentCycle();

    /* Add to bank's total energy. */
    if( p->EnergyModel == "current" )
    {
        /* DRAM Model */
        ncycle_t tRC = p->tRAS + p->tRP;

        subArrayEnergy += ( ( p->EIDD0 * (double)tRC ) 
                    - ( ( p->EIDD3N * (double)(p->tRAS) )
                    +  ( p->EIDD2N * (double)(p->tRP) ) ) ) / (double)(p->BANKS);

        activeEnergy += ( ( p->EIDD0 * (double)tRC ) 
                      - ( ( p->EIDD3N * (double)(p->tRAS) )
                      +  ( p->EIDD2N * (double)(p->tRP) ) ) ) / (double)(p->BANKS);
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
    uint64_t readRow;

    request->address.GetTranslatedAddress( &readRow, NULL, NULL, NULL, NULL, NULL );

    /* Check if we need to cancel or pause a write to service this request. */
    CheckWritePausing( );

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
                                + MAX( p->tBURST, p->tCCD ) * (request->burstCount - 1)
                                + p->tAL + p->tRTP + p->tRP );

        nextPrecharge = MAX( nextPrecharge, nextActivate );
        nextRead = MAX( nextRead, nextActivate );
        nextWrite = MAX( nextWrite, nextActivate );

        NVMainRequest *preReq = new NVMainRequest( );
        *preReq = *request;
        preReq->owner = this;

        /* insert the event to issue the implicit precharge */ 
        GetEventQueue( )->InsertEvent( EventResponse, this, preReq, 
                        GetEventQueue()->GetCurrentCycle() + p->tAL + p->tRTP 
                        + MAX( p->tBURST, p->tCCD ) * (request->burstCount - 1) );
    }
    else
    {
        nextPrecharge = MAX( nextPrecharge, 
                             GetEventQueue()->GetCurrentCycle() 
                                 + MAX( p->tBURST, p->tCCD ) * (request->burstCount - 1)
                                 + p->tAL + p->tBURST + p->tRTP - p->tCCD );

        nextRead = MAX( nextRead, 
                        GetEventQueue()->GetCurrentCycle() 
                            + MAX( p->tBURST, p->tCCD ) * request->burstCount );

        nextWrite = MAX( nextWrite, 
                         GetEventQueue()->GetCurrentCycle() 
                             + MAX( p->tBURST, p->tCCD ) * (request->burstCount  - 1)
                             + p->tCAS + p->tBURST + p->tRTRS - p->tCWD );
    }

    /* Read->Powerdown is typical the same for READ and READ_PRECHARGE. */
    nextPowerDown = MAX( nextPowerDown,
                         GetEventQueue()->GetCurrentCycle()
                            + MAX( p->tBURST, p->tCCD ) * (request->burstCount  - 1)
                            + p->tCAS + p->tAL + p->tBURST + 1 );

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
    if( p->EnergyModel == "current" )
    {
        /* DRAM Model */
        subArrayEnergy += ( ( p->EIDD4R - p->EIDD3N ) * (double)(p->tBURST) ) / (double)(p->BANKS);

        burstEnergy += ( ( p->EIDD4R - p->EIDD3N ) * (double)(p->tBURST) ) / (double)(p->BANKS);
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
    uint64_t writeRow;
    ncycle_t writeTimer;

    request->address.GetTranslatedAddress( &writeRow, NULL, NULL, NULL, NULL, NULL );

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

    /* Determine the write time. */
    writeTimer = WriteCellData( request ); // Assume write-through.
    if( request->flags & NVMainRequest::FLAG_PAUSED ) // This was paused, restart with remaining time
    {
        writeTimer = request->writeProgress;
        request->flags &= ~NVMainRequest::FLAG_PAUSED; // unpause this

        //std::cout << "Restarted request 0x" << std::hex << request->address.GetPhysicalAddress( )
        //          << std::dec << " with " << writeTimer << " cycles left." << std::endl;
    }
    else
    {
        writeTimer = WriteCellData( request ); // Assume write-through.
    }

    if( request->flags & NVMainRequest::FLAG_CANCELLED )
    {
        request->flags &= ~NVMainRequest::FLAG_CANCELLED; // restart this

        //std::cout << "Restarted CANCELLED request 0x" << std::hex << request->address.GetPhysicalAddress( )
        //          << std::dec << "... " << request->cancellations << " cancels so far" << std::endl;
    }

    if( writeMode == WRITE_BACK && writeCycle )
    {
        writeTimer = 0;
    }

    /* Write canceling/pausing only for write-through memory */
    if( writeMode == WRITE_THROUGH )
        request->writeProgress = writeTimer;

    /* Save the next* state incause we cancel a write. */
    nextActivatePreWrite = nextActivate;
    nextPrechargePreWrite = nextPrecharge;
    nextReadPreWrite = nextRead;
    nextWritePreWrite = nextWrite;
    nextPowerDownPreWrite = nextPowerDown;

    if( writeMode == WRITE_THROUGH )
    {
        averageWriteTime = ((averageWriteTime * static_cast<double>(measuredWriteTimes)) + static_cast<double>(writeTimer)) 
                         / (static_cast<double>(measuredWriteTimes) + 1.0);
        measuredWriteTimes++;
    }

    /* Update timing constraints */
    if( request->type == WRITE_PRECHARGE )
    {
        nextActivate = MAX( nextActivate, 
                            GetEventQueue()->GetCurrentCycle()
                            + MAX( p->tBURST, p->tCCD ) * (request->burstCount - 1)
                            + p->tAL + p->tCWD + p->tBURST 
                            + writeTimer + p->tWR + p->tRP );

        nextPrecharge = MAX( nextPrecharge, nextActivate );
        nextRead = MAX( nextRead, nextActivate );
        nextWrite = MAX( nextWrite, nextActivate );

        /* close the subarray */
        NVMainRequest *preReq = new NVMainRequest( );
        *preReq = *request;
        preReq->owner = this;

        /* insert the event to issue the implicit precharge */ 
        GetEventQueue( )->InsertEvent( EventResponse, this, preReq, 
            GetEventQueue()->GetCurrentCycle() 
            + MAX( p->tBURST, p->tCCD ) * (request->burstCount - 1)
            + p->tAL + p->tCWD + p->tBURST + writeTimer + p->tWR );
    }
    else
    {
        nextPrecharge = MAX( nextPrecharge, 
                             GetEventQueue()->GetCurrentCycle() 
                             + MAX( p->tBURST, p->tCCD ) * (request->burstCount - 1)
                             + p->tAL + p->tCWD + p->tBURST + writeTimer + p->tWR );

        nextRead = MAX( nextRead, 
                        GetEventQueue()->GetCurrentCycle() 
                        + MAX( p->tBURST, p->tCCD ) * (request->burstCount - 1)
                        + p->tCWD + p->tBURST + p->tWTR + writeTimer );

        nextWrite = MAX( nextWrite, 
                         GetEventQueue()->GetCurrentCycle() 
                         + MAX( p->tBURST, p->tCCD ) * request->burstCount + writeTimer );
    }

    nextPowerDown = MAX( nextPowerDown, nextPrecharge );

    /* Mark that a write is in progress in cause we want to pause/cancel. */
    isWriting = true;
    writeRequest = request;
    // TODO: Should we disallow pausing during the data burst?
    writeStart = GetEventQueue()->GetCurrentCycle();
    writeEnd = GetEventQueue()->GetCurrentCycle() + writeTimer;
    writeEventTime = GetEventQueue()->GetCurrentCycle() + p->tCWD 
                     + MAX( p->tBURST, p->tCCD ) * request->burstCount + writeTimer;

    //std::cout << GetEventQueue()->GetCurrentCycle() << " write start 0x" << std::hex
    //          << request->address.GetPhysicalAddress( ) << std::dec << " done at "
    //          << writeEnd << std::endl;

    /* The parent has our hook in the children list, we need to find this. */
    std::vector<NVMObject_hook *>& children = GetParent( )->GetTrampoline( )->GetChildren( );
    std::vector<NVMObject_hook *>::iterator it;
    NVMObject_hook *hook = NULL;

    for( it = children.begin(); it != children.end(); it++ )
    {
        if( (*it)->GetTrampoline() == this )
        {
            hook = (*it);
            break;
        }
    }

    assert( hook != NULL );

    writeEvent = new NVM::Event( );
    writeEvent->SetType( EventResponse );
    writeEvent->SetRecipient( hook );
    writeEvent->SetRequest( request );

    /* Issue a bus burst request when the burst starts. */
    NVMainRequest *busReq = new NVMainRequest( );
    *busReq = *request;
    busReq->type = BUS_READ;
    busReq->owner = this;

    GetEventQueue( )->InsertEvent( EventResponse, this, busReq, 
            GetEventQueue()->GetCurrentCycle() + p->tCWD );

    /* Notify owner of write completion as well */
    GetEventQueue( )->InsertEvent( writeEvent, writeEventTime );

    /* Calculate energy. */
    if( p->EnergyModel == "current" )
    {
        /* DRAM Model. */
        subArrayEnergy += ( ( p->EIDD4W - p->EIDD3N ) * (double)(p->tBURST) ) / (double)(p->BANKS);

        burstEnergy += ( ( p->EIDD4W - p->EIDD3N ) * (double)(p->tBURST) ) / (double)(p->BANKS);
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
    
    return true;
}

/*
 * Precharge() close a row and force the bank back to SUBARRAY_CLOSED
 */
bool SubArray::Precharge( NVMainRequest *request )
{
    ncycle_t writeTimer;

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
    writeTimer = MAX( 1, p->tRP ); // Assume write-through. Needs to be at least one due to event callback.
    if( writeMode == WRITE_BACK && writeCycle )
    {
        writeTimer = MAX( 1, p->tRP + WriteCellData( request ) );

        averageWriteTime = ((averageWriteTime * static_cast<double>(measuredWriteTimes)) + static_cast<double>(writeTimer)) 
                         / (static_cast<double>(measuredWriteTimes) + 1.0);
        measuredWriteTimes++;
    }

    nextActivate = MAX( nextActivate, 
                        GetEventQueue()->GetCurrentCycle() + writeTimer );

    /* the request is deleted by RequestComplete() */
    request->owner = this;
    GetEventQueue( )->InsertEvent( EventResponse, this, request, 
              GetEventQueue()->GetCurrentCycle() + writeTimer );

    /* set the subarray under precharging */
    state = SUBARRAY_PRECHARGING;

    return true;
}

/* 
 * Refresh() is treated as an ACT and can only be issued when the subarray 
 * is idle 
 */
bool SubArray::Refresh( NVMainRequest* request )
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

    /* 
     *  Copies of refresh requests are made at the rank level (in case of multi-bank refresh).
     *  We'll claim ownership and delete the copy that was allocated in the rank.
     */
    request->owner = this;
    GetEventQueue( )->InsertEvent( EventResponse, this, request, 
              GetEventQueue()->GetCurrentCycle() + p->tRFC );

    /* set the subarray under refreshing */
    state = SUBARRAY_REFRESHING;

    if( p->EnergyModel == "current" )
    {
        /* calibrate the refresh energy since we may have fine-grained refresh */
        subArrayEnergy += ( ( p->EIDD5B - p->EIDD3N ) 
                                * (double)(p->tRFC) / (double)(p->BANKS) ); 

        refreshEnergy += ( ( p->EIDD5B - p->EIDD3N ) 
                                * (double)p->tRFC / (double)(p->BANKS) ); 

        
    }
    else
    {
        subArrayEnergy += p->Eref;

        refreshEnergy += p->Eref;
    }

    return true;
}

void SubArray::CheckWritePausing( )
{
    if( p->WritePausing && isWriting )
    {
        /* Optimal write progress; no issues pausing at any time. */
        ncycle_t writeProgress = writeEnd - GetEventQueue()->GetCurrentCycle();
        ncycle_t writeTimer = writeEnd - writeStart;

        /* 
         *  Realistically, we need to cancel the current iteration and go back to
         *  the start of the previous one
         */
        if( p->pauseMode != PauseMode_Optimal )
        {
            ncycle_t nextIterationStart = writeStart;
            std::set<ncycle_t>::iterator iter;

            for( iter = writeIterationStarts.begin(); iter != writeIterationStarts.end(); iter++ )
            {
                if( (*iter) > GetEventQueue()->GetCurrentCycle() )
                {
                    break;
                }
                
                nextIterationStart = (*iter);
            }

            writeProgress = writeEnd - nextIterationStart;
        }

        /* Update write progress. */
        writeRequest->writeProgress = writeProgress;
        float writePercent = 1.0f - (static_cast<float>(writeProgress) / static_cast<float>(writeTimer));

        averagePausedRequestProgress = ((averagePausedRequestProgress * static_cast<double>(measuredProgresses)) 
                                     + writePercent) / static_cast<double>(measuredProgresses + 1);
        measuredProgresses++;

        //std::cout << "GOT A READ DURING WRITE; PROGRESS IS " << writePercent << std::endl;

        /* Pause after 40%, cancel otherwise. */
        if( writePercent > p->PauseThreshold )
        {
            /* If optimal is paused on last iteration, it's done. */
            if( writeProgress != writeEnd )
            {
                writeRequest->flags |= NVMainRequest::FLAG_PAUSED;
                pausedWrites++;
            }

            if( wpPauseMap.count( writePercent ) )
                wpPauseMap[writePercent]++;
            else
                wpPauseMap[writePercent] = 1;
        }
        else
        {
            writeRequest->flags |= NVMainRequest::FLAG_CANCELLED;

            /* Force writes to be completed after several cancellations to ensure forward progress. */
            if( ++writeRequest->cancellations >= p->MaxCancellations )
                writeRequest->flags |= NVMainRequest::FLAG_FORCED;

            cancelledWrites++;
            cancelledWriteTime += GetEventQueue()->GetCurrentCycle() - writeStart;

            if( wpCancelMap.count( writePercent ) )
                wpCancelMap[writePercent]++;
            else
                wpCancelMap[writePercent] = 1;
        }

        /* Delete the old event indicating write completion. */
        GetEventQueue( )->RemoveEvent( writeEvent, writeEventTime );
        delete writeEvent;
        writeEvent = NULL;

        /* Return this write as paused/cancelled. */
        GetEventQueue( )->InsertEvent( EventResponse, this, writeRequest,
                                    GetEventQueue()->GetCurrentCycle() + 1 );

        /* Restore the old next* state. */
        nextActivate = nextActivatePreWrite;
        nextPrecharge = nextPrechargePreWrite;
        nextWrite = nextWritePreWrite;
        nextRead = nextReadPreWrite;
        nextPowerDown = nextPowerDownPreWrite;
    }
}

bool SubArray::BetweenWriteIterations( )
{
    bool rv = false;

    if( isWriting && writeIterationStarts.count( GetEventQueue()->GetCurrentCycle() ) != 0 )
    {
        rv = true;
    }

    return rv;
}

ncycle_t SubArray::WriteCellData( NVMainRequest *request )
{
    if( p->UniformWrites )
    {
        writeIterationStarts.clear( );

        for( ncounter_t iter = 0; iter < averageWriteIterations; iter++ )
        {
            ncycle_t iterStart = GetEventQueue( )->GetCurrentCycle( );
            iterStart += iter * static_cast<ncycle_t>(p->tWP / averageWriteIterations);
            writeIterationStarts.insert( iterStart );
        }

        return p->tWP;
    }

    ncycle_t maxDelay = 0;

    Bank *parentBank = dynamic_cast<Bank*>(GetParent( )->GetTrampoline( ));
    assert( parentBank != NULL );

    ncounter_t parentBankId = parentBank->GetId( );
    unsigned int memoryWordSize = static_cast<unsigned int>(p->tBURST * p->RATE * p->BusWidth);
    unsigned int deviceCount = static_cast<unsigned int>(p->BusWidth / p->DeviceWidth);
    unsigned int writeSize = memoryWordSize / deviceCount;
    unsigned int writeBytes = writeSize / 8;

    /* Assume that data written is not interleaved over devices. */
    unsigned int offset = writeBytes * static_cast<unsigned int>(parentBankId);
    std::deque<uint8_t> writeBits;

    /* Get each byte, then push back each bit to the writeBits vector. */
    for( unsigned int byteIdx = 0; byteIdx < writeBytes; byteIdx++ )
    {
        uint8_t curByte = request->data.GetByte( byteIdx + offset );

        for( unsigned int bitIdx = 0; bitIdx < 8; bitIdx++ )
        {
            writeBits.push_back( ((curByte & 0x80) ? 1 : 0) );
            curByte = curByte << 1;
        }
    }

    /* Based on the MLC level count, get this many bits at once. */
    unsigned int writeCount = writeSize / static_cast<unsigned int>(p->MLCLevels);

    for( unsigned int writeIdx = 0; writeIdx < writeCount; writeIdx++ )
    {
        unsigned int cellData = 0;

        for( unsigned int bitIdx = 0; bitIdx < p->MLCLevels; bitIdx++ )
        {
            cellData <<= 1;
            cellData |= writeBits.front( );
            writeBits.pop_front( );
        }

        /* Get the delay and add the energy. Assume one-RESET-multiple-SET */
        ncycle_t writePulseTime = 0;
        ncounters_t programPulseCount = 0;

        if( p->MLCLevels == 1 )
        {
            if( cellData == 0 )
                writePulseTime = p->tWP0;
            else
                writePulseTime = p->tWP1;

            writeIterationStarts.insert( GetEventQueue( )->GetCurrentCycle( ) );
        }
        else if( p->MLCLevels == 2 )
        {
            ncounters_t max_stddev = p->WPMaxVariance;

            if( cellData == 0 )
            {
                programPulseCount = 0;
                num00Writes++;
                max_stddev = 0; // assume single RESET does not fail
            }
            else if( cellData == 1 ) // 01 -> Assume 1 RESET + nWP01 SETs
            {
                programPulseCount = p->nWP01;
                num01Writes++;
            }
            else if( cellData == 2 ) // 10 -> Assume 1 RESET + nWP1 SETs
            {
                programPulseCount = p->nWP10;
                num10Writes++;
            }
            else if( cellData == 3 ) // 11 -> Assume 1 RESET + nWP11 SETs
            {
                programPulseCount = p->nWP11;
                num11Writes++;
                max_stddev = 0; // assume single SET does not fail
            }
            else
                std::cout << "SubArray: Unknown cell value: " << (int)cellData << std::endl;

            /* Simulate program and verify failures */
            if( programPulseCount > 0 )
            {
                /* Inhibit weird outlier numbers. Using max stddev = 3 */
                ncounters_t maxPulseCount = max_stddev + programPulseCount;
                ncounters_t minPulseCount = programPulseCount - max_stddev;

                NormalDistribution norm;

                norm.SetMean( programPulseCount );
                norm.SetVariance( p->WPVariance );

                programPulseCount = norm.GetEndurance( );

                /* Make sure this is at least one. */
                if( programPulseCount < 1 ) programPulseCount = 1;

                if( programPulseCount > maxPulseCount )
                    programPulseCount = maxPulseCount;
                if( programPulseCount < minPulseCount )
                    programPulseCount = minPulseCount;

                if( p->programMode == ProgramMode_SRMS )
                {
                    writeIterationStarts.clear( );
                    ncycle_t iterStart = GetEventQueue( )->GetCurrentCycle( );

                    writePulseTime = p->tWP0 + programPulseCount * p->tWP1;
                    writeIterationStarts.insert( iterStart );
                    iterStart += p->tWP0;

                    for( ncounters_t iter = 0; iter < programPulseCount; iter++ )
                    {
                        writeIterationStarts.insert( iterStart );
                        iterStart += p->tWP1;
                    }
                }
                else // SSMR
                {
                    writeIterationStarts.clear( );
                    ncycle_t iterStart = GetEventQueue( )->GetCurrentCycle( );

                    writePulseTime = p->tWP1 + programPulseCount * p->tWP0;
                    writeIterationStarts.insert( iterStart );
                    iterStart += p->tWP1;

                    for( ncounters_t iter = 0; iter < programPulseCount; iter++ )
                    {
                        writeIterationStarts.insert( iterStart );
                        iterStart += p->tWP0;
                    }
                }
            }
            else
            {
                writeIterationStarts.clear( );
                ncycle_t iterStart = GetEventQueue( )->GetCurrentCycle( );
                writeIterationStarts.insert( iterStart );

                writePulseTime = p->tWP0;
            }

            /* Only calculate energy for energy-mode model. */
            if( p->EnergyModel != "current" )
            {
                subArrayEnergy += p->Ereset + static_cast<double>(programPulseCount) * p->Eset;
                writeEnergy += p->Ereset + static_cast<double>(programPulseCount) * p->Eset;
            }
        }

        /* See if this writePulseTime is the longest. */
        if( writePulseTime > maxDelay )
            maxDelay = writePulseTime;
    }

    if( mlcTimingMap.count( maxDelay ) > 0 )
        mlcTimingMap[maxDelay]++;
    else
        mlcTimingMap[maxDelay] = 1;

    if( maxDelay > worstCaseWrite )
        worstCaseWrite = maxDelay;

    return maxDelay;
}

/*
 * IsIssuable() tells whether one request satisfies the timing constraints
 */
bool SubArray::IsIssuable( NVMainRequest *req, FailReason *reason )
{
    uint64_t opRow;
    bool rv = true;

    req->address.GetTranslatedAddress( &opRow, NULL, NULL, NULL, NULL, NULL );

    if( nextCommand != CMD_NOP )
        return false;

    if( req->type == ACTIVATE )
    {
        if( nextActivate > (GetEventQueue()->GetCurrentCycle()) /* if it is too early to open */
            || (p->UsePrecharge && state != SUBARRAY_CLOSED)   /* or, the subarray needs a precharge */
            || (p->WritePausing && isWriting && writeRequest->flags & NVMainRequest::FLAG_FORCED) /* or, write can't be paused. */
            || (p->WritePausing && isWriting && !(req->flags & NVMainRequest::FLAG_PRIORITY)) ) /* Prevent normal row buffer misses from pausing writes at odd times. */
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
                actWaitTotal += nextActivate - (GetEventQueue()->GetCurrentCycle() );
            }
        }
    }
    else if( req->type == READ || req->type == READ_PRECHARGE )
    {
        if( nextRead > (GetEventQueue()->GetCurrentCycle()) /* if it is too early to read */
            || state != SUBARRAY_OPEN  /* or, the subarray is not active */
            || opRow != openRow        /* or, the target row is not the open row */
            || ( p->WritePausing && isWriting && writeRequest->flags & NVMainRequest::FLAG_FORCED ) ) /* or, write can't be paused. */
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
    else if( req->type == POWERDOWN_PDA 
             || req->type == POWERDOWN_PDPF 
             || req->type == POWERDOWN_PDPS )
    {
        /* Bank doesn't know write time, so we need to check the subarray */
        if( nextPowerDown > (GetEventQueue()->GetCurrentCycle()) || isWriting )
        {
            rv = false;
            if( reason )
                reason->reason = SUBARRAY_TIMING;
        }
    }
    else if( req->type == POWERUP )
    {
        /* We assume subarray can always power up, as it is under bank control. */
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
        /* 
         *  Assume subarray is the end-point for requests. If we haven't found this
         *  request yet, it is not supported.
         */
        rv = false;
        if( reason ) 
            reason->reason = UNSUPPORTED_COMMAND;
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

            case REFRESH:
                rv = this->Refresh( req );
                break;

            default:
                std::cerr << "NVMain Error : subarray detects unknown operation "
                    << "in command queue! " << req->type << std::endl;
                break;  
        }
    }

    return rv;
}

bool SubArray::RequestComplete( NVMainRequest *req )
{
    if( req->type == WRITE || req->type == WRITE_PRECHARGE )
    {
        //std::cout << GetEventQueue()->GetCurrentCycle() << " write done 0x" << std::hex
        //          << req->address.GetPhysicalAddress( ) << std::dec << std::endl;

        /*  
         *  Write-to-write timing causes new writes to come in before the previous completes.
         *  Don't set writing to false unless this hasn't happened (writeRequest would be the
         *  same).
         */
        if( writeRequest == req )
        {
            isWriting = false;
        }

        if( !( req->flags & NVMainRequest::FLAG_PAUSED || req->flags & NVMainRequest::FLAG_CANCELLED ) )
        {
            averagePausesPerRequest = ((averagePausesPerRequest * static_cast<double>(measuredPauses))
                                    + req->cancellations) / static_cast<double>(measuredPauses + 1.0);
            measuredPauses++;

            if( cancelCountMap.count( req->cancellations ) == 0 )
                cancelCountMap[req->cancellations] = 0;
            else
                cancelCountMap[req->cancellations]++;
        }
    }

    if( req->owner == this )
    {
        switch( req->type )
        {
            /* may implement more functions in the future */
            case ACTIVATE:
            case READ:
            case WRITE:
                delete req;
                break;

            case READ_PRECHARGE:
            case WRITE_PRECHARGE:
                req->type = PRECHARGE;
                state = SUBARRAY_PRECHARGING;

                /* insert the implicit precharge */
                GetEventQueue( )->InsertEvent( EventResponse, this, req, 
                    GetEventQueue()->GetCurrentCycle() + p->tRP );
                break;

            case PRECHARGE:
            case PRECHARGE_ALL:
                /* close the subarray, increment the statistic number */
                state = SUBARRAY_CLOSED;
                openRow = p->ROWS;
                precharges++;
                delete req;
                break;

            case REFRESH:
                /* close the subarray, increment the statistic number */
                state = SUBARRAY_CLOSED;
                openRow = p->ROWS;
                refreshes++;
                delete req;
                break;

            default:
                delete req;
                break;
        }

        return true;
    }
    else
        return GetParent( )->RequestComplete( req );
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

void SubArray::GetEnergy( double& total, double& active, 
                          double& burst, double& refresh )
{
    total = subArrayEnergy;
    active = activeEnergy;
    burst = burstEnergy;
    refresh = refreshEnergy;
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

void SubArray::CalculateStats( )
{
    worstCaseEndurance = endrModel->GetWorstLife( );
    averageEndurance = endrModel->GetAverageLife( );

    actWaitAverage = static_cast<double>(actWaitTotal) / static_cast<double>(actWaits);

    /* Print a histogram as a python-style dict. */
    mlcTimingHisto = PyDictHistogram<uint64_t, uint64_t>( mlcTimingMap );
    cancelCountHisto = PyDictHistogram<uint64_t, uint64_t>( cancelCountMap );
    wpPauseHisto = PyDictHistogram<double, uint64_t>( wpPauseMap );
    wpCancelHisto = PyDictHistogram<double, uint64_t>( wpCancelMap );
}

bool SubArray::Idle( )
{
    return ( state == SUBARRAY_CLOSED || state == SUBARRAY_PRECHARGING );
}

void SubArray::Cycle( ncycle_t )
{
}

