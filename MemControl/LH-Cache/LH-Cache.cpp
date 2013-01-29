/*
 *  This file is part of NVMain- A cycle accurate timing, bit-accurate
 *  energy simulator for non-volatile memory. Originally developed by 
 *  Matt Poremba at the Pennsylvania State University.
 *
 *  Website: http://www.cse.psu.edu/~poremba/nvmain/
 *  Email: mrp5060@psu.edu
 *
 *  ---------------------------------------------------------------------
 *
 *  If you use this software for publishable research, please include 
 *  the original NVMain paper in the citation list and mention the use 
 *  of NVMain.
 *
 */

#include "MemControl/LH-Cache/LH-Cache.h"
#include "include/NVMHelpers.h"
#include "NVM/nvmain.h"

#include <iostream>
#include <set>
#include <assert.h>


using namespace NVM;


LH_Cache::LH_Cache( Interconnect *memory, AddressTranslator *translator )
    : locks(*this), FQF(*this), NWB(*this)
{
  translator->GetTranslationMethod( )->SetOrder( 5, 1, 4, 3, 2 );


  SetMemory( memory );
  SetTranslator( translator );

  std::cout << "Create a Basic DRAM Cache!" << std::endl;

  averageHitLatency = 0.0f;
  averageHitQueueLatency = 0.0f;
  averageMissLatency = 0.0f;
  averageMissQueueLatency = 0.0f;
  averageMMLatency = 0.0f;
  averageMMQueueLatency = 0.0f;
  averageFillLatency = 0.0f;
  averageFillQueueLatency = 0.0f;

  measuredHitLatencies = 0;
  measuredHitQueueLatencies = 0;
  measuredMissLatencies = 0;
  measuredMissQueueLatencies = 0;
  measuredMMLatencies = 0;
  measuredMMQueueLatencies = 0;
  measuredFillLatencies = 0;
  measuredFillQueueLatencies = 0;

  mem_reads = 0;
  mem_writes = 0;

  mm_reqs = 0;
  mm_reads = 0;
  fills = 0;

  rb_hits = 0;
  rb_miss = 0;

  drcHits = 0;
  drcMiss = 0;

  starvation_precharges = 0;

  psInterval = 0;

  InitQueues( 2 );

  functionalCache = NULL;

  useWriteBuffer = true;

  mainMemory = NULL;

  /* Alias */
  drcQueue = &(transactionQueues[0]);
  fillQueue = &(transactionQueues[1]);
}


LH_Cache::~LH_Cache( )
{

}


void LH_Cache::SetConfig( Config *conf )
{
  /* Defaults */
  starvationThreshold = 4;
  drcQueueSize = 32;
  fillQueueSize = 8;
  useWriteBuffer = true;

  if( conf->KeyExists( "StarvationThreshold" ) )
    starvationThreshold = static_cast<unsigned int>( conf->GetValue( "StarvationThreshold" ) );
  if( conf->KeyExists( "DRCQueueSize" ) )
    drcQueueSize = static_cast<uint64_t>( conf->GetValue( "DRCQueueSize" ) );
  if( conf->KeyExists( "FillQueueSize" ) )
    fillQueueSize = static_cast<uint64_t>( conf->GetValue( "FillQueueSize" ) );
  if( conf->KeyExists( "UseWriteBuffer" ) && conf->GetString( "UseWriteBuffer" ) == "false" )
    useWriteBuffer = false;

  
  /*
   *  Lock banks between tag read and access. Initialize locks here.
   */
  ncounter_t banks, ranks;
  unsigned int i, j;

  ranks = static_cast<ncounter_t>( conf->GetValue( "RANKS" ) );
  banks = static_cast<ncounter_t>( conf->GetValue( "BANKS" ) );

  bankLocked = new bool*[ranks];
  functionalCache = new CacheBank**[ranks];
  for( i = 0; i < ranks; i++ )
    {
      bankLocked[i] = new bool[banks];
      functionalCache[i] = new CacheBank*[banks];

      for( j = 0; j < banks; j++ )
        {
          bankLocked[i][j] = false;
          functionalCache[i][j] = new CacheBank( conf->GetValue( "ROWS" ), 29, 64 );
        }
    }


  /*
   *  Initialize off-chip memory;
   */
  //std::string configFile;

  //configFile  = NVM::GetFilePath( conf->GetFileName( ) );
  //configFile += conf->GetString( "MM_CONFIG" );

  //mainMemoryConfig = new Config( );
  //mainMemoryConfig->Read( configFile );

  //mainMemory = new NVMain( );
  //mainMemory->SetConfig( mainMemoryConfig, "offChipMemory" );
  //mainMemory->SetParent( this );


  MemoryController::SetConfig( conf );
}


