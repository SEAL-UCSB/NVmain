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

#include "Prefetchers/STeMS/STeMS.h"
#include <iostream>

using namespace NVM;

void STeMS::FetchNextUnused( PatternSequence *rps, int count, 
                             std::vector<NVMAddress>& prefetchList )
{
    uint64_t *lastUnused = new uint64_t[count];
    bool *foundUnused = new bool[count];

    for( int i = 0; i < count; i++ )
    {
        lastUnused[i] = 0;
        foundUnused[i] = false;
    }

    /* Find the LAST requests marked as unused. */
    for( int i = static_cast<int>(rps->size - 1); i >= 0; i-- )
    {
        if( !rps->fetched[i] )
        {
            for( int j = 0; j < count - 1; j++ )
            {
                lastUnused[j] = lastUnused[j+1];
                foundUnused[j] = foundUnused[j+1];
            }

            lastUnused[count-1] = rps->offset[i];
            foundUnused[count-1] = true;
        }
        else
        {
            break;
        }
    }

    for( int i = 0; i < count; i++ )
    {
        if( foundUnused[i] )
        {
            rps->startedPrefetch = true;

#ifdef DBGPF
            std::cout << "Prefetching 0x" << std::hex
                    << rps->address + lastUnused[i] << std::dec << std::endl;
#endif

            NVMAddress pfAddr;
            pfAddr.SetPhysicalAddress( rps->address + lastUnused[i] );
            prefetchList.push_back( pfAddr );

            /* Mark offset as fetched. */
            for( uint64_t j = 0; j < rps->size; j++ )
            {
                if( rps->offset[j] == lastUnused[i] )
                    rps->fetched[j] = true;
            }
        }
    }
}

bool STeMS::NotifyAccess( NVMainRequest *accessOp, 
                          std::vector<NVMAddress>& prefetchList )
{
    bool rv = false;

    /* 
     * If this access came from a PC that has an allocated reconstruction 
     * buffer, but it is not the first unused address in the reconstruction 
     * buffer, deallocate the buffer.
     */
    if( ReconBuf.count( accessOp->programCounter ) )
    {
        PatternSequence *rps = ReconBuf[accessOp->programCounter];
        uint64_t address = accessOp->address.GetPhysicalAddress( );

        /* Can't evaluate prefetch effectiveness until we've issued some. */
        if( !rps->startedPrefetch )
            return false;

        /* 
         * If the access was a prefetch we did that was successful, prefetch 
         * more else if the access is something that was not fetched, 
         * deallocate the reconstruct buffer. 
         */
        bool successful = false;

        for( uint64_t i = 0; i < rps->size; i++ )
        {
            if( address == ( rps->address + rps->offset[i] ) )
            {
                if( rps->fetched[i] && !rps->used[i] )
                {
                    /* Successful prefetch, get more */
                    successful = true;

#ifdef DBGPF
                    std::cout << "Successful prefetch ! 0x" << std::hex << address
                              << std::dec << std::endl;
#endif

                    FetchNextUnused( rps, 4, prefetchList );

                    rv = true;

                    rps->used[i] = true;
                }
            }
        }

        if( !successful )
        {
            /* Check if the address failed because it's not in the PST. If most 
             * of the fetches were used, extend the PST.
             */
            uint64_t numSuccess = 0;

            for( uint64_t i = 0; i < rps->size; i++ )
            {
                if( rps->used[i] ) numSuccess++;
            }

            if( ((double)(numSuccess) / (double)(rps->size)) >= 0.6f )
            {
                PatternSequence *ps = NULL;
                std::map<uint64_t, PatternSequence*>::iterator it;

                it = PST.find( accessOp->programCounter );
                if( it != PST.end( ) )
                {
                    ps = it->second;

                    if( ps->size < 16 )
                    {
                        ps->offset[ps->size] = address - rps->address;
                        ps->delta[ps->size] = 0;
                        ps->size++;
                    }
                }
            }

            std::map<uint64_t, PatternSequence*>::iterator it;
            it = ReconBuf.find( accessOp->programCounter );
            ReconBuf.erase( it );
        }
    }

    return rv;
}

