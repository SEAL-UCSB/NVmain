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

#include "src/NVMObject.h"
#include "include/NVMainRequest.h"
#include "src/EventQueue.h"
#include "src/AddressTranslator.h"
#include "src/Rank.h"
#include "src/Debug.h"

#include <cassert>
#include <algorithm>

using namespace NVM;

NVMObject_hook::NVMObject_hook( NVMObject *t )
{
    trampoline = t;
}

NVMObject_hook::~NVMObject_hook( )
{
}

bool NVMObject_hook::IssueCommand( NVMainRequest *req )
{
    bool rv = true, dropRequest = false;
    std::vector<NVMObject *>& preHooks  = trampoline->GetHooks( NVMHOOK_PREISSUE );
    std::vector<NVMObject *>& postHooks = trampoline->GetHooks( NVMHOOK_POSTISSUE );
    std::vector<NVMObject *>::iterator it;

    /* Call pre-issue hooks */
    for( it = preHooks.begin(); it != preHooks.end(); it++ )
    {
        (*it)->SetParent( trampoline );
        (*it)->SetCurrentHookType( NVMHOOK_PREISSUE );
        dropRequest = !(*it)->IssueCommand( req );
        (*it)->UnsetParent( );
    }

    /* Call IssueCommand. */
    if( !dropRequest )
        rv = trampoline->IssueCommand( req );

    /* Call post-issue hooks. */
    for( it = postHooks.begin(); it != postHooks.end(); it++ )
    {
        (*it)->SetParent( trampoline );
        (*it)->SetCurrentHookType( NVMHOOK_POSTISSUE );
        (*it)->IssueCommand( req );
        (*it)->UnsetParent( );
    }

    return rv;
}

bool NVMObject_hook::IsIssuable( NVMainRequest *req, FailReason *reason )
{
    return trampoline->IsIssuable( req, reason );
}

bool NVMObject_hook::IssueAtomic( NVMainRequest *req )
{
    bool rv = true, dropRequest = false;
    std::vector<NVMObject *>& preHooks  = trampoline->GetHooks( NVMHOOK_PREISSUE );
    std::vector<NVMObject *>& postHooks = trampoline->GetHooks( NVMHOOK_POSTISSUE );
    std::vector<NVMObject *>::iterator it;

    /* Call pre-issue hooks */
    for( it = preHooks.begin(); it != preHooks.end(); it++ )
    {
        (*it)->SetParent( trampoline );
        (*it)->SetCurrentHookType( NVMHOOK_PREISSUE );
        dropRequest = !(*it)->IssueAtomic( req );
        (*it)->UnsetParent( );
    }

    /* Call IssueCommand. */
    if( !dropRequest )
        rv = trampoline->IssueAtomic( req );

    /* Call post-issue hooks. */
    for( it = postHooks.begin(); it != postHooks.end(); it++ )
    {
        (*it)->SetParent( trampoline );
        (*it)->SetCurrentHookType( NVMHOOK_POSTISSUE );
        (*it)->IssueAtomic( req );
        (*it)->UnsetParent( );
    }

    return rv;
}

bool NVMObject_hook::IssueFunctional( NVMainRequest *req )
{
    bool rv = true, dropRequest = false;
    std::vector<NVMObject *>& preHooks  = trampoline->GetHooks( NVMHOOK_PREISSUE );
    std::vector<NVMObject *>& postHooks = trampoline->GetHooks( NVMHOOK_POSTISSUE );
    std::vector<NVMObject *>::iterator it;

    /* Call pre-issue hooks */
    for( it = preHooks.begin(); it != preHooks.end(); it++ )
    {
        (*it)->SetParent( trampoline );
        (*it)->SetCurrentHookType( NVMHOOK_PREISSUE );
        dropRequest = !(*it)->IssueAtomic( req );
        (*it)->UnsetParent( );
    }

    /* Call IssueCommand. */
    if( !dropRequest )
        rv = trampoline->IssueFunctional( req );

    /* Call post-issue hooks. */
    for( it = postHooks.begin(); it != postHooks.end(); it++ )
    {
        (*it)->SetParent( trampoline );
        (*it)->SetCurrentHookType( NVMHOOK_POSTISSUE );
        (*it)->IssueAtomic( req );
        (*it)->UnsetParent( );
    }

    return rv;
}

ncycle_t NVMObject_hook::NextIssuable( NVMainRequest *req )
{
    return trampoline->NextIssuable( req );
}

bool NVMObject_hook::Idle( )
{
    return trampoline->Idle( );
}

bool NVMObject_hook::Drain( )
{
    return trampoline->Drain( );
}

