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


#include "Endurance/FlipNWrite/FlipNWrite.h"

#include <iostream>


using namespace NVM;



FlipNWrite::FlipNWrite( )
{
  /*
   *  Clear the life map, which will hold all of the endurance
   *  values for each of our rows. Do this to ensure it didn't
   *  happen to be allocated somewhere that thinks it contains 
   *  values.
   */
  life.clear( );

  flippedAddresses.clear( );


  /* Clear statistics */
  flipBitsWritten = 0;
  flipNWriteModify = 0;
  bitCompareModify = 0;

  SetGranularity( 1 );
}


FlipNWrite::~FlipNWrite( )
{
  /*
   *  Nothing to do here. We do not own the *config pointer, so
   *  don't delete that.
   */
}



void FlipNWrite::InvertData( NVMDataBlock& data, uint64_t startBit, uint64_t endBit )
{
  uint64_t wordSize;
  int startByte, endByte;

  wordSize = GetConfig( )->GetValue( "BusWidth" );
  wordSize *= GetConfig( )->GetValue( "tBURST" ) * GetConfig( )->GetValue( "RATE" );
  wordSize /= 8;

  startByte = (int)(startBit / 8);
  endByte = (int)((endBit - 1) / 8);

  for( int i = startByte; i <= endByte; i++ )
    {
      uint8_t originalByte = data.GetByte( i );
      uint8_t shiftByte = originalByte;
      uint8_t newByte = 0;

      for( int j = 0; j < 8; j++ )
        {
          uint64_t currentBit = i * 8 + j;
         
          if( currentBit < startBit || currentBit >= endBit )
            {
              shiftByte = static_cast<uint8_t>(shiftByte >> 1);
              continue;
            }

          if( !(shiftByte & 0x1) )
            {
              newByte = static_cast<uint8_t>(newByte | (1 << (7-j)));
            }

          shiftByte = static_cast<uint8_t>(shiftByte >> 1);
        }

      data.SetByte( i, newByte );
    }
}


