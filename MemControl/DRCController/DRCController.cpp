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


#include "MemControl/DRCController/DRCController.h"
#include "Interconnect/InterconnectFactory.h"
#include "MemControl/MemoryControllerFactory.h"
#include "Prefetchers/PrefetcherFactory.h"
#include "include/NVMHelpers.h"
#include "Decoders/DRCDecoder/DRCDecoder.h"

#ifndef TRACE
#include "base/statistics.hh"
#include "base/types.hh"
#include "sim/core.hh"
#include "sim/stat_control.hh"
#endif

#include <stdlib.h>
#include <assert.h>
#include <sstream>


using namespace NVM;




/* Tags to identify our special memory requests. */
#define DRC_TAGREAD1 1
#define DRC_TAGREAD2 2
#define DRC_TAGREAD3 3
#define DRC_DATAREAD 10
#define DRC_DATAWRITE 11
#define DRC_VICTIMREAD 12
#define DRC_DATAINSTALL 13
#define DRC_TAGUPDATE 20
#define DRC_DELETEME 30
#define DRC_PREFETCHED 40



namespace {

  struct InstallCacheEntry
  {
    bool referenced;
    bool prefetched;
    bool installed;
    NVMAddress triggerAddress;
  };


  struct DRCCacheEntry 
  {
    bool prefetched;
    uint64_t refCount;
  };

};


/*
 *  This memory controller attempts to issue a memory request to a memory controller
 *  (called the "try controller"). Memory requests to this controller may not be 
 *  gauranteed to return data (e.g., a DRAM cache). If data is returned, we mark the
 *  operation complete and NVMain will return the request to the sender. If the data
 *  is not returned by the try controller, the data is issued to a second controller
 *  which gaurantees the data will be found (called the "catch controller").
 */
DRCController::DRCController( Interconnect *memory, AddressTranslator *translator )
{
  //translator->GetTranslationMethod( )->SetOrder( 1, 2, 3, 4, 5 );
  translator->GetTranslationMethod( )->SetOrder( 4, 5, 3, 2, 1 );

  /*
   *  The translator and memory created by NVMain by default will be used for the 
   *  DRAM cache. The extra parameter MM_CONFIG will specify the configuration
   *  file for the main memory system, and we will build that in SetConfig.
   */
  SetMemory( memory );
  SetTranslator( translator );


  /*
   *  Use NULL to imply these aren't used. They won't be constructed if they aren't
   *  configured.
   */
  installCache = NULL;
  missMap = NULL;
  prefetcher = NULL;

  missMapHits = 0;
  missMapMisses = 0;

  evictions = 0;
  dirty_evictions = 0;
  clean_evictions = 0;

  misses = 0;
  read_hits = 0;
  write_hits = 0;
  tag_requeries = 0;

  app_reads = 0;
  app_writes = 0;
  maxDRCQueue = 0;
  totalDRCQueue = 0;
  countDRCQueue = 0;
  maxPFWQueue = 0;
  totalPFWQueue = 0;
  countPFWQueue = 0;

  averageLatency = 0.0f;
  averageQueueLatency = 0.0f;
  measuredLatencies = 0;
  measuredQueueLatencies = 0;

  averageMMLatency = 0.0f;
  averageMMQueueLatency = 0.0f;
  measuredMMLatencies = 0;
  measuredMMQueueLatencies = 0;

  icHits = 0;

  mmEvicts = 0;
  cleanMMEvicts = 0;
  dirtyMMEvicts = 0;
  mmForceEvicts = 0;

  prefetchesIssued = 0;
  prefetchHits = 0;
  prefetchMisses = 0;

  icPrefetchRefEvicts = 0;
  icDemandRefEvicts = 0;
  icPrefetchUnrefEvicts = 0;
  icDemandUnrefEvicts = 0;
  icInstalledEvicts = 0;
  icUninstalledEvicts = 0;

  drcRPdrops = 0;
  drcUPdrops = 0;
  drcUDdrops = 0;

  drcRBhits = 0;
  drcRBmiss = 0;

  watchAddr = 0;

  longAssRequests = 0;

  psInterval = 0;
}


DRCController::~DRCController( )
{
  if( mmConfig ) delete mmConfig;
  if( mmMethod ) delete mmMethod;
  if( mmTranslator ) delete mmTranslator;
  if( mmMemory ) delete mmMemory;
  if( mmController ) delete mmController;

  for( unsigned int i = 0; i < numRanks; i++ )
    {
      if( lastClose && lastClose[i] ) delete [] lastClose[i];
      if( bankQueue && bankQueue[i] ) delete [] bankQueue[i];
    }

  if( lastClose ) delete [] lastClose;
  if( bankQueue ) delete [] bankQueue;

  if( installCache ) delete installCache;

  for( unsigned int i = 0; i < numRanks; i++ )
    {
      delete [] actQueued[i];
      delete [] actRow[i];
    }

  delete [] actQueued;
  delete [] actRow;


}


void DRCController::SetConfig( Config *conf )
{
  int channels, ranks, banks, rows, cols;
  unsigned int drcRanks, drcBanks;

  drcRanks = conf->GetValue( "RANKS" );
  drcBanks = conf->GetValue( "BANKS" );

  if( !conf->KeyExists( "MM_CONFIG" ) )
    {
      std::cout << "DRCController: Configuration parameter `MM_CONFIG' "
                << "is required!" << std::endl;
      exit( 1 );
    }

  if( conf->KeyExists( "MaxQueue" ) )
    maxQueueLength = conf->GetValue( "MaxQueue" );
  else
    maxQueueLength = 150;


  if( conf->KeyExists( "IgnoreBits" ) )
    {
      DRCDecoder *drcDec;

      drcDec = static_cast<DRCDecoder *>(GetTranslator());
      drcDec->SetIgnoreBits( conf->GetValue( "IgnoreBits" ) );
    }

  /*
   *  Read MM_CONFIG (main memory configuration) file, and use this to create the 
   *  main memory system.
   */
  mmConfig = new Config( );
  std::string mmConfigFile;
  
  mmConfigFile  = GetFilePath( conf->GetFileName( ) );
  mmConfigFile += conf->GetString( "MM_CONFIG" );

  std::cout << "Reading Main Memory configuration file: " << mmConfigFile << std::endl;

  mmConfig->Read( mmConfigFile );
  mmConfig->SetSimInterface( conf->GetSimInterface( ) );

  channels = mmConfig->GetValue( "CHANNELS" );
  ranks = mmConfig->GetValue( "RANKS" );
  banks = mmConfig->GetValue( "BANKS" );
  rows = mmConfig->GetValue( "ROWS" );
  cols = mmConfig->GetValue( "COLS" );
  
  mmChannels = channels;


  /*
   *  Configure the functional cache model.
   */
  uint64_t assoc, sets, cachelineSize;

  /* Each DRAM row will be a set. */
  sets = conf->GetValue( "ROWS" );

  cachelineSize = 64;
  /* 
   *  Should be calculated based on number of columns, size of address (to determine tag
   *  size) and cacheline size. 
   */
  assoc = 29;

  functionalCache = new CacheBank ** [conf->GetValue( "RANKS" )];
  for( int i = 0; i < conf->GetValue( "RANKS" ); i++ )
    {
      functionalCache[i] = new CacheBank * [conf->GetValue( "BANKS" )];
      for( int j = 0; j < conf->GetValue( "BANKS" ); j++ )
        {
          functionalCache[i][j] = new CacheBank( sets, assoc, cachelineSize );
          functionalCache[i][j]->SetAddressTranslator( GetTranslator( ) );
        }
    }


  mmController = new MemoryController * [channels];

  /*
   *  Build our DRAM main memory system. We'll need a new address translator and
   *  interconnect to be build.
   */
  mmMethod = new TranslationMethod( );
  mmTranslator = new AddressTranslator( );

  mmMethod->SetBitWidths( mlog2( rows ),
                          mlog2( cols ),
                          mlog2( banks ),
                          mlog2( ranks ),
                          mlog2( channels )
                        );
  mmMethod->SetCount( rows, cols, banks, ranks, channels );
  mmTranslator->SetTranslationMethod( mmMethod );

  /* We need one interconnect and memory controller per channel. */
  mmMemory = new Interconnect* [channels];
  mmController = new MemoryController * [channels];

  for( int i = 0; i < channels; i++ )
    {
      std::stringstream formatter;

      /* One interconnect per channel. */
      mmMemory[i] = InterconnectFactory::CreateInterconnect( mmConfig->GetString( "INTERCONNECT" ) );

      formatter.str( "" );
      formatter << "offChipMemory.channel" << (100 * this->id + i);
      mmMemory[i]->StatName( formatter.str( ) );
      mmMemory[i]->SetConfig( mmConfig );

      /* One memory controller per channel as well. */
      mmController[i] = MemoryControllerFactory::CreateNewController( mmConfig->GetString( "MEM_CTL" ), mmMemory[i], mmTranslator );

      formatter.str( "" );
      formatter << "offChipMemory.Controller" << (100 * this->id + i) << "." 
                << mmConfig->GetString( "MEM_CTL" ); 
      mmController[i]->StatName( formatter.str( ) );
      mmController[i]->SetConfig( mmConfig );
      mmController[i]->SetID( 100 * this->id + i ); 
    }



  /*
   *  Initialize counters for starvation control and queues for DRAM commands. 
   */ 
  lastClose = new unsigned int * [drcRanks];
  bankQueue = new std::deque<MemOp *> * [drcRanks];
  for( unsigned int i = 0; i < drcRanks; i++ )
    {
      lastClose[i] = new unsigned int [drcBanks];
      bankQueue[i] = new std::deque<MemOp *> [drcBanks];
      for( unsigned int j = 0; j < drcBanks; j++ )
        {
          lastClose[i][j] = 0;
          bankQueue[i][j].clear( );
        }
    }
  numRanks = drcRanks;
  numBanks = drcBanks;


  /* Need atomic knowledge of banks being open/closed to schedule. */
  actQueued = new bool * [drcRanks];
  actRow = new uint64_t * [drcRanks];
  bankLocked = new bool * [drcRanks];
  for( unsigned int i = 0; i < drcRanks; i++ )
    {
      actQueued[i] = new bool[drcBanks];
      actRow[i] = new uint64_t[drcBanks];
      bankLocked[i] = new bool[drcBanks];

      for( unsigned int j = 0; j < drcBanks; j++ )
        {
          actQueued[i][j] = false;
          actRow[i][j] = 0;
          bankLocked[i][j] = false;
        }
    }

  /*
   *  Setup the install cache
   */
  uint64_t icSets, icAssoc, icLineSize;

  icSets = icAssoc = icLineSize = 0;

  if( conf->KeyExists( "IC_SETS" ) ) icSets  = static_cast<uint64_t>(conf->GetValue( "IC_SETS" ));
  if( conf->KeyExists( "IC_ASSOC" ) ) icAssoc = static_cast<uint64_t>(conf->GetValue( "IC_ASSOC" ));
  if( conf->KeyExists( "IC_LINESIZE" ) ) icLineSize = static_cast<uint64_t>(conf->GetValue( "IC_LINESIZE" ));

  if( conf->KeyExists( "UseInstallCache" ) && conf->GetString( "UseInstallCache" ) == "true" )
    installCache = new CacheBank( icSets, icAssoc, icLineSize );



  /*
   *  Setup the MissMap
   */
  uint64_t mmSets, mmAssoc, mmLineSize;

  mmSets = mmAssoc = mmLineSize = 0;

  if( conf->KeyExists( "MM_SETS" ) ) mmSets  = static_cast<uint64_t>(conf->GetValue( "MM_SETS" ));
  if( conf->KeyExists( "MM_ASSOC" ) ) mmAssoc = static_cast<uint64_t>(conf->GetValue( "MM_ASSOC" ));
  if( conf->KeyExists( "MM_LINESIZE" ) ) mmLineSize = static_cast<uint64_t>(conf->GetValue( "MM_LINESIZE" ));

  if( conf->KeyExists( "UseMissMap" ) && conf->GetString( "UseMissMap" ) == "true" )
    {
      missMap = new CacheBank( mmSets, mmAssoc, mmLineSize );
      missMap->isMissMap = true;
    }


  /*
   *  Setup the prefetcher
   */
  if( conf->KeyExists( "Prefetcher" ) )
    {
      prefetcher = PrefetcherFactory::CreateNewPrefetcher( conf->GetString( "Prefetcher" ) );
    }
  else
    {
      prefetcher = PrefetcherFactory::CreateNewPrefetcher( "asdf" );
    }


  /*
   *  Setup an address to watch
   */
  if( conf->KeyExists( "WatchAddr" ) )
    {
      std::stringstream fmat;

      fmat << std::hex << conf->GetString( "WatchAddr" );
      fmat >> watchAddr;

      std::cout << "WATCHADDR: Set watchAddr to 0x" << std::hex << watchAddr << std::dec << std::endl;
    }


  MemoryController::SetConfig( conf );
}


