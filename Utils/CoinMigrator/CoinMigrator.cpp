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

#include "Utils/CoinMigrator/CoinMigrator.h"
#include "Decoders/Migrator/Migrator.h"
#include "NVM/nvmain.h"
#include "src/SubArray.h"
#include "src/EventQueue.h"
#include "include/NVMHelpers.h"

using namespace NVM;

CoinMigrator::CoinMigrator( )
{
    /*
     *  We will eventually be injecting requests to perform migration, so we
     *  would like IssueCommand to be called on the original request first so
     *  that we do not unintentially fill up the transaction queue causing 
     *  the original request triggering migration to fail.
     */
    SetHookType( NVMHOOK_BOTHISSUE );

    promoRequest = NULL;
    demoRequest = NULL;
    promoBuffered = false;
    demoBuffered = false;

    migrationCount = 0;
    queueWaits = 0;
    bufferedReads = 0;

    queriedMemory = false;
    promotionChannelParams = NULL;
    currentPromotionPage = 0;
}


CoinMigrator::~CoinMigrator( )
{

}


void CoinMigrator::Init( Config *config )
{
    /* 
     *  Our seed for migration probability. This should be a known constant if
     *  you wish to reproduce the same results each simulation.
     */
    seed = 1;

    /* Chance to migrate: 0 = 0%, 1.00 = 100%. */
    probability = 0.02; 
    config->GetEnergy( "CoinMigratorProbability", probability ); 

    /* Specifies with channel is the "fast" memory. */
    promotionChannel = 0;
    config->GetValueUL( "CoinMigratorPromotionChannel", promotionChannel );

    /* If we want to simulate additional latency serving buffered requests. */
    bufferReadLatency = 4;
    config->GetValueUL( "MigrationBufferReadLatency", bufferReadLatency );

    /* 
     *  We migrate entire rows between banks, so the column count needs to
     *  match across all channels for valid results.
     */
    numCols = config->GetValue( "COLS" );

    AddStat(migrationCount);
    AddStat(queueWaits);
    AddStat(bufferedReads);
}


bool CoinMigrator::IssueAtomic( NVMainRequest *request )
{
    /* For atomic mode, we just swap the pages instantly. */
    return TryMigration( request, true );
}


bool CoinMigrator::IssueCommand( NVMainRequest *request )
{
    /* 
     *  In cycle-accurate mode, we must read each page, buffer it, enqueue a
     *  write request, and wait for write completion.
     */
    return TryMigration( request, false );
}


bool CoinMigrator::RequestComplete( NVMainRequest *request )
{
    if( NVMTypeMatches(NVMain) && GetCurrentHookType( ) == NVMHOOK_PREISSUE )
    {
        /* Ensure the Migrator translator is used. */
        Migrator *migratorTranslator = dynamic_cast<Migrator *>(parent->GetTrampoline( )->GetDecoder( ));
        assert( migratorTranslator != NULL );

        if( request->owner == parent->GetTrampoline( ) && request->tag == MIG_READ_TAG )
        {
            /* A migration read completed, update state. */
            migratorTranslator->SetMigrationState( request->address, MIGRATION_BUFFERED ); 

            /* If both requests are buffered, we can attempt to write. */
            bool bufferComplete = false;

            if( (request == promoRequest 
                 && migratorTranslator->IsBuffered( demotee ))
                || (request == demoRequest
                 && migratorTranslator->IsBuffered( promotee )) )
            {
                bufferComplete = true;
            }

            /* Make a new request to issue for write. Parent will delete current pointer. */
            if( request == promoRequest )
            {
                promoRequest = new NVMainRequest( );
                *promoRequest = *request;
            }
            else if( request == demoRequest )
            {
                demoRequest = new NVMainRequest( );
                *demoRequest = *request;
            }
            else
            {
                assert( false );
            }

            /* Swap the address and set type to write. */
            if( bufferComplete )
            {
                /* 
                 *  Note: once IssueCommand is called, this hook may receive
                 *  a different parent, but fail the NVMTypeMatch check. As a
                 *  result we need to save a pointer to the NVMain class we
                 *  are issuing requests to.
                 */
                NVMObject *savedParent = parent->GetTrampoline( );

                NVMAddress tempAddress = promoRequest->address;
                promoRequest->address = demoRequest->address;
                demoRequest->address = tempAddress;

                demoRequest->type = WRITE;
                promoRequest->type = WRITE;

                demoRequest->tag = MIG_WRITE_TAG;
                promoRequest->tag = MIG_WRITE_TAG;

                /* Try to issue these now, otherwise we can try later. */
                bool demoIssued, promoIssued;

                demoIssued = savedParent->GetChild( demoRequest )->IssueCommand( demoRequest );
                promoIssued = savedParent->GetChild( promoRequest )->IssueCommand( promoRequest );

                if( demoIssued )
                {
                    migratorTranslator->SetMigrationState( demoRequest->address, MIGRATION_WRITING );
                }
                if( promoIssued )
                {
                    migratorTranslator->SetMigrationState( promoRequest->address, MIGRATION_WRITING );
                }

                promoBuffered = !promoIssued;
                demoBuffered = !demoIssued;
            }
        }
        /* A write completed. */
        else if( request->owner == parent->GetTrampoline( ) && request->tag == MIG_WRITE_TAG )
        {
            // Note: request should be deleted by parent
            migratorTranslator->SetMigrationState( request->address, MIGRATION_DONE );

            migrationCount++;
        }
        /* Some other request completed, see if we can ninja issue some migration writes that did not queue. */
        else if( promoBuffered || demoBuffered )
        {
            bool demoIssued, promoIssued;

            if( promoBuffered )
            {
                promoIssued = parent->GetTrampoline( )->GetChild( promoRequest )->IssueCommand( promoRequest );
                promoBuffered = !promoIssued;
            }

            if( demoBuffered )
            {
                demoIssued = parent->GetTrampoline( )->GetChild( demoRequest )->IssueCommand( demoRequest );
                demoBuffered = !demoIssued;
            }
        }
    }

    return true;
}


