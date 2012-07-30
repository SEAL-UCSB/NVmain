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

#include "include/NVMDataBlock.h"

#include <iomanip>


using namespace NVM;




NVMDataBlock::NVMDataBlock( )
{
  data.clear( );
}



NVMDataBlock::~NVMDataBlock( )
{
  data.clear( );
}



uint8_t NVMDataBlock::GetByte( uint64_t byte )
{
  if( byte >= data.size( ) )
    return 0;

  return data[ byte ];
}


void NVMDataBlock::SetByte( uint64_t byte, uint8_t value )
{
  if( byte >= data.size( ) )
    {
      /* 
       *  There's probably some other way to do this, but extend
       *  the vector size by pushing 0s on the end.
       */
      for( size_t i = data.size( ); i <= byte; i++ )
        data.push_back( 0 );
    }

  data[ byte ] = value;
}


void NVMDataBlock::Print( std::ostream& out ) const
{
  for( size_t i = 0; i < data.size( ); i++ )
    out << std::hex << std::setw( 2 ) << std::setfill( '0' ) << (int)data[ data.size( ) - i - 1 ] << std::dec;
}


NVMDataBlock NVMDataBlock::operator=( NVMDataBlock m )
{
  for( size_t it = 0; it < data.size( ); it++ )
    {
      data.push_back( m.data[it] );
    }
  rawData = m.rawData;

  return *this;
}


std::ostream& operator<<( std::ostream& out, const NVMDataBlock& obj )
{
  obj.Print( out );
  return out;
}
