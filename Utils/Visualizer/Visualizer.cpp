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

#include "Utils/Visualizer/Visualizer.h"
#include "src/EventQueue.h"

/* Hooks must include any classes they are comparing types to filter. */
#include "src/Bank.h"
#include "src/Rank.h"
#include "NVM/nvmain.h"

using namespace NVM;

Visualizer::Visualizer( )
{
    /*
     * Set the type for the hook.
     *
     * Really we can hook before or after the IssueCommand/RequestComplete calls. 
     * We'll use PREISSUE here. 
     */
    SetHookType( NVMHOOK_PREISSUE );
}

Visualizer::~Visualizer( )
{
}

/* 
 *  After initialization, the parent will become whichever NVMObject the request
 *  currently resides at (e.g., interconnect, rank, bank, etc.).
 */
void Visualizer::Init( Config *conf )
{
    numRanks = static_cast<ncounter_t>( conf->GetValue( "RANKS" ) );
    numBanks = static_cast<ncounter_t>( conf->GetValue( "BANKS" ) );
    busBurstLength = static_cast<ncycle_t>( conf->GetValue( "tBURST" ) );

    lineLength = 100; // default value;
    if( conf->KeyExists( "VisLineLength" ) )
    {
        lineLength = static_cast<ncounter_t>( conf->GetValue( "VisLineLength" ) );
    }

    /* 
     * Create graphLines for each rank and bank, and set the default symbol 
     * to idle.
     */
    for( ncounter_t i = 0; i < (numRanks * numBanks + numRanks); i++ )
    {
        graphLines.push_back( "" );
        graphSymbol.push_back( '-' );
    }

    /* Schedule an event every so many cycles to print the visualization graph */
    ncycle_t firstOutput = static_cast<ncycle_t>(
            GetEventQueue( )->GetCurrentCycle( ) / lineLength) + lineLength;

    GetEventQueue( )->InsertEvent( EventCycle, this, firstOutput ); 
    
    endCycle = firstOutput;
    startCycle = endCycle - lineLength;
    endCycle2 = endCycle + lineLength;
}

/*
 * Generally nothing happens during atomic issues (in terms of bank activity).
 * This will call IssueCommand anyways for corner cases where NVMain's atomic 
 * issue is being used to return average latency values and simulating single 
 * requests, for example.
 */
bool Visualizer::IssueAtomic( NVMainRequest *req )
{
    return IssueCommand( req );
}

/*
 * Hook the IssueCommand. Here we are interested in bank and rank activity. 
 * For ranks we will place an X on the graph for a time of tCMD. For banks, 
 * label the bank graph with a letter corresponding to the current action the 
 * bank is undergoing (e.g, ACT, READ, WRITE, PRE).
 */
bool Visualizer::IssueCommand( NVMainRequest *req )
{
    uint64_t bank, rank;
    ncounter_t graphId;

    req->address.GetTranslatedAddress( NULL, NULL, &bank, &rank, NULL, NULL ); 

    /*
     *  Filter out everything but rank and bank issues here.
     */
    if( NVMTypeMatches(Bank) )
    {
        graphId = rank * (numBanks + 1) + (bank + 1);

        /* Fill the graph with the previous symbol. */
        ncounter_t fillStop = GetEventQueue( )->GetCurrentCycle( ) % lineLength;
        ncounter_t fillStart = graphLines[graphId].length( );

        for( ncounter_t i = fillStart; i < fillStop; i++ )
            graphLines[graphId] += graphSymbol[graphId];
      
        /* Change the symbol based on the current request type. */
        switch( req->type )
        {
            case ACTIVATE:
                graphSymbol[graphId] = 'A';
                break;

            case PRECHARGE:
                graphSymbol[graphId] = 'P';
                break;

            case POWERDOWN_PDA:
            case POWERDOWN_PDPF:
            case POWERDOWN_PDPS:
                graphSymbol[graphId] = 'D';
                break;

            case POWERUP:
                graphSymbol[graphId] = 'U';
                break;

            case REFRESH:
                graphSymbol[graphId] = 'F';
                break;
            /* 
             * Reads and writes are 'delayed' by tRAS and tCWD, respectively, 
             * so we do not create a symbol for these operation. A bus read or 
             * write will be issued once the read or write operation actually 
             * places data on the bus. This is generated by the bank via 
             * RequestComplete, so we will hook reads and writes in the 
             * RequestComplete function.
             */
            case READ:
            case WRITE:
                break;

            default:
                graphSymbol[graphId] = '?';
                break;
        }

    }
    else if( NVMTypeMatches(Rank) )
    {
        graphId = rank * (numBanks + 1);

        /* Fill the graph as idle. */
        if( GetEventQueue( )->GetCurrentCycle( ) % lineLength != 0 )
        {
            /* assumes tCMD = 1 */
            ncounter_t fillStop = 
                (GetEventQueue( )->GetCurrentCycle( ) % lineLength) - 1; 

            ncounter_t fillStart = graphLines[graphId].length( );

            for( ncounter_t i = fillStart; i < fillStop; i++ )
                graphLines[graphId] += '-';
        }

        graphLines[graphId] += 'X';
    }

    return true;
}

