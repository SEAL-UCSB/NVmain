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

#ifndef __INCLUDE_NVMTYPES_H__
#define __INCLUDE_NVMTYPES_H__


#include <stdint.h>
#include <deque>
#include <list>


namespace NVM {

class NVMainRequest;

typedef uint64_t  ncycle_t;
typedef int64_t   ncycles_t;

typedef uint64_t  ncounter_t;


typedef std::list<NVMainRequest *> NVMTransactionQueue;
typedef std::deque<NVMainRequest *> NVMCommandQueue;

};



#endif 

