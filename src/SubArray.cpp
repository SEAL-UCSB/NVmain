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
#include "Endurance/NullModel/NullModel.h"
#include "Endurance/Distributions/Normal.h"
#include "DataEncoders/DataEncoderFactory.h"

#include <signal.h>
#include <cassert>
#include <iostream>
#include <limits>

/*
 * Using -O3 in gcc causes the popcount methods to return incorrect values.
 * Disable optimizations in these methods using compiler specific attributes.
 */
#if defined(__clang__)
#define NO_OPT __attribute__((optnone))
#else
#define NO_OPT __attribute__((optimize("0")))
#endif

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

    endrModel = NULL;
    dataEncoder = NULL;

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
        {
            endrModel->SetConfig( conf, createChildren );
            endrModel->SetStats( GetStats( ) );
        }

        dataEncoder = DataEncoderFactory::CreateNewDataEncoder( p->DataEncoder );
        if( dataEncoder )
        {
            dataEncoder->SetConfig( conf, createChildren );
            dataEncoder->SetStats( GetStats( ) );
        }
    }
}

void SubArray::RegisterStats( )
{
    if( endrModel )
    {
        endrModel->RegisterStats( );
    }

    if( dataEncoder )
    {
        dataEncoder->RegisterStats( );
    }

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
                    GetEventQueue()->GetCurrentCycle() + p->tRCD );

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

    /* Any additional latency for data encoding. */
    ncycles_t decLat = (dataEncoder ? dataEncoder->Read( request ) : 0);

    /* Update timing constraints */
    if( request->type == READ_PRECHARGE )
    {
        nextActivate = MAX( nextActivate, 
                            GetEventQueue()->GetCurrentCycle()
                                + MAX( p->tBURST, p->tCCD ) * (request->burstCount - 1)
                                + p->tAL + p->tRTP + p->tRP + decLat );

        nextPrecharge = MAX( nextPrecharge, nextActivate );
        nextRead = MAX( nextRead, nextActivate );
        nextWrite = MAX( nextWrite, nextActivate );

        NVMainRequest *preReq = new NVMainRequest( );
        *preReq = *request;
        preReq->owner = this;

        /* insert the event to issue the implicit precharge */ 
        GetEventQueue( )->InsertEvent( EventResponse, this, preReq, 
                        GetEventQueue()->GetCurrentCycle() + p->tAL + p->tRTP + decLat
                        + MAX( p->tBURST, p->tCCD ) * (request->burstCount - 1) );
    }
    else
    {
        nextPrecharge = MAX( nextPrecharge, 
                             GetEventQueue()->GetCurrentCycle() 
                                 + MAX( p->tBURST, p->tCCD ) * (request->burstCount - 1)
                                 + p->tAL + p->tBURST + p->tRTP - p->tCCD + decLat );

        nextRead = MAX( nextRead, 
                        GetEventQueue()->GetCurrentCycle() 
                            + MAX( p->tBURST, p->tCCD ) * request->burstCount );

        nextWrite = MAX( nextWrite, 
                         GetEventQueue()->GetCurrentCycle() 
                             + MAX( p->tBURST, p->tCCD ) * (request->burstCount  - 1)
                             + p->tCAS + p->tBURST + p->tRTRS - p->tCWD + decLat );
    }

    /* Read->Powerdown is typical the same for READ and READ_PRECHARGE. */
    nextPowerDown = MAX( nextPowerDown,
                         GetEventQueue()->GetCurrentCycle()
                            + MAX( p->tBURST, p->tCCD ) * (request->burstCount  - 1)
                            + p->tCAS + p->tAL + p->tBURST + 1 + decLat );

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
            GetEventQueue()->GetCurrentCycle() + p->tCAS + decLat );

    /* Notify owner of read completion as well */
    GetEventQueue( )->InsertEvent( EventResponse, this, request, 
            GetEventQueue()->GetCurrentCycle() + p->tCAS + p->tBURST + decLat );


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
                    request->address.GetPhysicalAddress( ), NULL ) )
        {
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
    ncycle_t encLat = 0, endrLat = 0;
    ncounter_t numUnchangedBits = 0;

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

    if( writeMode == WRITE_THROUGH )
    {
        encLat = (dataEncoder ? dataEncoder->Write( request ) : 0);
        endrLat = UpdateEndurance( request );

        /* Count the number of bits modified. */
        if( !p->WriteAllBits )
        {
            uint8_t *bitCountData = new uint8_t[request->data.GetSize()];

            for( uint64_t bitCountByte = 0; bitCountByte < request->data.GetSize(); bitCountByte++ )
            {
                bitCountData[bitCountByte] = request->data.GetByte( bitCountByte )
                                           ^ request->oldData.GetByte( bitCountByte );
            }

            ncounter_t bitCountWords = request->data.GetSize()/4;

            ncounter_t numChangedBits = CountBitsMLC1( 1, (uint32_t*)bitCountData, bitCountWords );

            assert( request->data.GetSize()*8 >= numChangedBits );
            numUnchangedBits = request->data.GetSize()*8 - numChangedBits;
        }
    }

    /* Determine the write time. */
    writeTimer = WriteCellData( request ); // Assume write-through.
    if( request->flags & NVMainRequest::FLAG_PAUSED ) // This was paused, restart with remaining time
    {
        writeTimer = request->writeProgress;
        request->flags &= ~NVMainRequest::FLAG_PAUSED; // unpause this
    }

    if( request->flags & NVMainRequest::FLAG_CANCELLED )
    {
        request->flags &= ~NVMainRequest::FLAG_CANCELLED; // restart this
    }

    if( writeMode == WRITE_BACK && writeCycle )
    {
        writeTimer = 0;

        NVMainRequest *requestCopy = new NVMainRequest( );
        *requestCopy = *request;
        writeBackRequests.push_back( requestCopy );
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
        writeTimer += encLat + endrLat;

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
        subArrayEnergy += p->Ewr - p->Ewrpb * numUnchangedBits;

        burstEnergy += p->Ewr;
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

        ncycle_t encLat = 0;
        ncycle_t endrLat = 0;

        std::vector<NVMainRequest *>::iterator wit;
        for( wit = writeBackRequests.begin(); wit != writeBackRequests.end(); wit++ )
        {
            encLat += (dataEncoder ? dataEncoder->Write( *wit ) : 0);
            endrLat += UpdateEndurance( *wit );
        }
        writeBackRequests.clear( );

        writeTimer += encLat + endrLat;

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
    else if( (state != SUBARRAY_CLOSED && p->UsePrecharge) )
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
    writeIterationStarts.clear( );
    uint32_t *rawData = reinterpret_cast<uint32_t*>(request->data.rawData);
    unsigned int memoryWordSize = static_cast<unsigned int>(p->tBURST * p->RATE * p->BusWidth);
    unsigned int writeBytes32 = memoryWordSize / 32;

    if( p->UniformWrites )
    {
        if( p->MLCLevels > 1 )
        {
            for( ncounter_t iter = 0; iter < averageWriteIterations; iter++ )
            {
                ncycle_t iterStart = GetEventQueue( )->GetCurrentCycle( );
                iterStart += iter * static_cast<ncycle_t>(p->tWP / averageWriteIterations);
                writeIterationStarts.insert( iterStart );
            }
        }

        /* Since we are skipping MLC checks below, we need to calculate the energy here. */
        ncounter_t writeCount0;
        ncounter_t writeCount1;

        if( rawData )
        {
            writeCount0 = CountBitsMLC1( 0, rawData, writeBytes32 );
            writeCount1 = CountBitsMLC1( 1, rawData, writeBytes32 );
        }
        else
        {
            /* Assume uniformly random data if we don't have data. */
            writeCount0 = 256;
            writeCount1 = 256;
        }

        if( p->EnergyModel != "current" )
        {
            subArrayEnergy += p->Ereset * writeCount0;
            subArrayEnergy += p->Eset * writeCount1;
            writeEnergy += p->Ereset * writeCount0;
            writeEnergy += p->Eset * writeCount1;
        }

        return p->tWP;
    }

    ncycle_t maxDelay = 0;

    /* No data... assume all 0. */
    if( !rawData )
        return p->tWP0;

    /* Check the data for the worst-case write time. */
    if( p->MLCLevels == 1 )
    {
        ncounter_t writeCount0 = CountBitsMLC1( 0, rawData, writeBytes32 );
        ncounter_t writeCount1 = CountBitsMLC1( 1, rawData, writeBytes32 );

        if( p->EnergyModel != "current" )
        {
            subArrayEnergy += p->Ereset * writeCount0;
            subArrayEnergy += p->Eset * writeCount1;
            writeEnergy += p->Ereset * writeCount0;
            writeEnergy += p->Eset * writeCount1;
        }

        ncycle_t delay0 = (writeCount0 == 0) ? 0 : p->tWP0;
        ncycle_t delay1 = (writeCount1 == 0) ? 0 : p->tWP1;

        maxDelay = MAX(delay0, delay1);
    }
    else if( p->MLCLevels == 2 )
    {
        ncounter_t writeCount00 = CountBitsMLC2( 0, rawData, writeBytes32 );
        ncounter_t writeCount01 = CountBitsMLC2( 1, rawData, writeBytes32 );
        ncounter_t writeCount10 = CountBitsMLC2( 2, rawData, writeBytes32 );
        ncounter_t writeCount11 = CountBitsMLC2( 3, rawData, writeBytes32 );

        assert( (writeCount00 + writeCount01 + writeCount10 + writeCount11)
                == (memoryWordSize/2) );

        /* 
         *  Naive scheduling -- Assume we have enough write drivers for all the data.
         *  Then, simply choose the max write pulse time as the delay value.
         *
         *  TODO -- Tetris the write pulses based on number of write drivers.
         */
        ncycle_t oncePulseDelay = 0;
        ncycle_t repeatPulseDelay = 0;

        ncycle_t delay00 = (writeCount00 == 0) ? 0 : p->nWP00;
        ncycle_t delay01 = (writeCount01 == 0) ? 0 : p->nWP01;
        ncycle_t delay10 = (writeCount10 == 0) ? 0 : p->nWP10;
        ncycle_t delay11 = (writeCount11 == 0) ? 0 : p->nWP11;

        ncycle_t programPulseCount = MAX( MAX( delay00, delay01 ),
                                          MAX( delay10, delay11 ) );
        ncycle_t thisPulseCount = 0;

        /* Randomize the pulse count for intermediate states. */
        if( writeCount01 || writeCount10 )
        {
            /* Inhibit weird outlier numbers. Using max stddev = 3 */
            ncounters_t max_stddev = p->WPMaxVariance;
            ncounters_t maxPulseCount = max_stddev + programPulseCount;
            ncounters_t minPulseCount = programPulseCount - max_stddev;
            if( minPulseCount < 0 ) minPulseCount = 0;

            NormalDistribution norm;

            norm.SetMean( programPulseCount );
            norm.SetVariance( p->WPVariance );

            thisPulseCount = norm.GetEndurance( );

            if( thisPulseCount > static_cast<ncycle_t>(maxPulseCount) )
                thisPulseCount = static_cast<ncycle_t>(maxPulseCount);
            if( thisPulseCount < static_cast<ncycle_t>(minPulseCount) )
                thisPulseCount = static_cast<ncycle_t>(minPulseCount);

            if( p->programMode == ProgramMode_SRMS )
            {
                oncePulseDelay = p->tWP0;
                repeatPulseDelay = p->tWP1;
            }
            else // SSMR
            {
                oncePulseDelay = p->tWP1;
                repeatPulseDelay = p->tWP0;
            }
        }
        else if( writeCount00 && !writeCount11 )
        {
            oncePulseDelay = p->tWP0;
        }
        else if( !writeCount00 && writeCount11 )
        {
            oncePulseDelay = p->tWP1;
        }
        else
        {
            oncePulseDelay = MAX(p->tWP0, p->tWP1);
        }

        /* Insert times for write cancellation and pausing. */
        ncycle_t iterStart = GetEventQueue( )->GetCurrentCycle( );
        writeIterationStarts.insert( iterStart );
        iterStart += oncePulseDelay;

        for( ncycle_t iter = 0; iter < thisPulseCount; iter++ )
        {
            writeIterationStarts.insert( iterStart );
            iterStart += repeatPulseDelay;
        }

        maxDelay = oncePulseDelay + thisPulseCount * repeatPulseDelay;

        if( mlcTimingMap.count( maxDelay ) > 0 )
            mlcTimingMap[maxDelay]++;
        else
            mlcTimingMap[maxDelay] = 1;

        if( maxDelay > worstCaseWrite )
            worstCaseWrite = maxDelay;
    }

    return maxDelay;
}

ncycle_t SubArray::NextIssuable( NVMainRequest *request )
{
    ncycle_t nextCompare = 0;

    if( request->type == ACTIVATE ) nextCompare = nextActivate;
    else if( request->type == READ ) nextCompare = nextRead;
    else if( request->type == WRITE ) nextCompare = nextWrite;
    else if( request->type == PRECHARGE ) nextCompare = nextPrecharge;
        
    // Should have no children
    return nextCompare;
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
            || (state != SUBARRAY_CLOSED && p->UsePrecharge) ) /* or, the subarray is not idle */
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
                /* Note: If precharge is not used, we still assume that
                 * the state is open. This is because there may or may not
                 * be some form of buffered output, which would presumably
                 * hold the previously sensed data after a refresh command.
                 */
                if( p->UsePrecharge )
                {
                    state = SUBARRAY_CLOSED;
                    openRow = p->ROWS;
                }
                else
                {
                    /* The memory controller will not update effectiveRow
                     * if precharge is not used. To hold the assumption of
                     * buffered output, we should leave the openRow the same.
                     * If this assumption is broken, the refresh logic in
                     * the memory controller must reset the effective row
                     * when a refresh is pushed to the command queue.
                     */
                    state = SUBARRAY_OPEN;
                }
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
    {
        return GetParent( )->RequestComplete( req );
    }
}

