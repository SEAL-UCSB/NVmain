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

#include "Decoders/DRCDecoder/DRCDecoder.h"
#include "include/NVMHelpers.h"

#include <iostream>

using namespace NVM;


DRCDecoder::DRCDecoder( )
{
  ignoreBits = 0;
  cachelineSize = 64;

  std::cout << "Created a DRC decoder!" << std::endl;
}



void DRCDecoder::SetIgnoreBits( uint64_t numIgnore )
{
  ignoreBits = numIgnore;
}


void DRCDecoder::SetCachelineSize( uint64_t lineSize )
{
  cachelineSize = lineSize;
}


void DRCDecoder::Translate( uint64_t address, uint64_t *row, uint64_t *col, uint64_t *bank, uint64_t *rank, uint64_t *channel )
{
  int rowOrder, colOrder, bankOrder, rankOrder, channelOrder;
  unsigned int rowBits, colBits, bankBits, rankBits, channelBits;
  uint64_t workingAddr;

  /* 
   *  Get the widths and order from the translation method so we know what 
   *  the user wants for bank/rank/channel ordering.
   */
  GetTranslationMethod( )->GetBitWidths( &rowBits, &colBits, &bankBits, &rankBits, &channelBits );
  GetTranslationMethod( )->GetOrder( &rowOrder, &colOrder, &bankOrder, &rankOrder, &channelOrder );

  /*
   *  Chop off the cacheline length and ignore bits first.
   */
  workingAddr = address;
  workingAddr = workingAddr >> mlog2( (int)cachelineSize );
  if( ignoreBits != 0 )
    workingAddr = workingAddr >> ignoreBits;


  /*
   *  Row always goes first.
   */
  //*row = workingAddr % (1 << rowBits);
  //workingAddr = workingAddr >> rowBits;


  /*
   *  Column is ignored in our dram cache.
   */
  *col = 0;


  /* 
   *  Find out if bank, rank, or channel are first, then decode accordingly.
   */
  if( channelOrder < rankOrder && channelOrder < bankOrder )
    {
      *channel = workingAddr % (1 << channelBits);
      workingAddr = workingAddr >> channelBits;

      if( rankOrder < bankOrder )
        {
          *rank = workingAddr % (1 << rankBits);
          workingAddr = workingAddr >> rankBits;

          *bank = workingAddr % (1 << bankBits);
          workingAddr = workingAddr >> bankBits;
        }
      else
        {
          *bank = workingAddr % (1 << bankBits);
          workingAddr = workingAddr >> bankBits;

          *rank = workingAddr % (1 << rankBits);
          workingAddr = workingAddr >> rankBits;
        }
    }
  /* Try rank first */
  else if( rankOrder < channelOrder && rankOrder < bankOrder )
    {
      *rank = workingAddr % (1 << rankBits);
      workingAddr = workingAddr >> rankBits;

      if( channelOrder < bankOrder )
        {
          *channel = workingAddr % (1 << channelBits);
          workingAddr = workingAddr >> channelBits;

          *bank = workingAddr % (1 << bankBits);
          workingAddr = workingAddr >> bankBits;
        }
      else
        {
          *bank = workingAddr % (1 << bankBits);
          workingAddr = workingAddr >> bankBits;

          *channel = workingAddr % (1 << channelBits);
          workingAddr = workingAddr >> channelBits;
        }
    }
  /* Bank first */
  else
    {
      *bank = workingAddr % (1 << bankBits);
      workingAddr = workingAddr >> bankBits;

      if( channelOrder < rankOrder )
        {
          *channel = workingAddr % (1 << channelBits);
          workingAddr = workingAddr >> channelBits;

          *rank = workingAddr % (1 << rankBits);
          workingAddr = workingAddr >> rankBits;
        }
      else
        {
          *rank = workingAddr % (1 << rankBits);
          workingAddr = workingAddr >> rankBits;

          *channel = workingAddr % (1 << channelBits);
          workingAddr = workingAddr >> channelBits;
        }
    }


  *row = workingAddr % (1 << rowBits);
  workingAddr = workingAddr >> rowBits;
}

