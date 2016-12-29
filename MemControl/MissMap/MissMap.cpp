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

#include "MemControl/MissMap/MissMap.h"
#include "Decoders/DRCDecoder/DRCDecoder.h"
#include "MemControl/LH-Cache/LH-Cache.h"
#include "include/NVMHelpers.h"
#include "NVM/nvmain.h"
#include <assert.h>

using namespace NVM;

MissMap::MissMap( )
{
    missMap = NULL;

    missMapAllocations = 0;
    missMapWrites = 0;
    missMapHits = 0;
    missMapMisses = 0;
    missMapForceEvicts = 0;
    missMapMemReads = 0;

    psInterval = 0;
}

MissMap::~MissMap( )
{
}

void MissMap::SetConfig( Config *conf, bool createChildren )
{
    /* Initialize DRAM Cache channels */
    if( conf->KeyExists( "DRC_CHANNELS" ) )
        numChannels = static_cast<ncounter_t>( conf->GetValue( "DRC_CHANNELS" ) );
    else
        numChannels = 1;

    /* MissMap Setup */
    uint64_t mmSets, mmAssoc;

    mmSets = 256;
    if( conf->KeyExists( "MissMapSets" ) ) 
        mmSets = static_cast<uint64_t>( conf->GetValue( "MissMapSets" ) );

    mmAssoc = 16;
    if( conf->KeyExists( "MissMapAssoc" ) ) 
        mmAssoc = static_cast<uint64_t>( conf->GetValue( "MissMapAssoc" ) );

    missMapQueueSize = 32;
    if( conf->KeyExists( "MissMapQueueSize" ) ) 
        missMapQueueSize = static_cast<uint64_t>( conf->GetValue( "MissMapQueueSize" ) );

    uint64_t missMapLatency = 10;
    if( conf->KeyExists( "MissMapLatency" ) ) 
        missMapLatency = static_cast<uint64_t>( conf->GetValue( "MissMapLatency" ) );

    if( createChildren )
    {
        /* Initialize off-chip memory */
        std::string configFile;
        Config *mainMemoryConfig;

        configFile  = NVM::GetFilePath( conf->GetFileName( ) );
        configFile += conf->GetString( "MM_CONFIG" );

        mainMemoryConfig = new Config( );
        mainMemoryConfig->Read( configFile );

        mainMemory = new NVMain( );
        mainMemory->SetConfig( mainMemoryConfig, "offChipMemory", createChildren );
        mainMemory->SetParent( this ); 

        std::string drcVariant = "LH_Cache";
        if( conf->KeyExists( "DRCVariant" ) ) 
            drcVariant = conf->GetString( "DRCVariant" );

        drcChannels = new LH_Cache*[numChannels];
        for( ncounter_t i = 0; i < numChannels; i++ )
        {
            drcChannels[i] = dynamic_cast<LH_Cache *>( 
                    MemoryControllerFactory::CreateNewController( drcVariant ));

            drcChannels[i]->SetMainMemory( mainMemory );

            drcChannels[i]->SetID( static_cast<unsigned int>(i) );
            drcChannels[i]->StatName( this->statName ); 

            drcChannels[i]->SetParent( this );
            AddChild( drcChannels[i] );

            drcChannels[i]->SetConfig( conf, createChildren );
        }

        missMap = new CacheBank( 1, mmSets, mmAssoc, 64 ); 
        missMap->isMissMap = true;

        missMap->SetParent( this );
        AddChild( missMap );

        missMap->SetReadTime( missMapLatency );
        missMap->SetWriteTime( missMapLatency );
    }

    std::cout << "Created a MissMap!" << std::endl;
}

void MissMap::RegisterStats( )
{
    AddStat(missMapAllocations);
    AddStat(missMapWrites);
    AddStat(missMapHits);
    AddStat(missMapMisses);
    AddStat(missMapForceEvicts);
    AddStat(missMapMemReads);
}

bool MissMap::QueueFull( NVMainRequest * )
{
    return (missMapQueue.size() >= missMapQueueSize);
}

