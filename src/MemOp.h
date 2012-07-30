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

#ifndef __MEMOP_H__
#define __MEMOP_H__


#include <stdint.h>


#include "include/NVMainRequest.h"


namespace NVM {


class MemOp
{
 public:
  MemOp( );
  ~MemOp( );

  void SetCycle( uint64_t );
  void SetAddress( NVMAddress addr );
  void SetOperation( OpType op );
  void SetBulkCmd( BulkCommand cmd );
  void SetData( uint64_t d );
  void SetThreadId( int tid );

  uint64_t GetCycle( );
  NVMAddress& GetAddress( );
  OpType GetOperation( );
  BulkCommand GetBulkCmd( );
  uint64_t GetData( );
  int GetThreadId( );

  MemOp operator=( MemOp m );

  NVMainRequest *GetRequest( );
  void SetRequest( NVMainRequest *request );

 private:
  OpType operation;
  BulkCommand bulkCmd;
  NVMAddress address;
  uint64_t cycle;
  uint64_t data;  
  int threadId;

  NVMainRequest *request;

};


};


#endif
