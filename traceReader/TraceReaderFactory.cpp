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


#include "traceReader/TraceReaderFactory.h"

#include <iostream>


using namespace NVM;




GenericTrace *TraceReaderFactory::CreateNewTraceReader( std::string reader )
{
  GenericTrace *tracer = NULL;

  if( reader == "" )
    std::cout << "NVMain: TraceReader is not set in configuration file!" << std::endl;


  if( reader == "NVMainTrace" )
    tracer = new NVMainTrace( );
  else if( reader == "RubyTrace" )
    tracer = new RubyTrace( );
  

  if( tracer == NULL )
    std::cout << "NVMain: Unknown trace reader `" << reader << "'." << std::endl;

  return tracer;
}

