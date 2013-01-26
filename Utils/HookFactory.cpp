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

#include "Utils/HookFactory.h"

/* Add your hook's include files here.*/
#include "Utils/Visualizer/Visualizer.h"


using namespace NVM;


NVMObject *HookFactory::CreateHook( std::string hookName )
{
  NVMObject *hook = NULL;

  if( hookName == "Visualizer" ) hook = new Visualizer( );
  //else if( hookName == "MyHook" ) hook = new MyHook( );
  
  return hook;
}

