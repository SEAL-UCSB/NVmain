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


#include "Endurance/WordModel/WordModel.h"

#include <iostream>


using namespace NVM;



WordModel::WordModel( )
{
  /*
   *  Clear the life map, which will hold all of the endurance
   *  values for each of our rows. Do this to ensure it didn't
   *  happen to be allocated somewhere that thinks it contains 
   *  values.
   */
  life.clear( );
}


WordModel::~WordModel( )
{
  /*
   *  Nothing to do here. We do not own the *config pointer, so
   *  don't delete that.
   */
}


void WordModel::SetConfig( Config *conf )
{
  SetGranularity( conf->GetValue( "BusWidth" ) * 8 );

  EnduranceModel::SetConfig( conf );
}


bool WordModel::Write( NVMAddress address, NVMDataBlock /*oldData*/, NVMDataBlock /*newData*/ )
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

  rowSize = GetConfig( )->GetValue( "COLS" );
  wordSize = GetConfig( )->GetValue( "BusWidth" );

  /*
   *  Think of each row being partitioned into 64-bit divisions (or
   *  whatever the Bus Width is). Each row has rowSize / wordSize
   *  paritions. For the key we will use:
   *
   *  row * number of partitions + partition in this row
   */
  partitionCount = rowSize / wordSize;

  wordkey = row * partitionCount + (col % wordSize);

  /* 
   *  The faultAddress is aligned to the cacheline. Since this model
   *  is per cache, we don't need to change the faultAddr's physical
   *  address.
   */
  if( !DecrementLife( wordkey, faultAddr ) )
    rv = false;

  return rv;
}


