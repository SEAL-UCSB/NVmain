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

#include "Endurance/EnduranceModelFactory.h"

#include <iostream>


using namespace NVM;



EnduranceModel *EnduranceModelFactory::CreateEnduranceModel( std::string modelName )
{
  EnduranceModel *enduranceModel = NULL;

  if( modelName == "" )
    std::cout << "NVMain: EnduranceModel is not set in configuration file!\n";

  if( modelName == "RowModel" ) enduranceModel = new RowModel( );
  /*
   *  Add your custom endurance model here, for example:
   *
   *  else if( modelName == "MyModel" ) enduranceModel = new MyModel( );
   */
  else if( modelName == "WordModel" ) enduranceModel = new WordModel( );
  else if( modelName == "ByteModel" ) enduranceModel = new ByteModel( );
  else if( modelName == "BitModel"  ) enduranceModel = new BitModel( );
  else if( modelName == "FlipNWrite" ) enduranceModel = new FlipNWrite( );
  else if( modelName == "NullModel" ) enduranceModel = new NullModel( );


  if( enduranceModel == NULL )
    std::cout << "NVMain: Endurance model " << modelName << " not found in factory. Endurance will not be modelled.\n";

  return enduranceModel;
}