bool FlipNWrite::Write( NVMAddress address, NVMDataBlock oldData, NVMDataBlock newData )
{
  /*
   *  The default life map is an stl map< uint64_t, uint64_t >. 
   *  You may map row and col to this map_key however you want.
   *  It is up to you to ensure there are no collisions here.
   */
  uint64_t row;
  uint64_t col;
  bool rv = true;

  /*
   *  For our simple row model, we just set the key equal to the row.
   */
  address.GetTranslatedAddress( &row, &col, NULL, NULL, NULL );

  
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
  uint64_t currentBit;
  int fpSize;
  uint64_t flipPartitions;
  int *modifyCount;

  std::vector< uint64_t > *nonInvertedKeys;
  std::vector< uint64_t > *invertedKeys;
  std::vector< NVMAddress > *nonInvertedFaultAddr;
  std::vector< NVMAddress > *invertedFaultAddr;

  rowSize = GetConfig( )->GetValue( "COLS" );
  
  wordSize = GetConfig( )->GetValue( "BusWidth" );
  wordSize *= GetConfig( )->GetValue( "tBURST" ) * GetConfig( )->GetValue( "RATE" );
  wordSize /= 8;

  fpSize = GetConfig( )->GetValue( "FlipNWriteGranularity" );
  if( fpSize == -1 )
    fpSize = 32; // Some default size if the parameter is not specified.


  flipPartitions = ( wordSize * 8 ) / fpSize; 

  nonInvertedKeys = new std::vector< uint64_t >[ flipPartitions ];
  invertedKeys    = new std::vector< uint64_t >[ flipPartitions ];
  nonInvertedFaultAddr = new std::vector< NVMAddress >[ flipPartitions ];
  invertedFaultAddr = new std::vector< NVMAddress >[ flipPartitions ];
  modifyCount = new int[ flipPartitions ];

  /*
   *  Count the number of bits that are modified. If it is more than
   *  half, then we will invert the data then write.
   */
  for( uint64_t i = 0; i < flipPartitions; i++ )
    modifyCount[i] = 0;
  currentBit = 0;

  for( uint64_t i = 0; i < flipPartitions; i++ )
    {
      uint64_t curAddr = (address.GetPhysicalAddress( ) << 3) + i*static_cast<uint64_t>(fpSize);

      if( flippedAddresses.count( curAddr ) )
        {
          InvertData( oldData, i*fpSize, (i+1)*fpSize );
        }
    }



  /*
   *  Check each byte to see if it was modified.
   */
  for( uint64_t i = 0; i < wordSize; ++i )
    {
      /*
       *  If no bytes have changed we can just continue. Yes, I know this
       *  will check the byte 8 times, but i'd rather not change the iter.
       */
      uint8_t oldByte, newByte;

      oldByte = oldData.GetByte( i );
      newByte = newData.GetByte( i );

      if( oldByte == newByte )
      {
          currentBit += 8;
	      continue;
      }

      /*
       *  If the bytes are different, then at least one bit has changed.
       *  check each bit individually.
       */
      for( int j = 0; j < 8; j++ )
        {
          uint8_t oldBit, newBit;
          NVMAddress faultAddr;

          oldBit = ( oldByte >> j ) & 0x1;
          newBit = ( newByte >> j ) & 0x1;

          /*
           *  Think of each row being partitioned into 1-bit divisions. 
           *  Each row has rowSize * 8 paritions. For the key we will use:
           *
           *  row * number of partitions + partition in this row
           */
          partitionCount = rowSize * 8;
          
          wordkey = row * partitionCount + (col * wordSize * 8) + i * 8 + j;
    
          faultAddr = address;
          faultAddr.SetBitAddress( static_cast<uint8_t>(j) );
          faultAddr.SetPhysicalAddress( address.GetPhysicalAddress( ) + i );

          /*
           *  Bit is unchanged- Add to invertedKeys to specify this bit
           *  should be decremented if the word is inverted.
           */
          if( oldBit == newBit )
            {
              invertedKeys[(int)(currentBit/fpSize)].push_back( wordkey );
              invertedFaultAddr[(int)(currentBit/fpSize)].push_back( faultAddr );
            }
          else
            {
              nonInvertedKeys[(int)(currentBit/fpSize)].push_back( wordkey );
              nonInvertedFaultAddr[(int)(currentBit/fpSize)].push_back( faultAddr );
              modifyCount[(int)(currentBit/fpSize)]++;
            }

          currentBit++;
        }
    }

    /*
     *  Flip any partitions as needed. If the partition is flipped, use the
     *  invertedKeys vector, otherwise nonInvertedKeys
     */
    std::vector< uint64_t >::iterator it;
    std::vector< NVMAddress >::iterator fit;

    for( uint64_t i = 0; i < flipPartitions; i++ )
      {
        std::vector< uint64_t > *decrementVector;
        std::vector< NVMAddress > *faultVector;

        bitCompareModify += modifyCount[i];

        if( modifyCount[i] > (fpSize / 2) )
          {
            decrementVector = &(invertedKeys[i]);
            faultVector = &(invertedFaultAddr[i]);

            InvertData( newData, i*fpSize, (i+1)*fpSize );

            flipBitsWritten++;
            flipNWriteModify += modifyCount[i] - (fpSize - modifyCount[i]);

            /*
             *  Mark this address as flipped. If the data was already inverted, it
             *  is marked uninverted by removing it from the flippedAddresses set. 
             *  If the data was not inverted, it is marked as inverted by adding it
             *  to the flippedAddresses set.
             */
            if( flippedAddresses.count( (address.GetPhysicalAddress( ) << 3) + i*fpSize ) )
              {
                //std::cout << "Data was previously inverted. Now non-inverted." << std::endl;
                flippedAddresses.erase( (address.GetPhysicalAddress( ) << 3) + i*fpSize ); 
              }
            else
              {
                //std::cout << "Data was NOT previously inverted. Now inverted." << std::endl;
                flippedAddresses.insert( (address.GetPhysicalAddress( ) << 3) + i*fpSize );
              }
          }
        else
          {
            flipNWriteModify += modifyCount[i];

            decrementVector = &(nonInvertedKeys[i]);
            faultVector = &(nonInvertedFaultAddr[i]);
          }

        for( it = decrementVector->begin( ), fit = faultVector->begin( ); 
             it != decrementVector->end( ); it++, fit++ )
          {
            NVMAddress faultAddr;

            faultAddr.SetBitAddress( (*it) & 0x7 );
            faultAddr.SetPhysicalAddress( (*it) >> 3 );

            /* If any of the writes result in a hard-error, return write failure. */
            if( !DecrementLife( *it, *fit ) )
              {
                rv = false;
              }
          }
      }

    GetConfig( )->GetSimInterface( )->SetDataAtAddress( address.GetPhysicalAddress( ), newData );
    
    return rv;
}



void FlipNWrite::PrintStats( )
{
    uint64_t totalMod;
    float reduction;

    totalMod = flipNWriteModify + flipBitsWritten;
    if( bitCompareModify != 0 )
        reduction = (((float)totalMod / (float)bitCompareModify)*100.0f);
    else
        reduction = 100.0f;

    std::cout << "FlipNWrite: Modified " << flipNWriteModify << " bits + " << flipBitsWritten
              << " flip bits. (" << totalMod << " total). BCS would modify " 
              << bitCompareModify << " bits so far. " << reduction 
              << "% of BCS!" << std::endl;
}


