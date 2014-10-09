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

#include "MemControl/PerfectMemory/PerfectMemory.h"
#include "src/EventQueue.h"

#ifndef TRACE
#ifdef GEM5
  #include "base/statistics.hh"
  #include "base/types.hh"
  #include "sim/core.hh"
  #include "sim/stat_control.hh"
#endif
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
PerfectMemory::PerfectMemory( )
{
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

void PerfectMemory::PrintStats( std::ostream& )
{
    /*
     *  This memory controller is considered and end-node, meaning requests
     *  will never go further than this object. Therefore, we don't need to
     *  print any child module stats, if any exist.
     */
}
