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


#include "Prefetchers/PrefetcherFactory.h"

#include <iostream>


/* Add your prefetcher's include file below. */
#include "Prefetchers/NaivePrefetcher/NaivePrefetcher.h"
#include "Prefetchers/STeMS/STeMS.h"


using namespace NVM;


Prefetcher *PrefetcherFactory::CreateNewPrefetcher( std::string name )
{
  Prefetcher *prefetcher = NULL;


  if( name == "NaivePrefetcher" ) prefetcher = new NaivePrefetcher( );
  if( name == "STeMS" ) prefetcher = new STeMS( );


  /*
   *  If prefetcher isn't found, default to the NULL prefetcher.
   */
  if( prefetcher == NULL )
    {
      prefetcher = new Prefetcher( );
      
      std::cout << "Could not find prefetcher named `" << name << "'. Using default prefetcher." << std::endl;
    }


  return prefetcher;
}

