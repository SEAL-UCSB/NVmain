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


#include "Endurance/RowModel/RowModel.h"


using namespace NVM;




RowModel::RowModel( )
{
  /*
   *  Clear the life map, which will hold all of the endurance
   *  values for each of our rows. Do this to ensure it didn't
   *  happen to be allocated somewhere that thinks it contains 
   *  values.
   */
  life.clear( );
}


RowModel::~RowModel( )
{
  /*
   *  Nothing to do here. We do not own the *config pointer, so
   *  don't delete that.
   */
}


void RowModel::SetConfig( Config *conf )
{
  SetGranularity( conf->GetValue( "COLS" ) * 8 );

  EnduranceModel::SetConfig( conf );
}


bool RowModel::Write( NVMAddress address, NVMDataBlock /*oldData*/, NVMDataBlock /*newData*/ )
{
  /*
   *  The default life map is an stl map< uint64_t, uint64_t >. 
   *  You may map row and col to this map_key however you want.
   *  It is up to you to ensure there are no collisions here.
   */
  uint64_t row;
  bool rv = true;
  NVMAddress faultAddr;

  /*
   *  For our simple row model, we just set the key equal to the row.
   */
  address.GetTranslatedAddress( &row, NULL, NULL, NULL, NULL );
  faultAddr = address;

  
  /*
   *  If using the default life map, we can call the DecrementLife
   *  function which will check if the map_key already exists. If so,
   *  the life value is decremented (write count incremented). Otherwise 
   *  the map_key is inserted with a write count of 1.
   */
  if( !DecrementLife( row, faultAddr ) )
    rv = false;

  return rv;
}


