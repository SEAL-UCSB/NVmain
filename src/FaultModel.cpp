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

#include "src/FaultModel.h"


using namespace NVM;





FaultModel::FaultModel( )
{
  config = NULL;
}


FaultModel::~FaultModel( )
{

}


void FaultModel::SetConfig( Config *conf )
{
  config = conf;
}


Config *FaultModel::GetConfig( )
{
  return config;
}


bool FaultModel::Fault( NVMAddress /*faultAddr*/ )
{
  /*
   *  Default to no hard-error modelling, therefore we can't
   *  fix any faults. Just return false.
   */
  return false;
}


void FaultModel::Cycle( ncycle_t )
{

}



