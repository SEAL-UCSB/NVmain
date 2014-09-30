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

#include "MemControl/MemoryControllerFactory.h"
#include "MemControl/FCFS/FCFS.h"
#include "MemControl/FRFCFS/FRFCFS.h"
#include "MemControl/FRFCFS-WQF/FRFCFS-WQF.h"
#include "MemControl/PerfectMemory/PerfectMemory.h"
#include "MemControl/DRAMCache/DRAMCache.h"
#include "MemControl/LH-Cache/LH-Cache.h"
#include "MemControl/LO-Cache/LO-Cache.h"
#include "MemControl/PredictorDRC/PredictorDRC.h"

#include <iostream>

using namespace NVM;

MemoryController *MemoryControllerFactory::CreateNewController( std::string controller ) 
{
    MemoryController *memoryController = NULL;

    if( controller == "" )
        std::cout << "NVMain: MEM_CTL is not set in configuration file!" << std::endl;

    if( controller == "FCFS" )
        memoryController = new FCFS( );
    else if( controller == "FRFCFS" )
        memoryController = new FRFCFS( );
    else if( controller == "FRFCFS-WQF" || controller == "FRFCFS_WQF" )
        memoryController = new FRFCFS_WQF( );
    else if( controller == "PerfectMemory" )
        memoryController = new PerfectMemory( );
    else if( controller == "DRC" )
        memoryController = new DRAMCache( );
    else if( controller == "LH_Cache" )
        memoryController = new LH_Cache( );
    else if( controller == "LO_Cache" )
        memoryController = new LO_Cache( );
    else if( controller == "PredictorDRC" )
        memoryController = new PredictorDRC( );

    if( memoryController == NULL )
        std::cout << "NVMain: Unknown memory controller `" 
            << controller << "'." << std::endl;

    return memoryController;
}
