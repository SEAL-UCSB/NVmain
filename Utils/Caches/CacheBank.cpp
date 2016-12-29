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
*******************************************************************************/

#include "Utils/Caches/CacheBank.h"
#include "include/NVMHelpers.h"
#include "src/EventQueue.h"

#include <iostream>
#include <cassert>

using namespace NVM;

CacheBank::CacheBank( uint64_t rows, uint64_t sets, uint64_t assoc, uint64_t lineSize )
{
    uint64_t r, i, j;

    cacheEntry = new CacheEntry** [ rows ];
    for( r = 0; r < rows; r++ )
    {
        cacheEntry[r] = new CacheEntry* [ sets ];
        for( i = 0; i < sets; i++ )
        {
            cacheEntry[r][i] = new CacheEntry[ assoc ];
            for( j = 0; j < assoc; j++ )
            {
                /* Clear valid bit, dirty bit, etc. */
                cacheEntry[r][i][j].flags = CACHE_ENTRY_NONE;
            }
        }
    }

    numRows = rows;
    numSets = sets;
    numAssoc = assoc;
    cachelineSize = lineSize;

    state = CACHE_IDLE;
    stateTimer = 0;

    decodeClass = NULL;
    decodeFunc = NULL;
    SetDecodeFunction( this, 
            static_cast<CacheSetDecoder>(&NVM::CacheBank::DefaultDecoder) );

    readTime = 1; // 1 cycle
    writeTime = 1;  // 1 cycle

    isMissMap = false;
}

CacheBank::~CacheBank( )
{
    uint64_t i, r;

    for( r = 0; r < numRows; r++ )
    {
        for( i = 0; i < numSets; i++ )
        {
            delete [] cacheEntry[r][i];
        }
        delete [] cacheEntry[r];
    }

    delete [] cacheEntry;
}

void CacheBank::SetDecodeFunction( NVMObject *dcClass, CacheSetDecoder dcFunc )
{
    decodeClass = dcClass;
    decodeFunc = dcFunc;
}

uint64_t CacheBank::DefaultDecoder( NVMAddress &addr )
{
    return addr.GetCol() % numSets;
}

uint64_t CacheBank::SetID( NVMAddress& addr )
{
    /*
     *  By default we'll just chop off the bits for the cacheline and use the
     *  least significant bits as the set address, and the remaining bits are 
     *  the tag bits.
     */
    uint64_t setID;

    //if( isMissMap )
    //    setID = (addr.GetPhysicalAddress( )) % numSets;

    setID = (decodeClass->*decodeFunc)( addr );

    return setID;
}

CacheEntry *CacheBank::FindSet( NVMAddress& addr )
{
    /*
     *  By default we'll just chop off the bits for the cacheline and use the
     *  least significant bits as the set address, and the remaining bits are 
     *  the tag bits.
     */
    uint64_t setID = SetID( addr );

    return cacheEntry[addr.GetRow()][setID];
}

bool CacheBank::Present( NVMAddress& addr )
{
    CacheEntry *set = FindSet( addr );
    bool found = false;

    for( uint64_t i = 0; i < numAssoc; i++ )
    {
        if( set[i].address.GetPhysicalAddress( ) == addr.GetPhysicalAddress( ) 
            && (set[i].flags & CACHE_ENTRY_VALID ) )
        {
            found = true;
            break;
        }
    }

    return found;
}

bool CacheBank::SetFull( NVMAddress& addr )
{
    CacheEntry *set = FindSet( addr );
    bool rv = true;

    for( uint64_t i = 0; i < numAssoc; i++ )
    {
        /* If there is an invalid entry (e.g., not used) the set isn't full. */
        if( !(set[i].flags & CACHE_ENTRY_VALID) )
        {
            rv = false;
            break;
        }
    }

    return rv;
}