void DRCController::MissMapEvict( NVMAddress *victim )
{
  if( missMap != NULL )
    {
      NVMAddress pageAddress;
      uint64_t lineOffset;
      uint64_t *lineMap;
      NVMDataBlock lineList;

      pageAddress.SetPhysicalAddress( victim->GetPhysicalAddress( ) >> 12 );
      lineOffset = (victim->GetPhysicalAddress( ) & 0xFFF) / 64;

      mmForceEvicts++;

      if( missMap->Present( pageAddress ) )
        {
          missMap->Read( pageAddress, &lineList );

          lineMap = (uint64_t *)(lineList.rawData);
          *lineMap &= ~(1 << lineOffset);
          lineList.rawData = (void *)lineMap;

          missMap->Write( pageAddress, lineList );
        }
      else
        {
          std::cout << "WARNING: Attempted to remove evicted DRC line from MissMap, "
                    << "but the line is not present!" << std::endl;
        }
    }
}


void DRCController::MissMapInstall( NVMainRequest *request )
{
  if( missMap != NULL )
    {
      NVMAddress pageAddress;
      uint64_t lineOffset;

      pageAddress.SetPhysicalAddress( request->address.GetPhysicalAddress( ) >> 12 );
      lineOffset = (request->address.GetPhysicalAddress( ) & 0xFFF) / 64;

      //std::cout << "0x" << std::hex << request->address.GetPhysicalAddress( ) << " maps to set "
      //          << std::dec << missMap->SetID( pageAddress ) << " lineOffset " 
      //          << lineOffset << std::endl;

      if( missMap->Present( pageAddress ) )
        {
          NVMDataBlock lineList;
          uint64_t *lineMap;

          missMap->Read( pageAddress, &lineList );

          lineMap = (uint64_t *)(lineList.rawData);
          *lineMap |= (uint64_t)(1ULL << lineOffset);
          lineList.rawData = (void *)lineMap;

          missMap->Write( pageAddress, lineList );
        }
      else
        {
          if( missMap->SetFull( pageAddress ) )
            {
              /* Evict some line and also it's lines in the DRC. */
              NVMAddress vicPage;
              NVMDataBlock vicData;
              NVMAddress wbList[64];
              uint64_t wbSize = 0;
              uint64_t *lineMap;

              missMap->ChooseVictim( pageAddress, &vicPage );
              missMap->Evict( vicPage, &vicData );

              lineMap = (uint64_t *)(vicData.rawData);

              for( int i = 0; i < 64; i++ )
                {
                  if( *lineMap & 0x1 )
                    {
                      wbList[wbSize].SetPhysicalAddress( (vicPage.GetPhysicalAddress( ) << 12) + 64*i );
                      wbSize++;
                    }

                  *lineMap >>= 1ULL;
                }

              mmEvicts += wbSize;

              /* TODO: Simulate removal from DRC. */
              for( uint64_t i = 0; i < wbSize; i++ )
                {
                  uint64_t wrow, wcol, wbank, wrank, wchannel;
                  bool dirty;
                  NVMDataBlock dummy;

                  GetTranslator( )->Translate( wbList[i].GetPhysicalAddress( ), &wrow, &wcol, &wbank, &wrank, &wchannel );

                  dirty = functionalCache[wrank][wbank]->Evict( wbList[i], &dummy );
                  if( dirty )
                    dirtyMMEvicts++;
                  else
                    cleanMMEvicts++;
                }
            }


          NVMDataBlock lineList;
          uint64_t *lineMap = new uint64_t;

          *lineMap = 0;
          *lineMap |= (uint64_t)(1ULL << lineOffset);
          lineList.rawData = (void*)lineMap;

          missMap->Install( pageAddress, lineList );
        }
    }
}


bool DRCController::PrefetchInProgress( NVMAddress addr )
{
  std::deque<MemOp *>::iterator it;

  for( it = pfInProgress.begin(); it != pfInProgress.end( ); it++ )
    {
      if( (*it)->GetRequest()->address.GetPhysicalAddress( ) == addr.GetPhysicalAddress( ) )
        {
          return true;
        }
    }

  return false;
}


bool DRCController::AddressQueued( NVMAddress addr )
{
  std::deque<MemOp *>::iterator it;

  for( it = drcQueue.begin(); it != drcQueue.end(); it++ )
    {
      if( (*it)->GetRequest( )->address.GetPhysicalAddress( ) == addr.GetPhysicalAddress( ) )
        {
          return true;
        }
    }

  for( it = issuedQueue.begin(); it != issuedQueue.end(); it++ )
    {
      if( (*it)->GetRequest( )->address.GetPhysicalAddress( ) == addr.GetPhysicalAddress( ) )
        {
          return true;
        }
    }

  for( it = mmQueue.begin(); it != mmQueue.end(); it++ )
    {
      if( (*it)->GetRequest( )->address.GetPhysicalAddress( ) == addr.GetPhysicalAddress( ) )
        {
          return true;
        }
    }

  for( it = writeList.begin(); it != writeList.end(); it++ )
    {
      if( (*it)->GetRequest( )->address.GetPhysicalAddress( ) == addr.GetPhysicalAddress( ) )
        {
          return true;
        }
    }

  for( it = installList.begin(); it != installList.end(); it++ )
    {
      if( (*it)->GetRequest( )->address.GetPhysicalAddress( ) == addr.GetPhysicalAddress( ) )
        {
          return true;
        }
    }

  return false;
}


void DRCController::InjectPrefetch( MemOp *mop )
{
  MemOp *pfOp;
  NVMainRequest *pfReq;
  std::vector<NVMAddress> pfList;

  /* Prefetch algo should return addresses */
  if( prefetcher && prefetcher->DoPrefetch( mop, pfList ) && mop->GetOperation( ) == READ )
    {
      uint64_t prefetchDepth = pfList.size( );

      for( uint64_t i = 0; i < prefetchDepth; i++ )
        {
          uint64_t mmRow, mmCol, mmBank, mmRank, mmChannel;
          uint64_t drcRow, drcCol, drcBank, drcRank, drcChannel;

          /* 
           *  Filter prefetches that are already installed/waiting to install. 
           *  I assume the miss map can be used to determine this information
           */
          GetTranslator( )->Translate( pfList[i].GetPhysicalAddress( ), &drcRow, &drcCol, &drcBank, &drcRank, &drcChannel );

          if( functionalCache[drcRank][drcBank]->Present( pfList[i] ) )
            continue;
          if( AddressQueued( pfList[i] ) )
            continue;

          mmTranslator->Translate( pfList[i].GetPhysicalAddress( ), &mmRow, &mmCol, &mmBank, &mmRank, &mmChannel );
          pfList[i].SetTranslatedAddress( mmRow, mmCol, mmBank, mmRank, mmChannel );

          pfOp = new MemOp( );
          pfReq = new NVMainRequest( );

          pfOp->SetAddress( pfList[i] );
          pfOp->SetOperation( mop->GetOperation( ) );
          pfOp->SetBulkCmd( CMD_NOP );
          pfOp->SetRequest( pfReq );

          pfReq->address = pfList[i];
          pfReq->type = mop->GetOperation( );
          pfReq->memop = pfOp;
          pfReq->issueController = NULL;
          pfReq->tag = DRC_PREFETCHED;
          pfReq->arrivalCycle = currentCycle;
          pfReq->isPrefetch = true;
          pfReq->reqInfo = (void*)(pfOp);
          pfReq->pfTrigger.SetPhysicalAddress( mop->GetRequest( )->address.GetPhysicalAddress( ) );

          mmQueue.push_back( pfOp );
          pfInProgress.push_back( pfOp );

          if( watchAddr != 0 && ( pfReq->address.GetPhysicalAddress( ) == watchAddr || 
                                  mop->GetRequest( )->address.GetPhysicalAddress( ) == watchAddr ) )
            {
              std::cout << "Address 0x" << std::hex << mop->GetRequest( )->address.GetPhysicalAddress( ) 
                        << " caused prefetch for 0x" << pfReq->address.GetPhysicalAddress( ) 
                        << std::dec << ". " << currentCycle << std::endl;
            }

          prefetchesIssued++;
          allPrefetches.insert( pfReq->address.GetPhysicalAddress( ) );

          /*
           *  This doesn't need to go into the issuedQueue since there is no "original" request.
           */

          if( !mmController[mmChannel]->StartCommand( pfOp ) )
            {
              pfReq->status = MEM_REQUEST_RETRY;
            }
          else
            {
              pfReq->issueCycle = currentCycle;
            }
        }
    }
}



bool DRCController::QueueFull( NVMainRequest * /*request*/ )
{
  if( drcQueue.size( ) > maxQueueLength )
    {
        /*
      std::deque<MemOp *>::iterator it;
      int pos = 0;
      uint64_t channel, rank, bank, row, col;

      for( it = drcQueue.begin( ); it != drcQueue.end( ); ++it )
        {
          (*it)->GetRequest( )->address.GetTranslatedAddress( &row, &col, &bank, &rank, &channel ); 

          std::cout << pos++ << ": 0x" << std::hex << (*it)->GetRequest()->address.GetPhysicalAddress( )
                    << std::dec << ". H/R/B/R/C: " << channel << "/" << rank << "/" << bank << "/"
                    << row << "/" << col << std::endl;
        }
        */
      return true;
    }

  return false;
}


/*
 *  Here we just issue to the DRAM cache by default. We also place this memory operation 
 *  in a queue, so we can watch for hit/miss.
 */
