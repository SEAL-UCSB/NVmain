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

#include "include/NVMAddress.h"

using namespace NVM;

NVMAddress::NVMAddress( )
{
    translated = false;
    hasPhysicalAddress = false;
    physicalAddress = 0;
    subarray = row = col = bank = rank = channel = 0;
}

NVMAddress::~NVMAddress( )
{
}

NVMAddress::NVMAddress( uint64_t addrRow, uint64_t addrCol, uint64_t addrBank,
                        uint64_t addrRank, uint64_t addrChannel, uint64_t addrSA )
{
    SetTranslatedAddress( addrRow, addrCol, addrBank, addrRank, addrChannel, addrSA );
}

void NVMAddress::SetTranslatedAddress( uint64_t addrRow, uint64_t addrCol, uint64_t addrBank, 
                                       uint64_t addrRank, uint64_t addrChannel, uint64_t addrSA )
{
    translated = true;
    row = addrRow;
    col = addrCol;
    bank = addrBank;
    rank = addrRank;
    channel = addrChannel;
    subarray = addrSA;
}

void NVMAddress::SetPhysicalAddress( uint64_t pAddress )
{
    hasPhysicalAddress = true;
    physicalAddress = pAddress;
}

void NVMAddress::SetBitAddress( uint8_t bitAddr )
{
    bit = bitAddr;
}

void NVMAddress::GetTranslatedAddress( uint64_t *addrRow, uint64_t *addrCol, uint64_t *addrBank, 
                                       uint64_t *addrRank, uint64_t *addrChannel, uint64_t *addrSA )
{
    if( addrRow ) *addrRow = row;
    if( addrCol ) *addrCol = col;
    if( addrBank ) *addrBank = bank;
    if( addrRank ) *addrRank = rank;
    if( addrChannel ) *addrChannel = channel;
    if( addrSA ) *addrSA = subarray;
}

uint64_t NVMAddress::GetPhysicalAddress( )
{
    return physicalAddress;
}

uint64_t NVMAddress::GetBitAddress( )
{
    return bit;
}

uint64_t NVMAddress::GetRow( )
{
    return row;
}

uint64_t NVMAddress::GetCol( )
{
    return col;
}

uint64_t NVMAddress::GetBank( )
{
    return bank;
}

uint64_t NVMAddress::GetRank( )
{
    return rank;
}

uint64_t NVMAddress::GetChannel( )
{
    return channel;
}

uint64_t NVMAddress::GetSubArray( )
{
    return subarray;
}

bool NVMAddress::IsTranslated( )
{
    return translated;
}

bool NVMAddress::HasPhysicalAddress( )
{
    return hasPhysicalAddress;
}

NVMAddress& NVMAddress::operator=( const NVMAddress& m )
{
    translated = m.translated;
    hasPhysicalAddress = m.hasPhysicalAddress;
    physicalAddress = m.physicalAddress;
    row = m.row;
    col = m.col;
    bank = m.bank;
    rank = m.rank;
    channel = m.channel;
    subarray = m.subarray;
    bit = m.bit;

    return *this;
}