bool CacheBank::Install( NVMAddress& addr, NVMDataBlock& data )
{
    CacheEntry *set = FindSet( addr );
    bool rv = false;

    //assert( !Present( addr ) );

    for( uint64_t i = 0; i < numAssoc; i++ )
    {
        if( !(set[i].flags & CACHE_ENTRY_VALID) )
        {
            set[i].address = addr;
            set[i].data = data;
            set[i].flags |= CACHE_ENTRY_VALID; 
            rv = true;
            break;
        }
    }

    return rv;
}

bool CacheBank::Read( NVMAddress& addr, NVMDataBlock *data )
{
    CacheEntry *set = FindSet( addr );
    bool rv = false;

    assert( Present( addr ) );

    for( uint64_t i = 0; i < numAssoc; i++ )
    {
        if( set[i].address.GetPhysicalAddress( ) == addr.GetPhysicalAddress( ) 
            && (set[i].flags & CACHE_ENTRY_VALID) )
        {
            *data = set[i].data;
            rv = true;

            /* Move cache entry to MRU position */
            CacheEntry tmp;

            tmp.flags = set[i].flags;
            tmp.address = set[i].address;
            tmp.data = set[i].data;

            for( uint64_t j = i; j > 0; j-- )
            {
                set[j].flags = set[j-1].flags;
                set[j].address = set[j-1].address;
                set[j].data = set[j-1].data;
            }

            set[0].flags = tmp.flags;
            set[0].address = tmp.address;
            set[0].data = tmp.data;
        }
    }

    return rv;
}

bool CacheBank::Write( NVMAddress& addr, NVMDataBlock& data )
{
    CacheEntry *set = FindSet( addr );
    bool rv = false;

    assert( Present( addr ) );

    for( uint64_t i = 0; i < numAssoc; i++ )
    {
        if( set[i].address.GetPhysicalAddress( ) == addr.GetPhysicalAddress( )
            && (set[i].flags & CACHE_ENTRY_VALID) )
        {
            set[i].data = data;
            set[i].flags |= CACHE_ENTRY_DIRTY;
            rv = true;

            /* Move cache entry to MRU position */
            CacheEntry tmp;

            tmp.flags = set[i].flags;
            tmp.address = set[i].address;
            tmp.data = set[i].data;

            for( uint64_t j = i; j > 1; j-- )
            {
                set[j].flags = set[j-1].flags;
                set[j].address = set[j-1].address;
                set[j].data = set[j-1].data;
            }

            set[0].flags = tmp.flags;
            set[0].address = tmp.address;
            set[0].data = tmp.data;
        }
    }

    return rv;
}

/* 
 *  Updates data without changing dirty bit or LRU position
 *  Returns true if the block was found and updated.
 */
bool CacheBank::UpdateData( NVMAddress& addr, NVMDataBlock& data )
{
    CacheEntry *set = FindSet( addr );
    bool rv = false;

    assert( Present( addr ) );

    for( uint64_t i = 0; i < numAssoc; i++ )
    {
        if( set[i].address.GetPhysicalAddress( ) == addr.GetPhysicalAddress( )
            && (set[i].flags & CACHE_ENTRY_VALID) )
        {
            set[i].data = data;
            rv = true;
        }
    }

    return rv;
}

/* Return true if the victim data is dirty. */
bool CacheBank::ChooseVictim( NVMAddress& addr, NVMAddress *victim )
{
    bool rv = false;
    CacheEntry *set = FindSet( addr );

    assert( SetFull( addr ) );
    assert( set[numAssoc-1].flags & CACHE_ENTRY_VALID );

    *victim = set[numAssoc-1].address;
    
    if( set[numAssoc-1].flags & CACHE_ENTRY_DIRTY )
        rv = true;

    return rv;
}


bool CacheBank::Evict( NVMAddress& addr, NVMDataBlock *data )
{
    bool rv;
    CacheEntry *set = FindSet( addr );

    assert( Present( addr ) );

    rv = false; 

    for( uint64_t i = 0; i < numAssoc; i++ )
    {
        if( set[i].address.GetPhysicalAddress( ) == addr.GetPhysicalAddress( ) 
            && (set[i].flags & CACHE_ENTRY_VALID) )
        {
            if( set[i].flags & CACHE_ENTRY_DIRTY )
            {
                *data = set[i].data;
                rv = true;
            }
            else
            {
                *data = set[i].data;
                rv = false;
            }

            set[i].flags = CACHE_ENTRY_NONE;

            break;
        }
    }

    return rv;
}

