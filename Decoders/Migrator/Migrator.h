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

#ifndef __COIN_MIGRATOR_H__
#define __COIN_MIGRATOR_H__

#include "src/AddressTranslator.h"
#include "src/Config.h"
#include "include/NVMAddress.h"

namespace NVM
{


enum MigratorState
{      
    MIGRATION_UNKNOWN = 0, // Error state
    MIGRATION_READING,     // Read in progress for this page
    MIGRATION_BUFFERED,    // Read is done, waiting for writes to be queued
    MIGRATION_WRITING,     // Writes queued, waiting for request complete
    MIGRATION_DONE         // Migration successfully completed
};


class Migrator : public AddressTranslator
{
  public:
    Migrator();
    ~Migrator();

    void SetConfig( Config *config, bool createChildren = true );

    virtual void Translate( uint64_t address, uint64_t *row, uint64_t *col, uint64_t *bank, 
                            uint64_t *rank, uint64_t *channel, uint64_t *subarray );
    using AddressTranslator::Translate;

    void StartMigration( NVMAddress& promotee, NVMAddress& demotee );
    void SetMigrationState( NVMAddress& address, MigratorState newState );
    bool Migrating( );
    bool IsBuffered( NVMAddress& address );
    bool IsMigrated( NVMAddress& address );

    void RegisterStats( );

    void CreateCheckpoint( std::string dir );
    void RestoreCheckpoint( std::string dir );

  private:
    std::map<uint64_t, uint64_t> migrationMap;
    std::map<uint64_t, MigratorState> migrationState;

    uint64_t numChannels, numBanks, numRanks, numSubarrays;

    /* Pages being swapped in and swapped out. */
    bool migrating;
    uint64_t inputPage, outputPage;

    ncounter_t migratedAccesses;

    uint64_t GetAddressKey( NVMAddress& address );

};


};


#endif

