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

#ifndef __ENDURANCEMODEL_H__
#define __ENDURANCEMODEL_H__

#include <string>
#include <map>
#include <stdint.h>
#include "src/Config.h"
#include "src/Params.h"
#include "src/NVMObject.h"
#include "src/EnduranceDistribution.h"
#include "include/NVMDataBlock.h"
#include "include/NVMAddress.h"
#include "src/FaultModel.h"

namespace NVM {

class FaultModel;

class EnduranceModel : public NVMObject
{
  public:
    EnduranceModel( );
    ~EnduranceModel( ) {}

    /* Return -(latency+1) on error, or the additional number of cycles needed by the model otherwise. */
    virtual ncycles_t Read( NVMainRequest *request ) = 0;
    /* Return -(latency+1) on error, or the additional number of cycles needed by the model otherwise. */
    virtual ncycles_t Write( NVMainRequest *request, NVMDataBlock& oldData ) = 0;

    virtual void SetConfig( Config *conf, bool createChildren = true );

    uint64_t GetWorstLife( );
    uint64_t GetAverageLife( );

    virtual void PrintStats( ) { }

    void Cycle( ncycle_t steps );

  protected:
    EnduranceDistribution *enduranceDist;
    std::map<uint64_t, uint64_t> life;
    
    bool DecrementLife( uint64_t addr );
    bool IsDead( uint64_t addr );

    void SetGranularity( uint64_t bits );
    uint64_t GetGranularity( );

  private:
    uint64_t granularity;

};

};

#endif
