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

#ifndef __TRANSLATIONMETHOD_H__
#define __TRANSLATIONMETHOD_H__


#include <stdint.h>


namespace NVM {


enum MemoryPartition { MEM_ROW = 0, 
		       MEM_COL = 1, 
		       MEM_BANK = 2, 
		       MEM_RANK = 3, 
		       MEM_CHANNEL = 4 };


class TranslationMethod
{
 public:
  TranslationMethod( );
  ~TranslationMethod( );


  void SetBitWidths( unsigned int rowBits, unsigned int colBits, unsigned int bankBits, 
		     unsigned int rankBits, unsigned int channelBits );
  void SetOrder( int row, int col, int bank, int rank, int channel );
  void SetCount( uint64_t rows, uint64_t cols, uint64_t banks, uint64_t ranks, uint64_t channels );


  void GetBitWidths( unsigned int *rowBits, unsigned int *colBits, unsigned int *bankBits,
		     unsigned int *rankBits, unsigned int *channelBits );
  void GetOrder( int *row, int *col, int *bank, int *rank, int *channel );
  void GetCount( uint64_t *rows, uint64_t *cols, uint64_t *banks, uint64_t *ranks, uint64_t *channels );


 private:
  unsigned int bitWidths[5];
  uint64_t count[5];
  int order[5];

};


};


#endif