ncycle_t SubArray::UpdateEndurance( NVMainRequest *request )
{
    ncycle_t latency = 0;

    if( endrModel )
    {
        /* Don't track data for NullModel. */
        if( dynamic_cast<NullModel *>(endrModel) != NULL )
        {
            return latency;
        }

        NVMDataBlock oldData;

        if( conf->GetSimInterface( ) != NULL )
        {
            /* If the old data is not there, we will assume the data is 0.*/
            uint64_t wordSize;
            bool hardError;
            ncycles_t extraLatency;

            wordSize = p->BusWidth;
            wordSize *= p->tBURST * p->RATE;
            wordSize /= 8;

            if( request->oldData.IsValid( ) )
            {
                oldData = request->oldData;
            }
            else if( !request->oldData.IsValid( ) &&
                     !conf->GetSimInterface( )-> GetDataAtAddress( 
                        request->address.GetPhysicalAddress( ), &oldData ) )
            {
                for( uint64_t i = 0; i < wordSize; i++ )
                  oldData.SetByte( i, 0 );
            }
        
            /* Write the new data... */
            conf->GetSimInterface( )->SetDataAtAddress( 
                    request->address.GetPhysicalAddress( ), request->data );
    
            /* Model the endurance */
            hardError = false;
            
            extraLatency = endrModel->Write( request, oldData );
            if( extraLatency < 0 )
            {
                extraLatency = -extraLatency;
                extraLatency--; // We can't return -0 for error, but if we want an error with 0 latency, we need to +1 all latencies
                hardError = true;
            }

            latency = static_cast<ncycle_t>(extraLatency);

            if( hardError )
            {
                // TODO: Get extra latency from fault model
                // latency += ...;
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

    return latency;
}

SubArrayState SubArray::GetState( ) 
{
    return state;
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

/*
 *  Count the appearances for "value" for 2-bit MLC where value
 *  can be 0 (binary 00), 1 (binary 01), 2 (binary 10) or 3
 *  (binary 11).
 */
ncounter_t NO_OPT SubArray::Count32MLC2( uint8_t value, uint32_t data )
{
    /*
     *  This method counts the number of 11 pairs, so we need to convert
     *  the bit pattern we are looking for to be 11.
     */
    if( value == 0 )
        data ^= 0xFFFFFFFF;
    else if( value == 1 )
        data ^= 0xAAAAAAAA;
    else if( value == 2 )
        data ^= 0x55555555;

    assert( value < 4 );

    /* 
     *  Count number of "11" pairs by ANDing together every-event bit
     *  with every odd bit. The resulting ANDed value will be 1 if both
     *  even and odd bit are 1 (i.e. "11") and 0 otherwise.
     */
    uint32_t count = (data & 0x55555555) & ((data & 0xAAAAAAAA) >> 1);

    return Count32MLC1(count);
}


ncounter_t NO_OPT SubArray::CountBitsMLC2( uint8_t value, uint32_t *data, ncounter_t words )
{
    ncounter_t count = 0;

    for( ncounter_t i = 0; i < words; i++ )
    {
        count += Count32MLC2( value, data[i] );
    }

    return count;
}


ncounter_t NO_OPT SubArray::Count32MLC1( uint32_t data )
{
    /*
     *  Count the number of ones in this value using some
     *  bit-manipulation magic.
     */
    uint32_t count = data;
    count = count - ((count >> 1) & 0x55555555);
    count = (count & 0x33333333) + ((count >> 2) & 0x33333333);
    count = (((count + (count >> 4)) & 0x0f0f0f0f) * 0x01010101) >> 24;

    return static_cast<ncounter_t>(count);
}


ncounter_t NO_OPT SubArray::CountBitsMLC1( uint8_t value, uint32_t *data, ncounter_t words )
{
    ncounter_t count = 0;

    for( ncounter_t i = 0; i < words; i++ )
    {
        count += Count32MLC1( data[i] );
    }

    count = (value == 1) ? count : (words*32 - count);

    return count;
}
