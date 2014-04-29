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
*   Tao Zhang       ( Email: tzz106 at cse dot psu dot edu
*                     Website: http://www.cse.psu.edu/~tzz106 )
*******************************************************************************/

#ifndef __CACHEDDDR3BANK_H__
#define __CACHEDDDR3BANK_H__

#include "Banks/DDR3Bank/DDR3Bank.h"

namespace NVM {


struct CachedRowBuffer
{
    bool used;
    NVMAddress address;
    bool *dirty;
    ncounter_t colStart;
    ncounter_t colEnd;
    ncounter_t reads;
    ncounter_t writes;
};


class CachedDDR3Bank : public DDR3Bank
{
  public:
    CachedDDR3Bank( );
    ~CachedDDR3Bank( );

    virtual bool Activate( NVMainRequest *request );
    virtual bool Read( NVMainRequest *request );
    virtual bool Write( NVMainRequest *request );

    virtual bool IsIssuable( NVMainRequest *request, FailReason *reason = NULL );

    virtual void SetConfig( Config *config, bool createChildren = true );

    virtual void RegisterStats( );
    virtual void CalculateStats( );

  private:
    CachedRowBuffer **cachedRowBuffer;
    bool readOnlyBuffers;
    ncounter_t rowBufferSize;
    ncounter_t rowBufferCount;
    ncounter_t inRDBCount;
    ncounter_t RDBAllocations;
    ncounter_t writebackCount;
    ncounter_t RDBReads, RDBWrites;
    std::map<uint64_t, uint64_t> allocationReadsMap; // Number of reads in an allocation, Count
    std::map<uint64_t, uint64_t> allocationWritesMap;
    std::string allocationReadsHisto;
    std::string allocationWritesHisto;

};



};


#endif


