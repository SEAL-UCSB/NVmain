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


#include "Utils/Caches/CacheBank.h"
#include "include/NVMHelpers.h"

#include <iostream>
#include <assert.h>


using namespace NVM;



CacheBank::CacheBank( uint64_t sets, uint64_t assoc, uint64_t lineSize )
{
  uint64_t i, j;

  cacheEntry = new CacheEntry* [ sets ];
  for( i = 0; i < sets; i++ )
    {
      cacheEntry[i] = new CacheEntry[assoc];
      for( j = 0; j < assoc; j++ )
        {
          /* Clear valid bit, dirty bit, etc. */
          cacheEntry[i][j].flags = CACHE_ENTRY_NONE;
        }
    }


  numSets = sets;
  numAssoc = assoc;
  cachelineSize = lineSize;

  state = CACHE_IDLE;
  stateTimer = 0;

  decodeClass = NULL;
  decodeFunc = NULL;
  SetDecodeFunction( this, static_cast<CacheSetDecoder>(&NVM::CacheBank::DefaultDecoder) );

  readTime = 1; // 1 cycle
  writeTime = 1;  // 1 cycle

  isMissMap = false;
}



CacheBank::~CacheBank( )
{
  uint64_t i;

  for( i = 0; i < numSets; i++ )
    {
      delete [] cacheEntry[i];
    }

  delete [] cacheEntry;
}


void CacheBank::SetDecodeFunction( NVMObject *dcClass, CacheSetDecoder dcFunc )
{
  decodeClass = dcClass;
  decodeFunc = dcFunc;
}


uint64_t CacheBank::DefaultDecoder( NVMAddress &addr )
{
  return (addr.GetPhysicalAddress( ) >> (uint64_t)mlog2( (int)cachelineSize )) % numSets;
}


uint64_t CacheBank::SetID( NVMAddress& addr )
{
  /*
   *  By default we'll just chop off the bits for the cacheline and use the
   *  least significant bits as the set address, and the remaining bits are 
   *  the tag bits.
   */
  uint64_t setID;

      //if( isMissMap )
          //setID = (addr.GetPhysicalAddress( )) % numSets;

  setID = (decodeClass->*decodeFunc)( addr );


  return setID;
}



CacheEntry *CacheBank::FindSet( NVMAddress& addr )
{
  /*
   *  By default we'll just chop off the bits for the cacheline and use the
   *  least significant bits as the set address, and the remaining bits are 
   *  the tag bits.
   */
  uint64_t setID = SetID( addr );

  return cacheEntry[setID];
}


bool CacheBank::Present( NVMAddress& addr )
{
  CacheEntry *set = FindSet( addr );
  bool found = false;


  for( uint64_t i = 0; i < numAssoc; i++ )
    {
      if( set[i].address.GetPhysicalAddress( ) == addr.GetPhysicalAddress( ) 
          && (set[i].flags & CACHE_ENTRY_VALID ) )
        {
          found = true;
          break;
        }
    }


  return found;
}


bool CacheBank::SetFull( NVMAddress& addr )
{
  CacheEntry *set = FindSet( addr );
  bool rv = true;

  for( uint64_t i = 0; i < numAssoc; i++ )
    {
      /* If there is an invalid entry (e.g., not used) the set isn't full. */
      if( !(set[i].flags & CACHE_ENTRY_VALID) )
        {
          rv = false;
          break;
        }
    }

  return rv;
}


bool CacheBank::Install( NVMAddress& addr, NVMDataBlock& data )
{
  CacheEntry *set = FindSet( addr );
  bool rv = false;

  //assert( !Present( addr ) );

  for( uint64_t i = 0; i < numAssoc; i++ )
    {
      if( !(set[i].flags & CACHE_ENTRY_VALID) )
        {
          set[i].address = addr;
          set[i].data = data;
          set[i].flags |= CACHE_ENTRY_VALID; 
          rv = true;
          break;
        }
    }

  return rv;
}


bool CacheBank::Read( NVMAddress& addr, NVMDataBlock *data )
{
  CacheEntry *set = FindSet( addr );
  bool rv = false;

  assert( Present( addr ) );

  for( uint64_t i = 0; i < numAssoc; i++ )
    {
      if( set[i].address.GetPhysicalAddress( ) == addr.GetPhysicalAddress( ) 
          && (set[i].flags & CACHE_ENTRY_VALID) )
        {
          *data = set[i].data;
          rv = true;

          /*
           *  Move cache entry to MRU position.
           */
          CacheEntry tmp;

          tmp.flags = set[i].flags;
          tmp.address = set[i].address;
          tmp.data = set[i].data;

          for( uint64_t j = i; j > 0; j-- )
            {
              set[j].flags = set[j-1].flags;
              set[j].address = set[j-1].address;
              set[j].data = set[j-1].data;
            }

          set[0].flags = tmp.flags;
          set[0].address = tmp.address;
          set[0].data = tmp.data;
        }
    }

  return rv;
}



bool CacheBank::Write( NVMAddress& addr, NVMDataBlock& data )
{
  CacheEntry *set = FindSet( addr );
  bool rv = false;

  assert( Present( addr ) );

  for( uint64_t i = 0; i < numAssoc; i++ )
    {
      if( set[i].address.GetPhysicalAddress( ) == addr.GetPhysicalAddress( )
          && (set[i].flags & CACHE_ENTRY_VALID) )
        {
          set[i].data = data;
          set[i].flags |= CACHE_ENTRY_DIRTY;
          rv = true;

          /*
           *  Move cache entry to MRU position.
           */
          CacheEntry tmp;

          tmp.flags = set[i].flags;
          tmp.address = set[i].address;
          tmp.data = set[i].data;

          for( uint64_t j = i; j > 1; j-- )
            {
              set[j].flags = set[j-1].flags;
              set[j].address = set[j-1].address;
              set[j].data = set[j-1].data;
            }

          set[0].flags = tmp.flags;
          set[0].address = tmp.address;
          set[0].data = tmp.data;
        }
    }

  return rv;
}



