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

#include "Decoders/DRCDecoder/DRCDecoder.h"
#include "include/NVMHelpers.h"
#include <iostream>
#include <cstdlib>

using namespace NVM;

DRCDecoder::DRCDecoder( )
{
    ignoreBits = 0;
    cachelineSize = 64;

    std::cout << "Created a DRC decoder!" << std::endl;
}

void DRCDecoder::SetIgnoreBits( uint64_t numIgnore )
{
    ignoreBits = numIgnore;
}

void DRCDecoder::SetCachelineSize( uint64_t lineSize )
{
    cachelineSize = lineSize;
}

void DRCDecoder::Translate( uint64_t address, uint64_t *row, uint64_t *col, 
                            uint64_t *bank, uint64_t *rank, uint64_t *channel, uint64_t *subarray )
{
    int rowOrder, colOrder, bankOrder, rankOrder, channelOrder, subarrayOrder;
    unsigned int rowBits, colBits, bankBits, rankBits, channelBits, subarrayBits;
    uint64_t workingAddr;

    /* 
     *  Get the widths and order from the translation method so we know what
     *  the user wants for bank/rank/channel ordering.
     */
    GetTranslationMethod( )->GetBitWidths( &rowBits, &colBits, &bankBits, 
            &rankBits, &channelBits, &subarrayBits );
    GetTranslationMethod( )->GetOrder( &rowOrder, &colOrder, &bankOrder, 
            &rankOrder, &channelOrder, &subarrayOrder );

    /* Chop off the cacheline length and ignore bits first */
    workingAddr = address;
    workingAddr = workingAddr >> mlog2( (int)cachelineSize );
    if( ignoreBits != 0 )
      workingAddr = workingAddr >> ignoreBits;

    /* Assume column is the first order mapping, we want consecutive cachelines to map to the same dram row */
    *col = workingAddr % (1 << colBits);
    workingAddr = workingAddr >> colBits;

    /* Subarray is ignored in our dram cache */
    *subarray = 0;

    /* Find out if bank, rank, or channel are first, then decode accordingly */
    if( channelOrder < rankOrder && channelOrder < bankOrder )
    {
        *channel = workingAddr % (1 << channelBits);
        workingAddr = workingAddr >> channelBits;

        if( rankOrder < bankOrder )
        {
            *rank = workingAddr % (1 << rankBits);
            workingAddr = workingAddr >> rankBits;

            *bank = workingAddr % (1 << bankBits);
            workingAddr = workingAddr >> bankBits;
        }
        else
        {
            *bank = workingAddr % (1 << bankBits);
            workingAddr = workingAddr >> bankBits;

            *rank = workingAddr % (1 << rankBits);
            workingAddr = workingAddr >> rankBits;
        }
    }
    /* Try rank first */
    else if( rankOrder < channelOrder && rankOrder < bankOrder )
    {
        *rank = workingAddr % (1 << rankBits);
        workingAddr = workingAddr >> rankBits;

        if( channelOrder < bankOrder )
        {
            *channel = workingAddr % (1 << channelBits);
            workingAddr = workingAddr >> channelBits;

            *bank = workingAddr % (1 << bankBits);
            workingAddr = workingAddr >> bankBits;
        }
        else
        {
            *bank = workingAddr % (1 << bankBits);
            workingAddr = workingAddr >> bankBits;

            *channel = workingAddr % (1 << channelBits);
            workingAddr = workingAddr >> channelBits;
        }
    }
    /* Bank first */
    else
    {
        *bank = workingAddr % (1 << bankBits);
        workingAddr = workingAddr >> bankBits;

        if( channelOrder < rankOrder )
        {
            *channel = workingAddr % (1 << channelBits);
            workingAddr = workingAddr >> channelBits;

            *rank = workingAddr % (1 << rankBits);
            workingAddr = workingAddr >> rankBits;
        }
        else
        {
            *rank = workingAddr % (1 << rankBits);
            workingAddr = workingAddr >> rankBits;

            *channel = workingAddr % (1 << channelBits);
            workingAddr = workingAddr >> channelBits;
        }
    }

    *row = workingAddr % (1 << rowBits);
    workingAddr = workingAddr >> rowBits;
}

uint64_t DRCDecoder::ReverseTranslate( const uint64_t& row, 
                                       const uint64_t& /*col*/, 
                                       const uint64_t& bank,
				       const uint64_t& rank, 
                                       const uint64_t& channel,
                                       const uint64_t& /*subarray*/)
{
    uint64_t unitAddr = 1;
    uint64_t phyAddr = 0;
    MemoryPartition part;

    if( GetTranslationMethod( ) == NULL )
    {
        std::cerr << "Divider Translator: Translation method not specified!" << std::endl;
        exit(1);
    }
    
    uint64_t rowNum, colNum, bankNum, rankNum, channelNum, subarrayNum;

    GetTranslationMethod( )->GetCount( &rowNum, &colNum, &bankNum, 
                                       &rankNum, &channelNum, &subarrayNum );

    /* 
     *  DRAM cache always ignores the cachline bits + any extra ignore bits to force 
     *  adjacent cachelines into the same DRAM page.
     */
    unitAddr *= cachelineSize;
    unitAddr *= (1 << ignoreBits);
    
    /*
     *  Note: DRAM cache only needs the bank, rank, and channel for pre/act/ref.
     *  The row may be needed for other reverse translations, but since forward
     *  translation overrides the row order (always at the end), they are not used
     *  in this loop.
     */
    for( int i = 0; i < 6 ; i++ )
    {
        /* 0->4, low to high, FindOrder() will find the correct one */
        FindOrder( i, &part );

        switch( part )
        {
            case MEM_BANK:
                  phyAddr += ( bank * unitAddr ); 
                  unitAddr *= bankNum;
                  break;

            case MEM_RANK:
                  phyAddr += ( rank * unitAddr ); 
                  unitAddr *= rankNum;
                  break;

            case MEM_CHANNEL:
                  phyAddr += ( channel * unitAddr ); 
                  unitAddr *= channelNum;
                  break;

            default:
                  break;
        }
    }

    /* Row always in upper bits as in forward translation. */
    phyAddr += ( row * unitAddr );

    return phyAddr;
} 
