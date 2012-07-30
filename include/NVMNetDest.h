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


#ifndef __NVMNETDEST_H__
#define __NVMNETDEST_H__


namespace NVM {


enum NVMNetDest { NVMNETDEST_UNKNOWN = 0,

                  NVMNETDEST_LOCAL_MC,
                  NVMNETDEST_REMOTE_MC,
                  NVMNETDEST_ALL_MC,

                  NVMNETDEST_REMOTE_RANK,
                  NVMNETDEST_REMOTE_BANK,

                  NVMNETDEST_COUNT
};


};


#endif /* __NVMNETDEST_H__ */


