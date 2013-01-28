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

#ifndef __CYCLER_H__
#define __CYCLER_H__


#include "include/NVMTypes.h"
#include "include/FailReasons.h"
#include "src/EventQueue.h"
#include "Decoders/DecoderFactory.h"

#include <vector>
#include <typeinfo>



#define NVMObjectType (typeid(*(parent->GetTrampoline())).name())
#define NVMClass(a) (typeid(a).name())
#define HookedConfig (static_cast<NVMain *>(GetParent( )->GetTrampoline( ))->GetConfig( ))


namespace NVM {


class NVMainRequest;
class EventQueue;
class AddressTranslator;
class NVMObject;


enum HookType { NVMHOOK_NONE = 0,
                NVMHOOK_PREISSUE,                /* Call hook before IssueCommand */
                NVMHOOK_POSTISSUE,               /* Call hook after IssueCommand */
                NVMHOOK_COUNT
};


/*
 *  This class is used to hook IssueCommands to any NVMObject so that pre- and post-commands may be called.
 *  This is useful for implementing some debugging constructs as well as visualization and more powerful
 *  energy calculation.
 */
class NVMObject_hook
{
 public:
  NVMObject_hook( NVMObject *trampoline );

  bool IssueCommand( NVMainRequest *req );
  bool IsIssuable( NVMainRequest *req, FailReason *reason = NULL );
  bool IssueAtomic( NVMainRequest *req );

  bool RequestComplete( NVMainRequest *req );

  NVMObject *GetTrampoline( );

 private:
  NVMObject *trampoline;
};


/*
 *  Generic base class for all simulator classes. The cycle function is called
 *  at each simulation step (at each clock cycle).
 */
class NVMObject
{
 public:
  NVMObject( );
  virtual ~NVMObject(  ) { }

  virtual void Init( ); 

  virtual void Cycle( ncycle_t steps ) = 0;

  virtual bool IssueCommand( NVMainRequest *req );
  virtual bool IsIssuable( NVMainRequest *req, FailReason *reason = NULL );
  virtual bool IssueAtomic( NVMainRequest *req );

  virtual bool RequestComplete( NVMainRequest *req );

  virtual void SetParent( NVMObject *p );
  virtual void AddChild( NVMObject *c ); 

  virtual void SetEventQueue( EventQueue *eq );
  virtual EventQueue *GetEventQueue( );

  NVMObject_hook *GetParent( );
  std::vector<NVMObject_hook *>& GetChildren( );
  NVMObject_hook *GetChild( NVMainRequest *req );  

  virtual void SetDecoder( AddressTranslator *at );
  virtual AddressTranslator *GetDecoder( );

  HookType GetHookType( );
  void SetHookType( HookType );

  void AddHook( NVMObject *hook );
  std::vector<NVMObject *>& GetHooks( HookType h );

 protected:
  NVMObject_hook *parent;
  AddressTranslator *decoder;
  std::vector<NVMObject_hook *> children;
  std::vector<NVMObject *> *hooks;
  EventQueue *eventQueue;
  HookType hookType;
  /*
   * added by Tao @ 01/28/2013
   * it is better to implement MAX as a function, the original Macro defined
   * by "#define" may cause problem 
   */
  ncycle_t MAX( const ncycle_t, const ncycle_t );

};


};



#endif

