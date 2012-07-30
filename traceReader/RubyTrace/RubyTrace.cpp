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
#include <stdlib.h>

#include "traceReader/RubyTrace/RubyTrace.h"


using namespace NVM;





RubyTrace::RubyTrace( )
{
  traceFile = "";
}


RubyTrace::~RubyTrace( )
{
  /* Close the trace file if it is open. */
  if( trace.is_open( ) )
    trace.close( );
}


/* Set the trace file's filename. */
void RubyTrace::SetTraceFile( std::string file )
{
  traceFile = file;
}


/* Return the trace file's filename */
std::string RubyTrace::GetTraceFile( )
{
  return traceFile;
}



/*
 * Parse the trace file and find the next access to main memory. May read
 * multiple lines before a memory access is returned.
 */
bool RubyTrace::GetNextAccess( TraceLine *nextAccess )
{
  /* If trace file is not specified, we can't know what to do. */
  if( traceFile == "" )
    {
      std::cerr << "No trace file specified!" << std::endl;
      return false;
    }

  /* If trace file is not opened, we can't read from it. */
  if( !trace.is_open( ) )
    {
      trace.open( traceFile.c_str() );
      if( !trace.is_open( ) )
	{
	  std::cerr << "Could not open trace file: " << traceFile << "!" << std::endl;
	  return false;
	}
    }

  
  /* 
   * Read the next few lines from the file, looking for transactions that end
   * and do not end at one of the caches. Once the first one is found, return it.
   */
  std::string fullLine;

  /* We will break at errors / finishing points in the loop. */
  while( 1 )
    {
      NVMDataBlock dataBlock;
      unsigned int threadId;

      threadId = 0;

      /* Read a full line from the trace, ensuring we are not at the end of the file. */
      if( trace.eof( ) )
        {
          nextAccess->SetLine( 0xDEADC0DEDEADBEEFULL, NOP, 0, dataBlock, 0 );
          return false;
        }
      getline( trace, fullLine );

      /*
       * Insert the full ine into a string stream. We will use the string stream
       * to separate the fields in the trace files into useful data we need.
       */
      std::istringstream lineStream( fullLine );
      std::string field;
      unsigned char fieldId;
      
      /*
       * Interesting fields are the cycles, the unit issuing the trace command,
       * the command being executed on the memory, the address of the memory operation,
       * and the operation- such as load/store/fetch
       */
      std::string cycle, unit, command, address, memory, operation;
      uint64_t decAddress;
      unsigned int currentCycle = 0;
      unsigned int cycles = 0;
      OpType memOp;
      
      /*
       * In a Ruby Trace, most of the fields are not necessary for main memory purposes.
       * We will increment the field ID and use it as a reference to determine what we
       * are interested in. In this case, the format is as follows:
       *
       *     207 1 -1 Seq Done > [0x7ba4ce80, line 0x7ba4ce80] 206 cycles NULL IFETCH No
       *
       *      0  1  2  3    4  5      6        7        8       9    10    11    12   13
       *
       * Here we are interested in fields 3, 4, 6, 11, and 12. Field 3 is the unit
       * generating the memory request. Field 4 is the unit's command. Field 6 is the
       * address. Field 11 is the memory region where the result ends. Field 12 is the
       * memory operation.
       */
      fieldId = 0;
      while( getline( lineStream, field, ' ' ) )
        {
          if( field != "" )
            {
              if( fieldId == 0 )
            currentCycle = atoi( field.c_str( ) );
              else if( fieldId == 3 )
            unit = field;
              else if( fieldId == 4 )
            command = field;
              else if( fieldId == 6 )
            address = field.substr( 1, field.length( ) - 2 );
              else if( fieldId == 9 )
            cycles = atoi( field.c_str( ) );
              else if( fieldId == 11 )
            memory = field;
              else if( fieldId == 12 )
            operation = field;
              
              
              fieldId++;
            }
        }
      
      /*
       * If the unit generating the result is "Seq," it is the GEMS sequencer stepping
       * through the instructions. We want to find sequencer executing the "Done"
       * command. If the memory is "NULL," this is main memory. Other possibilites are
       * "L1Cache" or "L2Cache" for example.
       *
       * If it is a main memory request, we need to convert to either a read or write.
       * Ruby uses LD for load, IFETCH for instruction fetch, and ST for store. Both
       * LD and IFETCH will be mapped to a read command for the simulator. Stores are
       * mapped to write commands.
       */
      if( unit == "Seq" )
        {
          if( command == "Done" && memory == "NULL" ) 
            {
              std::stringstream fmat;

              fmat << std::hex << address;
              fmat >> decAddress;

              if( operation == "IFETCH" || operation == "LD" )
                memOp = READ;
              else if( operation == "ST" || operation == "ATOMIC" )
                memOp = WRITE;
              else
                {
                  memOp = NOP;
                  std::cout << "RubyTrace: Unknown memory operation! " << operation << std::endl;
                }

              nextAccess->SetLine( decAddress, memOp, currentCycle - cycles, dataBlock, threadId );
              break;
            }
        }
    }

  return true;
}


/* 
 * Get the next N accesses to main memory. Called GetNextAccess N times and places the
 * return values into a vector of TraceLine pointers.
 */
int RubyTrace::GetNextNAccesses( unsigned int N, std::vector<TraceLine *> *nextAccesses )
{
  int successes;
  class TraceLine *nextLine;

  /* Keep track of the actual number of accesses returned. */
  successes = 0;

  /* Loop N times, calling GetNextAccess each iteration. */
  for( unsigned int i = 0; i < N; i++ )
    {
      /* We need a new TraceLine so the old values are not overwritten. */
      nextLine = new TraceLine( );

      /* Get the next access and place it in "nextLine" */
      if( GetNextAccess( nextLine ) )
        {
          /* Push next line into the vector. */
          nextAccesses->push_back( nextLine );

          successes++;
        }
    }

  return successes;
}
