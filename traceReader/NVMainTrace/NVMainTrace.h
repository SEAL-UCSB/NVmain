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

#ifndef __NVMAINTRACE_H__
#define __NVMAINTRACE_H__



#include "traceReader/GenericTrace.h"

#include <string>
#include <iostream>
#include <fstream>


namespace NVM {


class NVMainTrace : public GenericTrace
{
 public:
  NVMainTrace( );
  ~NVMainTrace( );
  
  void SetTraceFile( std::string file );
  std::string GetTraceFile( );
  
  bool GetNextAccess( TraceLine *nextAccess );
  int  GetNextNAccesses( unsigned int N, std::vector<TraceLine *> *nextAccess );

  
 private:
  std::string traceFile;
  std::ifstream trace;

};


};



#endif

