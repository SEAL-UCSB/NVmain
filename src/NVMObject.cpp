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


#include "src/NVMObject.h"
#include "include/NVMainRequest.h"
#include "src/EventQueue.h"


using namespace NVM;



bool NVMObject::IssueAtomic( NVMainRequest * )
{
  return true;
}


bool NVMObject::IssueCommand( NVMainRequest * )
{
  return false;
}



bool NVMObject::IsIssuable( NVMainRequest *, FailReason * )
{
  return true;
}



bool NVMObject::RequestComplete( NVMainRequest *request )
{
  bool rv = false;

  /*
   *  By default, just tell the issuing controller the request has been completed
   *  as soon as it arrives on the interconnect.
   */
  if( request->owner == this )
    {
      delete request;
      rv = true;
    }
  else
    {
      /*
       *  If you get a segfault here, check to make sure your request has an
       *  owner that is set!
       *
       *  In GDB: print request->owner
       *
       *  Should not be 0x0!
       */
      rv = GetParent( )->RequestComplete( request );
    }

  return rv;
}


void NVMObject::SetEventQueue( EventQueue *eq )
{
  eventQueue = eq;
}


EventQueue *NVMObject::GetEventQueue( )
{
  return eventQueue;
}


void NVMObject::SetParent( NVMObject *p )
{
  parent = p;
  SetEventQueue( p->GetEventQueue( ) );
}


void NVMObject::AddChild( NVMObject *c )
{
  children.push_back( c );
}



NVMObject* NVMObject::GetParent( )
{
  return parent;
}


std::vector<NVMObject *>& NVMObject::GetChildren( )
{
  return children;
}


