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

#ifndef __DECODERFACTORY_H__
#define __DECODERFACTORY_H__

#include "src/AddressTranslator.h"
#include "Decoders/DRCDecoder/DRCDecoder.h"


#include <string>


namespace NVM {


class DecoderFactory
{
 public:
  DecoderFactory( ) { }
  ~DecoderFactory( ) { }

  static AddressTranslator *CreateNewDecoder( std::string decoder );

};


};



#endif

