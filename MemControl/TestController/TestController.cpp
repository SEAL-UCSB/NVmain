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

#include "MemControl/TestController/TestController.h"

//#define GEMS_TEST

#ifdef GEMS_TEST
#include "SimInterface/GemsInterface/GemsInterface.h"
#include "System.h"
#include "Profiler.h"
#include "CacheProfiler.h"
#endif 

#include <iostream>


using namespace NVM;



/*
 *  This simple memory controller is an example memory controller for NVMain, and operates as follows:
 *
 *  - After each read or write is issued, the page is closed.
 *    For each request, it simply prepends an activate before the read/write and appends a precharge
 *  - This memory controller leaves all banks and ranks in active mode (it does not do power management)
 */
TestController::TestController( Interconnect *memory, AddressTranslator *translator )
{
  /*
   *  First, we need to define how the memory is translated. The main() function already 
   *  sets the width of each partition based on the number of channels, ranks, banks, rows, and
   *  columns. For example, 4 banks would be 2-bits. 1 channel is 0-bits. 1024 columns is 10 bits
   *  of the address. Here, you just need to specify what order the partitions are in.
   *
   *  In this system, the address is broken up as follows:
   *
   *  -------------------------------------------------------------------------------------------
   *  |     COLUMN     |         RANK         |       BANK      |      ROW     |    CHANNEL     |
   *  -------------------------------------------------------------------------------------------
   */
  translator->GetTranslationMethod( )->SetOrder( 2, 3, 4, 5, 1 );


  /*
   *  We'll need these classes later, so copy them to our private member variables. 
   */
  this->memory = memory;
  this->translator = translator;

  std::cout << "Created a Test memory controller!" << std::endl;

  for( int i = 0; i < 4; i++ )
    {
      sourceThread[i] = 0;
      readCount[i] = 0;
    }
}



/*
 *  This method is called whenever a new transaction from the processor issued to
 *  this memory controller / channel. All scheduling decisions should be made here.
 */
int TestController::StartCommand( MemOp *mop )
{
  MemOp *nextOp;
  SimInterface *sim;
  double MPKI;
  unsigned int instructions;
  unsigned int misses;

  /*
   *  For this simple memory controller, we will always issue an ACTIVATE, followed
   *  by the READ/WRITE command and then a PRECHARGE command. Since the processor
   *  just send LOAD/STORE we need to create an ACTIVATE command.
   *
   *  To do that, just copy the old command (which contains the address) and change
   *  the operation to an ACTIVATE.
   */
  nextOp = new MemOp( );
  *nextOp = *mop;
  nextOp->SetOperation( ACTIVATE );
   

  sim = GetConfig( )->GetSimInterface( );
  if( sim != NULL )
    {
      instructions = sim->GetInstructionCount( 0 );
      misses = sim->GetCacheMisses( 0, 2 );
      if( instructions != 0 )
        MPKI = (double)( (double)(misses) / ((double)(instructions)/1000.0f) );
      else
        MPKI = 0.0f;
    }

  //std::cout << "Request from thread " << mop->GetThreadId( ); 
  //std::cout << " ... NMR thread id: " << mop->GetRequest( )->threadId;
  //std::cout << " ... Num instructions so far: " << instructions;
  //std::cout << " ... Num misses: " << misses;
  //std::cout << " ... MPKI " << MPKI;
  //std::cout << " ... CPI = " << (float)((float)currentCycle / (float)instructions);
  //std::cout << " ... IPC = " << (float)((float)instructions / (float)currentCycle);
  //std::cout << std::endl;


  commandQueue->push_back( nextOp ); 
  commandQueue->push_back( mop );


  sourceThread[ mop->GetRequest( )->threadId ]++;
  if( mop->GetOperation( ) == READ )
    readCount[ mop->GetRequest( )->threadId ]++;

  nextOp = new MemOp( );
  *nextOp = *mop;
  nextOp->SetOperation( PRECHARGE );

  commandQueue->push_back( nextOp );

  /*
   *  Return if the request could be queued or not. Return false if the queue is full.
   */
  return true;
}




void TestController::Cycle( )
{
#ifdef GEMS_TEST
  if( currentCycle % 10000 == 0 )
    {
      unsigned int sum = 0;
      
      std::cout << "Requests from cores: ";
       
      for( int i = 0; i < 4; i++ )
        sum += sourceThread[i];
      
      std::cout << sum << " [ ";

      for( int i = 0; i < 4; i++ )
        std::cout << sourceThread[i] << " ";

      std::cout << "]" << std::endl;

      // reads
      sum = 0;

      std::cout << "Reads from cores: ";
       
      for( int i = 0; i < 4; i++ )
        sum += readCount[i];
      
      std::cout << sum << " [ ";

      for( int i = 0; i < 4; i++ )
        std::cout << readCount[i] << " ";

      std::cout << "]" << std::endl;

      // writes
      sum = 0;

      std::cout << "Writes from cores: ";
       
      for( int i = 0; i < 4; i++ )
        sum += sourceThread[i] - readCount[i];
      
      std::cout << sum << " [ ";
      
      for( int i = 0; i < 4; i++ )
        std::cout << sourceThread[i] - readCount[i] << " ";

      std::cout << "]" << std::endl;


      // Misses
      sum = 0;

      std::cout << "Misses in cores: ";

      for( int i = 0; i < 4; i++ )
        sum += GetConfig( )->GetSimInterface( )->GetCacheMisses( i, 2 );

      std::cout << sum << " [ ";
      
      for( int i = 0; i < 4; i++ )
        std::cout << GetConfig( )->GetSimInterface( )->GetCacheMisses( i, 2 ) << " ";

      std::cout << "]" << std::endl;

      // User nmisses
      sum = 0;
      GemsInterface *gems = (GemsInterface *)GetConfig( )->GetSimInterface( );
      std::cout << "UMisses in cores: ";

      for( int i = 0; i < 4; i++ )
        sum += gems->GetUserMisses( i );

      std::cout << sum << " [ ";
      
      for( int i = 0; i < 4; i++ )
        std::cout << gems->GetUserMisses( i ) << " ";

      std::cout << "]" << std::endl;

      // Read count
      std::cout << "Read Count: ";
      
      sum = 0;
      for( int i = 0; i < 4; i++ )
        sum += readCount[i];

      std::cout << sum << " [ ";

      for( int i = 0; i < 4; i++ )
        std::cout << readCount[i] << " ";

      std::cout << "]" << std::endl;

      std::cout << std::endl;

    }
#endif
#if 0
      // request types
      sum = 0;

      //gems->GetSystemPtr( )->getProfiler( )->getL2CacheProfiler( )->printStats( std::cout );
      for( int i = 0; i < (int)(GenericRequestType_NUM); i++ )
        {
          std::cout << "Requests of type " << i << " = " 
                << gems->GetSystemPtr( )->getProfiler( )->getL2CacheProfiler( )->m_requestTypeVec_ptr->ref(i) 
                << std::endl;

          sum += gems->GetSystemPtr( )->getProfiler( )->getL2CacheProfiler( )->m_requestTypeVec_ptr->ref(i);
        }

      std::cout << "Total requests: " << sum << std::endl;

      std::cout << std::endl;
    }
#endif
  MemoryController::Cycle( );
}

