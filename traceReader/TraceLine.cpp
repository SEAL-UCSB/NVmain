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

#include "traceReader/TraceLine.h"


using namespace NVM;




TraceLine::TraceLine( )
{
  /*
   *  Set the address to some default bad value so it is not read by
   *  the memory simulator before it is set by the trace reader.
   */
  address = 0xDEADC0DE0BADC0DEULL;
  operation = NOP;
  cycle = 0;
  threadId = 0;
}


TraceLine::~TraceLine( )
{
  /* There is no heap memory to free. Do nothing. */
}


/* Set the values of the address and memory operation. */
void TraceLine::SetLine( uint64_t addr, OpType op, unsigned int cy, NVMDataBlock data, unsigned int threadId )
{
  this->address = addr;
  this->operation = op;
  this->cycle = cy;
  this->data = data;
  this->threadId = threadId;
}


/* Get the address of the memory operation. */
uint64_t TraceLine::GetAddress( )
{
  return address;
}


/* Get the memory command of the operation. */
OpType TraceLine::GetOperation( )
{
  return operation;
}


unsigned int TraceLine::GetCycle( ) 
{
  return cycle;
}


NVMDataBlock TraceLine::GetData( )
{
  return data;
}


unsigned int TraceLine::GetThreadId( )
{
  return threadId;
}