void CacheBank::SetReadTime( uint64_t rtime )
{
    readTime = rtime;
}

void CacheBank::SetWriteTime( uint64_t wtime )
{
    writeTime = wtime;
}

uint64_t CacheBank::GetReadTime( )
{
    return readTime;
}

uint64_t CacheBank::GetWriteTime( )
{
    return writeTime;
}

uint64_t CacheBank::GetAssociativity( )
{
    return numAssoc;
}

uint64_t CacheBank::GetCachelineSize( )
{
    return cachelineSize;
}

uint64_t CacheBank::GetSetCount( )
{
    return numSets;
}

double CacheBank::GetCacheOccupancy( )
{
    double occupancy;
    uint64_t valid, total;

    valid = 0;
    total = numRows*numSets*numAssoc;

    for( uint64_t rowIdx = 0; rowIdx < numRows; rowIdx++ )
    {
        for( uint64_t setIdx = 0; setIdx < numSets; setIdx++ )
        {
            CacheEntry *set = cacheEntry[rowIdx][setIdx];

            for( uint64_t assocIdx = 0; assocIdx < numAssoc; assocIdx++ )
            {
                if( set[assocIdx].flags & CACHE_ENTRY_VALID )
                    valid++;
            }
        }
    }

    occupancy = static_cast<double>(valid) / static_cast<double>(total);

    return occupancy;
}

bool CacheBank::IsIssuable( NVMainRequest * /*req*/, FailReason * /*reason*/ )
{
    bool rv = false;

    /* We can issue if the cache is idle. Pretty simple */
    if( state == CACHE_IDLE )
    {
        rv = true;
    }
    else
    {
        rv = false;
    }

    return rv;
}

bool CacheBank::IssueCommand( NVMainRequest *nreq )
{
    NVMDataBlock dummy;
    CacheRequest *req = static_cast<CacheRequest *>( nreq->reqInfo );

    assert( IsIssuable( nreq, NULL ) );

    if( !IsIssuable( nreq, NULL ) )
        return false;

    switch( req->optype )
    {
        case CACHE_READ:
            state = CACHE_BUSY;
            stateTimer = GetEventQueue( )->GetCurrentCycle( ) + readTime;
            req->hit = Present( req->address );
            if( req->hit ) 
                Read( req->address, &(req->data) ); 
            GetEventQueue( )->InsertEvent( EventResponse, this, nreq, stateTimer );
            break;

        case CACHE_WRITE:
            state = CACHE_BUSY;
            stateTimer = GetEventQueue( )->GetCurrentCycle( ) + writeTime;

            if( SetFull( req->address ) )
            {
                NVMainRequest *mmEvict = new NVMainRequest( );
                CacheRequest *evreq = new CacheRequest;
                
                ChooseVictim( req->address, &(evreq->address) );
                Evict( evreq->address, &(evreq->data) );

                evreq->optype = CACHE_EVICT;

                *mmEvict = *nreq;
                mmEvict->owner = nreq->owner;
                mmEvict->reqInfo = static_cast<void *>( evreq );
                mmEvict->tag = nreq->tag;

                GetEventQueue( )->InsertEvent( EventResponse, this, mmEvict,
                                               stateTimer );
            }

            req->hit = Present( req->address );
            if( req->hit ) 
                Write( req->address, req->data );
            else
                Install( req->address, req->data );

            GetEventQueue( )->InsertEvent( EventResponse, this, nreq, stateTimer );

            break;

        default:
            std::cout << "CacheBank: Unknown operation `" << req->optype << "'!"
                << std::endl;
            break;
    }

    return true;
}

bool CacheBank::RequestComplete( NVMainRequest *req )
{
    GetParent( )->RequestComplete( req );

    state = CACHE_IDLE;

    return true;
}

void CacheBank::Cycle( ncycle_t /*steps*/ )
{
}
