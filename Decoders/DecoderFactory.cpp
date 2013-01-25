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


#include "Decoders/DecoderFactory.h"

#include <iostream>

/* Add your decoder's include file below. */
#include "Decoders/DRCDecoder/DRCDecoder.h"

using namespace NVM;




AddressTranslator *DecoderFactory::CreateDecoder( std::string decoder )
{
  AddressTranslator *trans = NULL;

  if( decoder == "DRCDecoder" ) trans = new DRCDecoder( );

  return trans;
}



AddressTranslator *DecoderFactory::CreateNewDecoder( std::string decoder )
{
  AddressTranslator *trans = NULL;

  trans = CreateDecoder( decoder );

  /*
   *  If decoder isn't found, default to the regular address translator.
   */
  if( trans == NULL )
    {
      trans = new AddressTranslator( );
      
      std::cout << "Could not find Decoder named `" << decoder << "'. Using default decoder." << std::endl;
    }


  return trans;
}


AddressTranslator *DecoderFactory::CreateDecoderNoWarn( std::string decoder )
{
  AddressTranslator *trans = NULL;

  trans = CreateDecoder( decoder );

  if( trans == NULL ) trans = new AddressTranslator( );

  return trans;
}



