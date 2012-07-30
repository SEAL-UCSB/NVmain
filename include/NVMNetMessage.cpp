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


#include "include/NVMNetMessage.h"
#include "include/NVMNetDest.h"

#include <iostream>


using namespace NVM;




NVMNetMessage::NVMNetMessage( )
{
  msgAddr.SetPhysicalAddress( 0 );
  destType = NVMNETDESTTYPE_UNKNOWN;
  msgType = NVMNETMSGTYPE_UNKNOWN;
  msgData = NULL;
}


NVMNetMessage::~NVMNetMessage( )
{

}



void NVMNetMessage::SetDestination( NVMNetDest dest )
{
  msgDest = dest;

  switch( dest )
    {
      case NVMNETDEST_LOCAL_MC:
      case NVMNETDEST_REMOTE_MC:
      case NVMNETDEST_ALL_MC:
        destType = NVMNETDESTTYPE_MC;
        break;

      case NVMNETDEST_REMOTE_RANK:
        destType = NVMNETDESTTYPE_RANK;
        break;

      case NVMNETDEST_REMOTE_BANK:
        destType = NVMNETDESTTYPE_BANK;
        break;

      default:
        std::cout << "Warning: Unknown message type `" << dest
                  << "'!" << std::endl;
        break;
    }
}



void NVMNetMessage::SetAddress( NVMAddress addr )
{
  msgAddr = addr;
}



void NVMNetMessage::SetMessage( NVMNetMsgType type )
{
  msgType = type;
}



void NVMNetMessage::SetMessageData( void *data )
{
  msgData = data;
}



void NVMNetMessage::SetDirection( NVMNetDirection dir )
{
  msgDir = dir;
}


NVMNetDest NVMNetMessage::GetDestination( )
{
  return msgDest;
}


NVMAddress NVMNetMessage::GetAddress( )
{
  return msgAddr;
}


NVMNetMsgType NVMNetMessage::GetMessage( )
{
  return msgType;
}


void *NVMNetMessage::GetMessageData( )
{
  return msgData;
}


NVMNetDirection NVMNetMessage::GetDirection( )
{
  return msgDir;
}
