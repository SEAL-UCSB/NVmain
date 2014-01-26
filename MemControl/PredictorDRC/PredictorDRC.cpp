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

#include "MemControl/PredictorDRC/PredictorDRC.h"
#include "MemControl/MemoryControllerFactory.h"
#include "Utils/AccessPredictor/AccessPredictorFactory.h"
#include "include/NVMHelpers.h"
#include "NVM/nvmain.h"
#include <iostream>
#include <assert.h>

using namespace NVM;

PredictorDRC::PredictorDRC( Interconnect *memory, AddressTranslator *translator )
{
    translator->GetTranslationMethod( )->SetOrder( 5, 1, 4, 3, 2, 6 );

    SetMemory( memory );
    SetTranslator( translator );

    std::cout << "Created a PredictorDRC!" << std::endl;

    DRC = NULL;

    numChannels = 0;
}

PredictorDRC::~PredictorDRC( )
{
}

void PredictorDRC::SetConfig( Config *conf )
{
    /* Initialize DRAM cache */
    DRC = new DRAMCache( GetMemory(), GetTranslator() ); // Use this module's memory and translator (Assumes one predictor for all channels)

    /* Initialize access predictor. */
    if( !conf->KeyExists( "DRCPredictor" ) )
    {
        std::cout << "Error: No DRC predictor specified." << std::endl;
    }

    predictor = AccessPredictorFactory::CreateAccessPredictor( conf->GetString( "DRCPredictor" ) );
    predictor->SetParent( this );
    AddChild( predictor );

    DRC->SetParent( predictor );
    predictor->AddChild( DRC );

    DRC->StatName( this->statName );
    DRC->SetConfig( conf );

    predictor->SetMissDestination( DRC->GetMainMemory() );
    predictor->SetHitDestination( DRC );

    MemoryController::SetConfig( conf );

    SetDebugName( "PredictorDRC", conf );
}

void PredictorDRC::RegisterStats( )
{

}

bool PredictorDRC::IssueAtomic( NVMainRequest *req )
{
    return predictor->IssueAtomic( req );
}

bool PredictorDRC::IssueCommand( NVMainRequest *req )
{
    return predictor->IssueCommand( req );
}

bool PredictorDRC::RequestComplete( NVMainRequest *req )
{
    /*
     *  This module just routes requests, so no requests should be
     *  generated here. Use the standard RequestComplete.
     */
    bool rv = false;

    if( req->type == REFRESH )
        ProcessRefreshPulse( req );
    else if( req->owner == this )
    {
        delete req;
        rv = true;
    }
    else
    {
        rv = GetParent( )->RequestComplete( req );
    }

    return rv;
}

void PredictorDRC::Cycle( ncycle_t steps )
{
    /*
     *  This is a root module, so we need to cycle the child modules.
     */
    DRC->Cycle( steps );
}

void PredictorDRC::CalculateStats( )
{
    /*
     *  This is a root module, print the stats of all child modules.
     */
    DRC->CalculateStats( );
}
