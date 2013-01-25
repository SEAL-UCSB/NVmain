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

#ifndef __ADDRESSTRANSLATOR_H__
#define __ADDRESSTRANSLATOR_H__



#include "src/TranslationMethod.h"


namespace NVM {


typedef enum { NO_FIELD, ROW_FIELD, COL_FIELD, BANK_FIELD, RANK_FIELD, CHANNEL_FIELD } TranslationField;


class AddressTranslator
{
 public:
  AddressTranslator( );
  virtual ~AddressTranslator( );

  void SetTranslationMethod( TranslationMethod *m );
  TranslationMethod *GetTranslationMethod( );
  
  virtual void Translate( uint64_t address, uint64_t *row, uint64_t *col, uint64_t *bank, uint64_t *rank, uint64_t *channel );

  uint64_t Translate( uint64_t address );
  void SetDefaultField( TranslationField f ); 

  virtual void PrintStats( ) { }

 private:
  TranslationMethod *method;
  TranslationField defaultField;

 protected:
  uint64_t Divide( uint64_t partSize, MemoryPartition partition );
  uint64_t Modulo( uint64_t partialAddr, MemoryPartition partition );
  void FindOrder( int order, MemoryPartition *p );
};


};


#endif