void NVMObject_hook::Notify( NVMainRequest *req )
{
    trampoline->Notify( req );
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
        //(*it)->SetParent( trampoline->GetChild( req )->GetTrampoline( ) );
        (*it)->SetParent( trampoline );
        (*it)->SetCurrentHookType( NVMHOOK_PREISSUE );
        (*it)->RequestComplete( req );
        (*it)->UnsetParent( );
    }

    /* Call post-complete hooks -- Need to call here in case req is deleted. */
    for( it = postHooks.begin(); it != postHooks.end(); it++ )
    {
        //(*it)->SetParent( trampoline->GetChild( req )->GetTrampoline( ) );
        (*it)->SetParent( trampoline );
        (*it)->SetCurrentHookType( NVMHOOK_POSTISSUE );
        (*it)->RequestComplete( req );
        (*it)->UnsetParent( );
    }

    /* Call IssueCommand. */
    rv = trampoline->RequestComplete( req );

    return rv;
}

void NVMObject_hook::Callback( void *data )
{
    /* Call the trampoline callback. */
    trampoline->Callback( data );
}

void NVMObject_hook::CalculateStats( )
{
    if( trampoline->GetDecoder( ) )
        trampoline->GetDecoder( )->CalculateStats( );

    trampoline->CalculateStats( );
}

void NVMObject_hook::ResetStats( )
{
    trampoline->ResetStats( );
}

void NVMObject_hook::PrintHierarchy( int depth )
{
    trampoline->PrintHierarchy( depth );
}

void NVMObject_hook::SetStats( Stats *s )
{
    trampoline->SetStats( s );
}

Stats *NVMObject_hook::GetStats( )
{
    return trampoline->GetStats( );
}

void NVMObject_hook::RegisterStats( )
{
    trampoline->RegisterStats( );
}

void NVMObject_hook::StatName( std::string name )
{
    trampoline->StatName( name );
}

std::string NVMObject_hook::StatName( )
{
    return trampoline->StatName( );
}

void NVMObject_hook::Cycle( ncycle_t steps )
{
    trampoline->Cycle( steps );
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
    debugStream = NULL;
    tagGen = NULL;
}

NVMObject::~NVMObject( )
{
    for( size_t childIdx = 0; childIdx < GetChildCount( ); childIdx++ )
    {
        delete children[childIdx];
    }
}

void NVMObject::Init( Config * )
{
}

void NVMObject::Notify( NVMainRequest * )
{
}

bool NVMObject::IssueAtomic( NVMainRequest * )
{
    return true;
}

bool NVMObject::IssueFunctional( NVMainRequest * )
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

ncycle_t NVMObject::NextIssuable( NVMainRequest *req )
{
    /* Assume module has no timing constraints, simply ask child module. */
    return GetChild( req )->NextIssuable( req );
}

bool NVMObject::Idle( )
{
    return true;
}

