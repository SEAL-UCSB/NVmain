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


#ifndef __NVMAIN_UTILS_CACHES_CACHEBANK_H__
#define __NVMAIN_UTILS_CACHES_CACHEBANK_H__



#include <utility>

#include "include/NVMAddress.h"
#include "include/NVMDataBlock.h"
#include "src/NVMObject.h"
#include "src/AddressTranslator.h"



namespace NVM {


enum CacheState { CACHE_IDLE, CACHE_BUSY };
enum CacheOperation { CACHE_NONE, CACHE_READ, CACHE_WRITE, CACHE_SCRUB };

struct CacheRequest
{
  CacheOperation optype;
  NVMAddress address;
  NVMAddress endAddr;
  NVMDataBlock data;
  bool complete;
};


/* Must be powers of two! */
enum { CACHE_ENTRY_NONE = 0,
       CACHE_ENTRY_VALID = 1,
       CACHE_ENTRY_DIRTY = 2,
       CACHE_ENTRY_EXAMPLE = 4
};

struct CacheEntry
{
  uint64_t flags;
  NVMAddress address;
  NVMDataBlock data;
};


class CacheBank : public NVMObject
{
 public:
  CacheBank( uint64_t sets, uint64_t assoc, uint64_t lineSize );
  ~CacheBank( );

  /* Return true if the address is in the cache. */
  bool Present( NVMAddress& addr );

  /* Return true if the address was placed in the cache. */
  bool Install( NVMAddress& addr, NVMDataBlock& data );

  /* Return true if the set is full, i.e. needs an eviction. */
  bool SetFull( NVMAddress& addr );

  /* Return true if evicted data is dirty data in *data. data is set to NULL
   * if the address was not found. */
  bool Evict( NVMAddress& addr, NVMDataBlock *data );

  bool Read( NVMAddress& addr, NVMDataBlock *data );
  bool Write( NVMAddress& addr, NVMDataBlock& data );

  bool UpdateData( NVMAddress& addr, NVMDataBlock& data );

  void SetReadTime( uint64_t rtime );
  void SetWriteTime( uint64_t wtime );

  uint64_t GetReadTime( );
  uint64_t GetWriteTime( );

  bool IsIssuable( CacheRequest *req );
  void IssueCommand( CacheRequest *req );
  
  void Cycle( ncycle_t steps );

  bool ChooseVictim( NVMAddress& addr, NVMAddress *victim );

  void SetAddressTranslator( AddressTranslator *at );

// protected:
  uint64_t numSets, numAssoc, cachelineSize;
  CacheEntry **cacheEntry;
  uint64_t accessTime, stateTimer;
  uint64_t readTime, writeTime;
  CacheState state;
  CacheRequest *currentReq; // Cache bank can only handle one request at a time.

  AddressTranslator *addrTrans;
  CacheEntry *FindSet( NVMAddress& addr );
  uint64_t SetID( NVMAddress& addr );
  bool isMissMap;

};


}; // namespace NVM


#endif


