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
#include "src/AddressTranslator.h"
#include "src/Rank.h"


using namespace NVM;


NVMObject_hook::NVMObject_hook( NVMObject *t )
{
  trampoline = t;
}


bool NVMObject_hook::IssueCommand( NVMainRequest *req )
{
  bool rv;
  std::vector<NVMObject *>& preHooks  = trampoline->GetHooks( NVMHOOK_PREISSUE );
  std::vector<NVMObject *>& postHooks = trampoline->GetHooks( NVMHOOK_POSTISSUE );
  std::vector<NVMObject *>::iterator it;

  /* Call pre-issue hooks */
  for( it = preHooks.begin(); it != preHooks.end(); it++ )
    {
      (*it)->SetParent( trampoline );
      (*it)->IssueCommand( req );
    }

  /* Call IssueCommand. */
  rv = trampoline->IssueCommand( req );

  /* Call post-issue hooks. */
  for( it = postHooks.begin(); it != postHooks.end(); it++ )
    {
      (*it)->SetParent( trampoline );
      (*it)->IssueCommand( req );
    }

  return rv;
}


bool NVMObject_hook::IsIssuable( NVMainRequest *req, FailReason *reason )
{
  return trampoline->IsIssuable( req, reason );
}



bool NVMObject_hook::IssueAtomic( NVMainRequest *req )
{
  bool rv;
  std::vector<NVMObject *>& preHooks  = trampoline->GetHooks( NVMHOOK_PREISSUE );
  std::vector<NVMObject *>& postHooks = trampoline->GetHooks( NVMHOOK_POSTISSUE );
  std::vector<NVMObject *>::iterator it;

  /* Call pre-issue hooks */
  for( it = preHooks.begin(); it != preHooks.end(); it++ )
    {
      (*it)->SetParent( trampoline );
      (*it)->IssueAtomic( req );
    }

  /* Call IssueCommand. */
  rv = trampoline->IssueAtomic( req );

  /* Call post-issue hooks. */
  for( it = postHooks.begin(); it != postHooks.end(); it++ )
    {
      (*it)->SetParent( trampoline );
      (*it)->IssueAtomic( req );
    }

  return rv;
}



bool NVMObject_hook::RequestComplete( NVMainRequest *req )
{
  bool rv;
  std::vector<NVMObject *>& preHooks  = trampoline->GetHooks( NVMHOOK_PREISSUE );
  std::vector<NVMObject *>& postHooks = trampoline->GetHooks( NVMHOOK_POSTISSUE );
  std::vector<NVMObject *>::iterator it;

  /* Call pre-complete hooks */
  for( it = preHooks.begin(); it != preHooks.end(); it++ )
    {
      (*it)->SetParent( trampoline->GetChild( req )->GetTrampoline( ) );
      (*it)->RequestComplete( req );
    }

  /* Call IssueCommand. */
  rv = trampoline->RequestComplete( req );

  /* Call post-complete hooks. */
  for( it = postHooks.begin(); it != postHooks.end(); it++ )
    {
      (*it)->SetParent( trampoline->GetChild( req )->GetTrampoline( ) );
      (*it)->RequestComplete( req );
    }

  return rv;
}



NVMObject *NVMObject_hook::GetTrampoline( )
{
  return trampoline;
}



NVMObject::NVMObject( )
{
  parent = NULL;
  decoder = NULL;
  children.clear( );
  eventQueue = NULL;
  hookType = NVMHOOK_NONE;
  hooks = new std::vector<NVMObject *> [NVMHOOK_COUNT];
}


void NVMObject::Init( )
{

}


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
  NVMObject_hook *hook = new NVMObject_hook( p );

  parent = hook;
  SetEventQueue( p->GetEventQueue( ) );
}


void NVMObject::AddChild( NVMObject *c )
{
  NVMObject_hook *hook = new NVMObject_hook( c );
  std::vector<NVMObject *>::iterator it;

  /*
   *  Copy all hooks from the parent to the child NVMObject.
   */
  for( int i = 0; i < static_cast<int>(NVMHOOK_COUNT); i++ )
    {
      for( it = hooks[i].begin(); it != hooks[i].end(); it++ )
        {
          c->AddHook( (*it) );
        }
    }

  children.push_back( hook );
}



NVMObject_hook* NVMObject::GetParent( )
{
  return parent;
}


std::vector<NVMObject_hook *>& NVMObject::GetChildren( )
{
  return children;
}


NVMObject_hook *NVMObject::GetChild( NVMainRequest *req )
{
  /*
   * Use the specified decoder to choose the correct child. 
   */
  uint64_t child;

  child = GetDecoder( )->Translate( req->address.GetPhysicalAddress( ) );

  return children[child];
}


void NVMObject::SetDecoder( AddressTranslator *at )
{
  decoder = at;
}


AddressTranslator *NVMObject::GetDecoder( )
{
  return decoder;
}


HookType NVMObject::GetHookType( )
{
  return hookType;
}


void NVMObject::SetHookType( HookType h )
{
  hookType = h;
}


void NVMObject::AddHook( NVMObject *hook )
{
  HookType h = hook->GetHookType( );

  hooks[h].push_back( hook );
}


std::vector<NVMObject *>& NVMObject::GetHooks( HookType h )
{
  return hooks[h];
}

/*
void NVMObject::CallHooks( HookType h )
{
  std::vector<NVMObject *>& hookList = trampoline->GetHooks( h );
  std::vector<NVMObject *>::iterator it;

  for( it = hookList.begin(); it != hookList.end(); it++ )
    {
      (*it)->SetParent( trampoline );
      (*it)->RequestComplete( req );
    }
}
*/

ncycle_t NVMObject::MAX( const ncycle_t a, const ncycle_t b )
{
    return (( a > b ) ? a : b );
}