bool Visualizer::RequestComplete( NVMainRequest *req )
{
    uint64_t bank, rank;
    ncounter_t graphId;

    req->address.GetTranslatedAddress( NULL, NULL, &bank, &rank, NULL, NULL ); 

    if( NVMTypeMatches(Bank) )
    {
        graphId = rank * (numBanks + 1) + (bank + 1);

        /*
         * Since reads and writes don't mark a symbol, we will manually fill the
         * graph with read or write symbols depending on if it is a bus write or
         * bus read.
         */
        if( req->type == BUS_READ || req->type == BUS_WRITE )
        {
            char fillSymbol = '?';

            if( req->type == BUS_WRITE ) 
                fillSymbol = 'R'; 
            else if( req->type == BUS_READ ) 
                fillSymbol = 'W'; 

            ncounter_t fillStop = 
                GetEventQueue( )->GetCurrentCycle( ) % lineLength;

            ncounter_t fillStart = graphLines[graphId].length( );

            for( ncounter_t i = fillStart; i < fillStop; i++ )
                graphLines[graphId] += '-';
            
            for( ncounter_t i = 0; i < busBurstLength; i++ ) 
              graphLines[graphId] += fillSymbol;

        }
        else
        {
            /* Fill the graph with the previous symbol. */
            ncounter_t fillStop = 
                GetEventQueue( )->GetCurrentCycle( ) % lineLength;

            ncounter_t fillStart = graphLines[graphId].length( );

            for( ncounter_t i = fillStart; i < fillStop; i++ )
                graphLines[graphId] += graphSymbol[graphId];

            /* Change the symbol to idle. */
            graphSymbol[graphId] = '-';
        }
    }

    return true;
}

void Visualizer::Cycle( ncycle_t )
{
    endCycle = GetEventQueue()->GetCurrentCycle();
    startCycle = endCycle - lineLength;
    endCycle2 = endCycle + lineLength;

    /* 
     * Fill all the graph lines with the idle symbol up to the current 
     * simulation cycle and print. 
     */
    for( ncounter_t i = 0; i < numRanks; i++ )
    {
        /* Fill the graph with the idle symbol. */
        ncounter_t fillStop = lineLength;
        ncounter_t fillStart = graphLines[i * (numBanks+1)].length( );

        for( ncounter_t k = fillStart; k < fillStop; k++ )
            graphLines[i * (numBanks+1)] += '-';

        /* Print rank graph. */
        std::cout << "RANK " << i << " " 
            << graphLines[i * (numBanks+1)].substr(0,lineLength) << std::endl;

        for( ncounter_t j = 0; j < numBanks; j++ )
        {
            ncounter_t graphId = i * (numBanks+1) + (j+1);

            /* Fill the graph with the idle symbol. */
            ncounter_t fillStop = lineLength;
            ncounter_t fillStart = graphLines[graphId].length( );

            for( ncounter_t k = fillStart; k < fillStop; k++ )
                graphLines[graphId] += graphSymbol[graphId];

            /* Print bank graph. */
            std::cout << "BANK " << j << " " 
                << graphLines[graphId].substr(0,lineLength) << std::endl;
        }
    }

    /* Schedule the event for the next visualization print cycle. */
    GetEventQueue( )->InsertEvent( EventCycle, this, 
            GetEventQueue()->GetCurrentCycle() + lineLength ); 


    /* Prune the beginning of the graphs based on the configured line length. */
    for( ncounter_t i = 0; i < (numRanks * numBanks + numRanks); i++ )
    {
        std::string tempString;

        tempString = graphLines[i].substr( lineLength, std::string::npos );
        graphLines[i] = tempString;
    }
}
