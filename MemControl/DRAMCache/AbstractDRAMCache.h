

#ifndef __MEMCONTROL_ABSTRACTDRAMCACHE_H__
#define __MEMCONTROL_ABSTRACTDRAMCACHE_H__


#include "src/MemoryController.h"


namespace NVM {


class NVMain;


class AbstractDRAMCache : public MemoryController
{
 public:
  AbstractDRAMCache( ) { }
  virtual ~AbstractDRAMCache( ) { };

  /*
   *  DRAM cache always has some backing memory
   *  behind it for misses, so we need to set this
   *  here, since the backing memory is shared by
   *  all DRAM caches on every channel.
   */
  virtual void SetMainMemory( NVMain *mm ) = 0;
  virtual bool IssueFunctional( NVMainRequest *req ) = 0;
  virtual bool IsIssuable( NVMainRequest *request, FailReason *reason = NULL ) = 0;

};


};


#endif