int DRCController::StartCommand( MemOp *mop )
{
  if( mop->GetOperation( ) == READ )
    app_reads++;
  else if( mop->GetOperation( ) == WRITE )
    app_writes++;

  //std::cout << "0x" << std::hex << mop->GetRequest( )->address.GetPhysicalAddress( ) << std::dec 
  //          << ": command accepted to DRC at cycle " << currentCycle << std::endl;

  if( watchAddr != 0 && mop->GetRequest( )->address.GetPhysicalAddress( ) == watchAddr )
    {
      std::cout << "WATCHADDR: Entered STARTCOMMAND as type. " << mop->GetOperation( ) 
                << ". Request ptr " << (void*)(mop) << ". " << currentCycle << std::endl;
    }

  mop->GetRequest( )->arrivalCycle = currentCycle;

  if( countedPrefetches.count( mop->GetRequest( )->address.GetPhysicalAddress( ) ) == 0
      && allPrefetches.count( mop->GetRequest( )->address.GetPhysicalAddress( ) ) > 0 )
    {
      prefetchHits++;
      countedPrefetches.insert( mop->GetRequest( )->address.GetPhysicalAddress( ) );
    }

  /* Set the original request to itself */
  mop->GetRequest( )->reqInfo = (void *)(mop);

  std::map<uint64_t, uint64_t>::iterator refIt;

  refIt = rpRefCount.find( mop->GetRequest( )->address.GetPhysicalAddress( ) );
  if( refIt != rpRefCount.end( ) )
    refIt->second++;

  refIt = upRefCount.find( mop->GetRequest( )->address.GetPhysicalAddress( ) );
  if( refIt != upRefCount.end( ) )
    refIt->second++;

  refIt = rdRefCount.find( mop->GetRequest( )->address.GetPhysicalAddress( ) );
  if( refIt != rdRefCount.end( ) )
    refIt->second++;

  refIt = udRefCount.find( mop->GetRequest( )->address.GetPhysicalAddress( ) );
  if( refIt != udRefCount.end( ) )
    refIt->second++;


  if( prefetcher )
    {
      bool prefetchMore = false;
      MemOp *pfOp;
      NVMainRequest *pfReq;
      std::vector<NVMAddress> pfList;

      prefetchMore = prefetcher->NotifyAccess( mop, pfList );

      if( prefetchMore )
        {
          uint64_t prefetchDepth = pfList.size( );

          for( uint64_t i = 0; i < prefetchDepth; i++ )
            {
              uint64_t mmRow, mmCol, mmBank, mmRank, mmChannel;
              uint64_t drcRow, drcCol, drcBank, drcRank, drcChannel;

              /* 
               *  Filter prefetches that are already installed/waiting to install. 
               *  I assume the miss map can be used to determine this information
               */
              GetTranslator( )->Translate( pfList[i].GetPhysicalAddress( ), &drcRow, &drcCol, &drcBank, &drcRank, &drcChannel );

              if( functionalCache[drcRank][drcBank]->Present( pfList[i] ) )
                continue;
              if( AddressQueued( pfList[i] ) )
                continue;

              mmTranslator->Translate( pfList[i].GetPhysicalAddress( ), &mmRow, &mmCol, &mmBank, &mmRank, &mmChannel );
              pfList[i].SetTranslatedAddress( mmRow, mmCol, mmBank, mmRank, mmChannel );

              pfOp = new MemOp( );
              pfReq = new NVMainRequest( );

              pfOp->SetAddress( pfList[i] );
              pfOp->SetOperation( mop->GetOperation( ) );
              pfOp->SetBulkCmd( CMD_NOP );
              pfOp->SetRequest( pfReq );

              pfReq->address = pfList[i];
              pfReq->type = mop->GetOperation( );
              pfReq->memop = pfOp;
              pfReq->issueController = NULL;
              pfReq->tag = DRC_PREFETCHED;
              pfReq->arrivalCycle = currentCycle;
              pfReq->isPrefetch = true;
              pfReq->reqInfo = (void*)(pfOp);
              pfReq->pfTrigger.SetPhysicalAddress( mop->GetRequest( )->address.GetPhysicalAddress( ) );

              mmQueue.push_back( pfOp );
              pfInProgress.push_back( pfOp );

              if( watchAddr != 0 && ( pfReq->address.GetPhysicalAddress( ) == watchAddr || 
                                      mop->GetRequest( )->address.GetPhysicalAddress( ) == watchAddr ) )
                {
                  std::cout << "Address 0x" << std::hex << mop->GetRequest( )->address.GetPhysicalAddress( ) 
                            << " caused prefetch for 0x" << pfReq->address.GetPhysicalAddress( ) 
                            << std::dec << ". " << currentCycle << std::endl;
                }

              prefetchesIssued++;
              allPrefetches.insert( pfReq->address.GetPhysicalAddress( ) );

              /*
               *  This doesn't need to go into the issuedQueue since there is no "original" request.
               */

              if( !mmController[mmChannel]->StartCommand( pfOp ) )
                {
                  pfReq->status = MEM_REQUEST_RETRY;
                }
              else
                {
                  pfReq->issueCycle = currentCycle;
                }
            }
        }
    }
  

  if( PrefetchInProgress( mop->GetRequest( )->address ) )
    {
      if( watchAddr != 0 && mop->GetRequest( )->address.GetPhysicalAddress( ) == watchAddr )
        {
          std::cout << "WATCHADDR: Address being prefetched, entering wait queue. " << currentCycle << std::endl;
        }

      pfwaitQueue.push_back( mop );
      return true;
    }


  if( installCache && installCache->Present( mop->GetRequest( )->address )
      && mop->GetOperation( ) == READ )
    {
      InstallCacheEntry *icEntry;
      NVMDataBlock icData;

      icHits++;

      installCache->Read( mop->GetRequest( )->address, &icData );
      icEntry = (InstallCacheEntry *)(icData.rawData);

      icEntry->referenced = true;

      if( GetConfig( )->KeyExists( "DROP_RP" ) && GetConfig( )->GetString( "DROP_RP" ) == "true" )
        {
          if( icEntry->prefetched )
            {
              /* Drop from drcQueue and install cache... might want to leave in IC? */
              if( installCache->Present( mop->GetRequest( )->address ) )
                {
                  NVMDataBlock dummy;

                  installCache->Evict( mop->GetRequest( )->address, &dummy );
                }

              if( installCache->Present( icEntry->triggerAddress ) )
                {
                  NVMDataBlock dummy;

                  installCache->Evict( icEntry->triggerAddress, &dummy );
                }

              std::deque<MemOp *>::iterator rpit;

              for( rpit = drcQueue.begin( ); rpit != drcQueue.end( ); rpit++ )
                {
                  if( drcQueue.empty( ) || rpit == drcQueue.end( ) )
                    break;

                  if( (*rpit)->GetRequest( )->address.GetPhysicalAddress( )
                      == mop->GetRequest( )->address.GetPhysicalAddress( ) ||
                      (*rpit)->GetRequest( )->address.GetPhysicalAddress( )
                      == icEntry->triggerAddress.GetPhysicalAddress( ) )
                    {
                      rpit = drcQueue.erase( rpit );
#ifdef DBGQUEUE
                      std::cout << "--- RP deleted from DRC queue at cycle " << currentCycle << ". Size is " << drcQueue.size() << std::endl;
#endif

                      drcRPdrops++;
                    }

                  if( drcQueue.empty( ) || rpit == drcQueue.end( ) )
                    break;
                }
            }
        }

      mop->GetRequest( )->status = MEM_REQUEST_COMPLETE; // that's all folks

      return true;
    }


  if( missMap != NULL && mop->GetOperation( ) == READ )
    {
      /* if it's in the miss map, try to send to DRC as normal. */
      NVMAddress pageAddress;
      uint64_t lineOffset;
      uint64_t lineMask;
      bool linePresent = true;

      if( watchAddr != 0 && mop->GetRequest( )->address.GetPhysicalAddress( ) == watchAddr )
        {
          std::cout << "WATCHADDR: Checking for address in miss map. " << currentCycle << std::endl;
        }

      pageAddress.SetPhysicalAddress( mop->GetRequest( )->address.GetPhysicalAddress( ) >> 12 );
      lineOffset = (mop->GetRequest( )->address.GetPhysicalAddress( ) & 0xFFF) / 64;

      if( missMap->Present( pageAddress ) )
        {
          NVMDataBlock lineList;
          uint64_t *lineMap;

          missMap->Read( pageAddress, &lineList );
          lineMap = (uint64_t *)lineList.rawData;

          lineMask = (uint64_t)(1ULL << lineOffset);
          if( !((*lineMap) & lineMask) )
            {
              linePresent = false;
            }
        }
      else
        {
          linePresent = false;
        }

      /* if the address isn't present or the cache line is not, go to main mem. */
      if( !linePresent )
        {
          missMapMisses++;

          if( mmQueue.size( ) >= 150000 )
            {
              std::cout << "WARNING: Queue is full... This is usually bad." << std::endl;

              return false;
            }

          if( watchAddr != 0 && mop->GetRequest( )->address.GetPhysicalAddress( ) == watchAddr )
            {
              std::cout << "WATCHADDR: Address not in miss map, going to MM. " << currentCycle << std::endl;
            }

          MemOp *mmOp = new MemOp( );
          NVMainRequest *mmRequest = new NVMainRequest( );
          uint64_t mmRow, mmCol, mmBank, mmRank, mmChannel;

          *mmRequest = *(mop->GetRequest( ));
          mmRequest->status = MEM_REQUEST_INCOMPLETE;
          *mmOp = *mop;
          mmOp->SetRequest( mmRequest );
          mmRequest->tag = 0;
          mmRequest->arrivalCycle = currentCycle;
          mmRequest->programCounter = mop->GetRequest( )->programCounter;

          mmTranslator->Translate( mop->GetRequest( )->address.GetPhysicalAddress( ), &mmRow, &mmCol, &mmBank, &mmRank, &mmChannel );
          mmOp->GetRequest( )->address.SetTranslatedAddress( mmRow, mmCol, mmBank, mmRank, mmChannel );
          mmOp->SetAddress( mmOp->GetRequest( )->address );

          mmOp->GetRequest( )->reqInfo = (void*)mop;

          mop->GetRequest( )->arrivalCycle = currentCycle;
          mmOp->GetRequest( )->arrivalCycle = currentCycle;

          if( watchAddr != 0 && mop->GetRequest( )->address.GetPhysicalAddress( ) == watchAddr )
            {
              std::cout << "WATCHADDR: Original request set to "
                        << mmOp->GetRequest( )->reqInfo << ". mmRequest is " 
                        << (void*)(mmOp) << ". " << currentCycle << std::endl;
            }

          /* Try to issue, or block the cache if we can't (queue full) */
          if( !mmController[mmChannel]->StartCommand( mmOp ) )
            {
              mmOp->GetRequest( )->status = MEM_REQUEST_RETRY;
            }
          else
            {
              mmOp->GetRequest( )->issueCycle = currentCycle;
            }

          mop->GetRequest( )->issueCycle = currentCycle;
          
          mmQueue.push_back( mmOp );
          issuedQueue.push_back( mop ); // aoiwhefoiawhef

          if( !mop->GetRequest( )->isPrefetch )
            InjectPrefetch( mmOp );

          return true;
        }
      else
        {
          missMapHits++;
          
          if( watchAddr != 0 && mop->GetRequest( )->address.GetPhysicalAddress( ) == watchAddr )
            {
              std::cout << "WATCHADDR: Address FOUND in miss map, going to DRC. " << currentCycle << std::endl;
            }
        }
    }

  /* 
   *  Assume we can only hold so many instructions in this controller's queue.
   */
  if( drcQueue.size( ) >= 150000 )
    {
      std::cout << "WARNING: Queue is full... This is usually bad." << std::endl;

      if( watchAddr != 0 && mop->GetRequest( )->address.GetPhysicalAddress( ) == watchAddr )
        {
          std::cout << "WATCHADDR: Request could not be issued yet, returning to directory. " << currentCycle << std::endl;
        }
      return false;
    }

#ifdef DBGDRC
  std::cout << "0x" << std::hex << mop->GetRequest( )->address.GetPhysicalAddress( ) << std::dec 
            << ": command accepted to DRC queue at cycle " << currentCycle << std::endl;
#endif

  if( watchAddr != 0 && mop->GetRequest( )->address.GetPhysicalAddress( ) == watchAddr )
    {
      std::cout << "WATCHADDR: First sight of request in DRC cache controller. " << currentCycle << std::endl;
    }


#ifdef DBGQUEUE
  std::cout << "+++ Added item to DRC queue at cycle " << currentCycle << ". Size is " << drcQueue.size() << std::endl;
#endif
  if( missMap != NULL && mop->GetOperation( ) == READ )
    {
      // Prioritize demand reads 
      drcQueue.push_back( mop );
    }
  else
    {
      drcQueue.push_back( mop );
    }

  return true;
}


