/*******************************************************************************
* Copyright (c) 2012-2013, The Microsystems Design Labratory (MDL)
* Department of Computer Science and Engineering, The Pennsylvania State University
* All rights reserved.
* 
* This source code is part of NVMain - A cycle accurate timing, bit accurate
* energy simulator for both volatile (e.g., DRAM) and nono-volatile memory
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

#include "nvmain.h"
#include "src/Config.h"
#include "src/AddressTranslator.h"
#include "src/Interconnect.h"
#include "src/SimInterface.h"
#include "src/EventQueue.h"
#include "Interconnect/InterconnectFactory.h"
#include "MemControl/MemoryControllerFactory.h"
#include "Decoders/DecoderFactory.h"
#include "Utils/HookFactory.h"
#include "include/NVMainRequest.h"
#include "include/NVMHelpers.h"

#include <sstream>
#include <cassert>

using namespace NVM;

NVMain::NVMain( )
{
    config = NULL;
    memory = NULL;
    translator = NULL;
    memoryControllers = NULL;
    channelConfig = NULL;
    syncValue = 0.0f;
}

NVMain::~NVMain( )
{
    if( config ) 
        delete config;
    
    if( memoryControllers )
    {
        for( unsigned int i = 0; i < numChannels; i++ )
        {
            if( memoryControllers[i] )
                delete memoryControllers[i];
        }

        delete [] memoryControllers;
    }

    if( memory )
    {
        for( unsigned int i = 0; i < numChannels; i++ )
        {
            delete memory[i];
        }

        delete [] memory;
    }

    if( translator )
        delete translator;

    if( channelConfig )
    {
        for( unsigned int i = 0; i < numChannels; i++ )
        {
            delete channelConfig[i];
        }

        delete [] channelConfig;
    }
}

Config *NVMain::GetConfig( )
{
    return config;
}

void NVMain::SetConfig( Config *conf, std::string memoryName )
{
    TranslationMethod *method;
    int channels, ranks, banks, rows, cols;

    Params *params = new Params( );
    params->SetParams( conf );
    SetParams( params );

    config = conf;
    if( config->GetSimInterface( ) != NULL )
        config->GetSimInterface( )->SetConfig( conf );
    else
      std::cout << "Warning: Sim Interface should be allocated before configuration!" << std::endl;

    rows = (int)p->ROWS;
    cols = (int)p->COLS;
    banks = (int)p->BANKS;
    ranks = (int)p->RANKS;
    channels = (int)p->CHANNELS;

    if( config->KeyExists( "Decoder" ) )
        translator = DecoderFactory::CreateNewDecoder( config->GetString( "Decoder" ) );
    else
        translator = new AddressTranslator( );

    method = new TranslationMethod( );

    method->SetBitWidths( NVM::mlog2( rows ), 
          		NVM::mlog2( cols ), 
          		NVM::mlog2( banks ), 
          		NVM::mlog2( ranks ), 
          		NVM::mlog2( channels ) 
          		);
    method->SetCount( rows, cols, banks, ranks, channels );
    translator->SetTranslationMethod( method );
    translator->SetDefaultField( CHANNEL_FIELD );

    SetDecoder( translator );

    /*  Add any specified hooks */
    /*
    std::vector<std::string>& hookList = config->GetHooks( );

    for( size_t i = 0; i < hookList.size( ); i++ )
    {
        std::cout << "Creating hook " << hookList[i] << std::endl;

        NVMObject *hook = HookFactory::CreateHook( hookList[i] );
        
        if( hook != NULL )
        {
            AddHook( hook );
            hook->SetParent( this );
            hook->Init( );
        }
        else
        {
            std::cout << "Warning: Could not create a hook named `" 
                << hookList[i] << "'." << std::endl;
        }
    }
    */

    memory = new Interconnect* [channels];

    memoryControllers = new MemoryController* [channels];
    channelConfig = new Config* [channels];
    for( int i = 0; i < channels; i++ )
    {
        std::stringstream confString;
        std::string channelConfigFile;

        channelConfig[i] = new Config( *config );

        channelConfig[i]->SetSimInterface( config->GetSimInterface( ) );

        confString << "CONFIG_CHANNEL" << i;

        if( config->GetString( confString.str( ) ) != "" )
        {
            channelConfigFile  = config->GetString( confString.str( ) );

            if( channelConfigFile[0] != '/' )
            {
                channelConfigFile  = NVM::GetFilePath( config->GetFileName( ) );
                channelConfigFile += config->GetString( confString.str( ) );
            }
            
            std::cout << "Reading channel config file: " << channelConfigFile << std::endl;

            channelConfig[i]->Read( channelConfigFile );
        }

        /* Initialize ranks */
        memory[i] = InterconnectFactory::CreateInterconnect( config->GetString( "INTERCONNECT" ) );

        confString.str( "" );
        confString << memoryName << ".channel" << i;
        memory[i]->StatName( confString.str( ) );

        AddressTranslator *incAT = DecoderFactory::CreateDecoderNoWarn( conf->GetString( "Decoder" ) );
        incAT->SetTranslationMethod( method );
        incAT->SetDefaultField( RANK_FIELD );
        memory[i]->SetDecoder( incAT );


        /* Initialize memory controller */
        memoryControllers[i] = MemoryControllerFactory::CreateNewController( 
                channelConfig[i]->GetString( "MEM_CTL" ), memory[i], translator );

        confString.str( "" );
        confString << memoryName << ".channel" << i << "." << channelConfig[i]->GetString( "MEM_CTL" ); 
        memoryControllers[i]->StatName( confString.str( ) );
        memoryControllers[i]->SetID( i );

        AddChild( memoryControllers[i] );
        memoryControllers[i]->SetParent( this );

        memoryControllers[i]->AddChild( memory[i] );
        memory[i]->SetParent( memoryControllers[i] );

        /* Set Config recursively. */
        memory[i]->SetConfig( channelConfig[i] );
        memoryControllers[i]->SetConfig( channelConfig[i] );
    }
    
    numChannels = (unsigned int)channels;

    std::string pretraceFile;

    if( config->GetString( "PreTraceFile" ) != "" )
    {
        pretraceFile  = config->GetString( "PreTraceFile" );

        if( pretraceFile[0] != '/' )
        {
            pretraceFile  = NVM::GetFilePath( config->GetFileName( ) );
            pretraceFile += config->GetString( "PreTraceFile" );
        }

        std::cout << "Using trace file " << pretraceFile << std::endl;

        pretraceOutput.open( pretraceFile.c_str( ) );

        if( !pretraceOutput.is_open( ) )
              std::cout << "Warning: Could not open pretrace file " << config->GetString( "PreTraceFile" )
          	          << ". Output will be suppressed." << std::endl;
    }
}

