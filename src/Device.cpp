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
*******************************************************************************/

#include "src/Device.h"
#include <iostream>

using namespace NVM;

Device::Device( )
{
    count = 0;
    banks.clear( );
}

Device::~Device( )
{
    banks.clear( );
}

void Device::AddBank( Bank *newBank )
{
    banks.push_back( newBank );
    count++;
}

Bank *Device::GetBank( uint64_t bankId )
{
    if( bankId >= count )
    {
        std::cout << "NVMain: Attempted to get non-existant bank: " 
            << bankId << std::endl;
        return NULL;
    }

    return banks[bankId];
}

uint64_t Device::GetBankCount( )
{
    return count;
}

bool Device::PowerUp( uint64_t whichBank )
{
    bool returnValue = false;
    NVMainRequest req;


    req.bulkCmd = CMD_NOP;
    req.type = POWERUP;

    /* If we can issue a power up, send it to all banks.  */
    if( CanPowerUp( whichBank ) && whichBank < count )
    {
        banks[whichBank]->PowerUp( &req );

        returnValue = true;
    }

    return returnValue;
}

bool Device::PowerDown( bool fastExit )
{
    bool returnValue = false;
    bool allIdle = true;

    /*
     *  To determine the power down state, we need to check if
     *  all the banks are idle.
     *
     *  All idle banks -> Precharge Power Down Slow or Fast Exit
     *  Any banks active -> Active Power Down Fast Exit
     */
    /* TODO: Add the other bank states here... */
    for( uint64_t i = 0; i < count; i++ )
        if( banks[i]->GetState( ) == BANK_OPEN )
            allIdle = false;
    
    /*
     *  Issue the power down command in the wait state, so the bank will
     *  automatically transition to the powered down state.
     */
    if( CanPowerDown( ( ( fastExit ) ? POWERDOWN_PDPF : POWERDOWN_PDPS ) ) && allIdle )
    {
        if( fastExit )
        {
            for( uint64_t i = 0; i < count; i++ )
                banks[i]->PowerDown( BANK_PDPF );
        }
        else
        {
            for( uint64_t i = 0; i < count; i++ )
                banks[i]->PowerDown( BANK_PDPS );
        }
        
        returnValue = true;
    }
    else if( CanPowerDown( POWERDOWN_PDA ) && !allIdle )
    {
        for( uint64_t i = 0; i < count; i++ )
            banks[i]->PowerDown( BANK_PDA );

        returnValue = true;
    }

    return returnValue;
}

bool Device::CanPowerUp( uint64_t whichBank )
{
    bool issuable = true;
    NVMainRequest req;
    NVMAddress address;

    /*
     *  Create a dummy operation to determine if we can issue.
     */
    req.type = POWERUP;
    req.address.SetTranslatedAddress( 0, 0, whichBank, 0, 0 );
    req.address.SetPhysicalAddress( 0 );

    if( whichBank >= count || !banks[whichBank]->IsIssuable( &req ) )
        issuable = false;


    return issuable;
}

bool Device::CanPowerDown( OpType pdOp )
{
    bool issuable = true;
    NVMainRequest req;
    NVMAddress address;

    /*
     *  Create a dummy operation to determine if we can issue.
     */
    req.type = pdOp;
    req.address.SetTranslatedAddress( 0, 0, 0, 0, 0 );
    req.address.SetPhysicalAddress( 0 );

    for( uint64_t i = 0; i < count; i++ )
        if( !banks[i]->IsIssuable( &req ) )
        {
            issuable = false;
            break;
        }

    return issuable;
}



void Device::Cycle( ncycle_t )
{
}