/* 
 *  Updates data without changing dirty bit or LRU position
 *  Returns true if the block was found and updated.
 */
bool CacheBank::UpdateData( NVMAddress& addr, NVMDataBlock& data )
{
  CacheEntry *set = FindSet( addr );
  bool rv = false;

  assert( Present( addr ) );

  for( uint64_t i = 0; i < numAssoc; i++ )
    {
      if( set[i].address.GetPhysicalAddress( ) == addr.GetPhysicalAddress( )
          && (set[i].flags & CACHE_ENTRY_VALID) )
        {
          set[i].data = data;
          rv = true;
        }
    }

  return rv;
}



/* Return true if the victim data is dirty. */
bool CacheBank::ChooseVictim( NVMAddress& addr, NVMAddress *victim )
{
  bool rv = false;
  CacheEntry *set = FindSet( addr );

  assert( SetFull( addr ) );
  assert( set[numAssoc-1].flags & CACHE_ENTRY_VALID );

  *victim = set[numAssoc-1].address;
  
  if( set[numAssoc-1].flags & CACHE_ENTRY_DIRTY )
    rv = true;

  return rv;
}


bool CacheBank::Evict( NVMAddress& addr, NVMDataBlock *data )
{
  bool rv;
  CacheEntry *set = FindSet( addr );

  assert( Present( addr ) );

  rv = false; 

  for( uint64_t i = 0; i < numAssoc; i++ )
    {
      if( set[i].address.GetPhysicalAddress( ) == addr.GetPhysicalAddress( ) 
          && (set[i].flags & CACHE_ENTRY_VALID) )
        {
          if( set[i].flags & CACHE_ENTRY_DIRTY )
            {
              *data = set[i].data;
              rv = true;
            }
          else
            {
              *data = set[i].data;
              rv = false;
            }

          set[i].flags = CACHE_ENTRY_NONE;

          break;
        }
    }

  return rv;
}


void CacheBank::SetReadTime( uint64_t rtime )
{
  readTime = rtime;
}


void CacheBank::SetWriteTime( uint64_t wtime )
{
  writeTime = wtime;
}


uint64_t CacheBank::GetReadTime( )
{
  return readTime;
}


uint64_t CacheBank::GetWriteTime( )
{
  return writeTime;
}


uint64_t CacheBank::GetAssociativity( )
{
  return numAssoc;
}


uint64_t CacheBank::GetCachelineSize( )
{
  return cachelineSize;
}


uint64_t CacheBank::GetSetCount( )
{
  return numSets;
}


float CacheBank::GetCacheOccupancy( )
{
  float occupancy;
  uint64_t valid, total;

  valid = 0;
  total = numSets*numAssoc;

  for( uint64_t i = 0; i < numSets; i++ )
    {
      CacheEntry *set = cacheEntry[i];

      for( uint64_t j = 0; j < numAssoc; j++ )
        {
          if( set[i].flags & CACHE_ENTRY_VALID )
            valid++;
        }
    }

  occupancy = static_cast<float>(valid) / static_cast<float>(total);

  return occupancy;
}


bool CacheBank::IsIssuable( NVMainRequest * /*req*/ )
{
  bool rv = false;

  /* 
   *  We can issue if the cache is idle. Pretty simple.
   */
  if( state == CACHE_IDLE )
    {
      rv = true;
    }
  else
    {
      rv = false;
    }

  return rv;
}


bool CacheBank::IssueCommand( NVMainRequest *nreq )
{
  NVMDataBlock dummy;
  CacheRequest *req = static_cast<CacheRequest *>( nreq->reqInfo );

  assert( IsIssuable( nreq ) );

  if( !IsIssuable( nreq ) )
    return false;

  switch( req->optype )
    {
    case CACHE_READ:
      state = CACHE_BUSY;
      stateTimer = GetEventQueue( )->GetCurrentCycle( ) + readTime;
      req->hit = Present( req->address );
      if( req->hit ) Read( req->address, &(req->data) ); 
      GetEventQueue( )->InsertEvent( EventResponse, this, nreq, stateTimer );
      break;
    case CACHE_WRITE:
      state = CACHE_BUSY;
      stateTimer = GetEventQueue( )->GetCurrentCycle( ) + writeTime;

      if( SetFull( req->address ) )
        {
          NVMainRequest *mmEvict = new NVMainRequest( );
          CacheRequest *evreq = new CacheRequest;
          
          ChooseVictim( req->address, &(evreq->address) );
          Evict( evreq->address, &(evreq->data) );

          evreq->optype = CACHE_EVICT;

          *mmEvict = *nreq;
          mmEvict->owner = nreq->owner;
          mmEvict->reqInfo = static_cast<void *>( evreq );
          mmEvict->tag = nreq->tag;

          GetEventQueue( )->InsertEvent( EventResponse, this, mmEvict, stateTimer );
        }

      req->hit = Present( req->address );
      if( req->hit ) 
        Write( req->address, req->data );
      else
        Install( req->address, req->data );

      GetEventQueue( )->InsertEvent( EventResponse, this, nreq, stateTimer );

      break;
    default:
      std::cout << "CacheBank: Unknown operation `" << req->optype << "'!" << std::endl;
      break;
    }

  return true;
}


bool CacheBank::RequestComplete( NVMainRequest *req )
{
  GetParent( )->RequestComplete( req );

  state = CACHE_IDLE;

  return true;
}



void CacheBank::Cycle( ncycle_t /*steps*/ )
{
}

