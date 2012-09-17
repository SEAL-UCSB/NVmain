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

#ifndef __PREFETCHERS_STEMS_H__
#define __PREFETCHERS_STEMS_H__


#include "src/Prefetcher.h"

#include <map>

namespace NVM {


struct PatternSequence
{
    uint64_t size;
    uint64_t address;
    uint64_t offset[16];
    uint64_t delta[16];
    bool fetched[16];
    bool used[16];
    uint64_t useCount;
    bool startedPrefetch;
};

/*
 *  Note: This is not "true" STeMS describe in the paper. Since the AGT 
 *  would be ridiculously large if we stored miss patterns until a block is
 *  evicted, we only store miss patterns to a given threshold before store in
 *  the PST.
 */
class STeMS : public Prefetcher
{
 public:
  STeMS( ) { }
  ~STeMS( ) { }

  bool NotifyAccess( NVMainRequest *accessOp, std::vector<NVMAddress>& prefetchList );
  bool DoPrefetch( NVMainRequest *triggerOp, std::vector<NVMAddress>& prefetchList );

 private:
  std::map<uint64_t, PatternSequence*> PST; // Pattern Sequence Table
  std::map<uint64_t, PatternSequence*> AGT; // Active Generation Table
  std::map<uint64_t, PatternSequence*> ReconBuf; // Reconstruction Buffer

  void FetchNextUnused( PatternSequence *rps, int count, std::vector<NVMAddress>& prefetchList );

};



};


#endif


