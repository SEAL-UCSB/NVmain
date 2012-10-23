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
#include "NVM/nvmain.h"


using namespace NVM;



int main( int argc, char *argv[] )
{
  Config *config = new Config( );
  GenericTrace *trace = NULL;
  TraceLine *tl = new TraceLine( );
  SimInterface *simInterface = new NullInterface( );
  NVMain *nvmain = new NVMain( );


  unsigned int simulateCycles;
  unsigned int currentCycle;


  
  if( argc != 3 && argc != 4 )
    {
      std::cout << "Usage: nvmain CONFIG_FILE TRACE_FILE [CYCLES]" << std::endl;
      return 1;
    }



  config->Read( argv[1] );
  config->SetSimInterface( simInterface );
  simInterface->SetConfig( config );
  nvmain->SetConfig( config );


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


  currentCycle = 0;
  while( currentCycle <= simulateCycles || simulateCycles == 0 )
    {
      if( !trace->GetNextAccess( tl ) )
        {
          std::cout << "Could not read next line from trace file!" << std::endl;

          /* Just ride it out 'til the end. */
          while( currentCycle < simulateCycles )
            {
              nvmain->Cycle( 1 );
            
              currentCycle++;
            }

          break;
        }
      

      NVMainRequest *request = new NVMainRequest( );
      
      request->address.SetPhysicalAddress( tl->GetAddress( ) );
      request->type = tl->GetOperation( );
      request->bulkCmd = CMD_NOP;
      request->threadId = tl->GetThreadId( );
      request->data = tl->GetData( );
      request->status = MEM_REQUEST_INCOMPLETE;
      request->owner = (NVMObject *)nvmain;

      
      /* If you want to ignore the cycles used in the trace file, just set the cycle to 0. */
      if( config->KeyExists( "IgnoreTraceCycle" ) && config->GetString( "IgnoreTraceCycle" ) == "true" )
        tl->SetLine( tl->GetAddress( ), tl->GetOperation( ), 0, tl->GetData( ), tl->GetThreadId( ) );


      if( request->type != READ && request->type != WRITE )
	    std::cout << "traceMain: Unknown Operation: " << request->type << std::endl;

      /* If the next operation occurs after the requested number of cycles, we can quit. */
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

                  nvmain->Cycle( 1 );

                  currentCycle++;
                }

              if( currentCycle >= simulateCycles )
                break;
            }


          /* 
           *  Wait for the memory controller to accept the next command.. the trace
           *  reader is "stalling" until then.
           */
          while( !nvmain->NewRequest( request ) )
            {
              if( currentCycle >= simulateCycles )
                break;

              nvmain->Cycle( 1 );

              currentCycle++;
            }

          if( currentCycle >= simulateCycles )
            break;
        }
    }       

  nvmain->PrintStats( );

  std::cout << "Exiting at cycle " << currentCycle << " because simCycles " << simulateCycles 
            << " reached." << std::endl; 

  delete config;

  return 0;
}
