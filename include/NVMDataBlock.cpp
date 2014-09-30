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
#include <cassert>
#include <cstring>
#include <iostream>

using namespace NVM;

NVMDataBlock::NVMDataBlock( )
{
    rawData = NULL;
    isValid = false;
    size = 0;
}

NVMDataBlock::~NVMDataBlock( )
{
    delete[] rawData;
    rawData = NULL;
}

void NVMDataBlock::SetSize( uint64_t s )
{
    assert( rawData == NULL );
    rawData = new uint8_t[s];
    size = s;
    isValid = true;
}

uint64_t NVMDataBlock::GetSize( )
{
    return size;
}

uint8_t NVMDataBlock::GetByte( uint64_t byte )
{
    uint8_t rv = 0;

    if( isValid && byte <= size )
    {
        rv = *((uint8_t*)(rawData)+byte);
    }

    return rv;
}

void NVMDataBlock::SetByte( uint64_t byte, uint8_t value )
{
    if( byte <= size )
    {
        rawData[byte] = value;
    }
    else
    {
        assert( false );
    }
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
    out << std::hex;
    for( uint64_t i = 0; i < size; i++ )
    {
        out << std::setw(2) << std::setfill('0') << (int)rawData[i];
    }
    out << std::dec;
}

NVMDataBlock& NVMDataBlock::operator=( const NVMDataBlock& m )
{
    if( m.rawData )
    {
        if( rawData == NULL )
            rawData = new uint8_t[m.size];
        memcpy(rawData, m.rawData, m.size);
    }
    isValid = m.isValid;
    size = m.size;

    return *this;
}

std::ostream& operator<<( std::ostream& out, const NVMDataBlock& obj )
{
    obj.Print( out );
    return out;
}
