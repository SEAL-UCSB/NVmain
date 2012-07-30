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


#ifndef __NVMNETMESSAGE_H__
#define __NVMNETMESSAGE_H__


#include "include/NVMAddress.h"
#include "include/NVMNetDest.h"
#include "include/NVMNetMsgType.h"


namespace NVM {


enum NVMNetDestType { NVMNETDESTTYPE_UNKNOWN,

                      NVMNETDESTTYPE_MC,
                      NVMNETDESTTYPE_INT,
                      NVMNETDESTTYPE_RANK,
                      NVMNETDESTTYPE_BANK,

                      NVMNETDESTTYPE_COUNT
};

enum NVMNetDirection { NVMNETDIR_CHILD,
                       NVMNETDIR_PARENT,
                       NVMNETDIR_BCAST,

                       NVMNETDIR_COUNT
};


class NVMNetMessage
{
 public:
  NVMNetMessage( );
  ~NVMNetMessage( );

  void SetDestination( NVMNetDest dest );
  void SetAddress( NVMAddress addr );
  void SetMessage( NVMNetMsgType type );
  void SetMessageData( void *data );
  void SetDirection( NVMNetDirection dir );

  NVMNetDest GetDestination( );
  NVMAddress GetAddress( );
  NVMNetMsgType GetMessage( );
  void *GetMessageData( );
  NVMNetDirection GetDirection( );

  NVMNetMessage operator=( NVMNetMessage m );

 private:
  NVMAddress msgAddr;
  NVMNetDest msgDest;
  NVMNetDestType destType;
  NVMNetMsgType msgType;
  NVMNetDirection msgDir;
  void *msgData;

};



inline
NVMNetMessage NVMNetMessage::operator=( NVMNetMessage m )
{
  msgAddr = m.msgAddr;
  msgDest = m.msgDest;
  destType = m.destType;
  msgType = m.msgType;
  msgDir = m.msgDir;
  msgData = m.msgData;

  return *this;
}


};



#endif /* __NVMNETDEST_H__ */