void DRCController::RequestComplete( NVMainRequest *request )
{
  std::deque<MemOp *>::iterator it;


  /* 
   *  The following requests have no follow up, but we need to free the memory.
   */
  if( request->tag == DRC_TAGREAD1 || request->tag == DRC_TAGREAD2 )
    {
      //delete request->memop;
      //delete request;
    }
  /*
   *  After the 3rd tag is read we know 100% if the data is cached or not.
   */
  else if( request->tag == DRC_TAGREAD3 )
    {
      /*
       *  Use a functional cache model to determine if the address was in the DRC cache
       *  (so that we do not need to actually read and write data to the tags in simulation).
       */
      bool miss;
      NVMainRequest *originalRequest;
      MemOp *originalMemOp;
      uint64_t fcrow, fccol, fcrank, fcbank, fcchannel;
      uint64_t lrow, lcol, lrank, lbank, lchannel;

      originalMemOp = (MemOp *)request->reqInfo;
      originalRequest = originalMemOp->GetRequest( );

      GetTranslator( )->Translate( originalRequest->address.GetPhysicalAddress( ), &lrow, &lcol, &lbank, &lrank, &lchannel );

      bankLocked[lrank][lbank] = false;

      GetTranslator( )->Translate( originalRequest->address.GetPhysicalAddress( ), &fcrow, &fccol, &fcbank, &fcrank, &fcchannel );

      /*
       *  If the original request type is a write, we will assume it always hits. If there
       *  is no space, a victim will be evicted.
       */
      miss = true;

      if( originalRequest->type == WRITE )
        {
          miss = false;
        }
      else if( originalRequest->type == READ )
        {
          if( functionalCache[fcrank][fcbank]->Present( request->address ) )
            miss = false;
          else
            miss = true;
#ifdef DBGDRC
          std::cout << "Found in functional cache? " << (!miss) << std::endl;
#endif
        }
      else
        {
          std::cout << "DRCController: Unknown cache operation: " << (originalRequest->type) << std::endl;
        }

#ifdef DBGDRC
      std::cout << "0x" << std::hex << request->address.GetPhysicalAddress( ) << std::dec
                << " Finished tag look up. Resulted in miss? " << miss << std::endl;
#endif

      if( watchAddr != 0 && originalRequest->address.GetPhysicalAddress( ) == watchAddr )
        {
          std::cout << "WATCHADDR: DRC_TAGREAD3 complete. Miss = " << miss << ". " << currentCycle << std::endl;
        }

      if( GetConfig( )->KeyExists( "AlwaysHit" ) && GetConfig( )->GetString( "AlwaysHit" ) == "true" )
        miss = false;

      /*
       *  On a miss, install this address in the cache. To do this, we create a new request.
       *  The main memory controller will mark this request as completed when it completes.
       *  We will intercept this in Cycle() then forward it back to NVMain and install the
       *  item in the cache.
       */
      if( miss )
        {
          MemOp *mmOp = new MemOp( );
          NVMainRequest *mmRequest = new NVMainRequest( );
          uint64_t mmRow, mmCol, mmBank, mmRank, mmChannel;

          *mmRequest = *request;
          mmRequest->status = MEM_REQUEST_INCOMPLETE;
          *mmOp = *originalMemOp;
          mmOp->SetRequest( mmRequest );
          mmRequest->tag = 0;
          mmRequest->arrivalCycle = currentCycle;
          mmRequest->programCounter = originalRequest->programCounter;

          misses++;

          /* Main memory request will return to drcQueue as a write, so we'll decrease this. */
          write_hits--;
          
          /* 
           *  Re-translate the physical address for the main memory system.
           */
          mmTranslator->Translate( originalRequest->address.GetPhysicalAddress( ), &mmRow, &mmCol, &mmBank, &mmRank, &mmChannel );
          mmOp->GetRequest( )->address.SetTranslatedAddress( mmRow, mmCol, mmBank, mmRank, mmChannel );
          mmOp->SetAddress( mmOp->GetRequest( )->address );
          
          /*
           *  Save the pointer to the original MemOp so we can find it later to delete from queue.
           */
          mmOp->GetRequest( )->reqInfo = (void*)originalMemOp;

          mmQueue.push_back( mmOp );
          
          /*
           *  See if we want to prefetch as well.
           */
          if( !originalRequest->isPrefetch )
            InjectPrefetch( mmOp );

          /* 
           *  The request failed in the DRAM cache... Attempt to issue this to the 
           *  main memory controller. If the main memory controller's queue is full, 
           *  just hold it in our queue until it can issue.
           */
          if( !mmController[mmChannel]->StartCommand( mmOp ) )
            {
              mmRequest->status = MEM_REQUEST_RETRY; // retry issuing this command later, since it failed.
            }
          else
            {
              mmRequest->issueCycle = currentCycle;

              if( watchAddr != 0 && originalRequest->address.GetPhysicalAddress( ) == watchAddr )
                {
                  std::cout << "WATCHADDR: Sent miss to main memory! " << currentCycle << std::endl;
                }
            }

          /*
           *  Note: We leave the request in the issuedQueue, since this will be marked
           *  as complete when it returns from main memory. NVMain has a copy of this
           *  pointer and will check for the status to be MEM_REQUEST_COMPLETE.
           */
        }
      else
        {
#ifdef DBGDRC
          std::cout << "Cache hit! Type is " << originalRequest->type << std::endl;
#endif
          if( originalRequest->type == READ )
            read_hits++;
          else if( originalRequest->tag != DRC_DATAWRITE ) // MissMap won't do write_hits--
            write_hits++;

          /*
           *  Check if the row containing the data is opened. If it is not, we need to issue
           *  an activate command as well. Also check if the first activate from the tail of
           *  the queue is for this row.
           */
          uint64_t wrow, wcol, wrank, wbank, wchannel;

          GetTranslator( )->Translate( originalRequest->address.GetPhysicalAddress( ), &wrow, &wcol, &wbank, &wrank, &wchannel );


          /*
          uint64_t actRow;
          bool actQueued = false;
              
          for( it = bankQueue[wrank][wbank].begin( ); it != bankQueue[wrank][wbank].end( ); it++ )
            {
              if( (*it)->GetOperation( ) == ACTIVATE )
                {
                  actQueued = true;
                  (*it)->GetRequest( )->address.GetTranslatedAddress( &actRow, NULL, NULL, NULL, NULL );
                }
              else if( (*it)->GetOperation( ) == PRECHARGE )
                {
                  actQueued = false;
                }
            }
          */
    
          MemOp *actOp;
          MemOp *preOp;
          NVMainRequest *actReq;
          NVMainRequest *preReq;

          if( actQueued[wrank][wbank] && actRow[wrank][wbank] != wrow )
            {
              actOp = new MemOp( );
              actReq = new NVMainRequest( );

              actReq->type = ACTIVATE;
              actReq->bulkCmd = CMD_NOP;
              actReq->issueController = this;
              actReq->memop = actOp;
              actReq->tag = DRC_DELETEME;
              actReq->arrivalCycle = currentCycle;
              actOp->SetOperation( ACTIVATE );
              actOp->SetRequest( actReq );

              actReq->address.SetPhysicalAddress( originalRequest->address.GetPhysicalAddress( ) );
              actReq->address.SetTranslatedAddress( wrow, wcol, wbank, wrank, wchannel );
              actOp->SetAddress( actReq->address );

              preOp = new MemOp( );
              preReq = new NVMainRequest( );

              preReq->type = PRECHARGE;
              preReq->bulkCmd = CMD_NOP;
              preReq->issueController = this;
              preReq->memop = preOp;
              preReq->tag = DRC_DELETEME;
              preReq->arrivalCycle = currentCycle;
              preOp->SetOperation( PRECHARGE );
              preOp->SetRequest( preReq );

              preReq->address.SetPhysicalAddress( originalRequest->address.GetPhysicalAddress( ) );
              preReq->address.SetTranslatedAddress( wrow, wcol, wbank, wrank, wchannel );
              preOp->SetAddress( preReq->address );
            }


          if( originalRequest->type == READ )
            {
              /*
               *  Read hit, do the actual read (we only read the tags so far)
               */

              MemOp *cacheRead = new MemOp( );
              MemOp *tagWrite = new MemOp( );
              NVMainRequest *readReq = new NVMainRequest( );
              NVMainRequest *tagReq = new NVMainRequest( );

              *cacheRead = *originalMemOp;
              *tagWrite = *originalMemOp;

              *readReq = *originalRequest;
              *tagReq = *originalRequest;

              /* Convert the original request addresses from main memory to DRC translations */
              readReq->address.SetTranslatedAddress( wrow, wcol, wbank, wrank, wchannel );
              tagReq->address.SetTranslatedAddress( wrow, wcol, wbank, wrank, wchannel );

              cacheRead->SetAddress( readReq->address );
              tagWrite->SetAddress( tagReq->address );

              readReq->tag = DRC_DATAREAD;
              tagReq->tag = DRC_TAGUPDATE;

              readReq->type = READ;
              tagReq->type = WRITE;

              readReq->issueController = this;
              tagReq->issueController = this;

              readReq->arrivalCycle = currentCycle;
              tagReq->arrivalCycle = currentCycle;

              readReq->memop = cacheRead;
              tagReq->memop = tagWrite;

              readReq->reqInfo = (void*)originalMemOp;
              tagReq->reqInfo = (void*)originalMemOp;

              cacheRead->SetOperation( READ );
              tagWrite->SetOperation( WRITE );

              cacheRead->SetRequest( readReq );
              tagWrite->SetRequest( tagReq );

              if( actQueued[wrank][wbank] && actRow[wrank][wbank] != wrow )
                {
                  bankQueue[wrank][wbank].push_back( preOp );
                  bankQueue[wrank][wbank].push_back( actOp );
                  actQueued[wrank][wbank] = true;
                  actRow[wrank][wbank] = wrow;
                  tag_requeries++;
                }

              bankQueue[wrank][wbank].push_back( cacheRead );
              bankQueue[wrank][wbank].push_back( tagWrite );

              /* Read from functional cache to update LRU info. */
              NVMDataBlock dummy;
              DRCCacheEntry *drcEnt;

              if( !GetConfig( )->KeyExists( "AlwaysHit" ) || GetConfig( )->GetString( "AlwaysHit" ) != "true" )
                {
                  functionalCache[fcrank][fcbank]->Read( originalRequest->address, &dummy ); 

                  drcEnt = (DRCCacheEntry *)(dummy.rawData);
                  drcEnt->refCount++;

                  functionalCache[fcrank][fcbank]->UpdateData( originalRequest->address, dummy );
                }
            }
          else if( originalRequest->type == WRITE )
            {
              /* Write to functional cache to update LRU info if it's in the cache. */
              NVMDataBlock dummy;
              if( functionalCache[fcrank][fcbank]->Present( originalRequest->address ) )
                {
                  DRCCacheEntry *drcEnt;

                  functionalCache[fcrank][fcbank]->Read( originalRequest->address, &dummy ); 

                  drcEnt = (DRCCacheEntry *)(dummy.rawData);
                  drcEnt->refCount++;

                  functionalCache[fcrank][fcbank]->Write( originalRequest->address, dummy ); 
                }

              /* Issue our activate if the row will be closed when our writes are issued. */
              if( actQueued[wrank][wbank] && actRow[wrank][wbank] != wrow )
                {
                  bankQueue[wrank][wbank].push_back( preOp );
                  bankQueue[wrank][wbank].push_back( actOp );
                  actQueued[wrank][wbank] = true;
                  actRow[wrank][wbank] = wrow;
                  tag_requeries++;
                }

              /* 
               *  Set is full. Read out the victim to be written back to main memory first,
               *  then write the data to the DRAM cache, and send victim to main memory.
               */
              if( functionalCache[fcrank][fcbank]->SetFull( originalRequest->address ) )
                {
                  if( watchAddr != 0 && originalRequest->address.GetPhysicalAddress( ) == watchAddr )
                    {
                      std::cout << "WATCHADDR: Cache set is full, reading victim. " << currentCycle << std::endl;
                    }

                  NVMAddress victim;
                  bool dirty;
                  dirty = functionalCache[fcrank][fcbank]->ChooseVictim( originalRequest->address, &victim );

                  if( watchAddr != 0 && originalRequest->address.GetPhysicalAddress( ) == watchAddr )
                    {
                      std::cout << "WATCHADDR: Chose victam at address 0x" << std::hex
                                << victim.GetPhysicalAddress( ) << std::dec << ". "
                                << currentCycle << std::endl;
                    }

                  evictions++;
                  if( dirty )
                    {
                      uint64_t vicRow, vicCol, vicBank, vicRank, vicChannel;

                      GetTranslator( )->Translate( victim.GetPhysicalAddress( ), &vicRow, &vicCol, &vicBank, &vicRank, &vicChannel );

                      if( vicChannel != wchannel || vicRank != wrank || vicBank != wbank || vicRow != wrow )
                        {
                          std::cout << "ERROR: Victim channel/rank/bank/row does NOT match original request!"
                                    << std::endl;
                          std::cout << "Original request 0x" << std::hex << originalRequest->address.GetPhysicalAddress( )
                                    << std::dec << "CH/RA/BA/RO/CO = " << wchannel << "/" << wrank << "/"
                                    << wbank << "/" << wrow << "/" << wcol << std::endl;
                          std::cout << "Victim request 0x" << std::hex << victim.GetPhysicalAddress( )
                                    << std::dec << "CH/RA/BA/RO/CO = " << vicChannel << "/" << vicRank << "/"
                                    << vicBank << "/" << vicRow << "/" << vicCol << std::endl;
                        }

                      /* Send read request to cache first. */
                      MemOp *cacheRead = new MemOp( );
                      NVMainRequest *readReq = new NVMainRequest( );

                      *cacheRead = *originalMemOp;

                      *readReq = *originalRequest;

                      readReq->tag = DRC_VICTIMREAD;
                      readReq->type = READ;
                      readReq->reqInfo = (void*)originalMemOp;
                      readReq->issueController = this;
                      readReq->memop = cacheRead;
                      readReq->arrivalCycle = currentCycle;

                      cacheRead->SetOperation( READ );
                      cacheRead->SetRequest( readReq );

                      bankQueue[wrank][wbank].push_back( cacheRead );

                      dirty_evictions++;
                    }
                  else
                    {
                      clean_evictions++;
                    }

                  MissMapEvict( &victim );
                }

              /*
               *  Write the data into the DRAM cache and update the tags (simulate two writes)
               */
              MemOp *cacheWrite = new MemOp( );
              MemOp *tagWrite = new MemOp( );
              NVMainRequest *cacheReq = new NVMainRequest( );
              NVMainRequest *tagReq = new NVMainRequest( );

              *cacheWrite = *originalMemOp;
              *tagWrite = *originalMemOp;

              *cacheReq = *originalRequest;
              *tagReq = *originalRequest;

              /* Convert the original request addresses from main memory to DRC translations */
              cacheReq->address.SetTranslatedAddress( wrow, wcol, wbank, wrank, wchannel );
              tagReq->address.SetTranslatedAddress( wrow, wcol, wbank, wrank, wchannel );

              cacheWrite->SetAddress( cacheReq->address );
              tagWrite->SetAddress( tagReq->address );

              if( functionalCache[fcrank][fcbank]->Present( originalRequest->address ) )
                cacheReq->tag = DRC_DATAWRITE;
              else
                cacheReq->tag = DRC_DATAINSTALL;
              tagReq->tag = DRC_TAGUPDATE;

              cacheReq->arrivalCycle = currentCycle;
              tagReq->arrivalCycle = currentCycle;

              cacheReq->type = WRITE;
              tagReq->type = WRITE;

              cacheWrite->SetOperation( WRITE );
              tagWrite->SetOperation( WRITE );

              cacheWrite->SetRequest( cacheReq );
              tagWrite->SetRequest( tagReq );

              bankQueue[wrank][wbank].push_back( cacheWrite );
              bankQueue[wrank][wbank].push_back( tagWrite );

              /* Tell NVMain the request is done (was written to DRC, all done). */
              for( it = issuedQueue.begin( ); it != issuedQueue.end( ); it++ )
                {
                  if( (*it)->GetRequest( ) == originalRequest )
                    break;
                }

              assert( it != issuedQueue.end( ) );

              if( watchAddr != 0 && originalRequest->address.GetPhysicalAddress( ) == watchAddr )
                {
                  std::cout << "WATCHADDR: DRC write hit. Marking Original Request complete." << std::endl;
                  std::cout << "WATCHADDR: Original request ptr is 0x" << std::hex << (void*)originalRequest
                            << std::dec << ". " << currentCycle << std::endl;
                }

              /* Fake queue to see if we shouldn't prefetch something installing to DRC. */
              writeList.push_back( (*it) );

              originalRequest->status = MEM_REQUEST_COMPLETE;
              originalRequest->completionCycle = currentCycle;
              UpdateAverageLatency( originalRequest );
              issuedQueue.erase( it );
            }
        }

      //delete request->memop;
      //delete request;
    }
  else if( request->tag == DRC_DATAREAD )
    {
      /*
       *  Find the original request and send it back to NVMain.
       */
      MemOp *originalMemOp = (MemOp *)request->reqInfo;
      NVMainRequest *originalRequest = originalMemOp->GetRequest( );

      for( it = issuedQueue.begin( ); it != issuedQueue.end( ); it++ )
        {
          if( (*it)->GetRequest( ) == originalRequest )
            break;
        }

      assert( it != issuedQueue.end( ) );

      if( watchAddr != 0 && originalRequest->address.GetPhysicalAddress( ) == watchAddr )
        {
          std::cout << "WATCHADDR: DRC_DATAREAD complete. Marking Original Request complete." << std::endl;
          std::cout << "WATCHADDR: Original request ptr is 0x" << std::hex << (void*)originalRequest
                    << std::dec << ". " << currentCycle << std::endl;
        }

      originalRequest->status = MEM_REQUEST_COMPLETE;
      originalRequest->completionCycle = currentCycle;
      UpdateAverageLatency( originalRequest );
      issuedQueue.erase( it );

      //delete request->memop;
      //delete request;
    }
  else if( request->tag == DRC_VICTIMREAD )
    {
      /*
       *  Write the victim data back to main memory.
       */
      MemOp *victimWrite = new MemOp( );
      NVMainRequest *victimReq = new NVMainRequest( );
      uint64_t vrow, vcol, vrank, vbank, vchannel;
      NVMainRequest *originalRequest;
      MemOp *originalMemOp;

      originalMemOp = (MemOp *)request->reqInfo;
      originalRequest = originalMemOp->GetRequest( );

      mmTranslator->Translate( originalRequest->address.GetPhysicalAddress( ), &vrow, &vcol, &vbank, &vrank, &vchannel );

      *victimWrite = *originalMemOp;
      *victimReq = *originalRequest;

      victimReq->type = WRITE;
      victimReq->tag = 0;
      victimReq->arrivalCycle = currentCycle;

      victimWrite->SetOperation( WRITE );
      victimWrite->SetRequest( victimReq );


      if( watchAddr != 0 && originalRequest->address.GetPhysicalAddress( ) == watchAddr )
        {
          std::cout << "WATCHADDR: DRC_VICTIMREAD complete. Writing data back to MM. "
                    << currentCycle << std::endl;
        }


      if( !mmController[vchannel]->StartCommand( victimWrite ) )
        {
          wbQueue.push_back( victimWrite );
        }
#ifdef DBGDRC
      else
        {
          std::cout << "Wrote back victim to main memory!" << std::endl;
        }
#endif
        
      //delete request->memop;
      //delete request;
    }
  else if( request->tag == DRC_DATAWRITE || request->tag == DRC_DATAINSTALL )
    {
      /*
       *  Put this address in the functional cache now. 
       */
      NVMDataBlock oldData;
      NVMDataBlock newData;
      NVMAddress victim;
      DRCCacheEntry *drcEnt; 
      uint64_t fcrow, fccol, fcrank, fcbank, fcchannel;

      GetTranslator( )->Translate( request->address.GetPhysicalAddress( ), &fcrow, &fccol, &fcbank, &fcrank, &fcchannel );

      if( functionalCache[fcrank][fcbank]->SetFull( request->address ) )
        {
          functionalCache[fcrank][fcbank]->ChooseVictim( request->address, &victim );
          functionalCache[fcrank][fcbank]->Evict( victim, &oldData );
        }

      drcEnt = new DRCCacheEntry;
   
      drcEnt->prefetched = request->isPrefetch;
      drcEnt->refCount = (request->isPrefetch) ? 0 : 1; 
      newData.rawData = (void *)drcEnt;

      if( request->tag == DRC_DATAINSTALL )
        functionalCache[fcrank][fcbank]->Install( request->address, newData ); 
#ifdef DBGDRC
      std::cout << "0x" << std::hex << request->address.GetPhysicalAddress( ) << std::dec << ": Installed in functional cache." << std::endl;
#endif
 
      MissMapInstall( request );

      if( watchAddr != 0 && request->address.GetPhysicalAddress( ) == watchAddr )
        {
          if( request->tag == DRC_DATAWRITE )
            std::cout << "WATCHADDR: DRC_DATAWRITE complete. Address installed in functional cache. "
                      << currentCycle << std::endl;
          else
            std::cout << "WATCHADDR: DRC_DATAINSTALL complete. Address installed in functional cache. "
                      << currentCycle << std::endl;
        }


      if( installCache && installCache->Present( request->address ) )
        {
          InstallCacheEntry *icEntry;
          NVMDataBlock icData;

          installCache->Read( request->address, &icData );
          icEntry = (InstallCacheEntry *)(icData.rawData);

          icEntry->installed = true;

          std::map<uint64_t, uint64_t> *refCounter;

          if( icEntry->prefetched )
            {
              if( icEntry->referenced )
                refCounter = &rpRefCount;
              else
                refCounter = &upRefCount;
            }
          else
            {
              if( icEntry->referenced )
                refCounter = &rdRefCount;
              else
                refCounter = &udRefCount;
            }

          std::map<uint64_t, uint64_t>::iterator refIt;

          refIt = refCounter->find( request->address.GetPhysicalAddress( ) );

          if( refIt == refCounter->end( ) )
            refCounter->insert( std::pair<uint64_t, uint64_t>( request->address.GetPhysicalAddress( ), 1 ) );
        }


      std::deque<MemOp *>::iterator it;

      for( it = writeList.begin( ); it != writeList.end( ); it++ )
        {
          //if( (*it)->GetRequest( ) == originalRequest )
          if( (*it)->GetRequest( )->address.GetPhysicalAddress( ) == request->address.GetPhysicalAddress( ) )
            {
              writeList.erase( it );
              break;
            }
        }


      //delete request->memop;
      //delete request;
    }
  else if( request->tag == DRC_DELETEME )
    {
      //delete request->memop;
      //delete request;
    }
}


