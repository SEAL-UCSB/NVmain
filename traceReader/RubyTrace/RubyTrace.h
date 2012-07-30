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

#ifndef __READRUBYTRACE_H__
#define __READRUBYTRACE_H__

#include <iostream>
#include <fstream>

#include "traceReader/GenericTrace.h"


namespace NVM {


/*
 *  This trace reader class reads a trace file generated from GEMS' ruby module. 
 *  The reader was tested using outputs from the MSI_MOSI_CMP_directory protocol.
 */
class RubyTrace : public GenericTrace
{
 public:
  RubyTrace();
  ~RubyTrace();

  void SetTraceFile( std::string file );

  std::string GetTraceFile( );
  bool GetNextAccess( TraceLine *nextAccess );
  int  GetNextNAccesses( unsigned int N, std::vector<TraceLine *> *nextAccesses );

 private:
  std::string traceFile;
  std::ifstream trace;

};


};


#endif