void LH_Cache::SetMainMemory( NVMain *mm )
{
  mainMemory = mm;
}


void LH_Cache::CalculateLatency( NVMainRequest *req, float *average, uint64_t *measured )
{
      (*average) = (( (*average) * static_cast<float>(*measured))
                      + static_cast<float>(req->completionCycle)
                      - static_cast<float>(req->issueCycle))
                   / static_cast<float>((*measured)+1);
      (*measured) += 1;
}


void LH_Cache::CalculateQueueLatency( NVMainRequest *req, float *average, uint64_t *measured )
{
      (*average) = (( (*average) * static_cast<float>(*measured))
                      + static_cast<float>(req->issueCycle)
                      - static_cast<float>(req->arrivalCycle))
                   / static_cast<float>((*measured)+1);
      (*measured) += 1;
}


bool LH_Cache::IssueAtomic( NVMainRequest *req )
{
  uint64_t rank, bank, row;
  NVMDataBlock dummy;

  req->address.GetTranslatedAddress( &row, NULL, &bank, &rank, NULL );

  //std::cout << "DRC: Atomic Request to address 0x" << std::hex << req->address.GetPhysicalAddress()
  //          << std::dec << std::endl;

  if( functionalCache[rank][bank]->SetFull( req->address ) ) 
    {
      NVMAddress victim;

      (void)functionalCache[rank][bank]->ChooseVictim( req->address, &victim );
      (void)functionalCache[rank][bank]->Evict( victim, &dummy );
    }

  (void)functionalCache[rank][bank]->Install( req->address, dummy ); 

  return true;
}


bool LH_Cache::IssueCommand( NVMainRequest *req )
{
  if( drcQueue->size( ) >= drcQueueSize )
    {
      return false;
    }


  req->arrivalCycle = GetEventQueue()->GetCurrentCycle();

  //std::cout << "Adding request 0x" << std::hex << req->address.GetPhysicalAddress( )
  //          << " to DRC queue (0x" << req << std::dec << ")" << std::endl;

  /*
   *  We first check the DRAM cache *always.* If that misses, we then issue to
   *  main memory, which will trigger and install request and return the request
   *  to the higher-level caches.
   */
  drcQueue->push_back( req );


  if( req->type == READ )
    mem_reads++;
  else
    mem_writes++;


  return true;
}