/*
 *  Check memory operations in the DRC queue and issue tag read requests
 */
void DRCController::Cycle( )
{
  std::deque<MemOp *>::iterator it;


  /*
   *  Retry writebacks
   */
  for( it = wbQueue.begin( ); it != wbQueue.end( ); it++ )
    {
      if( wbQueue.empty( ) )
        break;

      uint64_t vrow, vcol, vrank, vbank, vchannel;

      mmTranslator->Translate( (*it)->GetRequest( )->address.GetPhysicalAddress( ), &vrow, &vcol, &vrank, &vbank, &vchannel );

      if( watchAddr != 0 && (*it)->GetRequest( )->address.GetPhysicalAddress( ) == watchAddr )
        {
          std::cout << "WATCHADDR: Writeback being retried. Writing data back to MM. " 
                    << currentCycle << std::endl;
        }

      if( mmController[vchannel]->StartCommand( (*it) ) )
        {
          wbQueue.erase( it );
        }
      else
        {
          std::cout << "WARN: WB to MM failed again " << std::endl;
        }
    }


  /*
   *  Look for memory requests that went to the DRAM cache but resulted in a miss.
   */
  for( it = mmQueue.begin( ); it != mmQueue.end( ); it++ )
    {
      if( mmQueue.empty( ) )
        break;

      /*
       *  This request couldn't be issued originally, so let's retry.
       */
      if( (*it)->GetRequest( )->status == MEM_REQUEST_RETRY )
        {
          uint64_t mmRow, mmCol, mmRank, mmBank, mmChannel;

          mmTranslator->Translate( (*it)->GetRequest( )->address.GetPhysicalAddress( ), &mmRow, &mmCol, &mmBank, &mmRank, &mmChannel );
          
          if( mmController[mmChannel]->StartCommand( (*it) ) )
            {
              (*it)->GetRequest( )->status = MEM_REQUEST_INCOMPLETE;
              (*it)->GetRequest( )->issueCycle = currentCycle;
#ifdef DRCDBG
              std::cout << "0x" << std::hex << (*it)->GetRequest( )->address.GetPhysicalAddress( ) << std::dec 
                        << " Issued read to main memory after RETRY." << std::endl;
#endif

              if( watchAddr != 0 && (*it)->GetRequest( )->address.GetPhysicalAddress( ) == watchAddr )
                {
                  std::cout << "WATCHADDR: Retry issued to MM. " 
                            << currentCycle << std::endl;
                }
            }
          else
            {
              if( watchAddr != 0 && (*it)->GetRequest( )->address.GetPhysicalAddress( ) == watchAddr )
                {
                  std::cout << "WATCHADDR: Can't issue to MM yet! "
                            << currentCycle << std::endl;
                }
            }
        }
      /* 
       *  See if a request completed. If a READ request went to main memory, we can assume
       *  that it missed in the DRAM cache, and we can install it. If it was a WRITE
       *  request, we can assume it was a writeback from the DRAM cache.
       */
      else if( (*it)->GetRequest( )->status == MEM_REQUEST_COMPLETE )
        {
          if( watchAddr != 0 && (*it)->GetRequest( )->address.GetPhysicalAddress( ) == watchAddr )
            {
              std::cout << "WATCHADDR: MM read completed. Attempting to install in DRC."
                        << " Note: DRC queue size is " << drcQueue.size( ) << ". "
                        << "mmRequest is " << (void*)(*it) << ". "
                        << currentCycle << std::endl;
            }


          if( (*it)->GetRequest( )->type == READ && drcQueue.size( ) < 150000 ) 
            {
              if( PrefetchInProgress( (*it)->GetRequest( )->address )  )
                {
                  std::deque<MemOp *>::iterator pfit;

                  /* If something is waiting for this prefetch, send it back now. */
                  for( pfit = pfwaitQueue.begin( ); pfit != pfwaitQueue.end( ); pfit++ )
                    {
                      if( pfwaitQueue.empty( ) )
                        break;

                      if( (*pfit)->GetRequest( )->address.GetPhysicalAddress( )
                          == (*it)->GetRequest( )->address.GetPhysicalAddress( ) )
                        {
                          (*pfit)->GetRequest( )->status = MEM_REQUEST_COMPLETE;
                          (*pfit)->GetRequest( )->completionCycle = currentCycle;
                          //UpdateAverageMMLatency( (*pfit)->GetRequest( ) );

                          if( watchAddr != 0 && (*it)->GetRequest( )->address.GetPhysicalAddress( ) == watchAddr )
                            {
                              std::cout << "WATCHADDR: Request returned from PF wait queue. "
                                        << currentCycle << std::endl;
                            }

                          pfit = pfwaitQueue.erase( pfit );

                          if( pfwaitQueue.empty() || pfit == pfwaitQueue.end() )
                            break;
                        }
                    }


                  /*Remove this from the prefetches in progress queue. */
                  for( pfit = pfInProgress.begin( ); pfit != pfInProgress.end( ); pfit++ )
                    {
                      if( pfInProgress.empty( ) )
                        break;

                      if( (*pfit)->GetRequest( )->address.GetPhysicalAddress( )
                          == (*it)->GetRequest( )->address.GetPhysicalAddress( ) )
                        {
                          if( watchAddr != 0 && (*it)->GetRequest( )->address.GetPhysicalAddress( ) == watchAddr )
                            {
                              std::cout << "WATCHADDR: Request removed from PF in progress queue. "
                                        << currentCycle << std::endl;
                            }

                          pfit = pfInProgress.erase( pfit );

                          if( pfInProgress.empty() || pfit == pfInProgress.end() )
                            break;
                        }
                    }
                }


              if( installCache )
                {
                  if( watchAddr != 0 && (*it)->GetRequest( )->address.GetPhysicalAddress( ) == watchAddr )
                    {
                      std::cout << "WATCHADDR: Request going in install cache. "
                                << currentCycle << std::endl;
                    }

                  if( !installCache->Present( (*it)->GetRequest( )->address ) )
                    {
                      if( installCache->SetFull( (*it)->GetRequest( )->address ) )
                        {
                          if( watchAddr != 0 && (*it)->GetRequest( )->address.GetPhysicalAddress( ) == watchAddr )
                            {
                              std::cout << "WATCHADDR: Install cache set full, need to evict. "
                                        << currentCycle << std::endl;
                            }

                          /* Evict whatever for right now. */
                          NVMDataBlock icData;
                          NVMAddress victim;
                          InstallCacheEntry *icEntry;

                          installCache->ChooseVictim( (*it)->GetRequest( )->address, &victim );
                          installCache->Evict( victim, &icData );

                          icEntry = (InstallCacheEntry *)(icData.rawData);

                          if( watchAddr != 0 && victim.GetPhysicalAddress( ) == watchAddr )
                            {
                              std::cout << "WATCHADDR: Address chosen as install cache victim. "
                                        << currentCycle << std::endl;
                            }


                          icEvicts++;
                          if( icEntry->referenced )
                            {
                              if( icEntry->prefetched )
                                icPrefetchRefEvicts++;
                              else
                                icDemandRefEvicts++;
                            }
                          else
                            {
                              if( icEntry->prefetched )
                                icPrefetchUnrefEvicts++;
                              else
                                icDemandUnrefEvicts++;
                            }

                          if( icEntry->installed )
                            icInstalledEvicts++;
                          else
                            icUninstalledEvicts++;

                          //delete icEntry;
                        }


                      NVMDataBlock icData;
                      InstallCacheEntry *icEntry = new InstallCacheEntry;

                      icEntry->referenced = false;
                      icEntry->prefetched = false;
                      icEntry->installed = false;

                      if( (*it)->GetRequest( )->tag == DRC_PREFETCHED )
                        {
                          icEntry->prefetched = true;
                          icEntry->triggerAddress.SetPhysicalAddress( (*it)->GetRequest( )->pfTrigger.GetPhysicalAddress( ) );
                        }

                      icData.rawData = (void *)icEntry;

                      installCache->Install( (*it)->GetRequest( )->address, icData );

                      writeList.push_back( (*it) );

                      if( watchAddr != 0 && (*it)->GetRequest( )->address.GetPhysicalAddress( ) == watchAddr )
                        {
                          std::cout << "WATCHADDR: Installed in install cache. "
                                    << currentCycle << std::endl;
                        }
                    }
                  else
                    {
                      if( watchAddr != 0 && (*it)->GetRequest( )->address.GetPhysicalAddress( ) == watchAddr )
                        {
                          std::cout << "WATCHADDR: Request already in install cache. "
                                    << currentCycle << std::endl;
                        }
                    }
                }



              MemOp *installOp = new MemOp( );
              *installOp = *(*it);
              NVMainRequest *installReq = new NVMainRequest( );
              *installReq = *((*it)->GetRequest( ));

              /* Re-translate address for DRC. */
              uint64_t wrow, wcol, wbank, wrank, wchannel;

              GetTranslator( )->Translate( installReq->address.GetPhysicalAddress( ), &wrow, &wcol, &wbank, &wrank, &wchannel );
              installReq->address.SetTranslatedAddress( wrow, wcol, wbank, wrank, wchannel );
              installOp->SetAddress( installReq->address );

              installReq->status = MEM_REQUEST_INCOMPLETE;
              installReq->tag = DRC_DATAWRITE;
              installReq->type = WRITE;
              installReq->arrivalCycle = currentCycle;
              installReq->issueController = this;
              installReq->reqInfo = (*it)->GetRequest( )->reqInfo;

              if( (*it)->GetRequest( )->isPrefetch )
                installReq->isPrefetch = true;
              else
                installReq->isPrefetch = false;


              installOp->SetRequest( installReq );
              installOp->SetOperation( WRITE );

#ifdef DBGQUEUE
              std::cout << "+++ Added install to drcQueue at cycle " << currentCycle << ". Size is " << drcQueue.size() << std::endl;
#endif
              drcQueue.push_back( installOp );

              (*it)->GetRequest( )->completionCycle = currentCycle;
              UpdateAverageMMLatency( (*it)->GetRequest( ) );

              /* Free memory allocated for the request and MemOp. */
              //delete (*it)->GetRequest( );
              //delete (*it);

              if( watchAddr != 0 && (*it)->GetRequest( )->address.GetPhysicalAddress( ) == watchAddr )
                {
                  std::cout << "WATCHADDR: Install request placed in drcQueue. "
                            << currentCycle << std::endl;
                }

              /*
               *  Search for this request in the issue queue, and mark as complete
               *  so it returns to NVMain.
               */
              std::deque<MemOp *>::iterator iit;
              bool foundOriginal = false;
              
              if( (*it)->GetRequest( )->tag != DRC_PREFETCHED )
                {
                  if( watchAddr != 0 && (*it)->GetRequest( )->address.GetPhysicalAddress( ) == watchAddr )
                    {
                      std::cout << "WATCHADDR: Looking for original request to mark. "
                                << currentCycle << std::endl;
                    }

                  for( iit = issuedQueue.begin( ); iit != issuedQueue.end( ); iit++ )
                    {
                      if( issuedQueue.empty( ) )
                        break;

                      if( (*iit) == (MemOp*)(*it)->GetRequest( )->reqInfo )
                        {
                          (*iit)->GetRequest( )->status = MEM_REQUEST_COMPLETE;
                          (*iit)->GetRequest( )->completionCycle = currentCycle;
                          //UpdateAverageLatency( (*iit)->GetRequest( ) );
                          issuedQueue.erase( iit );
                          foundOriginal = true;

                          if( watchAddr != 0 && (*it)->GetRequest( )->address.GetPhysicalAddress( ) == watchAddr )
                            {
                              std::cout << "WATCHADDR: Removed from issue queue and marked complete! " 
                                        << "Marked request is 0x" << (void*)(*iit) << ". "
                                        << currentCycle << std::endl;
                            }

                          break;
                        }
                    }

                  /* Sanity checks to make sure everything worked. */
                  if( !foundOriginal )
                    {
                      std::cout << "ERROR: Could not find original request to mark as complete! " 
                                << "Address is 0x" << std::hex
                                << (*it)->GetRequest( )->address.GetPhysicalAddress( ) 
                                << std::dec << std::endl;
                    }
                  else
                    {
                      for( iit = issuedQueue.begin( ); iit != issuedQueue.end( ); iit++ )
                        {
                          if( issuedQueue.empty( ) )
                            break;

                          if( (*iit) == (MemOp*)(*it)->GetRequest( )->reqInfo )
                            {
                              std::cout << "ERROR: Found multiple original requests! (One per error)." << std::endl;
                            }
                        }
                    }
                }

              mmQueue.erase( it );
            }
          else if( (*it)->GetRequest( )->type == WRITE )
            {
              // All done
              mmQueue.erase( it );
            }
          else
            {
              if( watchAddr != 0 && (*it)->GetRequest( )->address.GetPhysicalAddress( ) == watchAddr )
                {
                  std::cout << "WATCHADDR: Leaving in mmQueue! " << currentCycle << std::endl;
                }
            }

          if( it == mmQueue.end() || mmQueue.empty() )
            break;
        }
    }


  bool dropped = true;

  while( dropped )
    {
      dropped = false;

      if( installCache )
        {
          if( drcQueue.empty( ) )
            break;

          it = drcQueue.begin( );

          if( installCache->Present( (*it)->GetRequest( )->address ) )
            {
              InstallCacheEntry *icEntry;
              NVMDataBlock icData;
              NVMDataBlock dummy;

              installCache->Read( (*it)->GetRequest( )->address, &icData );
              icEntry = (InstallCacheEntry *)(icData.rawData);

              if( GetConfig()->KeyExists( "DROP_UP" ) && GetConfig( )->GetString( "DROP_UP" ) == "true" 
                  && icEntry->prefetched && !icEntry->referenced )
                {
#ifdef DBGQUEUE
                  std::cout << "--- UP deleted from DRC queue at cycle " << currentCycle << ". Size is " << drcQueue.size() << std::endl;
#endif
                  drcQueue.erase( it );

                  installCache->Evict( (*it)->GetRequest( )->address, &dummy );

                  dropped = true;
                  drcUPdrops++;
                }

              if( GetConfig()->KeyExists( "DROP_UD" ) && GetConfig( )->GetString( "DROP_UD" ) == "true" 
                  && !icEntry->prefetched && !icEntry->referenced )
                {
#ifdef DBGQUEUE
                  std::cout << "--- UD deleted from DRC queue at cycle " << currentCycle << ". Size is " << drcQueue.size() << std::endl;
#endif
                  drcQueue.erase( it );

                  installCache->Evict( (*it)->GetRequest( )->address, &dummy );

                  dropped = true;
                  drcUDdrops++;
                }
            }
        }
    }



  /*
   *  Schedule DRAM cache commands now. 
   */
  bool scheduled = false;

  /*
   *  Scheduling decision #1: If a bank is open already, and we have a request to the row,
   *                          prioritize that request to the open row. 
   */
  int posCount = 0;
  for( it = drcQueue.begin( ); it != drcQueue.end( ); it++ )
    {
      if( drcQueue.size( ) == 0 || scheduled == true ) // why the hell ?
        break;

      posCount++;

      if( watchAddr != 0 && (*it)->GetRequest( )->address.GetPhysicalAddress( ) == watchAddr )
        {
          std::cout << "WATCHADDR: Sitting in the DRC queue with " << drcQueue.size( )
                    << " other items. Position is " << posCount << ". " << currentCycle << std::endl; 
        }
      

      /*
      if( installCache && installCache->Present( (*it)->GetRequest( )->address ) 
           && (*it)->GetRequest( )->tag != DRC_DATAWRITE )
        {
          InstallCacheEntry *icEntry;
          NVMDataBlock icData;

          icHits++;
  
          installCache->Read( (*it)->GetRequest( )->address, &icData );
          icEntry = (InstallCacheEntry *)(icData.rawData);

          icEntry->referenced = true;
        }
      */

      uint64_t rank, bank, row;

      (*it)->GetRequest( )->address.GetTranslatedAddress( &row, NULL, &bank, &rank, NULL );

      if( ( actQueued[rank][bank] && actRow[rank][bank] == row ) && bankQueue[rank][bank].empty()
           && !bankLocked[rank][bank] )
        {
          if( watchAddr != 0 && (*it)->GetRequest( )->address.GetPhysicalAddress( ) == watchAddr )
            {
              std::cout << "WATCHADDR: Row buffer hit. Queuing up tag reads. Row is " << row 
                        << ". " << currentCycle << std::endl;
              std::cout << "WATCHADDR: Dumping bank queue. ACT row is " << actRow[rank][bank] << std::endl;

              std::deque<MemOp *>::iterator dit; 
              for( dit = bankQueue[rank][bank].begin( ); dit != bankQueue[rank][bank].end( ); dit++ )
                {
                  uint64_t dumpRow;

                  (*dit)->GetRequest( )->address.GetTranslatedAddress( &dumpRow, NULL, NULL, NULL, NULL ); 

                  switch( (*dit)->GetOperation( ) )
                    {
                      case READ:
                        std::cout << "           READ row " << dumpRow << std::endl; 
                        break;
                      case WRITE:
                        std::cout << "           WRITE row " << dumpRow << std::endl;
                        break;
                      case ACTIVATE:
                        std::cout << "           ACTIVATE row " << dumpRow << std::endl;
                        break;
                      case PRECHARGE:
                        std::cout << "           PRECHARGE row " << dumpRow << std::endl;
                        break;
                      default:
                        std::cout << "           UNKNOWN row " << dumpRow << std::endl;
                        break;
                    }
                }
            }
          /*
           *  Issue 3 reads for the tag.
           */
          MemOp *tagRead1 = new MemOp( );
          MemOp *tagRead2 = new MemOp( );
          MemOp *tagRead3 = new MemOp( );
          NVMainRequest *tagReq1 = new NVMainRequest( );
          NVMainRequest *tagReq2 = new NVMainRequest( );
          NVMainRequest *tagReq3 = new NVMainRequest( );

          *tagRead1 = *(*it);
          *tagRead2 = *(*it);
          *tagRead3 = *(*it);

          *tagReq1 = *(tagRead1->GetRequest( ));
          *tagReq2 = *(tagRead2->GetRequest( ));
          *tagReq3 = *(tagRead3->GetRequest( ));
          
          tagReq1->tag = DRC_TAGREAD1;
          tagReq2->tag = DRC_TAGREAD2;
          tagReq3->tag = DRC_TAGREAD3;

          tagReq1->type = READ;
          tagReq2->type = READ;
          tagReq3->type = READ;

          tagRead1->SetOperation( READ );
          tagRead2->SetOperation( READ );
          tagRead3->SetOperation( READ );

          tagRead1->SetRequest( tagReq1 );
          tagRead2->SetRequest( tagReq2 );
          tagRead3->SetRequest( tagReq3 );

          tagReq1->issueController = this;
          tagReq2->issueController = this;
          tagReq3->issueController = this;

          tagReq1->memop = tagRead1;
          tagReq2->memop = tagRead2;
          tagReq3->memop = tagRead3;

          tagReq1->arrivalCycle = currentCycle;
          tagReq2->arrivalCycle = currentCycle;
          tagReq3->arrivalCycle = currentCycle;

          /*
           *  We'll use the user defined request info to point to the
           *  original request.
           */
          tagReq1->reqInfo = (void*)((*it));
          tagReq2->reqInfo = (void*)((*it));
          tagReq3->reqInfo = (void*)((*it));


          bankQueue[rank][bank].push_back( tagRead1 ); 
          bankQueue[rank][bank].push_back( tagRead2 ); 
          bankQueue[rank][bank].push_back( tagRead3 ); 

          lastClose[rank][bank]++;
          bankLocked[rank][bank] = true;

          (*it)->GetRequest( )->issueCycle = currentCycle;

          issuedQueue.push_back( (*it) );
          drcQueue.erase( it );
#ifdef DBGQUEUE
          std::cout << "--- Issued 1 deleted from DRC queue at cycle " << currentCycle << ". Size is " << drcQueue.size() << std::endl;
#endif

          drcRBhits++;

          scheduled = true;
          break;
        }
    }


  /*
   *  Scheduling decision #2: If a bank is open already, and we have a request to a different,
   *                          row, or more than N requests were issued, close this row to
   *                          prevent starvation.
   */
  for( it = drcQueue.begin( ); it != drcQueue.end( ); it++ )
    {
      if( drcQueue.size( ) == 0 || scheduled == true ) // why the hell ?
        break;

      uint64_t rank, bank, row;

      (*it)->GetRequest( )->address.GetTranslatedAddress( &row, NULL, &bank, &rank, NULL );

      if( ( actQueued[rank][bank] && actRow[rank][bank] != row ) && bankQueue[rank][bank].empty()
          && !bankLocked[rank][bank] )
        {
          if( watchAddr != 0 && (*it)->GetRequest( )->address.GetPhysicalAddress( ) == watchAddr )
            {
              std::cout << "WATCHADDR: Row buffer miss. Iserting precharge and activate before tag reads. " 
                        << currentCycle << std::endl;
              std::cout << "WATCHADDR: Row is " << row << " ACT row is " << actRow[rank][bank] << std::endl;
              std::cout << (*it)->GetOperation( ) << std::endl;

              std::deque<MemOp *>::iterator dit; 
              for( dit = bankQueue[rank][bank].begin( ); dit != bankQueue[rank][bank].end( ); dit++ )
                {
                  uint64_t dumpRow;

                  (*dit)->GetRequest( )->address.GetTranslatedAddress( &dumpRow, NULL, NULL, NULL, NULL ); 

                  switch( (*dit)->GetOperation( ) )
                    {
                      case READ:
                        std::cout << "           READ row " << dumpRow << std::endl; 
                        break;
                      case WRITE:
                        std::cout << "           WRITE row " << dumpRow << std::endl;
                        break;
                      case ACTIVATE:
                        std::cout << "           ACTIVATE row " << dumpRow << std::endl;
                        break;
                      case PRECHARGE:
                        std::cout << "           PRECHARGE row " << dumpRow << std::endl;
                        break;
                      default:
                        std::cout << "           UNKNOWN row " << dumpRow << std::endl;
                        break;
                    }
                }
            }

          /*
           *  Precharge this row to prevent starvation.
           */
          MemOp *preOp = new MemOp( );
          NVMainRequest *preReq = new NVMainRequest( );

          preReq->type = PRECHARGE;
          preReq->bulkCmd = CMD_NOP;
          preReq->issueController = this;
          preReq->memop = preOp;
          preReq->tag = DRC_DELETEME;
          preReq->arrivalCycle = currentCycle;
          preOp->SetOperation( PRECHARGE );
          preOp->SetRequest( preReq );

          preReq->address.SetTranslatedAddress( 0, 0, bank, rank, 0 );
          preOp->SetAddress( preReq->address );

          bankQueue[rank][bank].push_back( preOp );
          lastClose[rank][bank] = 0;


          MemOp *actOp = new MemOp( );
          NVMainRequest *actReq = new NVMainRequest( );

          actReq->type = ACTIVATE;
          actReq->bulkCmd = CMD_NOP;
          actReq->issueController = this;
          actReq->memop = actOp;
          actReq->tag = DRC_DELETEME;
          actReq->arrivalCycle = currentCycle;
          actOp->SetOperation( ACTIVATE );
          actOp->SetRequest( actReq );

          actReq->address.SetPhysicalAddress( (*it)->GetRequest( )->address.GetPhysicalAddress( ) );
          actReq->address.SetTranslatedAddress( row, 0, bank, rank, 0 );
          actOp->SetAddress( actReq->address );

          bankQueue[rank][bank].push_back( actOp );

          actQueued[rank][bank] = true;
          actRow[rank][bank] = row;

          /*
           *  Issue 3 reads for the tag.
           */
          MemOp *tagRead1 = new MemOp( );
          MemOp *tagRead2 = new MemOp( );
          MemOp *tagRead3 = new MemOp( );
          NVMainRequest *tagReq1 = new NVMainRequest( );
          NVMainRequest *tagReq2 = new NVMainRequest( );
          NVMainRequest *tagReq3 = new NVMainRequest( );

          *tagRead1 = *(*it);
          *tagRead2 = *(*it);
          *tagRead3 = *(*it);

          *tagReq1 = *(tagRead1->GetRequest( ));
          *tagReq2 = *(tagRead2->GetRequest( ));
          *tagReq3 = *(tagRead3->GetRequest( ));

          tagReq1->tag = DRC_TAGREAD1;
          tagReq2->tag = DRC_TAGREAD2;
          tagReq3->tag = DRC_TAGREAD3;

          tagReq1->type = READ;
          tagReq2->type = READ;
          tagReq3->type = READ;

          tagRead1->SetOperation( READ );
          tagRead2->SetOperation( READ );
          tagRead3->SetOperation( READ );

          tagRead1->SetRequest( tagReq1 );
          tagRead2->SetRequest( tagReq2 );
          tagRead3->SetRequest( tagReq3 );

          tagReq1->issueController = this;
          tagReq2->issueController = this;
          tagReq3->issueController = this;

          tagReq1->memop = tagRead1;
          tagReq2->memop = tagRead2;
          tagReq3->memop = tagRead3;

          tagReq1->arrivalCycle = currentCycle;
          tagReq2->arrivalCycle = currentCycle;
          tagReq3->arrivalCycle = currentCycle;

          /*
           *  We'll use the user defined request info to point to the
           *  original request.
           */
          tagReq1->reqInfo = (void*)((*it));
          tagReq2->reqInfo = (void*)((*it));
          tagReq3->reqInfo = (void*)((*it));

          bankQueue[rank][bank].push_back( tagRead1 ); 
          bankQueue[rank][bank].push_back( tagRead2 ); 
          bankQueue[rank][bank].push_back( tagRead3 ); 

          lastClose[rank][bank]++;
          bankLocked[rank][bank] = true;

          (*it)->GetRequest( )->issueCycle = currentCycle;

          issuedQueue.push_back( (*it) );
          drcQueue.erase( it );
#ifdef DBGQUEUE
          std::cout << "--- Issued 2 deleted from DRC queue at cycle " << currentCycle << ". Size is " << drcQueue.size() << std::endl;
#endif

          drcRBmiss++;

          scheduled = true;
          break;
        }
    }


  /*
   *  Scheduling decision #3: If a bank is closed, and we have a request to this bank,
   *                          activate the bank and issue the tag reads.
   */
  for( it = drcQueue.begin( ); it != drcQueue.end( ); it++ )
    {
      if( drcQueue.size( ) == 0 || scheduled == true ) // why the hell ?
        break;

      uint64_t rank, bank, row;

      (*it)->GetRequest( )->address.GetTranslatedAddress( &row, NULL, &bank, &rank, NULL );

      if( !actQueued[rank][bank] && bankQueue[rank][bank].empty() && !bankLocked[rank][bank] )
        {
          if( watchAddr != 0 && (*it)->GetRequest( )->address.GetPhysicalAddress( ) == watchAddr )
            {
              std::cout << "WATCHADDR: Bank closed. Queuing activate before tag reads. "
                        << currentCycle << std::endl;
            }

          MemOp *actOp = new MemOp( );
          NVMainRequest *actReq = new NVMainRequest( );

          actReq->type = ACTIVATE;
          actReq->bulkCmd = CMD_NOP;
          actReq->issueController = this;
          actReq->memop = actOp;
          actReq->tag = DRC_DELETEME;
          actReq->arrivalCycle = currentCycle;
          actOp->SetOperation( ACTIVATE );
          actOp->SetRequest( actReq );

          actReq->address.SetPhysicalAddress( (*it)->GetRequest( )->address.GetPhysicalAddress( ) );
          actReq->address.SetTranslatedAddress( row, 0, bank, rank, 0 );
          actOp->SetAddress( actReq->address );

          bankQueue[rank][bank].push_back( actOp );

          actQueued[rank][bank] = true;
          actRow[rank][bank] = row;

          /*
           *  Issue 3 reads for the tag.
           */
          MemOp *tagRead1 = new MemOp( );
          MemOp *tagRead2 = new MemOp( );
          MemOp *tagRead3 = new MemOp( );
          NVMainRequest *tagReq1 = new NVMainRequest( );
          NVMainRequest *tagReq2 = new NVMainRequest( );
          NVMainRequest *tagReq3 = new NVMainRequest( );

          *tagRead1 = *(*it);
          *tagRead2 = *(*it);
          *tagRead3 = *(*it);

          *tagReq1 = *(tagRead1->GetRequest( ));
          *tagReq2 = *(tagRead2->GetRequest( ));
          *tagReq3 = *(tagRead3->GetRequest( ));

          tagReq1->tag = DRC_TAGREAD1;
          tagReq2->tag = DRC_TAGREAD2;
          tagReq3->tag = DRC_TAGREAD3;

          tagReq1->type = READ;
          tagReq2->type = READ;
          tagReq3->type = READ;

          tagRead1->SetOperation( READ );
          tagRead2->SetOperation( READ );
          tagRead3->SetOperation( READ );

          tagRead1->SetRequest( tagReq1 );
          tagRead2->SetRequest( tagReq2 );
          tagRead3->SetRequest( tagReq3 );

          tagReq1->issueController = this;
          tagReq2->issueController = this;
          tagReq3->issueController = this;

          tagReq1->memop = tagRead1;
          tagReq2->memop = tagRead2;
          tagReq3->memop = tagRead3;

          tagReq1->arrivalCycle = currentCycle;
          tagReq2->arrivalCycle = currentCycle;
          tagReq3->arrivalCycle = currentCycle;

          /*
           *  We'll use the user defined request info to point to the
           *  original request.
           */
          tagReq1->reqInfo = (void*)((*it));
          tagReq2->reqInfo = (void*)((*it));
          tagReq3->reqInfo = (void*)((*it));


          bankQueue[rank][bank].push_back( tagRead1 ); 
          bankQueue[rank][bank].push_back( tagRead2 ); 
          bankQueue[rank][bank].push_back( tagRead3 ); 

          lastClose[rank][bank]++;
          bankLocked[rank][bank] = true;

          (*it)->GetRequest( )->issueCycle = currentCycle;

          issuedQueue.push_back( (*it) );
          drcQueue.erase( it );
#ifdef DBGQUEUE
          std::cout << "--- Issued 3 deleted from DRC queue at cycle " << currentCycle << ". Size is " << drcQueue.size() << std::endl;
#endif

          drcRBmiss++;

          scheduled = true;
          break;
        }
    }
  
  

  /*
   *  Finally, actually do the issuing of the raw DRAM commands to the banks.
   */
  for( unsigned int i = 0; i < numRanks; i++ )
    {
      for( unsigned int j = 0; j < numBanks; j++ )
        {
          if( !bankQueue[i][j].empty( )
              && memory->IsIssuable( bankQueue[i][j].at( 0 ) ) )
            {
              memory->IssueCommand( bankQueue[i][j].at( 0 ) );

              //bankQueue[i][j].at( 0 )->GetRequest( )->issueCycle = currentCycle;

              if( watchAddr != 0 && bankQueue[i][j].at( 0 )->GetRequest( )->address.GetPhysicalAddress( ) == watchAddr )
                {
                  switch( bankQueue[i][j].at( 0 )->GetOperation( ) )
                  {
                  case READ:
                    std::cout << "WATCHADDR: ISSUED READ TO RANK " << i << " BANK " << j << ". "
                              << currentCycle << std::endl;
                    break;
                  case WRITE:
                    std::cout << "WATCHADDR: ISSUED WRITE TO RANK " << i << " BANK " << j << ". "
                              << currentCycle << std::endl;
                    break;
                  case ACTIVATE:
                    std::cout << "WATCHADDR: ISSUED ACTIVATE TO RANK " << i << " BANK " << j << ". "
                              << currentCycle << std::endl;
                    break;
                  case PRECHARGE:
                    std::cout << "WATCHADDR: ISSUED PRECHARGE TO RANK " << i << " BANK " << j << ". "
                              << currentCycle << std::endl;
                    break;
                  default:
                    break;
                  }
                }

              bankQueue[i][j].erase( bankQueue[i][j].begin( ) );
            }
          else if( !bankQueue[i][j].empty() )
            {
              /* Check for very delayed commands. */
              MemOp *queueHead = bankQueue[i][j].at( 0 );

              if( ( currentCycle - queueHead->GetRequest( )->arrivalCycle ) > 10000 )
                {
                  std::cout << "WARNING: Operation has not been issued after a very long time: " << std::endl;
                  std::cout << "         Address: 0x" << std::hex << queueHead->GetRequest( )->address.GetPhysicalAddress( )
                            << std::dec << ". Queued time: " << queueHead->GetRequest( )->arrivalCycle
                            << ". Current time: " << currentCycle << ". Type: " << queueHead->GetOperation( )
                            << std::endl;
                }
            }
        }
    }


  countDRCQueue++;
  countPFWQueue++;

  totalDRCQueue += drcQueue.size( );
  totalPFWQueue += pfwaitQueue.size( );

  if( drcQueue.size( ) > maxDRCQueue )
    maxDRCQueue = drcQueue.size( );
  if( pfwaitQueue.size( ) > maxPFWQueue )
    maxPFWQueue = pfwaitQueue.size( );

  averageDRCQueue = static_cast<float>( totalDRCQueue ) / static_cast<float>( countDRCQueue );
  averagePFWQueue = static_cast<float>( totalPFWQueue ) / static_cast<float>( countPFWQueue );


  currentCycle++;
  memory->Cycle( );
  
  //if( this->id == 0 )
    {
      for( unsigned int i = 0; i < mmChannels; i++ )
        {
          mmController[i]->Cycle( );
          mmController[i]->FlushCompleted( );
        }
    }
}



