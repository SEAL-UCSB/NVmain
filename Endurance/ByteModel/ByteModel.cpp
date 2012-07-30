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


#include "Endurance/ByteModel/ByteModel.h"

#include <iostream>


using namespace NVM;



ByteModel::ByteModel( )
{
  /*
   *  Clear the life map, which will hold all of the endurance
   *  values for each of our rows. Do this to ensure it didn't
   *  happen to be allocated somewhere that thinks it contains 
   *  values.
   */
  life.clear( );

  SetGranularity( 8 );
}


ByteModel::~ByteModel( )
{
  /*
   *  Nothing to do here. We do not own the *config pointer, so
   *  don't delete that.
   */
}


bool ByteModel::Write( NVMAddress address, NVMDataBlock oldData, NVMDataBlock newData )
{
  /*
   *  The default life map is an stl map< uint64_t, uint64_t >. 
   *  You may map row and col to this map_key however you want.
   *  It is up to you to ensure there are no collisions here.
   */
  uint64_t row;
  uint64_t col;
  bool rv = true;
  NVMAddress faultAddr;

  /*
   *  For our simple row model, we just set the key equal to the row.
   */
  address.GetTranslatedAddress( &row, &col, NULL, NULL, NULL );
  faultAddr = address;

  
  /*
   *  If using the default life map, we can call the DecrementLife
   *  function which will check if the map_key already exists. If so,
   *  the life value is decremented (write count incremented). Otherwise 
   *  the map_key is inserted with a write count of 1.
   */
  uint64_t wordkey;
  uint64_t rowSize;
  uint64_t wordSize;
  uint64_t partitionCount;

  // Size of a row in bytes
  rowSize = GetConfig( )->GetValue( "COLS" );

  // wordSize is the size of a word written to memory, usually a cacheline. This size is in bytes.
  wordSize = GetConfig( )->GetValue( "BusWidth" );
  wordSize *= GetConfig( )->GetValue( "tBURST" ) * GetConfig( )->GetValue( "RATE" );
  wordSize /= 8;

  /*
   *  Check each byte to see if it was modified.
   */
  for( int i = (int)wordSize - 1; i >= 0; --i )
    {
      uint8_t oldByte, newByte;

      oldByte = oldData.GetByte( i );
      newByte = newData.GetByte( i );

      if( oldByte == newByte ) 
        continue;

      /*
       *  Think of each row being partitioned into 8-bit divisions. Each 
       *  row has rowSize / 8 paritions. For the key we will use:
       *
       *  row * number of partitions + partition in this row
       */
      partitionCount = ( rowSize / 8 );

      wordkey = row * partitionCount + (int)(address.GetPhysicalAddress( ) / 8) + i;
    
      faultAddr.SetPhysicalAddress( address.GetPhysicalAddress( ) + i );
      if( !DecrementLife( wordkey, faultAddr ) )
        rv = false;  
    }

  return rv;
}