bool LH_Cache::RequestComplete( NVMainRequest *req )
{
  bool rv = false;

  req->completionCycle = GetEventQueue()->GetCurrentCycle();

  if( req->tag == DRC_TAGREAD3 )
    {
      bool miss;
      uint64_t rank, bank, row;
      NVMainRequest *originalRequest = static_cast<NVMainRequest *>(req->reqInfo);

      req->address.GetTranslatedAddress( &row, NULL, &bank, &rank, NULL );

      /*
       *  Check functional cache for hit or miss status here.
       */
      miss = true;
      if( originalRequest->type == WRITE || functionalCache[rank][bank]->Present( req->address ) )
        miss = false;

      /*
       *  If it is a hit, issue a request to the bank for the cache line
       */
      if( !miss )
        {
          bankQueues[rank][bank].push_back( MakeDRCRequest( req ) );

          drcHits++;
        }
      /*
       *  If it is a miss, issue a request to main memory for the cache line to be filled.
       */
      else
        {
          NVMainRequest *memReq = new NVMainRequest( );

          *memReq = *req;
          memReq->owner = this;
          memReq->tag = DRC_MEMREAD;

          mm_reqs++;

          /* TODO: Figure out what to do if this fails. */
          mainMemory->NewRequest( memReq );

          drcMiss++;
        }

      /* 
       *  In either case, unlock the bank. 
       * 
       *  For a miss, we need to go to main memory, so unlock since this is time consuming.
       *  For a hit, we already injected the DRC request into the bank queue, so we can't 
       *             issue anyways.
       */
      bankLocked[rank][bank] = false;

      //std::cout << "Completed tag lookup 0x" << std::hex << req->address.GetPhysicalAddress( )
      //          << " (0x" << req << ") with owner 0x" << req->owner << std::dec << std::endl;
    }
  else if( req->tag == DRC_MEMREAD )
    {
      /*
       *  Issue new fill request to drcQueue to be filled. 
       */
      NVMainRequest *fillReq = new NVMainRequest( );

      *fillReq = *req;
      fillReq->owner = this;
      fillReq->tag = DRC_FILL;
      fillReq->arrivalCycle = GetEventQueue()->GetCurrentCycle();

      /* TODO: Figure out what to do if this is full. */
      if( useWriteBuffer )
        fillQueue->push_back( fillReq );
      else
        drcQueue->push_back( fillReq );

      mm_reads++;

      CalculateLatency( req, &averageMMLatency, &measuredMMLatencies );
      CalculateQueueLatency( req, &averageMMQueueLatency, &measuredMMQueueLatencies );

      /*
       *  Mark the original request complete 
       */
      NVMainRequest *originalRequest = static_cast<NVMainRequest *>(req->reqInfo);

      GetParent( )->RequestComplete( originalRequest );

      originalRequest->completionCycle = GetEventQueue()->GetCurrentCycle();

      CalculateLatency( originalRequest, &averageMissLatency, &measuredMissLatencies );
      CalculateQueueLatency( originalRequest, &averageMissQueueLatency, &measuredMissQueueLatencies );

      //std::cout << "Completed mem read 0x" << std::hex << req->address.GetPhysicalAddress( )
      //          << " (0x" << req << ") with owner 0x" << req->owner << std::dec << std::endl;
    }
  else if( req->tag == DRC_FILL )
    {
      fills++;

      /*
       *  Fill complete, just calculate some stats
       */
      CalculateLatency( req, &averageFillLatency, &measuredFillLatencies );
      CalculateQueueLatency( req, &averageFillQueueLatency, &measuredFillQueueLatencies );

      //std::cout << "Completed fill 0x" << std::hex << req->address.GetPhysicalAddress( )
      //          << " (0x" << req << ") with owner 0x" << req->owner << std::dec << std::endl;
    }
  else if( req->tag == DRC_ACCESS )
    {
      CalculateLatency( req, &averageHitLatency, &measuredHitLatencies );
      CalculateQueueLatency( req, &averageHitQueueLatency, &measuredHitQueueLatencies );
    }

  
  if( req->type == REFRESH )
      ProcessRefreshPulse( req );
  else
  if( req->owner == this )
    {
      delete req;
      rv = true;
    }
  else
    {
      GetParent( )->RequestComplete( req );
      rv = false;
    }

  return rv;
}


bool LH_Cache::FillQueueFull::operator() (uint64_t, uint64_t)
{
  if( memoryController.useWriteBuffer && draining == false
      && memoryController.fillQueue->size() >= memoryController.fillQueueSize )
    {
      draining = true;
    }
  else if( memoryController.fillQueue->size() == 0
           && draining == true )
    {
      draining = false;
    }

  return draining;
}


bool LH_Cache::BankLocked::operator() (uint64_t rank, uint64_t bank)
{
  bool rv = false;

  if( memoryController.bankLocked[rank][bank] == false
      && !memoryController.FQF(rank, bank) )
    rv = true;

  return rv;
}


bool LH_Cache::NoWriteBuffering::operator() (uint64_t, uint64_t)
{
  return !memoryController.useWriteBuffer;
}


void LH_Cache::Cycle( ncycle_t /*steps*/ )
{
  NVMainRequest *nextRequest = NULL;


  /* Check fill queue (write buffering). */
  if( FindStarvedRequest( *fillQueue, &nextRequest, FQF ) )
    {
      rb_miss++;
      starvation_precharges++;
    }
  else if( FindRowBufferHit( *fillQueue, &nextRequest, FQF ) )
    {
      rb_hits++;
    }
  else if( FindOldestReadyRequest( *fillQueue, &nextRequest, FQF ) )
    {
      rb_miss++;
    }
  else if( FindClosedBankRequest( *fillQueue, &nextRequest, FQF ) )
    {
      rb_miss++;
    }

  /* Check request queue. */
  else if( FindStarvedRequest( *drcQueue, &nextRequest, locks ) )
    {
      rb_miss++;
      starvation_precharges++;
    }
  else if( FindRowBufferHit( *drcQueue, &nextRequest, locks ) )
    {
      rb_hits++;
    }
  else if( FindOldestReadyRequest( *drcQueue, &nextRequest, locks ) )
    {
      rb_miss++;
    }
  else if( FindClosedBankRequest( *drcQueue, &nextRequest, locks ) )
    {
      rb_miss++;
    }

  /* Check fill queue (no write buffering). */
  else if( FindStarvedRequest( *fillQueue, &nextRequest, NWB ) )
    {
      rb_miss++;
      starvation_precharges++;
    }
  else if( FindRowBufferHit( *fillQueue, &nextRequest, NWB ) )
    {
      rb_hits++;
    }
  else if( FindOldestReadyRequest( *fillQueue, &nextRequest, NWB ) )
    {
      rb_miss++;
    }
  else if( FindClosedBankRequest( *fillQueue, &nextRequest, NWB ) )
    {
      rb_miss++;
    }

  if( nextRequest != NULL )
    {
      if( nextRequest->tag == DRC_FILL )
        IssueFillCommands( nextRequest );
      else
        IssueDRCCommands( nextRequest );
    }


  CycleCommandQueues( );


  //mainMemory->Cycle( steps );
}



