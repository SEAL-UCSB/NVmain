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

#ifndef __INCLUDE_FAIL_REASONS_H__
#define __INCLUDE_FAIL_REASONS_H__



namespace NVM {


enum FailReasons { UNKNOWN_FAILURE,
                   OPEN_REFRESH_WAITING,
                   CLOSED_REFRESH_WAITING,
                   BANK_TIMING,
                   RANK_TIMING
                 };

class FailReason
{
 public:
  FailReason( ) : reason(UNKNOWN_FAILURE) { }
  ~FailReason( ) { }

  FailReasons reason;
};


};



#endif


