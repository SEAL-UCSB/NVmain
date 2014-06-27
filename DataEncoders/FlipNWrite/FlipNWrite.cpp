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

#include "DataEncoders/FlipNWrite/FlipNWrite.h"

#include <iostream>

using namespace NVM;

FlipNWrite::FlipNWrite( )
{
    flippedAddresses.clear( );

    /* Clear statistics */
    bitsFlipped = 0;
    bitCompareSwapWrites = 0;
}

FlipNWrite::~FlipNWrite( )
{
    /*
     *  Nothing to do here. We do not own the *config pointer, so
     *  don't delete that.
     */
}

void FlipNWrite::SetConfig( Config *config, bool /*createChildren*/ )
{
    Params *params = new Params( );
    params->SetParams( config );
    SetParams( params );

    /* Cache granularity size. */
    fpSize = config->GetValue( "FlipNWriteGranularity" );

    /* Some default size if the parameter is not specified */
    if( fpSize == -1 )
        fpSize = 32; 
}

void FlipNWrite::RegisterStats( )
{
    AddStat(bitsFlipped);
    AddStat(bitCompareSwapWrites);
    AddUnitStat(flipNWriteReduction, "%");
}

void FlipNWrite::InvertData( NVMDataBlock& data, uint64_t startBit, uint64_t endBit )
{
    uint64_t wordSize;
    int startByte, endByte;

    wordSize = p->BusWidth;
    wordSize *= p->tBURST * p->RATE;
    wordSize /= 8;

    startByte = (int)(startBit / 8);
    endByte = (int)((endBit - 1) / 8);

    for( int i = startByte; i <= endByte; i++ )
    {
        uint8_t originalByte = data.GetByte( i );
        uint8_t shiftByte = originalByte;
        uint8_t newByte = 0;

        for( int j = 0; j < 8; j++ )
        {
            uint64_t currentBit = i * 8 + j;
           
            if( currentBit < startBit || currentBit >= endBit )
            {
                shiftByte = static_cast<uint8_t>(shiftByte >> 1);
                continue;
            }

            if( !(shiftByte & 0x1) )
            {
                newByte = static_cast<uint8_t>(newByte | (1 << (7-j)));
            }

            shiftByte = static_cast<uint8_t>(shiftByte >> 1);
        }

        data.SetByte( i, newByte );
    }
}

ncycle_t FlipNWrite::Read( NVMainRequest* /*request*/ )
{
    ncycle_t rv = 0;

    // TODO: Add some energy here

    return rv;
}

ncycle_t FlipNWrite::Write( NVMainRequest *request ) 
{
    NVMDataBlock& newData = request->data;
    NVMDataBlock& oldData = request->oldData;
    NVMAddress address = request->address;

    /*
     *  The default life map is an stl map< uint64_t, uint64_t >. 
     *  You may map row and col to this map_key however you want.
     *  It is up to you to ensure there are no collisions here.
     */
    uint64_t row;
    uint64_t col;
    ncycle_t rv = 0;

    request->address.GetTranslatedAddress( &row, &col, NULL, NULL, NULL, NULL );

    /*
     *  If using the default life map, we can call the DecrementLife
     *  function which will check if the map_key already exists. If so,
     *  the life value is decremented (write count incremented). Otherwise 
     *  the map_key is inserted with a write count of 1.
     */
    uint64_t rowSize;
    uint64_t wordSize;
    uint64_t currentBit;
    uint64_t flipPartitions;
    uint64_t rowPartitions;
    int *modifyCount;

    wordSize = p->BusWidth;
    wordSize *= p->tBURST * p->RATE;
    wordSize /= 8;

    rowSize = p->COLS * wordSize;
    rowPartitions = ( rowSize * 8 ) / fpSize;
    
    flipPartitions = ( wordSize * 8 ) / fpSize; 

    modifyCount = new int[ flipPartitions ];

    /*
     *  Count the number of bits that are modified. If it is more than
     *  half, then we will invert the data then write.
     */
    for( uint64_t i = 0; i < flipPartitions; i++ )
        modifyCount[i] = 0;

    currentBit = 0;

    /* Get what is currently in the memory (i.e., if it was previously flipped, get the flipped data. */
    for( uint64_t i = 0; i < flipPartitions; i++ )
    {
        uint64_t curAddr = row * rowPartitions + col * flipPartitions + i;

        if( flippedAddresses.count( curAddr ) )
        {
            InvertData( oldData, i*fpSize, (i+1)*fpSize );
        }
    }

    /* Check each byte to see if it was modified */
    for( uint64_t i = 0; i < wordSize; ++i )
    {
        /*
         *  If no bytes have changed we can just continue. Yes, I know this
         *  will check the byte 8 times, but i'd rather not change the iter.
         */
        uint8_t oldByte, newByte;

        oldByte = oldData.GetByte( i );
        newByte = newData.GetByte( i );

        if( oldByte == newByte )
        {
            currentBit += 8;
            continue;
        }

        /*
         *  If the bytes are different, then at least one bit has changed.
         *  check each bit individually.
         */
        for( int j = 0; j < 8; j++ )
        {
            uint8_t oldBit, newBit;

            oldBit = ( oldByte >> j ) & 0x1;
            newBit = ( newByte >> j ) & 0x1;

            if( oldBit != newBit )
            {
                modifyCount[(int)(currentBit/fpSize)]++;
            }

            currentBit++;
        }
    }

    /*
     *  Flip any partitions as needed and mark them as inverted or not.
     */
    for( uint64_t i = 0; i < flipPartitions; i++ )
    {
        bitCompareSwapWrites += modifyCount[i];

        uint64_t curAddr = row * rowPartitions + col * flipPartitions + i;

        /* Invert if more than half of the bits are modified. */
        if( modifyCount[i] > (fpSize / 2) )
        {
            InvertData( newData, i*fpSize, (i+1)*fpSize );

            bitsFlipped += (fpSize - modifyCount[i]);

            /*
             *  Mark this address as flipped. If the data was already inverted, it
             *  should remain as inverted for the new data.
             */
            if( !flippedAddresses.count( curAddr ) )
            {
                flippedAddresses.insert( curAddr );
            }
        }
        else
        {
            /*
             *  This data is not inverted and should not be marked as such.
             */
            if( flippedAddresses.count( curAddr ) )
            {
                flippedAddresses.erase( curAddr );
            }

            bitsFlipped += modifyCount[i];
        }
    }

    delete modifyCount;
    
    return rv;
}

void FlipNWrite::CalculateStats( )
{
    if( bitCompareSwapWrites != 0 )
        flipNWriteReduction = (((double)bitsFlipped / (double)bitCompareSwapWrites)*100.0);
    else
        flipNWriteReduction = 100.0;
}
