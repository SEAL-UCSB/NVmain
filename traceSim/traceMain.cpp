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

#include <sstream>
#include <cmath>
#include <stdlib.h>
#include <fstream>

#include "src/Interconnect.h"
#include "Interconnect/InterconnectFactory.h"
#include "src/Config.h"
#include "src/TranslationMethod.h"
#include "traceReader/TraceReaderFactory.h"
#include "src/AddressTranslator.h"
#include "Decoders/DecoderFactory.h"
#include "src/MemoryController.h"
#include "MemControl/MemoryControllerFactory.h"
#include "Endurance/EnduranceDistributionFactory.h"
#include "SimInterface/NullInterface/NullInterface.h"
#include "include/NVMHelpers.h"
#include "Utils/HookFactory.h"
#include "src/EventQueue.h"
#include "NVM/nvmain.h"

using namespace NVM;

int main( int argc, char *argv[] )
{
    Stats *stats = new Stats( );
    Config *config = new Config( );
    GenericTraceReader *trace = NULL;
    TraceLine *tl = new TraceLine( );
    SimInterface *simInterface = new NullInterface( );
    NVMain *nvmain = new NVMain( );
    EventQueue *mainEventQueue = new EventQueue( );
    TagGenerator *tagGenerator = new TagGenerator( 1000 );
    bool IgnoreData = false;

    unsigned int simulateCycles;
    unsigned int currentCycle;
    
    if( argc < 4 )
    {
        std::cout << "Usage: nvmain CONFIG_FILE TRACE_FILE CYCLES [PARAM=value ...]" 
            << std::endl;
        return 1;
    }

    /* Print out the command line that was provided. */
    std::cout << "NVMain command line is:" << std::endl;
    for( int curArg = 0; curArg < argc; ++curArg )
    {
        std::cout << argv[curArg] << " ";
    }
    std::cout << std::endl << std::endl;

    config->Read( argv[1] );
    config->SetSimInterface( simInterface );
    nvmain->SetEventQueue( mainEventQueue );
    nvmain->SetStats( stats );
    nvmain->SetTagGenerator( tagGenerator );
    std::ofstream statStream;

    /* Allow for overriding config parameter values for trace simulations from command line. */
    if( argc > 4 )
    {
        for( int curArg = 4; curArg < argc; ++curArg )
        {
            std::string clParam, clValue, clPair;
            
            clPair = argv[curArg];
            clParam = clPair.substr( 0, clPair.find_first_of("="));
            clValue = clPair.substr( clPair.find_first_of("=") + 1, std::string::npos );

            std::cout << "Overriding " << clParam << " with '" << clValue << "'" << std::endl;

            config->SetValue( clParam, clValue );
        }
    }


    if( config->KeyExists( "StatsFile" ) )
    {
        statStream.open( config->GetString( "StatsFile" ).c_str(), 
                         std::ofstream::out | std::ofstream::app );
    }

    if( config->KeyExists( "IgnoreData" ) && config->GetString( "IgnoreData" ) == "true" )
    {
        IgnoreData = true;
    }

    /*  Add any specified hooks */
    std::vector<std::string>& hookList = config->GetHooks( );

    for( size_t i = 0; i < hookList.size( ); i++ )
    {
        std::cout << "Creating hook " << hookList[i] << std::endl;

        NVMObject *hook = HookFactory::CreateHook( hookList[i] );
        
        if( hook != NULL )
        {
            nvmain->AddHook( hook );
            hook->SetParent( nvmain );
            hook->Init( config );
        }
        else
        {
            std::cout << "Warning: Could not create a hook named `" 
                << hookList[i] << "'." << std::endl;
        }
    }

    simInterface->SetConfig( config, true );
    nvmain->SetConfig( config, "defaultMemory", true );

    nvmain->PrintHierarchy( );

    if( config->KeyExists( "TraceReader" ) )
        trace = TraceReaderFactory::CreateNewTraceReader( 
                config->GetString( "TraceReader" ) );
    else
        trace = TraceReaderFactory::CreateNewTraceReader( "NVMainTrace" );

    trace->SetTraceFile( argv[2] );

    if( argc == 3 )
        simulateCycles = 0;
    else
        simulateCycles = atoi( argv[3] );

    simulateCycles *= (unsigned int)ceil( (double)(config->GetValue( "CPUFreq" )) 
                        / (double)(config->GetValue( "CLK" )) ); 


    currentCycle = 0;
    while( currentCycle <= simulateCycles || simulateCycles == 0 )
    {
        if( !trace->GetNextAccess( tl ) )
        {
            std::cout << "Could not read next line from trace file!" 
                << std::endl;

            break;

            /* Just ride it out 'til the end. */
            while( currentCycle < simulateCycles )
            {
                nvmain->Cycle( 1 );
              
                currentCycle++;
            }

            break;
        }

        NVMainRequest *request = new NVMainRequest( );
        
        request->address = tl->GetAddress( );
        request->type = tl->GetOperation( );
        request->bulkCmd = CMD_NOP;
        request->threadId = tl->GetThreadId( );
        if( !IgnoreData ) request->data = tl->GetData( );
        request->status = MEM_REQUEST_INCOMPLETE;
        request->owner = (NVMObject *)nvmain;
        
        /* 
         * If you want to ignore the cycles used in the trace file, just set
         * the cycle to 0. 
         */
        if( config->KeyExists( "IgnoreTraceCycle" ) 
                && config->GetString( "IgnoreTraceCycle" ) == "true" )
            tl->SetLine( tl->GetAddress( ), tl->GetOperation( ), 0, 
                            tl->GetData( ), tl->GetThreadId( ) );

        if( request->type != READ && request->type != WRITE )
            std::cout << "traceMain: Unknown Operation: " << request->type 
                << std::endl;

        /* 
         * If the next operation occurs after the requested number of cycles,
         * we can quit. 
         */
        if( tl->GetCycle( ) > simulateCycles && simulateCycles != 0 )
        {
            /* Just ride it out 'til the end. */
            while( currentCycle < simulateCycles )
            {
                nvmain->Cycle( 1 );
              
                currentCycle++;
            }

            break;
        }
        else
        {
            /* 
             *  If the command is in the past, it can be issued. This would 
             *  occur since the trace was probably generated with an inaccurate 
             *  memory *  simulator, so the cycles may not match up. Otherwise, 
             *  we need to wait.
             */
            if( tl->GetCycle( ) > currentCycle )
            {
                /* Wait until currentCycle is the trace operation's cycle. */
                while( currentCycle < tl->GetCycle( ) )
                {
                    if( currentCycle >= simulateCycles && simulateCycles != 0 )
                        break;

                    nvmain->Cycle( 1 );

                    currentCycle++;
                }

                if( currentCycle >= simulateCycles && simulateCycles != 0 )
                    break;
            }

            /* 
             *  Wait for the memory controller to accept the next command.. 
             *  the trace reader is "stalling" until then.
             */
            while( !nvmain->IssueCommand( request ) )
            {
                if( currentCycle >= simulateCycles && simulateCycles != 0 )
                    break;

                nvmain->Cycle( 1 );

                currentCycle++;
            }

            if( currentCycle >= simulateCycles && simulateCycles != 0 )
                break;
        }
    }       

    nvmain->CalculateStats( );
    std::ostream& refStream = (statStream.is_open()) ? statStream : std::cout;
    stats->PrintAll( refStream );

    std::cout << "Exiting at cycle " << currentCycle << " because simCycles " 
        << simulateCycles << " reached." << std::endl; 

    delete config;
    delete stats;

    return 0;
}