NVMainRequest *LH_Cache::MakeTagRequest( NVMainRequest *triggerRequest, int tag )
{
  NVMainRequest *tagRequest = new NVMainRequest( );

  tagRequest->type = READ;
  tagRequest->issueCycle = GetEventQueue()->GetCurrentCycle();
  tagRequest->address = triggerRequest->address;
  tagRequest->tag = tag;
  tagRequest->owner = this;

  /* The reqInfo pointer will point to the original request from cache. */
  tagRequest->reqInfo = static_cast<void *>( triggerRequest );

  return tagRequest;
}



NVMainRequest *LH_Cache::MakeDRCRequest( NVMainRequest *triggerRequest)
{
  /* Retreive the original request. */
  NVMainRequest *drcRequest = static_cast<NVMainRequest *>(triggerRequest->reqInfo);

  drcRequest->tag = DRC_ACCESS;

  /* Set the request as issued now. */
  drcRequest->issueCycle = GetEventQueue()->GetCurrentCycle();

  return drcRequest;
}



NVMainRequest *LH_Cache::MakeTagWriteRequest( NVMainRequest *triggerRequest )
{
  NVMainRequest *tagRequest = new NVMainRequest( );

  tagRequest->type = WRITE;
  tagRequest->issueCycle = GetEventQueue()->GetCurrentCycle();
  tagRequest->address = triggerRequest->address;
  tagRequest->owner = this;

  /* The reqInfo pointer will point to the original request from cache. */
  tagRequest->reqInfo = static_cast<void *>( triggerRequest );

  return tagRequest;
}



bool LH_Cache::IssueDRCCommands( NVMainRequest *req )
{
  bool rv = false;
  uint64_t rank, bank, row;

  req->address.GetTranslatedAddress( &row, NULL, &bank, &rank, NULL );

  if( !activateQueued[rank][bank] && bankQueues[rank][bank].empty() )
    {
      /* Any activate will request the starvation counter */
      starvationCounter[rank][bank] = 0;
      activateQueued[rank][bank] = true;
      effectiveRow[rank][bank] = row;

      req->issueCycle = GetEventQueue()->GetCurrentCycle();

      bankQueues[rank][bank].push_back( MakeActivateRequest( req ) );
      bankQueues[rank][bank].push_back( MakeTagRequest( req, DRC_TAGREAD1 ) );
      bankQueues[rank][bank].push_back( MakeTagRequest( req, DRC_TAGREAD2 ) );
      bankQueues[rank][bank].push_back( MakeTagRequest( req, DRC_TAGREAD3 ) );
      bankLocked[rank][bank] = true;

      rv = true;
    }
  else if( activateQueued[rank][bank] && effectiveRow[rank][bank] != row && bankQueues[rank][bank].empty() )
    {
      /* Any activate will request the starvation counter */
      starvationCounter[rank][bank] = 0;
      activateQueued[rank][bank] = true;
      effectiveRow[rank][bank] = row;

      req->issueCycle = GetEventQueue()->GetCurrentCycle();

      bankQueues[rank][bank].push_back( MakePrechargeRequest( req ) );
      bankQueues[rank][bank].push_back( MakeActivateRequest( req ) );
      bankQueues[rank][bank].push_back( MakeTagRequest( req, DRC_TAGREAD1 ) );
      bankQueues[rank][bank].push_back( MakeTagRequest( req, DRC_TAGREAD2 ) );
      bankQueues[rank][bank].push_back( MakeTagRequest( req, DRC_TAGREAD3 ) );
      bankLocked[rank][bank] = true;

      rv = true;
    }
  else if( activateQueued[rank][bank] && effectiveRow[rank][bank] == row )
    {
      starvationCounter[rank][bank]++;

      req->issueCycle = GetEventQueue()->GetCurrentCycle();

      bankQueues[rank][bank].push_back( MakeTagRequest( req, DRC_TAGREAD1 ) );
      bankQueues[rank][bank].push_back( MakeTagRequest( req, DRC_TAGREAD2 ) );
      bankQueues[rank][bank].push_back( MakeTagRequest( req, DRC_TAGREAD3 ) );
      bankLocked[rank][bank] = true;

      rv = true;
    }
  else
    {
      rv = false;
    }

  return rv;
}



