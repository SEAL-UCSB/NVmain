/*
 *  This file is part of NVMain- A cycle accurate timing, bit-accurate
 *  energy simulator for non-volatile memory. Originally developed by 
 *  Matt Poremba at the Pennsylvania State University.
 *
 *  Website: http://www.cse.psu.edu/~poremba/nvmain/
 *  Email: mrp5060@psu.edu
 *
 *  ---------------------------------------------------------------------
 *
 *  If you use this software for publishable research, please include 
 *  the original NVMain paper in the citation list and mention the use 
 *  of NVMain.
 *
 */

#include <sstream>
#include <cmath>
#include <stdlib.h>

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


using namespace NVM;




char *binaryString( char *str, unsigned int x, int from, int to )
{
  int j;

  for( j = from - 1; j < to; j++ )
    str[ to - j - 1 ] = x & (1 << j)  ? '1' : '0';
  
  str[ to - from + 1 ] = '\0';

  return str;
}


void PeriodicStats( unsigned int currentCycle, int channelCount, MemoryController **mc, Interconnect **memory )
{
  int statsPeriod = 0;
  
  if( mc[0]->GetConfig( )->KeyExists( "PeriodicStatsInterval" ) )
    statsPeriod = mc[0]->GetConfig( )->GetValue( "PeriodicStatsInterval" );

  if( statsPeriod <= 0 )
    return;

  if( currentCycle % statsPeriod == 0 )
    {
      std::cout << "=========================================================================" << std::endl;
      for( int i = 0; i < channelCount; i++ )
        {
          mc[i]->PrintStats( );
          memory[i]->PrintStats( );
        }
    }
}



