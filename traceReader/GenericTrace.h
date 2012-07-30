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

#ifndef __GENERICTRACE_H__
#define __GENERICTRACE_H__


#include <string>
#include <vector>


#include "traceReader/TraceLine.h"


namespace NVM {


class GenericTrace
{
 public:
  GenericTrace() { }
  virtual ~GenericTrace() { }

  virtual void SetTraceFile( std::string file ) = 0;

  virtual std::string GetTraceFile( ) = 0;
  virtual bool GetNextAccess( TraceLine *nextAccess ) = 0;
  virtual int  GetNextNAccesses( unsigned int N, std::vector<TraceLine *> *nextAccesses ) = 0;


};


};


#endif

