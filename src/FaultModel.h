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

#ifndef __FAULTMODEL_H__
#define __FAULTMODEL_H__



#include "src/Config.h"
#include "src/NVMObject.h"
#include "include/NVMAddress.h"


namespace NVM {


class EnduranceModel;

class FaultModel : public NVMObject
{
 public:
  FaultModel( );
  ~FaultModel ( );

  /* Fault returns true if the fault could be fixed. */
  virtual bool Fault( NVMAddress faultAddr ); 


  virtual void SetConfig( Config *conf );
  Config *GetConfig( );

  void Cycle( ncycle_t steps );


 protected:
  Config *config;
  EnduranceModel *endurance;

};


};


#endif


