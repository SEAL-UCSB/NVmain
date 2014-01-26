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

    bool NotifyAccess( NVMainRequest *accessOp, 
                       std::vector<NVMAddress>& prefetchList );

    bool DoPrefetch( NVMainRequest *triggerOp, 
                     std::vector<NVMAddress>& prefetchList );

  private:
    std::map<uint64_t, PatternSequence*> PST; // Pattern Sequence Table
    std::map<uint64_t, PatternSequence*> AGT; // Active Generation Table
    std::map<uint64_t, PatternSequence*> ReconBuf; // Reconstruction Buffer

    void FetchNextUnused( PatternSequence *rps, int count, 
                          std::vector<NVMAddress>& prefetchList );
};

};

#endif
