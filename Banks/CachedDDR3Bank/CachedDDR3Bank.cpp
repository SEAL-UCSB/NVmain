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

#include "Banks/CachedDDR3Bank/CachedDDR3Bank.h"
#include "include/NVMHelpers.h"
#include "src/EventQueue.h"

#include <cassert>

using namespace NVM;


CachedDDR3Bank::CachedDDR3Bank( )
{
    cachedRowBuffer = NULL;
    readOnlyBuffers = true;
    rowBufferSize = 32;
    rowBufferCount = 4;
    
    inRDBCount = 0;
    RDBAllocations = 0;
    writebackCount = 0;
    RDBReads = 0;
    RDBWrites = 0;
    allocationReadsHisto = "";
    allocationWritesHisto = "";
}


CachedDDR3Bank::~CachedDDR3Bank( )
{
    if( cachedRowBuffer )
    {
        for( ncounter_t bufferIdx = 0; bufferIdx < rowBufferCount; bufferIdx++ )
        {
            delete cachedRowBuffer[bufferIdx]->dirty;
            delete cachedRowBuffer[bufferIdx];
        }

        delete [] cachedRowBuffer;
    }
}


void CachedDDR3Bank::SetConfig( Config *config, bool createChildren )
{
    // Assume entire row is cached if CachedRowSize is unset
    config->GetValueUL( "COLS", rowBufferSize );

    config->GetBool( "CachedRowsReadOnly", readOnlyBuffers );
    config->GetValueUL( "CachedRowSize", rowBufferSize );
    config->GetValueUL( "CachedRowCount", rowBufferCount );

    /* Initialize row buffers. */
    cachedRowBuffer = new CachedRowBuffer*[rowBufferCount];
    for( ncounter_t bufferIdx = 0; bufferIdx < rowBufferCount; bufferIdx++ )
    {
        cachedRowBuffer[bufferIdx] = new CachedRowBuffer;
        cachedRowBuffer[bufferIdx]->used = false;
        cachedRowBuffer[bufferIdx]->dirty = new bool[rowBufferSize];
        for( ncounter_t dirtyIdx = 0; dirtyIdx < rowBufferSize; dirtyIdx++ )
        {
            cachedRowBuffer[bufferIdx]->dirty[dirtyIdx] = false;
        }
    }

    DDR3Bank::SetConfig( config, createChildren );
}