bool CoinMigrator::CheckIssuable( NVMAddress address, OpType type )
{
    NVMainRequest request;

    request.address = address;
    request.type = type;

    return parent->GetTrampoline( )->GetChild( &request )->IsIssuable( &request );
}


bool CoinMigrator::TryMigration( NVMainRequest *request, bool atomic )
{
    bool rv = true;

    if( NVMTypeMatches(NVMain) )
    {
        /* Ensure the Migrator translator is used. */
        Migrator *migratorTranslator = dynamic_cast<Migrator *>(parent->GetTrampoline( )->GetDecoder( ));
        assert( migratorTranslator != NULL );

        /* Migrations in progress must be served from the buffers during migration. */
        if( GetCurrentHookType( ) == NVMHOOK_PREISSUE && migratorTranslator->IsBuffered( request->address ) )
        {
            /* Short circuit this request so it is not queued. */
            rv = false;

            /* Complete the request, adding some buffer read latency. */
            GetEventQueue( )->InsertEvent( EventResponse, parent->GetTrampoline( ), request,
                              GetEventQueue()->GetCurrentCycle()+bufferReadLatency );

            bufferedReads++;

            return rv;
        }

        /* Don't inject results before the original is issued to prevent deadlock */
        if( GetCurrentHookType( ) != NVMHOOK_POSTISSUE )
        {
            return rv;
        }

        /* See if any migration is possible (i.e., no migration is in progress) */
        bool migrationPossible = false;

        if( !migratorTranslator->Migrating( ) 
            && !migratorTranslator->IsMigrated( request->address ) 
            && request->address.GetChannel( ) != promotionChannel )
        {
                migrationPossible = true;
        }

        if( migrationPossible )
        {
            assert( !demoBuffered && !promoBuffered );

            /* Flip a biased coin to determine whether to migrate. */
            double coinToss = static_cast<double>(::rand_r(&seed)) 
                            / static_cast<double>(RAND_MAX);

            if( coinToss <= probability )
            {
                /* 
                 *  Note: once IssueCommand is called, this hook may receive
                 *  a different parent, but fail the NVMTypeMatch check. As a
                 *  result we need to save a pointer to the NVMain class we
                 *  are issuing requests to.
                 */
                NVMObject *savedParent = parent->GetTrampoline( );

                /* Discard the unused column address. */
                uint64_t row, bank, rank, channel, subarray;
                request->address.GetTranslatedAddress( &row, NULL, &bank, &rank, &channel, &subarray );
                uint64_t promoteeAddress = migratorTranslator->ReverseTranslate( row, 0, bank, rank, channel, subarray ); 

                promotee.SetPhysicalAddress( promoteeAddress );
                promotee.SetTranslatedAddress( row, 0, bank, rank, channel, subarray );

                /* Pick a victim to replace. */
                ChooseVictim( migratorTranslator, promotee, demotee );

                assert( migratorTranslator->IsMigrated( demotee ) == false );
                assert( migratorTranslator->IsMigrated( promotee ) == false );

                if( atomic )
                {
                    migratorTranslator->StartMigration( request->address, demotee );
                    migratorTranslator->SetMigrationState( promotee, MIGRATION_DONE );
                    migratorTranslator->SetMigrationState( demotee, MIGRATION_DONE );
                }
                /* Lastly, make sure we can queue the migration requests. */
                else if( CheckIssuable( promotee, READ ) &&
                         CheckIssuable( demotee, READ ) )
                {
                    migratorTranslator->StartMigration( request->address, demotee );

                    promoRequest = new NVMainRequest( ); 
                    demoRequest = new NVMainRequest( );

                    promoRequest->address = promotee;
                    promoRequest->type = READ;
                    promoRequest->tag = MIG_READ_TAG;
                    promoRequest->burstCount = numCols;

                    demoRequest->address = demotee;
                    demoRequest->type = READ;
                    demoRequest->tag = MIG_READ_TAG;
                    demoRequest->burstCount = numCols;

                    promoRequest->owner = savedParent;
                    demoRequest->owner = savedParent;
                    savedParent->IssueCommand( promoRequest );
                    savedParent->IssueCommand( demoRequest );
                }
                else
                {
                    queueWaits++;
                }
            }
        }
    }

    return rv;
}