int main( int argc, char *argv[] )
{
  Interconnect **memory;
  Config *config = new Config( );
  TranslationMethod *tm = new TranslationMethod( );
  AddressTranslator *at;
  GenericTrace *trace = NULL;
  MemoryController **mc;
  TraceLine *tl = new TraceLine( );
  MemoryControllerManager *memoryControllerManager;
  Config **channelConfig;
  SimInterface *simInterface = new NullInterface( );

  unsigned int simulateCycles;
  unsigned int currentCycle;
  int channels;


  
  if( argc != 3 && argc != 4 )
    {
      std::cout << "Usage: nvmain CONFIG_FILE TRACE_FILE [CYCLES]" << std::endl;
      return 1;
    }



  config->Read( argv[1] );
  config->SetSimInterface( simInterface );
  simInterface->SetConfig( config );


  if( config->KeyExists( "Decoder" ) )
    at = DecoderFactory::CreateNewDecoder( config->GetString( "Decoder" ) );
  else
    at = new AddressTranslator( );


  channels = config->GetValue( "CHANNELS" );
  if( channels <= 0 || config->GetValue( "ROWS" ) <= 0 || config->GetValue( "COLS" ) <= 0 
      || config->GetValue( "BANKS" ) <= 0 || config->GetValue( "RANKS" ) <= 0 )
    {
      std::cout << "Number of channels, ranks, banks, rows, or columns not specified in configuration file.\n";
      return 1;
    }


  if( config->GetString( "MEM_CTL" ) == "" )
    {
      std::cout << "Memory controller to use is not specified.\n";
      return 1;
    }

  tm->SetBitWidths( mlog2( config->GetValue( "ROWS" ) ),
		    mlog2( config->GetValue( "COLS" ) ),
		    mlog2( config->GetValue( "BANKS" ) ),
		    mlog2( config->GetValue( "RANKS" ) ),
		    mlog2( config->GetValue( "CHANNELS" ) )
		    );
  tm->SetCount( config->GetValue( "ROWS" ),
		config->GetValue( "COLS" ),
		config->GetValue( "BANKS" ),
		config->GetValue( "RANKS" ),
		config->GetValue( "CHANNELS" ) );
  at->SetTranslationMethod( tm );

  std::cout << "Width of rank is " << mlog2( config->GetValue( "RANKS" ) ) << std::endl;

  if( config->KeyExists( "TraceReader" ) )
    trace = TraceReaderFactory::CreateNewTraceReader( config->GetString( "TraceReader" ) );
  else
    trace = TraceReaderFactory::CreateNewTraceReader( "NVMainTrace" );
  trace->SetTraceFile( argv[2] );

  if( argc == 3 )
    simulateCycles = 0;
  else
    simulateCycles = atoi( argv[3] );

  simulateCycles *= (unsigned int)ceil( (double)(config->GetValue( "CPUFreq" )) / (double)(config->GetValue( "CLK" )) ); 

  memory = new Interconnect* [channels];

  memoryControllerManager = new MemoryControllerManager( );
  memoryControllerManager->SetConfig( config );

  mc = new MemoryController* [channels];
  channelConfig = new Config* [channels];
  for( int i = 0; i < channels; i++ )
    {
      std::stringstream confString;
      std::string channelConfigFile;

      channelConfig[i] = new Config( );

      channelConfig[i]->Read( config->GetFileName( ) );
      channelConfig[i]->SetSimInterface( config->GetSimInterface( ) );

      confString << "CONFIG_CHANNEL" << i;

      if( config->GetString( confString.str( ) ) != "" )
        {
          channelConfigFile  = GetFilePath( config->GetFileName( ) );
          channelConfigFile += config->GetString( confString.str( ) );
          
          std::cout << "Reading channel config file: " << channelConfigFile << std::endl;

          channelConfig[i]->Read( channelConfigFile );
        }

      /*
       *  Initialize ranks
       */
      memory[i] = InterconnectFactory::CreateInterconnect( config->GetString( "INTERCONNECT" ) );

      confString.str( "" );
      confString << "defaultMemory.channel" << i;
      memory[i]->StatName( confString.str( ) );
      memory[i]->SetConfig( channelConfig[i] );


      /*
       *  Initialize memory controller
       */
      mc[i] = MemoryControllerFactory::CreateNewController( channelConfig[i]->GetString( "MEM_CTL" ), memory[i], at );
      
      confString.str( "" );
      confString << "defaultMemory.Controller" << i << "." << channelConfig[i]->GetString( "MEM_CTL" ); 
      mc[i]->StatName( confString.str( ) );
      mc[i]->SetConfig( channelConfig[i] );
      mc[i]->SetID( i );

      memoryControllerManager->AddController( mc[i] );
    }

  currentCycle = 0;
  while( currentCycle <= simulateCycles || simulateCycles == 0 )
    {
      MemOp *nextOp = new MemOp( );
      uint64_t nextRow, nextCol, nextBank, nextRank, nextChannel;
      NVMAddress opAddr;

      if( !trace->GetNextAccess( tl ) )
        {
          std::cout << "Could not read next line from trace file!" << std::endl;

          /* Just ride it out 'til the end. */
          while( currentCycle < simulateCycles )
            {
              for( int i = 0; i < channels; i++ )
                {
                  mc[i]->Cycle( );
                  mc[i]->FlushCompleted( );
                }
            
              currentCycle++;

              PeriodicStats( currentCycle, channels, mc, memory );
            }

          break;
        }
      

      at->Translate( tl->GetAddress( ), &nextRow, &nextCol, &nextBank, &nextRank, &nextChannel );

      //std::cout << "Address 0x" << std::hex << tl->GetAddress( ) << std::dec << " translates to "
      //          << "R " << nextRow << " / " << " C " << nextCol << " / "
      //          << " B " << nextBank << " / " << " K " << nextRank << " / "
      //          << " H " << nextChannel << std::endl;

      opAddr = nextOp->GetAddress( );
      opAddr.SetTranslatedAddress( nextRow, nextCol, nextBank, nextRank, nextChannel );
      opAddr.SetPhysicalAddress( tl->GetAddress( ) );
      nextOp->SetAddress( opAddr );
      nextOp->SetOperation( tl->GetOperation( ) );
      nextOp->SetThreadId( tl->GetThreadId( ) );

      NVMainRequest *request = new NVMainRequest( );
      
      request->address = nextOp->GetAddress( );
      request->type = nextOp->GetOperation( );
      request->bulkCmd = CMD_NOP;
      request->threadId = nextOp->GetThreadId( );
      request->data = tl->GetData( );
      request->status = MEM_REQUEST_INCOMPLETE;

      nextOp->SetRequest( request );

      
      /* If you want to ignore the cycles used in the trace file, just set the cycle to 0. */
      if( config->KeyExists( "IgnoreTraceCycle" ) && config->GetString( "IgnoreTraceCycle" ) == "true" )
        tl->SetLine( tl->GetAddress( ), tl->GetOperation( ), 0, tl->GetData( ), tl->GetThreadId( ) );

      nextOp->SetCycle( tl->GetCycle( ) );

      if( nextOp->GetOperation( ) != READ && nextOp->GetOperation( ) != WRITE )
	    std::cout << "traceMain: Unknown Operation: " << nextOp->GetOperation( ) << std::endl;

      /* If the next operation occurs after the requested number of cycles, we can quit. */
      if( tl->GetCycle( ) > simulateCycles && simulateCycles != 0 )
        {
          /* Just ride it out 'til the end. */
          while( currentCycle < simulateCycles )
            {
              for( int i = 0; i < channels; i++ )
                {
                  mc[i]->Cycle( );
                  mc[i]->FlushCompleted( );
                }
            
              currentCycle++;

              PeriodicStats( currentCycle, channels, mc, memory );
            }

          break;
        }
      else
        {
          /* 
           *  If the command is in the past, it can be issued. This would occur
           *  since the trace was probably generated with an inaccurate memory
           *  simulator, so the cycles may not match up. Otherwise, we need to wait.
           */
          if( tl->GetCycle( ) > currentCycle )
            {
              /* Wait until currentCycle is the trace operation's cycle. */
              while( currentCycle < tl->GetCycle( ) )
                {
                  if( currentCycle >= simulateCycles )
                    break;

                  for( int i = 0; i < channels; i++ )
                    {
                      mc[i]->Cycle( );
                      mc[i]->FlushCompleted( );
                    }
                
                  currentCycle++;

                  PeriodicStats( currentCycle, channels, mc, memory );
                }

              if( currentCycle >= simulateCycles )
                break;
            }


          /* 
           *  Wait for the memory controller to accept the next command.. the trace
           *  reader is "stalling" until then.
           */
          while( !mc[nextChannel]->StartCommand( nextOp ) )
            {
              if( currentCycle >= simulateCycles )
                break;

              for( int i = 0; i < channels; i++ )
                {
                  mc[i]->Cycle( );
                  mc[i]->FlushCompleted( );
                }

              currentCycle++;

              PeriodicStats( currentCycle, channels, mc, memory );
            }

          if( currentCycle >= simulateCycles )
            break;
        }
    }       


  std::cout << "Exiting at cycle " << currentCycle << " because simCycles " << simulateCycles 
            << " reached." << std::endl; 

  for( int i = 0; i < channels; i++ )
    {
      std::cout << "Channel " << i << std::endl << std::endl;

      mc[i]->PrintStats( );

      std::cout << std::endl << std::endl;
    }

  for( int i = 0; i < channels; i++ )
    {
      delete mc[i];
    }

  delete [] mc;

  delete tm;
  delete config;
  delete memory;

  return 0;
}