bool CachedDDR3Bank::Activate( NVMainRequest *request )
{
    bool foundRDB = false;

    assert( nextActivate <= GetEventQueue()->GetCurrentCycle() );

    /* See if there is an available row buffer. */
    for( ncounter_t bufferIdx = 0; bufferIdx < rowBufferCount; bufferIdx++ )
    {
        /* Check if this row is already active. For read-only, we must activate to allow for write-through to bank. */
        if( cachedRowBuffer[bufferIdx]->used && 
            request->address.GetRow( ) == cachedRowBuffer[bufferIdx]->address.GetRow( ) )
        {
            foundRDB = true;
            break;
        }

        /* Look for unused buffers. Don't break so the assertion is checked. */
        if( !cachedRowBuffer[bufferIdx]->used && !foundRDB )
        {
            foundRDB = true;
            cachedRowBuffer[bufferIdx]->used = true;
            cachedRowBuffer[bufferIdx]->address = request->address;
            cachedRowBuffer[bufferIdx]->colStart = request->address.GetCol() 
                                                 - (request->address.GetCol() % rowBufferSize);
            cachedRowBuffer[bufferIdx]->colEnd = cachedRowBuffer[bufferIdx]->colStart + rowBufferSize;
            cachedRowBuffer[bufferIdx]->reads = 0;
            cachedRowBuffer[bufferIdx]->writes = 0;

            RDBAllocations++;

            //std::cout << statName << ": Buffer " << bufferIdx << " bound to address 0x" << std::hex
            //          << request->address.GetPhysicalAddress( ) << std::dec << " from col "
            //          << cachedRowBuffer[bufferIdx]->colStart << " to col "
            //          << cachedRowBuffer[bufferIdx]->colEnd << std::endl;

            break;
        }
    }

    /* If we haven't found an RDB, evict the LRU row. */
    ncounter_t dirtyCount = 0;

    if( !foundRDB )
    {
        ncounter_t bufferIdx = rowBufferCount - 1;

        if( allocationReadsMap.count( cachedRowBuffer[bufferIdx]->reads ) )
            allocationReadsMap[ cachedRowBuffer[bufferIdx]->reads ]++;
        else
            allocationReadsMap[ cachedRowBuffer[bufferIdx]->reads ] = 1;

        if( allocationWritesMap.count( cachedRowBuffer[bufferIdx]->writes ) )
            allocationWritesMap[ cachedRowBuffer[bufferIdx]->writes ]++;
        else
            allocationWritesMap[ cachedRowBuffer[bufferIdx]->writes ] = 1;

        cachedRowBuffer[bufferIdx]->used = true;
        cachedRowBuffer[bufferIdx]->address = request->address;
        cachedRowBuffer[bufferIdx]->colStart = request->address.GetCol() 
                                             - (request->address.GetCol() % rowBufferSize);
        cachedRowBuffer[bufferIdx]->colEnd = cachedRowBuffer[bufferIdx]->colStart + rowBufferSize;
        cachedRowBuffer[bufferIdx]->reads = 0;
        cachedRowBuffer[bufferIdx]->writes = 0;

        RDBAllocations++;

        //std::cout << statName << ": Buffer " << bufferIdx << " bound to address 0x" << std::hex
        //          << request->address.GetPhysicalAddress( ) << std::dec << " from col "
        //          << cachedRowBuffer[bufferIdx]->colStart << " to col "
        //          << cachedRowBuffer[bufferIdx]->colEnd << std::endl;

        for( ncounter_t dirtyIdx = 0; dirtyIdx < rowBufferSize; dirtyIdx++ )
        {
            if( cachedRowBuffer[bufferIdx]->dirty[dirtyIdx] )
                dirtyCount++;

            cachedRowBuffer[bufferIdx]->dirty[dirtyIdx] = false;
        }

        writebackCount += dirtyCount;
    }

    assert( !(readOnlyBuffers && dirtyCount > 0) );

    ncycle_t activateTimer = 0;

    /* If dirty, simulate writebacks. */
    // TODO: Check if row is already activated? It's probably not since LRU is evicted..
    if( dirtyCount > 0 )
    {
        activateTimer += p->tRCD;                  /* Time for extra activate */
        activateTimer -= p->tAL;                   /* Act -> Write time. */
        activateTimer += MAX( p->tBURST, p->tCCD ) /* Write time. */
                       * (dirtyCount - 1);
        activateTimer += p->tAL + p->tCWD          /* Write + Write -> Precharge time. */
                       + p->tBURST + p->tWR;
        activateTimer += (p->UsePrecharge          /* Precharge time. */
                          ? p->tRP : 0);
    }

    activateTimer += p->tRCD;                       /* The activate issued to this method. */
    activateTimer += rowBufferSize * p->tCCD;       /* The time to read the selected row region. */

    /* 
     * Update timing constraints.
     *
     * Assume we can write immediately after activate, and can read after one burst (Assumes 
     * trigger request is prioritized...) 
     */
    nextRead = MAX( nextRead, GetEventQueue()->GetCurrentCycle() + activateTimer - p->tAL - rowBufferSize * p->tCCD + p->tCCD );
    nextWrite = MAX( nextWrite, GetEventQueue()->GetCurrentCycle() + activateTimer - p->tAL - rowBufferSize * p->tCCD );
    /* Don't allow closing the row until the RDB is full. */
    nextPrecharge = MAX( nextPrecharge, GetEventQueue()->GetCurrentCycle() + MAX(activateTimer, p->tRAS) );
    nextPowerDown = MAX( nextPowerDown, GetEventQueue()->GetCurrentCycle() + MAX(activateTimer, p->tRAS) );

    /* Set the bank state. */
    ncounter_t activateRow, activateSubArray;
    request->address.GetTranslatedAddress( &activateRow, NULL, NULL, NULL, NULL, &activateSubArray );

    /* issue ACTIVATE to the target subarray */
    // TODO: Should we delay this in the case of writebacks for visualization purposes?
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


bool CachedDDR3Bank::Read( NVMainRequest *request )
{
    bool rv = false;

    /* Check if this is in the RDB. */
    for( ncounter_t bufferIdx = 0; bufferIdx < rowBufferCount; bufferIdx++ )
    {
        if( cachedRowBuffer[bufferIdx]->address.GetRow( ) == request->address.GetRow( )
            && request->address.GetCol( ) >= cachedRowBuffer[bufferIdx]->colStart
            && request->address.GetCol( ) <  cachedRowBuffer[bufferIdx]->colEnd )
        {
            rv = true;

            /* Only update read and write based on RDB timings; other commands will bypass RDB. */
            nextRead = MAX( nextRead, GetEventQueue()->GetCurrentCycle() + MAX( p->tBURST, p->tRDB ) );
            nextWrite = MAX( nextWrite, GetEventQueue()->GetCurrentCycle() + MAX( p->tBURST, p->tRDB ) + p->tRTRS );

            /* Assume the data is placed on the bus immediately after the command. */
            NVMainRequest *busReq = new NVMainRequest( );
            *busReq = *request;
            busReq->type = BUS_READ;
            busReq->owner = this;

            GetEventQueue( )->InsertEvent( EventResponse, this, busReq, 
                    GetEventQueue()->GetCurrentCycle() + 1 );

            /* Notify owner of read completion as well */
            GetEventQueue( )->InsertEvent( EventResponse, this, request, 
                    GetEventQueue()->GetCurrentCycle() + MAX( p->tBURST, p->tRDB ) );

            /* Swap the cached status back to normal. */
            request->type = (request->type == CACHED_READ ? READ : request->type);

            //std::cout << GetEventQueue()->GetCurrentCycle() << " " << statName 
            //          << ": Served read request 0x" << std::hex
            //          << request->address.GetPhysicalAddress() << std::dec << std::endl;
            
            RDBReads++;
            cachedRowBuffer[bufferIdx]->reads++;

            /* Move this buffer to LRU. */
            CachedRowBuffer *tempBuffer = cachedRowBuffer[bufferIdx];

            for( uint64_t j = bufferIdx; j > 0; j-- )
            {
                cachedRowBuffer[j] = cachedRowBuffer[j-1];
            }

            cachedRowBuffer[0] = tempBuffer;

            break;
        }
    }

    if( !rv )
    {
        rv = DDR3Bank::Read( request );
    }

    return rv;
}


bool CachedDDR3Bank::Write( NVMainRequest *request )
{
    bool rv = false;

    /* Check if this is in the RDB. */
    if( !readOnlyBuffers )
    {
        for( ncounter_t bufferIdx = 0; bufferIdx < rowBufferCount; bufferIdx++ )
        {
            if( cachedRowBuffer[bufferIdx]->address.GetRow( ) == request->address.GetRow( )
                && request->address.GetCol( ) >= cachedRowBuffer[bufferIdx]->colStart
                && request->address.GetCol( ) <  cachedRowBuffer[bufferIdx]->colEnd )
            {
                rv = true;

                /* Only update read and write based on RDB timings; other commands will bypass RDB. */
                nextRead = MAX( nextRead, GetEventQueue()->GetCurrentCycle() + MAX( p->tBURST, p->tRDB ) + p->tRTRS );
                nextWrite = MAX( nextWrite, GetEventQueue()->GetCurrentCycle() + MAX( p->tBURST, p->tRDB ) );

                /* Set this word to be dirty. */
                cachedRowBuffer[bufferIdx]->dirty[request->address.GetCol( )] = true;

                /* Assume the data is placed on the bus immediately after the command. */
                NVMainRequest *busReq = new NVMainRequest( );
                *busReq = *request;
                busReq->type = BUS_WRITE;
                busReq->owner = this;

                GetEventQueue( )->InsertEvent( EventResponse, this, busReq, 
                        GetEventQueue()->GetCurrentCycle() + 1 );

                /* Notify owner of read completion as well */
                GetEventQueue( )->InsertEvent( EventResponse, this, request, 
                        GetEventQueue()->GetCurrentCycle() + MAX( p->tBURST, p->tRDB ) );

                /* Swap the cached status back to normal. */
                request->type = (request->type == CACHED_WRITE ? WRITE : request->type);

                //std::cout << GetEventQueue()->GetCurrentCycle() << " " << statName 
                //          << ": Served write request 0x" << std::hex
                //          << request->address.GetPhysicalAddress() << std::dec << std::endl;

                RDBWrites++;
                cachedRowBuffer[bufferIdx]->writes++;

                /* Move this buffer to LRU. */
                CachedRowBuffer *tempBuffer = cachedRowBuffer[bufferIdx];

                for( uint64_t j = bufferIdx; j > 0; j-- )
                {
                    cachedRowBuffer[j] = cachedRowBuffer[j-1];
                }

                cachedRowBuffer[0] = tempBuffer;

                break;
            }
        }
    }

    if( !rv )
    {
        rv = DDR3Bank::Write( request );
    }

    return rv;
}


bool CachedDDR3Bank::IsIssuable( NVMainRequest *request, FailReason *reason )
{
    bool rv = false; 
    bool inRDB = false;
    bool cacheableRequest = false;

    for( ncounter_t bufferIdx = 0; bufferIdx < rowBufferCount; bufferIdx++ )
    {
        if( cachedRowBuffer[bufferIdx]->address.GetRow( ) == request->address.GetRow( )
            && request->address.GetCol( ) >= cachedRowBuffer[bufferIdx]->colStart
            && request->address.GetCol( ) <  cachedRowBuffer[bufferIdx]->colEnd )
        {
            inRDB = true;
        }
    }

    if( request->type == READ || request->type == READ_PRECHARGE || request->type == CACHED_READ ||
        ( !readOnlyBuffers && (request->type == WRITE || request->type == WRITE_PRECHARGE 
                               || request->type == CACHED_WRITE )
        ) 
      )
    {
        cacheableRequest = true;
    }


    if( inRDB && cacheableRequest )
    {
        assert( request->type == READ || request->type == READ_PRECHARGE ||
                request->type == WRITE || request->type == WRITE_PRECHARGE ||
                request->type == CACHED_READ || request->type == CACHED_WRITE );
        inRDBCount++;
        rv = true;

        //std::cout << GetEventQueue()->GetCurrentCycle() << " " << statName << ": Found 0x"
        //          << std::hex << request->address.GetPhysicalAddress( ) << std::dec
        //          << " in RDB. Row = " << request->address.GetRow( ) << " Col = "
        //          << request->address.GetCol( ) << std::endl;
    }
    else if( request->type != CACHED_READ && request->type != CACHED_WRITE )
    {
        rv = DDR3Bank::IsIssuable( request, reason );
    }

    return rv;
}


void CachedDDR3Bank::RegisterStats( )
{
    AddStat(inRDBCount);
    AddStat(RDBAllocations);
    AddStat(writebackCount);
    AddStat(RDBReads);
    AddStat(RDBWrites);
    AddStat(allocationReadsHisto);
    AddStat(allocationWritesHisto);
}

void CachedDDR3Bank::CalculateStats( )
{
    allocationReadsHisto = PyDictHistogram<uint64_t, uint64_t>( allocationReadsMap );
    allocationWritesHisto = PyDictHistogram<uint64_t, uint64_t>( allocationWritesMap );
}



