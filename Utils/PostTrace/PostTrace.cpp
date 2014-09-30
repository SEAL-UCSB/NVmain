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

#include "Utils/PostTrace/PostTrace.h"
#include "src/EventQueue.h"

/* Hooks must include any classes they are comparing types to filter. */
#include "src/Bank.h"
#include "src/Rank.h"
#include "NVM/nvmain.h"
#include "include/NVMHelpers.h"
#include "traceWriter/TraceWriterFactory.h"

#include <sstream>

using namespace NVM;

PostTrace::PostTrace( )
{
    SetHookType( NVMHOOK_PREISSUE );
}

PostTrace::~PostTrace( )
{
}

/* 
 *  After initialization, the parent will become whichever NVMObject the request
 *  currently resides at (e.g., interconnect, rank, bank, etc.).
 */
void PostTrace::Init( Config *conf )
{
    numRanks = static_cast<ncounter_t>( conf->GetValue( "RANKS" ) );
    numBanks = static_cast<ncounter_t>( conf->GetValue( "BANKS" ) );
    numChannels = static_cast<ncycle_t>( conf->GetValue( "CHANNELS" ) );

    std::string traceWriterName = "NVMainTrace";
    std::string baseFileName;

    if( conf->KeyExists( "PostTraceWriter" ) )
        traceWriterName = conf->GetString( "PostTraceWriter" );

    GenericTraceWriter *testTracer = TraceWriterFactory::CreateNewTraceWriter( traceWriterName );

    if( conf->KeyExists( "PostTracePerChannel" ) && conf->GetBool( "PostTracePerChannel" ) )
        testTracer->SetPerChannelTraces( true );
    if( conf->KeyExists( "PostTracePerRank" ) && conf->GetBool( "PostTracePerRank" ) )
        testTracer->SetPerRankTraces( true );

    if( testTracer->GetPerChannelTraces() && testTracer->GetPerRankTraces() )
    {
        std::cout << "PostTrace: Cannot have per channel and per rank traces!" << std::endl;
        exit(0);
    }

    if( conf->GetString( "PostTraceFile" ) == "" )
        baseFileName = "nvmain_posttrace.nvt";
    else
        baseFileName = conf->GetString( "PostTraceFile" );

    if( baseFileName[0] != '/' )
    {
        baseFileName  = NVM::GetFilePath( conf->GetFileName( ) );
        baseFileName += conf->GetString( "PostTraceFile" );
    }

    std::cout << "PostTrace: Using trace file " << baseFileName << std::endl;

    /* Determine the number of channels with their own writers. */
    if( testTracer->GetPerChannelTraces() || testTracer->GetPerRankTraces() )
    {
        traceChannels = numChannels;
    }
    else
    {
        traceChannels = 1;
    }
    
    /* Allocate the number of channels with their own writers. */
    traceWriter = new GenericTraceWriter ** [traceChannels];

    /* Determine the number of ranks with their own writers. */
    if( testTracer->GetPerRankTraces() )
    {
        traceRanks = numRanks;
    }
    else
    {
        traceRanks = 1;
    }

    /* Allocate the number of ranks with their own writers. */
    for( ncounter_t channelIdx = 0; channelIdx < traceChannels; channelIdx++ )
    {
        traceWriter[channelIdx] = new GenericTraceWriter * [traceRanks];
    }

    /* Allocate all of the trace writers. */
    for( ncounter_t channelIdx = 0; channelIdx < traceChannels; channelIdx++ )
    {
        for( ncounter_t rankIdx = 0; rankIdx < traceRanks; rankIdx++ )
        {
            std::stringstream traceFileName;
            traceFileName << baseFileName << "_ch" << channelIdx << "_rk" << rankIdx;

            traceWriter[channelIdx][rankIdx] = TraceWriterFactory::CreateNewTraceWriter( traceWriterName );
            traceWriter[channelIdx][rankIdx]->SetTraceFile( traceFileName.str() );
            traceWriter[channelIdx][rankIdx]->SetEcho( conf->GetBool( "EchoPostTrace" ) );
            traceWriter[channelIdx][rankIdx]->Init( conf );
        }
    }

    delete testTracer;
}

/*
 * Generally nothing happens during atomic issues (in terms of bank activity).
 * This will call IssueCommand anyways for corner cases where NVMain's atomic 
 * issue is being used to return average latency values and simulating single 
 * requests, for example.
 */
bool PostTrace::IssueAtomic( NVMainRequest *request )
{
    return IssueCommand( request );
}

/*
 * Hook the IssueCommand. Here we are interested in bank and rank activity. 
 * For ranks we will place an X on the graph for a time of tCMD. For banks, 
 * label the bank graph with a letter corresponding to the current action the 
 * bank is undergoing (e.g, ACT, READ, WRITE, PRE).
 */
bool PostTrace::IssueCommand( NVMainRequest *request )
{
    bool isPowerCommand = (request->type == POWERDOWN_PDA || request->type == POWERDOWN_PDPF ||
                           request->type == POWERDOWN_PDPS || request->type == POWERUP );
    /*
     *  Filter out everything but bank issues here.
     */
    if( (isPowerCommand && NVMTypeMatches(Rank)) || (!isPowerCommand && NVMTypeMatches(Bank)) )
    {
        uint64_t rank, channel, rk, ch;

        request->address.GetTranslatedAddress( NULL, NULL, NULL, &rank, &channel, NULL ); 
        rk = (traceRanks == 1) ? 0 : rank;
        ch = (traceChannels == 1) ? 0 : channel;

        assert( rk < numRanks );
        assert( ch < numChannels );

        TraceLine tl;

        tl.SetLine( request->address,
                    request->type,
                    GetEventQueue( )->GetCurrentCycle( ),
                    request->data,
                    request->oldData,
                    request->threadId 
                  );

        traceWriter[ch][rk]->SetNextAccess( &tl );
    }

    return true;
}

bool PostTrace::RequestComplete( NVMainRequest * /*request*/ )
{
    return true;
}

void PostTrace::Cycle( ncycle_t )
{
}
