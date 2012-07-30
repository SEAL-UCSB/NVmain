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


#ifndef __TRACEREADER_TRACEREADERFACTORY_H__
#define __TRACEREADER_TRACEREADERFACTORY_H__



#include "traceReader/NVMainTrace/NVMainTrace.h"
#include "traceReader/RubyTrace/RubyTrace.h"

#include "traceReader/GenericTrace.h"

#include <string>


namespace NVM {


class TraceReaderFactory
{
 public:
  TraceReaderFactory( ) { }
  ~TraceReaderFactory( ) { }

  static GenericTrace *CreateNewTraceReader( std::string reader );
};


};



#endif