bool STeMS::DoPrefetch( NVMainRequest *triggerOp, 
                        std::vector<NVMAddress>& prefetchList )
{
    NVMAddress pfAddr;

    /* If there is an entry in the PST for this PC, build a recon buffer */
    if( PST.count( triggerOp->programCounter ) )
    {
        PatternSequence *ps = PST[triggerOp->programCounter];
        uint64_t address = triggerOp->address.GetPhysicalAddress( );
        uint64_t pc = triggerOp->programCounter;

        /* Check for an RB that is actively being built */
        if( ReconBuf.count( pc ) )
        {
            PatternSequence *rps = ReconBuf[pc];
            uint64_t numUsed, numFetched;

            numUsed = numFetched = 0;

            /* Mark this address as used. */
            for( uint64_t i = 0; i < rps->size; i++ )
            {
                if( ( rps->address + rps->offset[i] ) == address )
                {
                    rps->used[i] = rps->fetched[i] = true;
                }

                if( rps->used[i] ) 
                    numUsed++;

                if( rps->fetched[i] ) 
                    numFetched++;
            }

            /* 
             * If there are enough used out of the fetched values, 
             * get some more. 
             */
            if( numUsed >= 2 )
            {
                FetchNextUnused( rps, 4, prefetchList );
            }
        }
        /* Create new recon buffer by copying the PST entry */
        else
        {
            PatternSequence *rps = new PatternSequence;
            uint64_t pc = triggerOp->programCounter;

            rps->size = ps->size;
            rps->address = triggerOp->address.GetPhysicalAddress( );

            for( uint64_t i = 0; i < ps->size; i++ )
            {
                rps->offset[i] = ps->offset[i];
                rps->delta[i] = ps->delta[i];
                rps->used[i] = false;
                rps->fetched[i] = false;
            }
            rps->useCount = 1;
            rps->startedPrefetch = false;

            /* 
             * Mark the address that triggered reconstruction as 
             * fetched and used 
             */
            rps->used[0] = rps->fetched[0] = true;

            ReconBuf.insert( std::pair<uint64_t, PatternSequence*>( pc, rps ) );
        }

#ifdef DBGPF
        std::cout << "Found a PST entry for PC 0x" << std::hex
                  << triggerOp->programCounter << std::dec << std::endl;
        
        std::cout << "Triggered by 0x" << std::hex
                  << triggerOp->address.GetPhysicalAddress( ) << std::dec
                  << std::endl;
        
        std::cout << "Start address 0x" << std::hex << ps->address 
            << ": " << std::dec;
        
        for( uint64_t i = 0; i < ps->size; i++ )
        {
            std::cout << "[" << ps->offset[i] << "," << ps->delta[i] << "], ";
        }

        std::cout << std::endl;
#endif

    }
    else
    {
        /* 
         * If there is no entry in the PST for this PC, start building an 
         * AGT entry.
         */ 
        /* Check one of the AGT buffers for misses at this PC */
        if( AGT.count( triggerOp->programCounter ) )
        {
            uint64_t address = triggerOp->address.GetPhysicalAddress( );
            uint64_t pc = triggerOp->programCounter;
            PatternSequence *ps = AGT[pc];

            /* 
             * If a buffer for this PC exists, append to it. If the buffer size
             * exceeds some threshold (say 4) and matches something in the pattern
             * table, issue a prefetch for the remaining items in the pattern (up to
             * say 8).
             */

            uint64_t addressDiff;
 
            addressDiff = ((address > ps->address) ? 
                    (address - ps->address) : (ps->address - address));

            if( ( addressDiff / 64 ) < 256 )
            {
                ps->offset[ps->size] = address - ps->address;
                ps->delta[ps->size] = 0;
                ps->size++;

                /* 
                 * If a buffer exists and the size exceeds some threshold, 
                 * but does not match anything in the pattern table, add this 
                 * buffer to the pattern table
                 */
                if( ps->size >= 8 )
                {
                    PST.insert( std::pair<uint64_t, PatternSequence*>( pc, ps ) );

                    std::map<uint64_t, PatternSequence*>::iterator it;
                    it = AGT.find( pc );
                    AGT.erase( it );
                }
            }
        }
        /* If a buffer does not exist, create one. */
        else
        {
            PatternSequence *ps = new PatternSequence;
            uint64_t pc = triggerOp->programCounter;
            
            ps->address = triggerOp->address.GetPhysicalAddress( );
            ps->size = 1;
            ps->offset[0] = 0;
            ps->delta[0] = 0;
            
            AGT.insert( std::pair<uint64_t, PatternSequence*>( pc, ps ) );
        }
    }

    return false;
}