void DRCController::PrintStats( )
{
  std::cout << "i" << psInterval << "." << statName << id << ".app_reads " << app_reads << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".app_write " << app_writes << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".read_hits " << read_hits << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".write_hits " << write_hits << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".misses " << misses << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".evictions " << evictions << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".clean_evictions " << clean_evictions << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".dirty_evictions " << dirty_evictions << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".drcRBhits " << drcRBhits << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".drcRBmiss " << drcRBmiss << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".drcRPdrops " << drcRPdrops << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".drcUPdrops " << drcUPdrops << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".drcUDdrops " << drcUDdrops << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".longAssRequests " << longAssRequests << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".averageLatency " << averageLatency << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".averageQueueLatency " << averageQueueLatency << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".measuredLatencies " << measuredLatencies << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".measuredQueueLatencies " << measuredQueueLatencies << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".averageMMLatency " << averageMMLatency << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".averageMMQueueLatency " << averageMMQueueLatency << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".measuredMMLatencies " << measuredMMLatencies << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".measuredMMQueueLatencies " << measuredMMQueueLatencies << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".mmEvicts " << mmEvicts << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".cleanMMEvicts " << cleanMMEvicts << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".dirtyMMEvicts " << dirtyMMEvicts << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".mmForceEvicts " << mmForceEvicts << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".missMapHits " << missMapHits << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".missMapMisses " << missMapMisses << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".prefetchesIssued " << prefetchesIssued << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".prefetchHits " << prefetchHits << std::endl;
  //std::cout << "i" << psInterval << "." << statName << id << ".prefetchesMisses " << prefetchMisses << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".maxDRCQueue " << maxDRCQueue << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".totalDRCQueue " << totalDRCQueue << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".countDRCQueue " << countDRCQueue << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".averageDRCQueue " << averageDRCQueue << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".maxPFWQueue " << maxPFWQueue << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".totalPFWQueue " << totalPFWQueue << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".countPFWQueue " << countPFWQueue << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".averagePFWQueue " << averagePFWQueue << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".icHits " << icHits << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".icDemandRefEvicts " << icDemandRefEvicts << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".icDemandUnrefEvicts " << icDemandUnrefEvicts << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".icPrefetchRefEvicts " << icPrefetchRefEvicts << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".icPrefetchUnrefEvicts " << icPrefetchUnrefEvicts << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".icInstalledEvicts " << icInstalledEvicts << std::endl;
  std::cout << "i" << psInterval << "." << statName << id << ".icUninstalledEvicts " << icUninstalledEvicts << std::endl;



  /* Print the reference count junk out */
  std::map<uint64_t, uint64_t>::iterator it;
  uint64_t maxRef;
  uint64_t *refCounts;

  maxRef = 0;
  for( it = rpRefCount.begin( ); it != rpRefCount.end( ); ++it )
    {
      if( it->second > maxRef )
        maxRef = it->second;
    }
  
  refCounts = new uint64_t [ maxRef+1 ];
  for( uint64_t i = 0; i < maxRef+1; ++i )
    {
      refCounts[i] = 0;
    }

  for( it = rpRefCount.begin( ); it != rpRefCount.end( ); ++it )
    {
      refCounts[it->second]++;
    }

  std::cout << "i" << psInterval << "." << statName << id << ".rpRefCount ";
  for( uint64_t i = 0; i < maxRef+1; i++ )
    {
      std::cout << i << " " << refCounts[i] << " ";
    }
  std::cout << std::endl;


  // UP's
  maxRef = 0;
  for( it = upRefCount.begin( ); it != upRefCount.end( ); ++it )
    {
      if( it->second > maxRef )
        maxRef = it->second;
    }
  
  refCounts = new uint64_t [ maxRef+1 ];
  for( uint64_t i = 0; i < maxRef+1; ++i )
    {
      refCounts[i] = 0;
    }

  for( it = upRefCount.begin( ); it != upRefCount.end( ); ++it )
    {
      refCounts[it->second]++;
    }

  std::cout << "i" << psInterval << "." << statName << id << ".upRefCount ";
  for( uint64_t i = 0; i < maxRef+1; i++ )
    {
      std::cout << i << " " << refCounts[i] << " ";
    }
  std::cout << std::endl;



  // RD's
  maxRef = 0;
  for( it = rdRefCount.begin( ); it != rdRefCount.end( ); ++it )
    {
      if( it->second > maxRef )
        maxRef = it->second;
    }
  
  refCounts = new uint64_t [ maxRef+1 ];
  for( uint64_t i = 0; i < maxRef+1; ++i )
    {
      refCounts[i] = 0;
    }

  for( it = rdRefCount.begin( ); it != rdRefCount.end( ); ++it )
    {
      refCounts[it->second]++;
    }

  std::cout << "i" << psInterval << "." << statName << id << ".rdRefCount ";
  for( uint64_t i = 0; i < maxRef+1; i++ )
    {
      std::cout << i << " " << refCounts[i] << " ";
    }
  std::cout << std::endl;


  // UD's
  maxRef = 0;
  for( it = udRefCount.begin( ); it != udRefCount.end( ); ++it )
    {
      if( it->second > maxRef )
        maxRef = it->second;
    }
  
  refCounts = new uint64_t [ maxRef+1 ];
  for( uint64_t i = 0; i < maxRef+1; ++i )
    {
      refCounts[i] = 0;
    }

  for( it = udRefCount.begin( ); it != udRefCount.end( ); ++it )
    {
      refCounts[it->second]++;
    }

  std::cout << "i" << psInterval << "." << statName << id << ".udRefCount ";
  for( uint64_t i = 0; i < maxRef+1; i++ )
    {
      std::cout << i << " " << refCounts[i] << " ";
    }
  std::cout << std::endl;






  /* Remember to call the base class so we get statistics for the rest
   * of the memory system!
   */
  MemoryController::PrintStats( );

  for( unsigned int i = 0; i < mmChannels; i++ )
    mmController[i]->PrintStats( );

  psInterval++;

#ifndef TRACE
  Stats::schedStatEvent( true, false );
#endif
}