bool MissMap::IssueAtomic( NVMainRequest *req )
{
    /* Just install in the miss map if the address does not yet exist */
    uint64_t *lineMap, lineMask, lineOffset;
    NVMAddress testAddr;
    NVMDataBlock data;

    /* 6 = log2(cacheline size), 12 = log2(segment size) */
    testAddr.SetPhysicalAddress( (req->address.GetPhysicalAddress( ) >> 6) >> 12 ); 

    /* 6 = log2(cacheline size), 0xFFF = segment size - 1, 64 = 
     * segment size / cacheline size 
     */
    lineOffset = ((req->address.GetPhysicalAddress( ) >> 6) & 0xFFF) / 64; 
    lineMask = (uint64_t)(1ULL << lineOffset);

    std::cout << "Address 0x" << std::hex << req->address.GetPhysicalAddress() 
        << " maps to page 0x" << testAddr.GetPhysicalAddress( ) << std::dec 
        << " with offset " << lineOffset << std::endl;
    
    /* Entry exists, so augment the existing bit-vector */
    if( missMap->Present( testAddr ) )
    {
        missMap->Read( req->address, &data );

        lineMap = reinterpret_cast<uint64_t *>(data.rawData);

        if( !((*lineMap) & lineMask) ) // NOT in MissMap.
        {
            *lineMap |= lineMask;
        }
    }
    /* Entry doesn't exist. Create a new bit-vector and write it. */
    else
    {
        /* If there is no space, toss something. */
        if( missMap->SetFull( testAddr ) )
        {
            NVMAddress victim;
            NVMDataBlock dummy;

            missMap->ChooseVictim( testAddr, &victim );
            missMap->Evict( victim, &dummy );
        }

        /* Create a new bit-vector with just this cacheline as the entry. */
        lineMap = new uint64_t;

        *lineMap = lineMask;
        data.rawData = reinterpret_cast<uint8_t*>(lineMap);

        missMap->Install( testAddr, data ); 
    }

    return true;
}

bool MissMap::IssueCommand( NVMainRequest *req )
{
    bool rv = false;

    /* Make sure there is space in the MissMap's queue */
    if( missMapQueue.size( ) < missMapQueueSize )
    {
        NVMainRequest *mmReq = new NVMainRequest( );
        CacheRequest *creq = new CacheRequest;

        *mmReq = *req;
        mmReq->tag = MISSMAP_READ;
        mmReq->reqInfo = static_cast<void *>( creq );
        mmReq->owner = this;

        creq->optype = CACHE_READ;
        creq->address = req->address; 

        /* Use the PAGE ADDRESS for the miss map */
        creq->address.SetPhysicalAddress( 
                (creq->address.GetPhysicalAddress( ) >> 6) >> 12 ); 
        creq->owner = this;
        creq->originalRequest = req;

        missMapQueue.push( mmReq );

#ifdef DBGMISSMAP
        std::cout << "Enqueued a request to the miss map. " << std::endl;
#endif

        rv = true;
    }

    return rv;
}

