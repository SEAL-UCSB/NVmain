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


#include "traceReader/NVMainTrace/NVMainTrace.h"

#include <sstream>
#include <stdlib.h>


using namespace NVM;





NVMainTrace::NVMainTrace( )
{
  traceFile = "";
}


NVMainTrace::~NVMainTrace( )
{
  if( trace.is_open( ) )
    trace.close( );
}


void NVMainTrace::SetTraceFile( std::string file )
{
  traceFile = file;
}


std::string NVMainTrace::GetTraceFile( )
{
  return traceFile;
}


/*
 *  This trace is printed from nvmain.cpp. The format is:
 *
 *  CYCLE OP ADDRESS DATA THREADID
 */
bool NVMainTrace::GetNextAccess( TraceLine *nextAccess )
{
  /* If there is no trace file, we can't do anything. */
  if( traceFile == "" )
    {
      std::cerr << "No trace file specified!" << std::endl;
      return false;
    }

  /* If the trace file is not open, open it if possible. */
  if( !trace.is_open( ) )
    {
      trace.open( traceFile.c_str( ) );
      if( !trace.is_open( ) )
        {
          std::cerr << "Could not open trace file: " << traceFile << "!" << std::endl;
          return false;
        }
    }


  std::string fullLine;

  /*
   *  We will read in a full line and fill in these values.
   */
  unsigned int cycle = 0;
  OpType operation = READ;
  uint64_t address;
  NVMDataBlock dataBlock;
  unsigned int threadId = 0;
  
  /*
   *  There are no more lines in the trace... Send back a "dummy" line.
   */
  getline( trace, fullLine );
  if( trace.eof( ) )
    {
      nextAccess->SetLine( 0xDEADC0DEDEADBEEFULL, NOP, 0, dataBlock, 0 );
      std::cout << "NVMainTrace: Reached EOF!" << std::endl;
      return false;
    }
  
  
  std::istringstream lineStream( fullLine );
  std::string field;
  unsigned char fieldId = 0;
  
  /*
   *  Again, the format is : CYCLE OP ADDRESS DATA THREADID
   *  So the field ids are :   0    1    2      3      4
   */
  while( getline( lineStream, field, ' ' ) )
    {
      if( field != "" )
        {
          if( fieldId == 0 )
            cycle = atoi( field.c_str( ) );
          else if( fieldId == 1 )
            {
              if( field == "R" )
            operation = READ;
              else if( field == "W" )
            operation = WRITE;
              else
            std::cout << "Warning: Unknown operation `" << field << "'" << std::endl;
            }
          else if( fieldId == 2 )
            {
              std::stringstream fmat;

              fmat << std::hex << field;
              fmat >> address;
            }
          else if( fieldId == 3 )
            {
              int byte;
              int start, end;

              /*
               *  Default all bytes to zero.
               */
              /* Assumes 64-bit words.... */
              for( byte = 0; byte < 8; byte++ )
               dataBlock.SetByte( byte, 0 );

              
              byte = 0;
              end = (int)field.length( ) - 2*byte;
              start = (int)field.length( ) - 2*byte - 2;

            
              while( end >= 0 )
                {
                  uint8_t decimal;
                  std::stringstream fmat;
                  
                  fmat << std::hex << field.substr( start, end - start );
                  fmat >> decimal;

                  dataBlock.SetByte( byte, decimal );
                  
                  byte++;

                  if( start == 0 )
                    break;
                  
                  end = (int)field.length( ) - 2*byte;
                  start = (int)field.length( ) - 2*byte - 2;

                  if( start < 0 ) start = 0;
                }
            }
          else if( fieldId == 4 )
            threadId = atoi( field.c_str( ) );
          
          fieldId++;
        }
    }


  static unsigned int linenum = 0;

  linenum++;

  if( operation != READ && operation != WRITE )
    std::cout << "NVMainTrace: Unknown Operation: " << operation << "Line number is " 
	      << linenum << ". Full Line is \"" << fullLine << "\"" << std::endl;

  /*
   *  Set the line parameters.
   */
  nextAccess->SetLine( address, operation, cycle, dataBlock, threadId );

  return true;
}


/* 
 * Get the next N accesses to main memory. Called GetNextAccess N times and places the
 * return values into a vector of TraceLine pointers.
 */
int NVMainTrace::GetNextNAccesses( unsigned int N, std::vector<TraceLine *> *nextAccesses )
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