void DRCController::UpdateAverageMMLatency( NVMainRequest *timedRequest )
{
    averageMMLatency = ((averageMMLatency * static_cast<float>(measuredMMLatencies)) 
                         + static_cast<float>(timedRequest->completionCycle) 
                         - static_cast<float>(timedRequest->issueCycle))
                       / static_cast<float>(measuredMMLatencies+1);
    measuredMMLatencies += 1;

    averageMMQueueLatency = ((averageMMQueueLatency * static_cast<float>(measuredMMQueueLatencies)) 
                              + static_cast<float>(timedRequest->issueCycle) 
                              - static_cast<float>(timedRequest->arrivalCycle))
                            / static_cast<float>(measuredMMQueueLatencies+1);
    measuredMMQueueLatencies += 1;

/*
    std::cout << "MM Request for 0x" << std::hex
              << timedRequest->address.GetPhysicalAddress( ) << std::dec 
              << " completed in " << (timedRequest->completionCycle - timedRequest->issueCycle)
              << " cycles. Queued for " << (timedRequest->issueCycle - timedRequest->arrivalCycle) << std::endl;
              */
}



void DRCController::UpdateAverageLatency( NVMainRequest *timedRequest )
{
    averageLatency = ((averageLatency * static_cast<float>(measuredLatencies)) 
                       + static_cast<float>(timedRequest->completionCycle) 
                       - static_cast<float>(timedRequest->issueCycle))
                     / static_cast<float>(measuredLatencies+1);
    measuredLatencies += 1;

    averageQueueLatency = ((averageQueueLatency * static_cast<float>(measuredQueueLatencies)) 
                            + static_cast<float>(timedRequest->issueCycle) 
                            - static_cast<float>(timedRequest->arrivalCycle))
                          / static_cast<float>(measuredQueueLatencies+1);
    measuredQueueLatencies += 1;


    if( (timedRequest->completionCycle - timedRequest->arrivalCycle) > 1000 )
      {
        longAssRequests++;
/*
        std::cout << "Request completed in " << (timedRequest->completionCycle - timedRequest->issueCycle)
                  << " cycles. Queued for " << (timedRequest->issueCycle - timedRequest->arrivalCycle)
                  << ". Started at " << timedRequest->arrivalCycle << ". Address is 0x" << std::hex
                  << timedRequest->address.GetPhysicalAddress( ) << std::dec << ". A/I/C = "
                  << timedRequest->arrivalCycle << "/" << timedRequest->issueCycle << "/"
                  << timedRequest->completionCycle << std::endl;
                 */
      }

}