bool MissMap::RequestComplete( NVMainRequest *req )
{
    bool rv = false;

    if( req->owner == this )
    {
        if( req->tag == MISSMAP_READ )
        {
            CacheRequest *cacheReq = static_cast<CacheRequest *>( req->reqInfo );

#ifdef DBGMISSMAP
            std::cout << "MissMap read complete. Hit = " 
                << cacheReq->hit << std::endl;
#endif

            /* Found miss map entry for this page. */
            if( cacheReq->hit )
            {
                /* Check MissMap entry for this cacheline. */
                uint64_t *lineMap;
                uint64_t lineMask;
                uint64_t lineOffset; 

                lineOffset = ((req->address.GetPhysicalAddress( ) >> 6) & 0xFFF) / 64;
                lineMap = reinterpret_cast<uint64_t *>(cacheReq->data.rawData);
                lineMask = (uint64_t)(1ULL << lineOffset);

                /* Check for this bit corresponding to this cache line. */
                if( (*lineMap) & lineMask )
                {
#ifdef DBGMISSMAP
                    std::cout << "Found cacheline in miss map, issuing to DRC." 
                        << std::endl;
#endif

                    /* In DRC -- Issue to DRC. */
                    uint64_t chan;

                    cacheReq->originalRequest->address.GetTranslatedAddress( 
                            NULL, NULL, NULL, NULL, &chan, NULL );

                    assert( chan < numChannels );

                    drcChannels[chan]->IssueCommand( cacheReq->originalRequest );

                    missMapHits++;
                }
                else
                {
#ifdef DBGMISSMAP
                    std::cout << "Did not find cacheline; issuing to main memory." 
                        << std::endl;
#endif

                    /* 
                     *  Not in DRC -- Issue to main memory.
                     *  Writes go to DRC since they don't miss.
                     */
                    if( cacheReq->originalRequest->type == READ )
                    {
                        cacheReq->originalRequest->tag = MISSMAP_MEMREAD;
                        mainMemory->IssueCommand( cacheReq->originalRequest );
                    }
                    else
                    {
                        uint64_t chan;

                        cacheReq->originalRequest->address.GetTranslatedAddress( 
                                NULL, NULL, NULL, NULL, &chan, NULL );

                        drcChannels[chan]->IssueCommand( cacheReq->originalRequest );
                    }

                    /* Write the new miss map line as well. */
                    NVMainRequest *mmFill = new NVMainRequest( );
                    CacheRequest *fillCReq = new CacheRequest( );

                    *lineMap |= (uint64_t)(1ULL << lineOffset);
                    fillCReq->data.rawData = reinterpret_cast<uint8_t *>( lineMap );

                    fillCReq->optype = CACHE_WRITE;
                    fillCReq->address = req->address; 
                    /* Use the PAGE ADDRESS for the miss map */
                    fillCReq->address.SetPhysicalAddress( 
                            (fillCReq->address.GetPhysicalAddress( ) >> 6) >> 12 ); 
                    fillCReq->owner = this;
                    fillCReq->originalRequest = NULL;

                    *mmFill = *req;
                    mmFill->owner = this;
                    mmFill->reqInfo = static_cast<void *>( fillCReq );
                    mmFill->tag = MISSMAP_WRITE;

                    missMapWrites++;
                    missMapMisses++;

                    missMapFillQueue.push( mmFill );

#ifdef DBGMISSMAP
                    std::cout << "Updating miss map entry 0x" 
                        << fillCReq->address.GetPhysicalAddress( )
                        << " with bit vector 0x" << std::hex << (*lineMap) 
                        << std::dec << std::endl;
#endif
                }
            }
            /* No miss map entry for this page. */
            else
            {
                /* 
                 *  Not in DRC -- Issue to main memory.
                 *  Writes go to DRC since they don't miss.
                 */
                if( cacheReq->originalRequest->type == READ )
                {
                    cacheReq->originalRequest->tag = MISSMAP_MEMREAD;
                    mainMemory->IssueCommand( cacheReq->originalRequest );
                }
                else
                {
                    uint64_t chan;

                    cacheReq->originalRequest->address.GetTranslatedAddress( 
                            NULL, NULL, NULL, NULL, &chan, NULL );

                    drcChannels[chan]->IssueCommand( cacheReq->originalRequest );
                }

                /* Install this line to MissMap. */
                NVMainRequest *mmFill = new NVMainRequest( );
                CacheRequest *fillCReq = new CacheRequest( );
                uint64_t lineOffset = ((req->address.GetPhysicalAddress( ) >> 6) & 0xFFF) / 64;
                uint64_t *lineMap = new uint64_t;

                *lineMap = 0;
                *lineMap |= (uint64_t)(1ULL << lineOffset);
                fillCReq->data.rawData = reinterpret_cast<uint8_t *>(lineMap);

                fillCReq->optype = CACHE_WRITE;
                fillCReq->address = req->address; 
                /* Use the PAGE ADDRESS for the miss map */
                fillCReq->address.SetPhysicalAddress( 
                        (fillCReq->address.GetPhysicalAddress( ) >> 6) >> 12 ); 
                fillCReq->owner = this;
                fillCReq->originalRequest = NULL;

                *mmFill = *req;
                mmFill->owner = this;
                mmFill->reqInfo = static_cast<void *>( fillCReq );
                mmFill->tag = MISSMAP_WRITE;

                missMapAllocations++;
                missMapWrites++;
                missMapMisses++;

                missMapFillQueue.push( mmFill );

#ifdef DBGMISSMAP
                std::cout << "Adding new miss map entry 0x" 
                    << fillCReq->address.GetPhysicalAddress( )
                    << " with bit vector 0x" << std::hex << (*lineMap) 
                    << std::dec << std::endl;
#endif
            }

            delete cacheReq;
        }
        else if( req->tag == MISSMAP_WRITE )
        {
            /* Just delete the cache request struct. */
            CacheRequest *creq = static_cast<CacheRequest *>( req->reqInfo );

#ifdef DBGMISSMAP
            std::cout << "Wrote to the miss map." << std::endl;
#endif

            /* Check if there was an eviction. */
            if( creq->optype == CACHE_EVICT )
            {
                /* Ensure consistency in DRC by evicting any cachelines in this miss map entry. */
                uint64_t chan;

                req->address.GetTranslatedAddress( NULL, NULL, NULL, NULL, &chan, NULL );
                assert( chan < numChannels );

                NVMainRequest *evictReq = new NVMainRequest( );

                *evictReq = *req;
                evictReq->owner = this;
                evictReq->tag = MISSMAP_FORCE_EVICT;

#ifdef DBGMISSMAP
                std::cout << "Miss map evicted a line.." << std::endl;
#endif

                /* Count the number of cachelines being evicted. */
                uint64_t *lineMap = reinterpret_cast<uint64_t *>(creq->data.rawData);
                uint64_t lineCount = 0;

                for( int i = 0; i < 64; i++ )
                {
                    if( *lineMap & 0x1 )
                        lineCount++;

                    *lineMap >>= 1;
                }

                missMapForceEvicts += lineCount;

                /* 
                 * We should be able to delete this allocated memory at this 
                 * point. 
                 */
                delete lineMap;

                drcChannels[chan]->IssueCommand( evictReq );
            }

            delete creq;
        }

        delete req;
        rv = true;
    }
    /* 
     * MISSMAP_MEMREAD is a tagged request originating from the sequencer, 
     * so we aren't the owner. 
     */
    else if( req->tag == MISSMAP_MEMREAD )
    {
        uint64_t chan;

        req->address.GetTranslatedAddress( NULL, NULL, NULL, NULL, &chan, NULL );

        // This is now a fill request.
        req->type = WRITE;
        req->tag = 0;

#ifdef DBGMISSMAP
        std::cout << "MissMap memory access returned, filling DRAM cache. " 
            << std::endl;
#endif

        drcChannels[chan]->IssueCommand( req );

        missMapMemReads++;

        rv = false;
    }

    return rv;
}

void MissMap::Cycle( ncycle_t )
{
    /* Issue MissMap commands */
    if( missMap && (!missMapQueue.empty( ) || !missMapFillQueue.empty()) )
    {
        /* Give priority to write to install new miss map entires */
        if( !missMapFillQueue.empty() )
        {
            if( missMap->IsIssuable( missMapFillQueue.front(), NULL ) )
            {
                missMap->IssueCommand( missMapFillQueue.front() );
                missMapFillQueue.pop( );

#ifdef DBGMISSMAP
                std::cout << "Issued a fill to the miss map." << std::endl;
#endif
            }
        }
        else
        {
            if( missMap->IsIssuable( missMapQueue.front(), NULL ) )
            {
                missMap->IssueCommand( missMapQueue.front() );
                missMapQueue.pop( );

#ifdef DBGMISSMAP
                std::cout << "Issued a probe to the miss map." << std::endl;
#endif
            }
        }
    }
}

void MissMap::CalculateStats( )
{
}
