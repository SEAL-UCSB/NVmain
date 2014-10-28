/*******************************************************************************
* Copyright (c) 2012-2014, The Microsystems Design Labratory (MDL)
* Department of Computer Science and Engineering, The Pennsylvania State University
* All rights reserved.
* 
* This source code is part of NVMain - A cycle accurate timing, bit accurate
* energy simulator for both volatile (e.g., DRAM) and non-volatile memory
* (e.g., PCRAM). The source code is free and you can redistribute and/or
* modify it by providing that the following conditions are met:
* 
*  1) Redistributions of source code must retain the above copyright notice,
*     this list of conditions and the following disclaimer.
* 
*  2) Redistributions in binary form must reproduce the above copyright notice,
*     this list of conditions and the following disclaimer in the documentation
*     and/or other materials provided with the distribution.
* 
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
* 
* Author list: 
*   Matt Poremba    ( Email: mrp5060 at psu dot edu 
*                     Website: http://www.cse.psu.edu/~poremba/ )
*   Tao Zhang       ( Email: tzz106 at cse dot psu dot edu
*                     Website: http://www.cse.psu.edu/~tzz106 )
*******************************************************************************/

#ifndef __CYCLER_H__
#define __CYCLER_H__

#include "include/NVMTypes.h"
#include "include/FailReasons.h"
#include "Decoders/DecoderFactory.h"
#include "src/Stats.h"
#include "src/TagGenerator.h"

#include <ostream>
#include <vector>
#include <typeinfo>

#define NVMObjectType (typeid(*(parent->GetTrampoline())).name())
#define NVMClass(a) (typeid(a).name())
#define NVMTypeMatches(a) ((dynamic_cast<a *>(parent->GetTrampoline())) != NULL)

// a = NVMainRequest*, b=Class Name
#define FindChild(a,b) (dynamic_cast<b *>(_FindChild(a,typeid(b).name())))

// a = NVMainRequest*, b=Class Name, c=NVMObject*
#define FindChildType(a,b,c) FindModuleChildType(a,b,c,this)

// a = NVMainRequest*, b=Class Name, c=NVMObject*, d=NVMObject* (start module)
#define FindModuleChildType(a,b,c,d)                                         \
{                                                                            \
    NVMObject *curChild = d;                                                 \
                                                                             \
    while( curChild != NULL && dynamic_cast<b *>(curChild) == NULL )         \
    {                                                                        \
        NVMObject_hook *curHook = curChild->GetChild( a );                   \
                                                                             \
        if( curHook == NULL )                                                \
        {                                                                    \
            c = NULL;                                                        \
            break;                                                           \
        }                                                                    \
                                                                             \
        curChild = curHook->GetTrampoline();                                 \
    }                                                                        \
                                                                             \
    if( dynamic_cast<b *>(curChild) != NULL )                                \
    {                                                                        \
        c = curChild;                                                        \
    }                                                                        \
    else                                                                     \
    {                                                                        \
        c = NULL;                                                            \
    }                                                                        \
}

namespace NVM {

class NVMainRequest;
class EventQueue;
class GlobalEventQueue;
class AddressTranslator;
class NVMObject;
class Config;
class Params;

enum HookType { NVMHOOK_NONE = 0,
                NVMHOOK_PREISSUE,                /* Call hook before IssueCommand */
                NVMHOOK_POSTISSUE,               /* Call hook after IssueCommand */
                NVMHOOK_COUNT,                   /* Number of UNIQUE hook types. */
                NVMHOOK_BOTHISSUE                /* Call hook before and after */
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
    virtual ~NVMObject_hook( );

    bool IssueCommand( NVMainRequest *req );
    bool IsIssuable( NVMainRequest *req, FailReason *reason = NULL );
    bool IssueAtomic( NVMainRequest *req );
    bool IssueFunctional( NVMainRequest *req );
    void Notify( NVMainRequest *req );
    ncycle_t NextIssuable( NVMainRequest *req );
    virtual bool Idle( );
    virtual bool Drain( );

    bool RequestComplete( NVMainRequest *req );
    void Callback( void *data );

    void Cycle( ncycle_t steps );

    void CalculateStats( );
    void ResetStats( );

    void PrintHierarchy( int depth );

    void SetStats( Stats* );
    Stats* GetStats( );
    void RegisterStats( );

    void StatName( std::string name );
    std::string StatName( );

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
    virtual ~NVMObject(  );

    virtual void Init( Config *conf ); 

    virtual void Cycle( ncycle_t steps ) = 0;

    virtual bool IssueCommand( NVMainRequest *req );
    virtual bool IsIssuable( NVMainRequest *req, FailReason *reason = NULL );
    virtual bool IssueAtomic( NVMainRequest *req );
    virtual bool IssueFunctional( NVMainRequest *req );
    virtual void Notify( NVMainRequest *req );
    virtual ncycle_t NextIssuable( NVMainRequest *req );
    virtual bool Idle( );
    virtual bool Drain( );

    virtual bool RequestComplete( NVMainRequest *req );
    virtual void Callback( void *data );

    virtual void SetParent( NVMObject *p );
    void UnsetParent( );
    virtual void AddChild( NVMObject *c ); 
    NVMObject *_FindChild( NVMainRequest *req, const char *childClass );

    virtual void SetEventQueue( EventQueue *eq );
    virtual EventQueue *GetEventQueue( );
    virtual void SetGlobalEventQueue( GlobalEventQueue *geq );
    virtual GlobalEventQueue *GetGlobalEventQueue( );

    NVMObject_hook *GetParent( );
    std::vector<NVMObject_hook *>& GetChildren( );
    NVMObject_hook *GetChild( NVMainRequest *req );  
    NVMObject_hook *GetChild( ncounter_t child );
    NVMObject_hook *GetChild( );
    ncounter_t GetChildId( NVMObject *c );
    ncounter_t GetChildCount( );

    virtual void SetDecoder( AddressTranslator *at );
    virtual AddressTranslator *GetDecoder( );

    virtual void CalculateStats( );
    virtual void ResetStats( );

    virtual void CreateCheckpoint( std::string dir );
    virtual void RestoreCheckpoint( std::string dir );

    void PrintHierarchy( int depth = 0 );

    void SetStats( Stats* );
    Stats* GetStats( );
    virtual void RegisterStats( );

    void SetParams( Params *params );
    Params *GetParams( );

    void StatName( std::string name );
    std::string StatName( );

    void SetTagGenerator( TagGenerator *tg );
    TagGenerator *GetTagGenerator( );

    HookType GetHookType( );
    void SetHookType( HookType );

    HookType GetCurrentHookType( );
    void SetCurrentHookType( HookType );

    void AddHook( NVMObject *hook );
    std::vector<NVMObject *>& GetHooks( HookType h );

    virtual void SetDebugName( std::string dn, Config *config );

  protected:
    NVMObject_hook *parent;
    AddressTranslator *decoder;
    Stats *stats;
    Params *p;
    std::string statName;
    std::vector<NVMObject_hook *> children;
    std::vector<NVMObject *> *hooks;
    EventQueue *eventQueue;
    GlobalEventQueue *globalEventQueue;
    std::ostream *debugStream;
    TagGenerator *tagGen;
    HookType hookType, currentHookType;

    void AddHookUnique( std::vector<NVMObject *>& list, NVMObject *hook );

    ncycle_t MAX( const ncycle_t, const ncycle_t );
    ncycle_t MIN( const ncycle_t, const ncycle_t );
};

};

#endif
