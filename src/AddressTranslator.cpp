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

#include <iostream>


#include "src/AddressTranslator.h"


using namespace NVM;




AddressTranslator::AddressTranslator( )
{
  method = NULL;
  defaultField = NO_FIELD;
}


AddressTranslator::~AddressTranslator( )
{

}


void AddressTranslator::SetTranslationMethod( TranslationMethod *m )
{
  method = m;
}


TranslationMethod *AddressTranslator::GetTranslationMethod( )
{
  return method;
}

/*
 * added by Tao @ 01/28/2013 
 * ReverseTranslate can return a physical address based on the translated
 * address
 */
uint64_t AddressTranslator::ReverseTranslate( uint64_t *row, uint64_t *col, uint64_t *bank,
				   uint64_t *rank, uint64_t *channel )
{
    uint64_t unitAddr = 1;
    uint64_t phyAddr = 0;
    MemoryPartition part;

    if( GetTranslationMethod( ) == NULL )
    {
        std::cerr << "Divider Translator: Translation method not specified!" << std::endl;
        exit(1);
    }
    
    unsigned rowBits, colBits, bankBits, rankBits, channelBits;

    GetTranslationMethod( )->GetBitWidths( &rowBits, &colBits, &bankBits, &rankBits, &channelBits );
    
    for( int i = 4; i >= 0; i-- )
    {
        /* 0->4, high to low, FindOrder() will find the correct one */
        FindOrder( i, &part );

        switch( part )
        {
            case MEM_ROW:
                  unitAddr <<= rowBits;
                  if( row != NULL )
                      phyAddr += (*row) * unitAddr; 
                  break;

            case MEM_COL:
                  unitAddr <<= colBits;
                  if( col != NULL )
                      phyAddr += (*col) * unitAddr; 
                  break;

            case MEM_BANK:
                  unitAddr <<= bankBits;
                  if( bank != NULL )
                      phyAddr += (*bank) * unitAddr; 
                  break;

            case MEM_RANK:
                  unitAddr <<= rankBits;
                  if( rank != NULL )
                      phyAddr += (*rank) * unitAddr; 
                  break;

            case MEM_CHANNEL:
                  unitAddr <<= channelBits;
                  if( channel != NULL )
                      phyAddr += (*channel) * unitAddr; 
                  break;

            default:
                  break;
        }
    }

    return phyAddr;
} 


void AddressTranslator::Translate( uint64_t address, uint64_t *row, uint64_t *col, uint64_t *bank,
				   uint64_t *rank, uint64_t *channel )
{
  uint64_t refAddress;
  MemoryPartition part;

  uint64_t *partitions[5] = { row, col, bank, rank, channel };

  if( GetTranslationMethod( ) == NULL )
    {
      std::cerr << "Divider Translator: Translation method not specified!" << std::endl;
      return;
    }


  /* NVMain assumes memory word addresses! */
  //address = address >> 6; // TODO: make this the config value of cacheline size
  refAddress = address;


  /* Find the partition that is first in order. */
  FindOrder( 0, &part );

  /* 
   *  The new memsize does not include this partition, so dividing by the
   *  new memsize will give us the right channel/rank/bank/whatever.
   */
  *partitions[part] = Modulo( refAddress, part );

  /*
   *  "Mask off" the first partition number we got. For example if memsize = 1000
   *  and the address is 8343, the partition would be 8, and we will look at 343 
   *  to determine the rest of the address now.
   */
  refAddress = Divide( refAddress, part );


  /* Next find the 2nd in order, and repeat the process. */
  FindOrder( 1, &part );
  *partitions[part] = Modulo( refAddress, part );
  refAddress = Divide( refAddress, part );

  /* 3rd... */
  FindOrder( 2, &part );
  *partitions[part] = Modulo( refAddress, part );
  refAddress = Divide( refAddress, part );


  /* 4th... */
  FindOrder( 3, &part );
  *partitions[part] = Modulo( refAddress, part );
  refAddress = Divide( refAddress, part );


  /* and 5th... */
  FindOrder( 4, &part );
  *partitions[part] = Modulo( refAddress, part );
  refAddress = Divide( refAddress, part );
} 



uint64_t AddressTranslator::Translate( uint64_t address )
{
  uint64_t row, col, bank, rank, channel;
  uint64_t rv;

  Translate( address, &row, &col, &bank, &rank, &channel );

  switch( defaultField )
    {
      case ROW_FIELD:
        rv = row;
        break;

      case COL_FIELD:
        rv = col;
        break;

      case BANK_FIELD:
        rv = bank;
        break;

      case RANK_FIELD:
        rv = rank;
        break;

      case CHANNEL_FIELD:
        rv = channel;
        break;

      case NO_FIELD:
      default:
        rv = 0;
        break;
    }

  return rv;
}


void AddressTranslator::SetDefaultField( TranslationField f )
{
  defaultField = f;
}




uint64_t AddressTranslator::Divide( uint64_t partSize, MemoryPartition partition )
{
  uint64_t retSize = partSize;
  uint64_t numChannels, numRanks, numBanks, numRows, numCols;

  method->GetCount( &numRows, &numCols, &numBanks, &numRanks, &numChannels );
  
  if( partition == MEM_ROW )
    retSize /= numRows;
  else if( partition == MEM_COL )
    retSize /= numCols;
  else if( partition == MEM_BANK )
    retSize /= numBanks;
  else if( partition == MEM_RANK )
    retSize /= numRanks;
  else if( partition == MEM_CHANNEL )
    retSize /= numChannels;
  else
    std::cout << "Divider Translator: Warning: Invalid partition " << (int)partition << std::endl;

  return retSize;
}


uint64_t AddressTranslator::Modulo( uint64_t partialAddr, MemoryPartition partition )
{
  uint64_t retVal = partialAddr;
  uint64_t numChannels, numRanks, numBanks, numRows, numCols;

  method->GetCount( &numRows, &numCols, &numBanks, &numRanks, &numChannels );
  
  if( partition == MEM_ROW )
    retVal = partialAddr % numRows;
  else if( partition == MEM_COL )
    retVal = partialAddr % numCols;
  else if( partition == MEM_BANK )
    retVal = partialAddr % numBanks;
  else if( partition == MEM_RANK )
    retVal = partialAddr % numRanks;
  else if( partition == MEM_CHANNEL )
    retVal = partialAddr % numChannels;
  else
    std::cout << "Modulo Translator: Warning: Invalid partition " << (int)partition << std::endl;

  return retVal;
}



void AddressTranslator::FindOrder( int order, MemoryPartition *p )
{
  unsigned int rowBits, colBits, bankBits, rankBits, channelBits;
  int rowOrder, colOrder, bankOrder, rankOrder, channelOrder;

  method->GetBitWidths( &rowBits, &colBits, &bankBits, &rankBits, &channelBits );
  method->GetOrder( &rowOrder, &colOrder, &bankOrder, &rankOrder, &channelOrder );

  if( rowOrder == order )
    *p = MEM_ROW;
  else if( colOrder == order )
    *p = MEM_COL;
  else if( bankOrder == order )
    *p = MEM_BANK;
  else if( rankOrder == order )
    *p = MEM_RANK;
  else if( channelOrder == order )
    *p = MEM_CHANNEL;
  else
    std::cerr << "Address Translator: No order " << order << std::endl << "Row = " << rowOrder
	      << " Col = " << colOrder << " Bank = " << bankOrder << " Rank = " << rankOrder
	      << " Chan = " << channelOrder << std::endl;
}
