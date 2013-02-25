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

#include <iostream>
#include <cstdlib>


#include "src/AddressTranslator.h"


using namespace NVM;

AddressTranslator::AddressTranslator( )
{
    method = NULL;
    defaultField = NO_FIELD;
}


AddressTranslator::~AddressTranslator( )
{
}


void AddressTranslator::SetTranslationMethod( TranslationMethod *m )
{
    method = m;
}


TranslationMethod *AddressTranslator::GetTranslationMethod( )
{
    return method;
}

uint64_t AddressTranslator::ReverseTranslate( const uint64_t& row, 
                                              const uint64_t& col, 
                                              const uint64_t& bank,
				              const uint64_t& rank, 
                                              const uint64_t& channel )
{
    uint64_t unitAddr = 1;
    uint64_t phyAddr = 0;
    MemoryPartition part;

    if( GetTranslationMethod( ) == NULL )
    {
        std::cerr << "Divider Translator: Translation method not specified!" << std::endl;
        exit(1);
    }
    
    uint64_t rowNum, colNum, bankNum, rankNum, channelNum;

    GetTranslationMethod( )->GetCount( &rowNum, &colNum, &bankNum, &rankNum, &channelNum );
    
    for( int i = 0; i < 5 ; i++ )
    {
        /* 0->4, low to high, FindOrder() will find the correct one */
        FindOrder( i, &part );

        switch( part )
        {
            case MEM_ROW:
                  phyAddr += ( row * unitAddr ); 
                  unitAddr *= rowNum;
                  break;

            case MEM_COL:
                  phyAddr += ( col * unitAddr ); 
                  unitAddr *= colNum;
                  break;

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

    return phyAddr;
} 

/*
 * Translate() translates the physical address and returns the corresponding
 * values of each memory domain 
 */
void AddressTranslator::Translate( uint64_t address, uint64_t *row, uint64_t *col, uint64_t *bank,
				   uint64_t *rank, uint64_t *channel )
{
    uint64_t refAddress;
    MemoryPartition part;

    uint64_t *partitions[5] = { row, col, bank, rank, channel };

    if( GetTranslationMethod( ) == NULL )
    {
        std::cerr << "Divider Translator: Translation method not specified!" << std::endl;
        return;
    }


    /* NVMain assumes memory word addresses! */
    //address = address >> 6; // TODO: make this the config value of cacheline size
    refAddress = address;


    /* Find the partition that is first in order. */
    FindOrder( 0, &part );

    /* 
     *  The new memsize does not include this partition, so dividing by the
     *  new memsize will give us the right channel/rank/bank/whatever.
     */
    *partitions[part] = Modulo( refAddress, part );

    /*
     *  "Mask off" the first partition number we got. For example if memsize = 1000
     *  and the address is 8343, the partition would be 8, and we will look at 343 
     *  to determine the rest of the address now.
     */
    refAddress = Divide( refAddress, part );


    /* Next find the 2nd in order, and repeat the process. */
    FindOrder( 1, &part );
    *partitions[part] = Modulo( refAddress, part );
    refAddress = Divide( refAddress, part );

    /* 3rd... */
    FindOrder( 2, &part );
    *partitions[part] = Modulo( refAddress, part );
    refAddress = Divide( refAddress, part );


    /* 4th... */
    FindOrder( 3, &part );
    *partitions[part] = Modulo( refAddress, part );
    refAddress = Divide( refAddress, part );


    /* and 5th... */
    FindOrder( 4, &part );
    *partitions[part] = Modulo( refAddress, part );
    refAddress = Divide( refAddress, part );
} 

uint64_t AddressTranslator::Translate( uint64_t address )
{
    uint64_t row, col, bank, rank, channel;
    uint64_t rv;

    Translate( address, &row, &col, &bank, &rank, &channel );

    switch( defaultField )
    {
        case ROW_FIELD:
            rv = row;
            break;

        case COL_FIELD:
            rv = col;
            break;

        case BANK_FIELD:
            rv = bank;
            break;

        case RANK_FIELD:
            rv = rank;
            break;

        case CHANNEL_FIELD:
            rv = channel;
            break;

        case NO_FIELD:
        default:
            rv = 0;
            break;
    }

    return rv;
}

void AddressTranslator::SetDefaultField( TranslationField f )
{
    defaultField = f;
}

/*
 * Divide() right shift the physical address for address translation
 */
uint64_t AddressTranslator::Divide( uint64_t partSize, MemoryPartition partition )
{
    uint64_t retSize = partSize;
    uint64_t numChannels, numRanks, numBanks, numRows, numCols;

    method->GetCount( &numRows, &numCols, &numBanks, &numRanks, &numChannels );
    
    if( partition == MEM_ROW )
        retSize /= numRows;
    else if( partition == MEM_COL )
        retSize /= numCols;
    else if( partition == MEM_BANK )
        retSize /= numBanks;
    else if( partition == MEM_RANK )
        retSize /= numRanks;
    else if( partition == MEM_CHANNEL )
        retSize /= numChannels;
    else
        std::cout << "Divider Translator: Warning: Invalid partition " << (int)partition << std::endl;

    return retSize;
}

/*
 * Modulo() extract the corresponding bits for translation
 */
uint64_t AddressTranslator::Modulo( uint64_t partialAddr, MemoryPartition partition )
{
    uint64_t retVal = partialAddr;
    uint64_t numChannels, numRanks, numBanks, numRows, numCols;

    method->GetCount( &numRows, &numCols, &numBanks, &numRanks, &numChannels );
    
    if( partition == MEM_ROW )
        retVal = partialAddr % numRows;
    else if( partition == MEM_COL )
        retVal = partialAddr % numCols;
    else if( partition == MEM_BANK )
        retVal = partialAddr % numBanks;
    else if( partition == MEM_RANK )
        retVal = partialAddr % numRanks;
    else if( partition == MEM_CHANNEL )
        retVal = partialAddr % numChannels;
    else
        std::cout << "Modulo Translator: Warning: Invalid partition " << (int)partition << std::endl;

    return retVal;
}

/*
 * FindOrder() finding the right memory domain that matches the input order
 */
void AddressTranslator::FindOrder( int order, MemoryPartition *p )
{
    unsigned int rowBits, colBits, bankBits, rankBits, channelBits;
    int rowOrder, colOrder, bankOrder, rankOrder, channelOrder;

    method->GetBitWidths( &rowBits, &colBits, &bankBits, &rankBits, &channelBits );
    method->GetOrder( &rowOrder, &colOrder, &bankOrder, &rankOrder, &channelOrder );

    if( rowOrder == order )
        *p = MEM_ROW;
    else if( colOrder == order )
        *p = MEM_COL;
    else if( bankOrder == order )
        *p = MEM_BANK;
    else if( rankOrder == order )
        *p = MEM_RANK;
    else if( channelOrder == order )
        *p = MEM_CHANNEL;
    else
        std::cerr << "Address Translator: No order " << order << std::endl << "Row = " << rowOrder
	      << " Col = " << colOrder << " Bank = " << bankOrder << " Rank = " << rankOrder
	      << " Chan = " << channelOrder << std::endl;
}
