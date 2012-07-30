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


#ifndef __NVMNET_H__
#define __NVMNET_H__


#include "include/NVMNetNode.h"

#include <vector>
#include <map>


namespace NVM {


class NVMNet
{
 public:
  NVMNet( );
  virtual ~NVMNet( );

  void AddParent( NVMNet *parent, NVMNetNode *node );
  void AddChild( NVMNet *child, NVMNetNode *node );

  void SendMessage( NVMNetMessage *msg );
  virtual void RecvMessage( NVMNetMessage *msg ) = 0;

 private:
  std::vector< NVMNet* > parents;
  std::vector< NVMNetNode* > parentNodes;
  std::vector< NVMNet* > children;
  std::vector< NVMNetNode* > childNodes;

};


};



#endif /* __NVMNET_H__ */