void CoinMigrator::ChooseVictim( Migrator *at, NVMAddress& /*promotee*/, NVMAddress& victim )
{
    /*
     *  Since this is no method called after every module in the system is 
     *  initialized, we check here to see if we have queried the memory system
     *  about the information we need.
     */
    if( !queriedMemory )
    {
        /*
         *  Our naive replacement policy will simply circle through all the pages
         *  in the fast memory. In order to count the pages we need to count the
         *  number of rows in the fast memory channel. We do this by creating a
         *  dummy request which would route to the fast memory channel. From this
         *  we can grab it's config pointer and calculate the page count.
         */
        NVMainRequest queryRequest;

        queryRequest.address.SetTranslatedAddress( 0, 0, 0, 0, promotionChannel, 0 );
        queryRequest.address.SetPhysicalAddress( 0 );
        queryRequest.type = READ;
        queryRequest.owner = this;

        NVMObject *curObject = NULL;
        FindModuleChildType( &queryRequest, SubArray, curObject, parent->GetTrampoline( ) );

        SubArray *promotionChannelSubarray = NULL;
        promotionChannelSubarray = dynamic_cast<SubArray *>( curObject );

        assert( promotionChannelSubarray != NULL );
        Params *p = promotionChannelSubarray->GetParams( );
        promotionChannelParams = p;

        totalPromotionPages = p->RANKS * p->BANKS * p->ROWS;
        currentPromotionPage = 0;

        if( p->COLS != numCols )
        {
            std::cout << "Warning: Page size of fast and slow memory differs." << std::endl;
        }

        queriedMemory = true;
    }

    /*
     *  From the current promotion page, simply craft some translated address together
     *  as the victim address.
     */
    uint64_t victimRank, victimBank, victimRow, victimSubarray, subarrayCount;
    ncounter_t promoPage = currentPromotionPage;

    victimRank = promoPage % promotionChannelParams->RANKS;
    promoPage >>= NVM::mlog2( promotionChannelParams->RANKS );

    victimBank = promoPage % promotionChannelParams->BANKS;
    promoPage >>= NVM::mlog2( promotionChannelParams->BANKS );

    subarrayCount = promotionChannelParams->ROWS / promotionChannelParams->MATHeight;
    victimSubarray = promoPage % subarrayCount;
    promoPage >>= NVM::mlog2( subarrayCount );

    victimRow = promoPage;

    victim.SetTranslatedAddress( victimRow, 0, victimBank, victimRank, promotionChannel, victimSubarray );
    uint64_t victimAddress = at->ReverseTranslate( victimRow, 0, victimBank, victimRank, promotionChannel, victimSubarray );
    victim.SetPhysicalAddress( victimAddress );

    currentPromotionPage = (currentPromotionPage + 1) % totalPromotionPages;
}


void CoinMigrator::Cycle( ncycle_t /*steps*/ )
{

}

