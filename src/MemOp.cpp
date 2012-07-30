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

#include "src/MemOp.h"

#include <stdlib.h>


using namespace NVM;



MemOp::MemOp( )
{
  request = NULL;
}


MemOp::~MemOp( )
{
}


void MemOp::SetCycle( uint64_t cy ) 
{
  cycle = cy;
}


void MemOp::SetAddress( NVMAddress addr )
{
  this->address = addr;
}


void MemOp::SetOperation( OpType op )
{
  operation = op;
}


void MemOp::SetBulkCmd( BulkCommand cmd )
{
  bulkCmd = cmd;
}


void MemOp::SetData( uint64_t d )
{
  data = d;
}


NVMainRequest *MemOp::GetRequest( )
{
  return request;
}


void MemOp::SetRequest( NVMainRequest *request )
{
  this->request = request;
}


void MemOp::SetThreadId( int tid )
{
  threadId = tid;
}


uint64_t MemOp::GetCycle( )
{
  return cycle;
}


NVMAddress& MemOp::GetAddress( )
{
  return this->address;
}


OpType MemOp::GetOperation( )
{
  return operation;
}


BulkCommand MemOp::GetBulkCmd( )
{
  return bulkCmd;
}


uint64_t MemOp::GetData( )
{
  return data;
}


int MemOp::GetThreadId( )
{
  return threadId;
}


MemOp MemOp::operator=( MemOp m )
{
  operation = m.operation;
  address = m.address;
  cycle = m.cycle;
  data = m.data;
  threadId = m.threadId;
  request = m.request;

  return *this;
}
