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
*                     Website: http://www.cse.psu.edu/~tzz106/ )
*******************************************************************************/

#include <iostream>
#include "src/TranslationMethod.h"

using namespace NVM;

TranslationMethod::TranslationMethod( )
{
    /* Set some default translation method:
     *
     * The order is channel - rank - row - bank - col from MSB to LSB.
     * The method is for a 256 MB memory => 29 bits total.
     * The bits widths for each are 1 - 1 - 16 - 3 - 8
     */
    SetBitWidths( 16, 8, 3, 1, 1 );
    SetOrder( 3, 1, 2, 4, 5 );
}

TranslationMethod::~TranslationMethod( )
{
}

void TranslationMethod::SetBitWidths( unsigned int rowBits, unsigned int colBits, unsigned int bankBits,
				      unsigned int rankBits, unsigned int channelBits )
{
    if( colBits < 8 )
        std::cout << "NVMain: Column bits must be greater than or equal to the maximum burst length.\n";

    bitWidths[MEM_ROW] = rowBits;
    /*
     *  We subtract 3 from the column bits since we can only address by byte. When translation
     *  occurs, we will align to the nearest byte in the currently open row, then N bits will
     *  be output in a burst, depending on the device with. In the translator you must return
     *  the bit address, however, so simply shift by 3 to the left to pad with 0s. 
     */
    bitWidths[MEM_COL] = colBits - 3;
    bitWidths[MEM_BANK] = bankBits;
    bitWidths[MEM_RANK] = rankBits;
    bitWidths[MEM_CHANNEL] = channelBits;
}

void TranslationMethod::SetOrder( int row, int col, int bank, int rank, int channel )
{
    if( row == col || row == bank || row == rank || row == channel
        || col == bank || col == rank || col == channel
        || bank == rank || bank == channel || rank == channel )
    {
        std::cout << "Translation Method: Orders are not unique!" << std::endl;
    }

    order[MEM_ROW] = row - 1;
    order[MEM_COL] = col - 1;
    order[MEM_BANK] = bank - 1;
    order[MEM_RANK] = rank - 1;
    order[MEM_CHANNEL] = channel - 1;
}

void TranslationMethod::SetCount( uint64_t rows, uint64_t cols, 
                                  uint64_t banks, uint64_t ranks, uint64_t channels )
{
    count[MEM_ROW] = rows;
    count[MEM_COL] = cols;
    count[MEM_BANK] = banks;
    count[MEM_RANK] = ranks;
    count[MEM_CHANNEL] = channels;
}

void TranslationMethod::GetBitWidths( unsigned int *rowBits, unsigned int *colBits, unsigned int *bankBits,
				      unsigned int *rankBits, unsigned int *channelBits )
{
    *rowBits = bitWidths[MEM_ROW];
    *colBits = bitWidths[MEM_COL];
    *bankBits = bitWidths[MEM_BANK];
    *rankBits = bitWidths[MEM_RANK];
    *channelBits = bitWidths[MEM_CHANNEL];
}

void TranslationMethod::GetOrder( int *row, int *col, int *bank, int *rank, int *channel )
{
    *row = order[MEM_ROW];
    *col = order[MEM_COL];
    *bank = order[MEM_BANK];
    *rank = order[MEM_RANK];
    *channel = order[MEM_CHANNEL];
}

void TranslationMethod::GetCount( uint64_t *rows, uint64_t *cols, uint64_t *banks, 
                                  uint64_t *ranks, uint64_t *channels )
{
    *rows = count[MEM_ROW];
    *cols = count[MEM_COL];
    *banks = count[MEM_BANK];
    *ranks = count[MEM_RANK];
    *channels = count[MEM_CHANNEL];
}

/*
 * Set the address mapping scheme
 * "R"-Row, "C"-Column, "BK"-Bank, "RK"-Rank, "CH"-Channel
 */
void TranslationMethod::SetAddressMappingScheme( std::string scheme )
{
    /* maximize row buffer hit */
    if( !scheme.compare( "R:RK:BK:CH:C" ) )
        SetOrder( 5, 1, 3, 4, 2 );
    else if( !scheme.compare( "R:BK:RK:CH:C" ) )
        SetOrder( 5, 1, 4, 3, 2 );
    else if( !scheme.compare( "R:CH:BK:RK:C" ) )
        SetOrder( 5, 1, 3, 2, 4 );
    else if( !scheme.compare( "R:BK:CH:RK:C" ) )
        SetOrder( 5, 1, 4, 2, 3 );
    else if( !scheme.compare( "R:CH:RK:BK:C" ) )
        SetOrder( 5, 1, 2, 3, 4 );
    else if( !scheme.compare( "R:RK:CH:BK:C" ) )
        SetOrder( 5, 1, 2, 4, 3 );
    /* channel interleaving */
    else if( !scheme.compare( "R:RK:BK:C:CH" ) )
        SetOrder( 5, 2, 3, 4, 1 );
    else if( !scheme.compare( "R:BK:RK:C:CH" ) )
        SetOrder( 5, 2, 4, 3, 1 );
    else if( !scheme.compare( "R:C:BK:RK:CH" ) )
        SetOrder( 5, 4, 3, 2, 1 );
    else if( !scheme.compare( "R:BK:C:RK:CH" ) )
        SetOrder( 5, 3, 4, 2, 1 );
    else if( !scheme.compare( "R:C:RK:BK:CH" ) )
        SetOrder( 5, 4, 2, 3, 1 );
    else if( !scheme.compare( "R:RK:C:BK:CH" ) )
        SetOrder( 5, 3, 2, 4, 1 );
    /* bank interleaving */
    else if( !scheme.compare( "R:RK:CH:C:BK" ) )
        SetOrder( 5, 2, 1, 4, 3 );
    else if( !scheme.compare( "R:CH:RK:C:BK" ) )
        SetOrder( 5, 2, 1, 3, 4 );
    else if( !scheme.compare( "R:C:CH:RK:BK" ) )
        SetOrder( 5, 4, 1, 2, 3 );
    else if( !scheme.compare( "R:CH:C:RK:BK" ) )
        SetOrder( 5, 3, 1, 2, 4 );
    else if( !scheme.compare( "R:C:RK:CH:BK" ) )
        SetOrder( 5, 4, 1, 3, 2 );
    else if( !scheme.compare( "R:RK:C:CH:BK" ) )
        SetOrder( 5, 3, 1, 4, 2 );
    /* rank interleaving */
    else if( !scheme.compare( "R:BK:CH:C:RK" ) )
        SetOrder( 5, 2, 4, 1, 3 );
    else if( !scheme.compare( "R:CH:BK:C:RK" ) )
        SetOrder( 5, 2, 3, 1, 4 );
    else if( !scheme.compare( "R:C:CH:BK:RK" ) )
        SetOrder( 5, 4, 2, 1, 3 );
    else if( !scheme.compare( "R:CH:C:BK:RK" ) )
        SetOrder( 5, 3, 2, 1, 4 );
    else if( !scheme.compare( "R:C:BK:CH:RK" ) )
        SetOrder( 5, 4, 3, 1, 2 );
    else if( !scheme.compare( "R:BK:C:CH:RK" ) )
        SetOrder( 5, 3, 4, 1, 2 );
    else
    {
        SetOrder( 5, 1, 3, 4, 2 );
        std::cout << "NVMain Warning: Unrecognized address mapping scheme."
            << " Set the default scheme: R:RK:BK:CH:C." << std::endl;
    }
}
