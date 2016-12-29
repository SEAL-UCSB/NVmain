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

#ifndef __NVMAIN_UTILS_CACHES_CACHEBANK_H__
#define __NVMAIN_UTILS_CACHES_CACHEBANK_H__

#include <utility>
#include "include/NVMAddress.h"
#include "include/NVMDataBlock.h"
#include "src/NVMObject.h"
#include "src/AddressTranslator.h"

namespace NVM {

typedef uint64_t (NVMObject::*CacheSetDecoder)(NVMAddress&);

enum CacheState { CACHE_IDLE, CACHE_BUSY };
enum CacheOperation { CACHE_NONE, CACHE_READ, CACHE_WRITE, 
                      CACHE_SCRUB, CACHE_EVICT };

struct CacheRequest
{
    CacheOperation optype;
    NVMAddress address;
    NVMAddress endAddr;
    NVMDataBlock data;
    bool hit;
    NVMObject *owner;
    NVMainRequest *originalRequest;
};

/* Must be powers of two! */
enum { CACHE_ENTRY_NONE = 0,
       CACHE_ENTRY_VALID = 1,
       CACHE_ENTRY_DIRTY = 2,
       CACHE_ENTRY_EXAMPLE = 4
};

struct CacheEntry
{
    uint64_t flags;
    NVMAddress address;
    NVMDataBlock data;
};

class CacheBank : public NVMObject
{
  public:
    CacheBank( uint64_t rows, uint64_t sets, uint64_t assoc, uint64_t lineSize );
    ~CacheBank( );

    /* Return true if the address is in the cache. */
    bool Present( NVMAddress& addr );

    /* Return true if the address was placed in the cache. */
    bool Install( NVMAddress& addr, NVMDataBlock& data );

    /* Return true if the set is full, i.e. needs an eviction. */
    bool SetFull( NVMAddress& addr );

    /* Return true if evicted data is dirty data in *data. data is set to NULL
     * if the address was not found. */
    bool Evict( NVMAddress& addr, NVMDataBlock *data );

    bool Read( NVMAddress& addr, NVMDataBlock *data );
    bool Write( NVMAddress& addr, NVMDataBlock& data );

    bool UpdateData( NVMAddress& addr, NVMDataBlock& data );

    void SetReadTime( uint64_t rtime );
    void SetWriteTime( uint64_t wtime );

    uint64_t GetReadTime( );
    uint64_t GetWriteTime( );

    uint64_t GetAssociativity( );
    uint64_t GetCachelineSize( );
    uint64_t GetSetCount( );
    double GetCacheOccupancy( );

    bool IsIssuable( NVMainRequest *req, FailReason *reason );
    bool IssueCommand( NVMainRequest *req );
    bool RequestComplete( NVMainRequest *req );
    
    void Cycle( ncycle_t steps );

    bool ChooseVictim( NVMAddress& addr, NVMAddress *victim );

    void SetDecodeFunction( NVMObject *dcClass, CacheSetDecoder dcFunc );

    uint64_t numRows, numSets, numAssoc, cachelineSize;
    CacheEntry ***cacheEntry;
    uint64_t accessTime, stateTimer;
    uint64_t readTime, writeTime;
    CacheState state;

    CacheEntry *FindSet( NVMAddress& addr );
    uint64_t SetID( NVMAddress& addr );
    bool isMissMap;

    CacheSetDecoder decodeFunc;
    NVMObject *decodeClass;
    uint64_t DefaultDecoder( NVMAddress& addr );
};

}; 

#endif
