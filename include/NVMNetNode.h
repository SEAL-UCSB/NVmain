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


#ifndef __NVMNETNODE_H__
#define __NVMNETNODE_H__


#include "include/NVMNetMessage.h"

#include <stdint.h>


namespace NVM {


class NVMNetNode
{
 public:
  NVMNetNode( ) { nodeType = NVMNETDESTTYPE_UNKNOWN; nodeChannel = nodeRank = nodeBank = 0; }
  NVMNetNode( NVMNetDestType type, uint64_t channel, uint64_t rank, uint64_t bank ) { nodeType = type; nodeChannel = channel; nodeRank = rank; nodeBank = bank; }
  ~NVMNetNode( ) { }


  void SetNodeType( NVMNetDestType type ) { nodeType = type; }
  void SetNodeChannel( uint64_t channel ) { nodeChannel = channel; }
  void SetNodeRank( uint64_t rank ) { nodeRank = rank; }
  void SetNodebank( uint64_t bank ) { nodeBank = bank; }


  NVMNetDestType GetNodeType( ) { return nodeType; }
  uint64_t GetNodeChannel( ) { return nodeChannel; }
  uint64_t GetNodeRank( ) { return nodeRank; }
  uint64_t GetNodeBank( ) { return nodeBank; }


 private:
  NVMNetDestType nodeType;
  uint64_t nodeChannel;
  uint64_t nodeRank;
  uint64_t nodeBank;

};



};


#endif /* __NVMNETNODE_H__ */