bool LH_Cache::IssueFillCommands( NVMainRequest *req )
{
  bool rv = false;
  uint64_t rank, bank, row;

  req->address.GetTranslatedAddress( &row, NULL, &bank, &rank, NULL );

  if( !activateQueued[rank][bank] && bankQueues[rank][bank].empty() )
    {
      /* Any activate will request the starvation counter */
      starvationCounter[rank][bank] = 0;
      activateQueued[rank][bank] = true;
      effectiveRow[rank][bank] = row;

      req->issueCycle = GetEventQueue()->GetCurrentCycle();

      bankQueues[rank][bank].push_back( MakeActivateRequest( req ) );
      bankQueues[rank][bank].push_back( MakeTagWriteRequest( req ) );
      bankQueues[rank][bank].push_back( req );

      rv = true;
    }
  else if( activateQueued[rank][bank] && effectiveRow[rank][bank] != row && bankQueues[rank][bank].empty() )
    {
      /* Any activate will request the starvation counter */
      starvationCounter[rank][bank] = 0;
      activateQueued[rank][bank] = true;
      effectiveRow[rank][bank] = row;

      req->issueCycle = GetEventQueue()->GetCurrentCycle();

      bankQueues[rank][bank].push_back( MakePrechargeRequest( req ) );
      bankQueues[rank][bank].push_back( MakeActivateRequest( req ) );
      bankQueues[rank][bank].push_back( MakeTagWriteRequest( req ) );
      bankQueues[rank][bank].push_back( req );

      rv = true;
    }
  else if( activateQueued[rank][bank] && effectiveRow[rank][bank] == row )
    {
      starvationCounter[rank][bank]++;

      req->issueCycle = GetEventQueue()->GetCurrentCycle();

      bankQueues[rank][bank].push_back( MakeTagWriteRequest( req ) );
      bankQueues[rank][bank].push_back( req );

      rv = true;
    }
  else
    {
      rv = false;
    }

  return rv;
}


void LH_Cache::PrintStats( )
{
  std::cout << "i" << psInterval << "." << statName << id << ".mem_reads " << mem_reads << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".mem_writes " << mem_writes << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".rb_hits " << rb_hits << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".rb_miss " << rb_miss << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".drcHits " << drcHits << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".drcMiss " << drcMiss << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".mm_reqs " << mm_reqs << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".mm_reads " << mm_reads << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".fills " << fills << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".starvation_precharges " << starvation_precharges << std::endl;

  std::cout << "i" << psInterval << "." << statName << id << ".averageHitLatency " << averageHitLatency << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".measuredHitLatencies " << measuredHitLatencies << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".averageHitQueueLatency " << averageHitQueueLatency << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".measuredHitQueueLatencies " << measuredHitQueueLatencies << std::endl;

  std::cout << "i" << psInterval << "." << statName << id << ".averageMissLatency " << averageMissLatency << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".measuredMissLatencies " << measuredMissLatencies << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".averageMissQueueLatency " << averageMissQueueLatency << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".measuredMissQueueLatencies " << measuredMissQueueLatencies << std::endl;
 
  std::cout << "i" << psInterval << "." << statName << id << ".averageMMLatency " << averageMMLatency << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".measuredMMLatencies " << measuredMMLatencies << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".averageMMQueueLatency " << averageMMQueueLatency << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".measuredMMQueueLatencies " << measuredMMQueueLatencies << std::endl;

  std::cout << "i" << psInterval << "." << statName << id << ".averageFillLatency " << averageFillLatency << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".measuredFillLatencies " << measuredFillLatencies << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".averageFillQueueLatency " << averageFillQueueLatency << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".measuredFillQueueLatencies " << measuredFillQueueLatencies << std::endl;

  MemoryController::PrintStats( );

  psInterval++;
}


