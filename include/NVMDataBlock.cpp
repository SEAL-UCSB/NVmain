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

#include "include/NVMDataBlock.h"
#include <iomanip>

using namespace NVM;

NVMDataBlock::NVMDataBlock( )
{
    data.clear( );
    mask.clear( );
    rawData = NULL;
    isValid = false;
}

NVMDataBlock::~NVMDataBlock( )
{
    data.clear( );
    mask.clear( );
}

uint8_t NVMDataBlock::GetByte( uint64_t byte )
{
    if( byte >= data.size( ) )
        return 0;

    return data[ byte ];
}

void NVMDataBlock::SetByte( uint64_t byte, uint8_t value )
{
    if( byte >= data.size( ) )
    {
        /* 
         *  There's probably some other way to do this, but extend
         *  the vector size by pushing 0s on the end.
         */
        for( size_t i = data.size( ); i <= byte; i++ )
            data.push_back( 0 );
    }

    data[ byte ] = value;
}

uint8_t NVMDataBlock::GetMask( uint64_t byte )
{
    if( byte >= mask.size( ) )
        return 0xFF;

    return mask[ byte ];
}

void NVMDataBlock::SetMask( uint64_t byte, uint8_t value )
{
    if( byte >= mask.size( ) )
    {
        /* 
         *  There's probably some other way to do this, but extend
         *  the vector size by pushing 0s on the end.
         */
        for( size_t i = mask.size( ); i <= byte; i++ )
            mask.push_back( 0xFF );
    }

    mask[ byte ] = value;
}

void NVMDataBlock::SetValid( bool valid )
{
    isValid = valid;
}

bool NVMDataBlock::IsValid( )
{
    return isValid;
}

void NVMDataBlock::Print( std::ostream& out ) const
{
    for( size_t i = 0; i < data.size( ); i++ )
        out << std::hex << std::setw( 2 ) << std::setfill( '0' ) 
            << (int)data[ data.size( ) - i - 1 ] << std::dec;
}

NVMDataBlock& NVMDataBlock::operator=( const NVMDataBlock& m )
{
    data.clear( );
    for( size_t it = 0; it < m.data.size( ); it++ )
    {
        data.push_back( m.data[it] );
    }
    rawData = m.rawData;

    return *this;
}

std::ostream& operator<<( std::ostream& out, const NVMDataBlock& obj )
{
    obj.Print( out );
    return out;
}
