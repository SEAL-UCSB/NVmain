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


#include "include/NVMAddress.h"


using namespace NVM;




NVMAddress::NVMAddress( )
{
  physicalAddress = 0;
  row = col = bank = rank = channel = 0;
}


NVMAddress::~NVMAddress( )
{

}


void NVMAddress::SetTranslatedAddress( uint64_t addrRow, uint64_t addrCol, uint64_t addrBank, uint64_t addrRank, uint64_t addrChannel )
{
  row = addrRow;
  col = addrCol;
  bank = addrBank;
  rank = addrRank;
  channel = addrChannel;
}


void NVMAddress::SetPhysicalAddress( uint64_t pAddress )
{
  physicalAddress = pAddress;
}


void NVMAddress::SetBitAddress( uint8_t bitAddr )
{
  bit = bitAddr;
}


void NVMAddress::GetTranslatedAddress( uint64_t *addrRow, uint64_t *addrCol, uint64_t *addrBank, uint64_t *addrRank, uint64_t *addrChannel )
{
  if( addrRow ) *addrRow = row;
  if( addrCol ) *addrCol = col;
  if( addrBank ) *addrBank = bank;
  if( addrRank ) *addrRank = rank;
  if( addrChannel ) *addrChannel = channel;
}


uint64_t NVMAddress::GetPhysicalAddress( )
{
  return physicalAddress;
}


uint8_t NVMAddress::GetBitAddress( )
{
  return bit;
}


NVMAddress& NVMAddress::operator=( const NVMAddress& m )
{
  physicalAddress = m.physicalAddress;
  row = m.row;
  col = m.col;
  bank = m.bank;
  rank = m.rank;
  channel = m.channel;
  bit = m.bit;

  return *this;
}
