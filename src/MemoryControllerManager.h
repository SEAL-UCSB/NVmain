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

#ifndef __MEMORYCONTROLLERMANAGER_H__
#define __MEMORYCONTROLLERMANAGER_H__



#include <vector>


#include "src/NVMObject.h"
#include "src/Config.h"
#include "src/MemoryControllerMessage.h"


#define MSG_NOT_FOUND 0
#define MSG_FOUND     1


namespace NVM {


class MemoryController;


class MemoryControllerManager : public NVMObject
{
 public:
  MemoryControllerManager( );
  ~MemoryControllerManager( );

  
  void SendMessage( MemoryControllerMessage *msg );
  int  RecvMessage( MemoryControllerMessage *msg );
  

  void AddController( MemoryController *controller );


  void SetConfig( Config *conf );
  Config *GetConfig( );
  

  void Cycle( );

 private:
  Config *config;

  std::vector< MemoryController * > controllers;
  std::vector< MemoryControllerMessage * > messages;

};


};


#endif

