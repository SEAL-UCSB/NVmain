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


#include "Utils/AccessPredictor/VariablePredictor/VariablePredictor.h"
#include "NVM/nvmain.h"
#include "src/MemoryController.h"
#include "MemControl/DRAMCache/DRAMCache.h"

#include <iostream>
#include <cstdlib>
#include <cassert>



using namespace NVM;



VariablePredictor::VariablePredictor( )
{
    /* Approximate default value from the literature. */
    accuracy = 0.95;

    seed = 1;
}


VariablePredictor::~VariablePredictor( )
{

}


void VariablePredictor::SetConfig( Config *config, bool /*createChildren*/ )
{
    /* Check for user defined accuracy. */
    config->GetEnergy( "VariablePredictorAccuracy", accuracy ); 

    AddStat(truePredictions);
    AddStat(falsePredictions);
}

uint64_t VariablePredictor::Translate( NVMainRequest *request )
{
    /* Write always hits, no prediction should be done. */
    if( request->type == WRITE || request->type == WRITE_PRECHARGE )
        return GetHitDestination( );

    ncounter_t dest = GetMissDestination( );

    /* 
     * We assume the "hit" destination is some kind of cache for this to make sense.
     * For now, assume that it is a DRAM cache, although later this may be changed
     * to be any generic type of cache.
     *
     * This predictor only returns which route the request should take. We assume
     * that this module's parent has two children representing the multiple routes.
     * The parent module must set this module's parent on initialization.
     */
    assert( parent != NULL );

    double coinToss = static_cast<double>(::rand_r(&seed)) 
                    / static_cast<double>(RAND_MAX);
    bool hit = GetParent()->GetTrampoline()->GetChild(GetHitDestination())->IssueFunctional(request);

    /* Predict correctly with probability "p" (accuracy) and mispredict with 1-p. */
    if( (hit && coinToss < accuracy) || (!hit && coinToss >= accuracy) )
    {
        if (hit) truePredictions++;
        else falsePredictions++;

        /* Request would be success to cache, issue there. */
        dest = GetHitDestination( ); 
    }
    else
    {
        if (hit) falsePredictions++;
        else truePredictions++;

        /* Request would fail to cache, issue to backing memory. */
        dest = GetMissDestination( ); 
    }

    return dest;
}



