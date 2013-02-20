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

#include "MemControl/PerfectMemory/PerfectMemory.h"

#ifndef TRACE
  #include "base/statistics.hh"
  #include "base/types.hh"
  #include "sim/core.hh"
  #include "sim/stat_control.hh"
#endif

#include <iostream>

using namespace NVM;

/*
 *  This simple memory controller is an example memory controller for NVMain, 
 *  it operates as follows:
 *
 *  - Commands are returned immediately to the simulator as if the memory access 
 *    latency is 0.
 */
PerfectMemory::PerfectMemory( Interconnect *memory, AddressTranslator *translator )
{
    /*
     *  First, we need to define how the memory is translated. The main() 
     *  function already *  sets the width of each partition based on the number 
     *  of channels, ranks, banks, rows, and *  columns. 
     *  For example, 4 banks *  would be 2-bits. 1 channel is 0-bits. 
     *  1024 columns is 10 bits *  of the address. Here, you just need to 
     *  specify what order the partitions are in.
     *
     *  In the set order function, you define the order of the row, column, 
     *  bank, rank then channel.
     *  For example, to set column as the lowest bits, it must be 1st in 
     *  the order, so the second parameter should be set to 1.
     *
     *  In this system, the address is broken up as follows:
     *
     *  ------------------------------------------------------------
     *  |  CHANNEL   |     ROW     |  RANK  | BANK |    COLUMN     |
     *  ------------------------------------------------------------
     *
     *  So the orders are column first, rank second, bank third, row fourth, 
     *  channel fifth.
     *
     *  void SetOrder( int row, int col, int bank, int rank, int channel );
     */
    translator->GetTranslationMethod( )->SetOrder( 4, 1, 2, 3, 5 );

    /*
     *  We'll need these classes later, so copy them. the "memory" and 
     *  "translator" variables are *  defined in the protected section of 
     *  the MemoryController base class. 
     */
    SetMemory( memory );
    SetTranslator( translator );

    std::cout << "Created a Perfect Memory memory controller!" << std::endl;
}

/*
 * This method is called whenever a new transaction from the processor issued to
 * this memory controller / channel. All scheduling decisions should be made here.
 */
bool PerfectMemory::IssueCommand( NVMainRequest *req )
{
    GetEventQueue()->InsertEvent( EventResponse, this, req, 
            GetEventQueue()->GetCurrentCycle()+1 );

    /* 
     * Return whether the request could be queued. 
     * Return false if the queue is full 
     */
    return true;
}

void PerfectMemory::Cycle( ncycle_t )
{
}

void PerfectMemory::PrintStats( )
{
#ifndef TRACE
    if( GetConfig( )->KeyExists( "CTL_DUMP" ) 
            && GetConfig( )->GetString( "CTL_DUMP" ) == "true" )
        Stats::schedStatEvent( true, false );
#endif
}