bool NVMain::IsIssuable( NVMainRequest *request, FailReason * /*reason*/ )
{
    uint64_t channel, rank, bank, row, col;
    bool rv;

    if( request != NULL )
    {
        translator->Translate( request->address.GetPhysicalAddress( ), &row, &col, &rank, &bank, &channel );

        rv = !memoryControllers[channel]->QueueFull( request );
    }
    else
    {
        /* 
         *  Since we don't know what queue this will go to, we need to return if
         *  any of the queues are full..
         */
        rv = true;

        for( uint64_t i = 0; i < numChannels; i++ )
        {
            if( memoryControllers[i]->QueueFull( request ) )
              rv = false;
        }
    }

    return rv;
}

void NVMain::PrintPreTrace( NVMainRequest *request )
{
    /*
     *  Here we can generate a data trace to use with trace-based testing later.
     *
     *  CYCLE OP ADDRESS DATA THREADID
     */
    if( p->PrintPreTrace )
    {
        if( p->EchoPreTrace )
        {
            /* Output Cycle */
            std::cout << GetEventQueue()->GetCurrentCycle() << " ";
            
            /* Output operation */
            if( request->type == READ )
                std::cout << "R ";
            else
                std::cout << "W ";

            /* Output Address */
            std::cout << std::hex << "0x" << (request->address.GetPhysicalAddress( )) << std::dec << " ";
            
            /* Output Data */
            std::cout << request->data << " ";
            
            /* Output thread id */
            std::cout << request->threadId << std::endl;
        }
        
        if( pretraceOutput.is_open( ) )
        {
            /* Output Cycle */
            pretraceOutput << GetEventQueue()->GetCurrentCycle() << " ";
            
            /* Output operation */
            if( request->type == READ )
              pretraceOutput << "R ";
            else
              pretraceOutput << "W ";
            
            /* Output Address */
            pretraceOutput << std::hex << "0x" << (request->address.GetPhysicalAddress( )) << std::dec << " ";
            
            /* Output Data */
            pretraceOutput << request->data << " ";
            
            /* Output thread id */
            pretraceOutput << request->threadId << std::endl;
        }
    }
}

bool NVMain::IssueCommand( NVMainRequest *request )
{
    uint64_t channel, rank, bank, row, col;
    int mc_rv;

    if( !config )
    {
        std::cout << "NVMain: Received request before configuration!\n";
        return false;
    }

    /* Translate the address, then copy to the address struct, and copy to request. */
    translator->Translate( request->address.GetPhysicalAddress( ), &row, &col, &bank, &rank, &channel );
    request->address.SetTranslatedAddress( row, col, bank, rank, channel );
    request->bulkCmd = CMD_NOP;

    assert( GetChild( request )->GetTrampoline( ) == memoryControllers[channel] );
    mc_rv = GetChild( request )->IssueCommand( request );
    if( mc_rv == true )
        PrintPreTrace( request );

    return mc_rv;
}

bool NVMain::IssueAtomic( NVMainRequest *request )
{
    uint64_t channel, rank, bank, row, col;
    int mc_rv;

    if( !config )
    {
        std::cout << "NVMain: Received request before configuration!\n";
        return false;
    }

    /* Translate the address, then copy to the address struct, and copy to request. */
    translator->Translate( request->address.GetPhysicalAddress( ), &row, &col, &bank, &rank, &channel );
    request->address.SetTranslatedAddress( row, col, bank, rank, channel );
    request->bulkCmd = CMD_NOP;

    mc_rv = memoryControllers[channel]->IssueAtomic( request );
    if( mc_rv == true )
        PrintPreTrace( request );
    
    return mc_rv;
}

void NVMain::Cycle( ncycle_t )
{
    /*
     *  Previous errors can prevent config from being set. Likewise, if the first memoryController is
     *  NULL, so are all the others, so return here instead of seg faulting.
     */
    if( !config || !memoryControllers )
      return;

    /* Sync the memory clock with the cpu clock. */
    float cpuFreq = static_cast<float>(p->CPUFreq);
    float busFreq = static_cast<float>(p->CLK);

    syncValue += static_cast<float>( busFreq / cpuFreq );

    if( syncValue >= 1.0f )
    {
        syncValue -= 1.0f;
    }
    else
    {
        return;
    }

    for( unsigned int i = 0; i < numChannels; i++ )
      {
        memoryControllers[i]->Cycle( 1 );
      }

    GetEventQueue()->Loop( );
}

void NVMain::PrintStats( )
{
    for( unsigned int i = 0; i < numChannels; i++ )
        memoryControllers[i]->PrintStats( );
}