bool NVMObject::Drain( )
{
    bool rv = true;

    /* Assuming no requests to drain this module -- Drain children. */
    std::vector<NVMObject_hook *>::iterator it;

    for( it = children.begin(); it != children.end(); it++ )
    {
        bool child_rv = (*it)->Drain( );

        rv = !child_rv ? false : rv;
    }

    return rv;
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

void NVMObject::Callback( void * /*data*/ )
{
    // Ignored by default.
}

void NVMObject::SetEventQueue( EventQueue *eq )
{
    eventQueue = eq;
}

EventQueue *NVMObject::GetEventQueue( )
{
    return eventQueue;
}

void NVMObject::SetGlobalEventQueue( GlobalEventQueue *geq )
{
    globalEventQueue = geq;
}

GlobalEventQueue *NVMObject::GetGlobalEventQueue( )
{
    return globalEventQueue;
}

void NVMObject::SetParent( NVMObject *p )
{
    NVMObject_hook *hook = new NVMObject_hook( p );

    parent = hook;
    SetEventQueue( p->GetEventQueue( ) );
    SetGlobalEventQueue( p->GetGlobalEventQueue( ) );
    SetStats( p->GetStats( ) );
    SetTagGenerator( p->GetTagGenerator( ) );
}

void NVMObject::UnsetParent( )
{
    if( parent != NULL )
    {
        delete parent;
        parent = NULL;
    }
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

NVMObject *NVMObject::_FindChild( NVMainRequest *req, const char *childClass )
{
    NVMObject *curChild = this;

    while( curChild != NULL && NVMClass(*curChild) != childClass )
    {
        NVMObject_hook *curHook = curChild->GetChild( req );

        /* This object has no child objects. */
        if( curHook == NULL )
            return NULL;

        curChild = curChild->GetChild( req )->GetTrampoline();
    }

    return curChild;
}

ncounter_t NVMObject::GetChildId( NVMObject *c )
{
    std::vector<NVMObject_hook *>::iterator it;
    ncounter_t id = 0;
    ncounter_t rv = 0;

    for( it = children.begin(); it != children.end(); ++it )
    {
        if( (*it)->GetTrampoline() == c )
        {
            rv = id;
            break;
        }

        ++id;
    }

    return rv;
}

ncounter_t NVMObject::GetChildCount( )
{
    return children.size();
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
    /* If there is only one child (e.g., MC with interconnect child), use the other method. */
    if( GetDecoder( ) == NULL )
        return GetChild( );

    /*Use the specified decoder to choose the correct child. */
    uint64_t child;

    child = GetDecoder( )->Translate( req );

    return children[child];
}

NVMObject_hook *NVMObject::GetChild( ncounter_t child )
{
    assert( child < children.size() );

    return children[child];
}

NVMObject_hook *NVMObject::GetChild( void )
{
    if( children.size() == 0 )
        return NULL;

    /* This should only be used if there is gauranteed to be only one child. */
    assert( children.size() == 1 );

    return children[0];
}

void NVMObject::SetDecoder( AddressTranslator *at )
{
    decoder = at;

    decoder->StatName( statName + ".decoder" );
    decoder->SetStats( GetStats( ) );
    decoder->RegisterStats( );
}

AddressTranslator *NVMObject::GetDecoder( )
{
    return decoder;
}

void NVMObject::CalculateStats( )
{
    std::vector<NVMObject_hook *>::iterator it;

    for( it = children.begin(); it != children.end(); it++ )
    {
        (*it)->CalculateStats( );
    }
}

void NVMObject::ResetStats( )
{
    std::vector<NVMObject_hook *>::iterator it;

    for( it = children.begin(); it != children.end(); it++ )
    {
        (*it)->ResetStats( );
    }
}

void NVMObject::CreateCheckpoint( std::string dir )
{
    std::vector<NVMObject_hook *>::iterator it;

    for( it = children.begin(); it != children.end(); it++ )
    {
        (*it)->GetTrampoline( )->CreateCheckpoint( dir );
    }

    if( GetDecoder( ) )
        GetDecoder( )->CreateCheckpoint( dir );
}

void NVMObject::RestoreCheckpoint( std::string dir )
{
    std::vector<NVMObject_hook *>::iterator it;

    for( it = children.begin(); it != children.end(); it++ )
    {
        (*it)->GetTrampoline( )->RestoreCheckpoint( dir );
    }

    if( GetDecoder( ) )
        GetDecoder( )->RestoreCheckpoint( dir );
}

void NVMObject::PrintHierarchy( int depth )
{
    std::vector<NVMObject_hook *>::iterator it;

    if( depth > 0 )
    {
        std::cout << std::string(depth*2, '-') << " " << StatName( ) << std::endl;
    }
    else
    {
        std::cout << StatName( ) << std::endl;
    }

    for( it = children.begin(); it != children.end(); it++ )
    {
        (*it)->PrintHierarchy( depth + 1 );
    }
}

void NVMObject::SetStats( Stats *s )
{
    stats = s;
}

Stats *NVMObject::GetStats( )
{
    return stats;
}

void NVMObject::RegisterStats( )
{
}

void NVMObject::SetParams( Params *params )
{
    p = params;
}

Params *NVMObject::GetParams( )
{
    return p;
}

void NVMObject::StatName( std::string name )
{
    statName = name;
}

std::string NVMObject::StatName( )
{
    return statName;
}

void NVMObject::SetTagGenerator( TagGenerator *tg )
{
    tagGen = tg;
}

TagGenerator *NVMObject::GetTagGenerator( )
{
    return tagGen;
}

HookType NVMObject::GetHookType( )
{
    return hookType;
}

void NVMObject::SetHookType( HookType h )
{
    hookType = h;
}

HookType NVMObject::GetCurrentHookType( )
{
    return currentHookType;
}

void NVMObject::SetCurrentHookType( HookType h )
{
    currentHookType = h;
}

void NVMObject::AddHook( NVMObject *hook )
{
    HookType h = hook->GetHookType( );

    if( h == NVMHOOK_BOTHISSUE )
    {
        AddHookUnique( hooks[NVMHOOK_PREISSUE], hook );
        AddHookUnique( hooks[NVMHOOK_POSTISSUE], hook );
    }
    else
    {
        AddHookUnique( hooks[h], hook );
    }
}

void NVMObject::AddHookUnique( std::vector<NVMObject *>& list, NVMObject *hook )
{
    if( std::find( list.begin(), list.end(), hook ) == list.end() )
    {
        list.push_back( hook );
    }
}

std::vector<NVMObject *>& NVMObject::GetHooks( HookType h )
{
    return hooks[h];
}

void NVMObject::SetDebugName( std::string dn, Config *config )
{
    Params *params = new Params( );
    params->SetParams( config );

    /* Debugging a parent will add debug prints for all children. */
    if( debugStream == config->GetDebugLog( ) || debugStream == &std::cerr )
        return;

    /* Note: This should be called from SetConfig to ensure config was read! */
    if( params->debugOn && params->debugClasses.count( dn ) )
    {
        debugStream = config->GetDebugLog( );
    }
    else
    {
        debugStream = &nvmainDebugInhibitor;
    }
}

ncycle_t NVMObject::MAX( const ncycle_t a, const ncycle_t b )
{
    return (( a > b ) ? a : b );
}

ncycle_t NVMObject::MIN( const ncycle_t a, const ncycle_t b )
{
    return (( a < b ) ? a : b );
}

