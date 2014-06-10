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

#ifndef __NVMAIN_PREFETCHER_H__
#define __NVMAIN_PREFETCHER_H__

#include "include/NVMAddress.h"
#include "include/NVMainRequest.h"
#include <vector>

namespace NVM {

class Prefetcher
{
  public:
    Prefetcher( ) { }
    virtual ~Prefetcher( ) { }

    /*
     *  Called upon successful prefetch. Return true if we should prefetch more
     *  addresses and populate the prefetchList. Return false otherwise.
     */
    virtual bool NotifyAccess( NVMainRequest *accessOp, 
            std::vector<NVMAddress>& prefetchList );

    /*
     *  Asks the prefetcher if we should prefetch. Returns true if we should
     *  prefetch and returns a list of addresses to prefetch. Return false
     *  otherwise and do not prefetch. This method should also be used to look
     *  for patterns in addresses, and is called for each request issued.
     */
    virtual bool DoPrefetch( NVMainRequest *triggerOp, 
            std::vector<NVMAddress>& prefetchList );
};

};

#endif
