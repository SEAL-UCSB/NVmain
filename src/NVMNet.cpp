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


#include "src/NVMNet.h"


using namespace NVM;




NVMNet::NVMNet( )
{
  parents.clear( );
  parentNodes.clear( );
  children.clear( );
  childNodes.clear( );
}


NVMNet::~NVMNet( )
{

}


void NVMNet::AddParent( NVMNet *parent, NVMNetNode *node )
{
  parents.push_back( parent );
  parentNodes.push_back( node );
}


void NVMNet::AddChild( NVMNet *child, NVMNetNode *node )
{
  children.push_back( child );
  childNodes.push_back( node );
}



void NVMNet::SendMessage( NVMNetMessage *msg )
{
  /*
   *  Forward this message down the path to other NVMNet nodes
   */
  if( msg->GetDirection( ) == NVMNETDIR_CHILD )
    {
      for( uint64_t i = 0; i < children.size( ); i++ )
        children[i]->SendMessage( msg );
    }
  else if( msg->GetDirection( ) == NVMNETDIR_PARENT )
    {
      for( uint64_t i = 0; i < parents.size( ); i++ )
        parents[i]->SendMessage( msg );
    }
  else if( msg->GetDirection( ) == NVMNETDIR_BCAST )
    {
      NVMNetMessage *cmsg = new NVMNetMessage( );
      NVMNetMessage *pmsg = new NVMNetMessage( );

      *cmsg = *msg;
      *pmsg = *msg;

      pmsg->SetDirection( NVMNETDIR_PARENT );
      cmsg->SetDirection( NVMNETDIR_CHILD );

      for( uint64_t i = 0; i < children.size( ); i++ )
        children[i]->SendMessage( cmsg );
      for( uint64_t i = 0; i < parents.size( ); i++ )
        parents[i]->SendMessage( pmsg );
    }

  /*
   *  See if we are interested in this message. If so, call the callback.
   */
  RecvMessage( msg );
}


