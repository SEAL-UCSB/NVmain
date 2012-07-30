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

#ifndef __ENDURANCEMODELFACTORY_H__
#define __ENDURANCEMODELFACTORY_H__


#include "Endurance/RowModel/RowModel.h"
/*
 *  #include your custom endurance model here, for example:
 *
 *  #include "MyModel/MyModel.h"
 */
#include "Endurance/WordModel/WordModel.h"
#include "Endurance/ByteModel/ByteModel.h"
#include "Endurance/BitModel/BitModel.h"
#include "Endurance/FlipNWrite/FlipNWrite.h"
#include "Endurance/NullModel/NullModel.h"


namespace NVM {


class EnduranceModelFactory
{
 public:
  EnduranceModelFactory( ) {}
  ~EnduranceModelFactory( ) {}

  static EnduranceModel *CreateEnduranceModel( std::string modelName );
};


};


#endif
