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
*******************************************************************************/

#include "Utils/RequestTracer/RequestTracer.h"
#include "src/Config.h"

#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <cxxabi.h>
#include <csignal>
#include <cassert>

using namespace NVM;


RequestTracer::RequestTracer( )
{
    /* Call our hook before IssueCommand/RequestComplete */
    SetHookType( NVMHOOK_PREISSUE );

    selfHook = new NVMObject_hook( this );
    detectDeadlocks = true;
    printTrace = true;
    deadlockThreshold = 800000;
}

RequestTracer::~RequestTracer( )
{
    delete selfHook;
}



/* 
 *  After initialization, the parent will become whichever NVMObject the request
 *  currently resides at (e.g., interconnect, rank, bank, etc.).
 */
void RequestTracer::Init( Config *conf )
{
    // Note: Defaults for these are set in ctor
    if( conf->KeyExists( "DeadlockThreshold" ) )
    {
        deadlockThreshold = static_cast<ncycle_t>( conf->GetValue("DeadlockThreshold") );
        detectDeadlocks = true; // Assume if the user sets a threshold, they probably want to use it.
    }

    if( conf->KeyExists( "DetectDeadlocks" ) )
    {
        // Default is true -- Look for explicit "false"
        if( conf->GetString( "DetectDeadlocks" ) == "false" )
            detectDeadlocks = false;
    }

    if( conf->KeyExists( "PrintRequestTrace" ) )
    {
        // Default is true -- Look for explicit "false"
        if( conf->GetString( "PrintRequestTrace" ) == "false" )
            printTrace = false;
    }
}


bool RequestTracer::IssueAtomic( NVMainRequest * /*req*/ )
{
    // It's difficult to trace atomic requests, since there is no
    // indication when the request is completed, so they are not traced.
    return true;
}


void RequestTracer::TraceAddress( NVMainRequest *req, TracedType traceType )
{
    // Use the request pointer as the unique key. Maybe this should be
    // ptr_t to avoid reinterpret_cast ?
    uint64_t addr = reinterpret_cast<uint64_t>(req);

    /* Check if the traced request chain is in the map already. */
    if( tracedRequests.count( addr ) )
    {
        TracedRequest *tr = tracedRequests[addr];

        while( tr->next != NULL )
        {
            tr = tr->next;
        }

        tr->next = new TracedRequest( );
        tr = tr->next;

        tr->moduleName = Demangle(NVMObjectType);
        tr->type = traceType;
        tr->next = NULL;
    }
    /* Not in the map, create an entry. */
    else
    {
        TracedRequest *tr = new TracedRequest( );

        tr->moduleName = Demangle(NVMObjectType);
        tr->type = traceType;
        tr->next = NULL;

        tracedRequests.insert( std::pair<uint64_t, TracedRequest*>(addr, tr) );

        /* Issue a deadlock detection event for this request. */
        if( detectDeadlocks )
        {
            Event *deadlockEvent = new Event( );
            ncycle_t deadlockTimer = GetEventQueue()->GetCurrentCycle() + deadlockThreshold;

            deadlockEvent->SetType( EventCallback );
            deadlockEvent->SetRecipient( selfHook );
            deadlockEvent->SetData( req );

            tr->deadlockEvent = deadlockEvent;
            tr->deadlockTimer = deadlockTimer;

            GetEventQueue()->InsertEvent( deadlockEvent, deadlockTimer );
        }
    }
}


bool RequestTracer::IssueCommand( NVMainRequest *req )
{
    TraceAddress( req, TracedIssue );

    return true;
}


bool RequestTracer::RequestComplete( NVMainRequest *req )
{
    TraceAddress( req, TracedCompletion );

    if( req->owner == GetParent()->GetTrampoline() )
    {
        uint64_t addr = reinterpret_cast<uint64_t>(req);

        if( printTrace )
        {
            std::string reqType = "NOP";
            
            switch( req->type )
            {
              case ACTIVATE:
                reqType = "ACT";
                break;
              case READ:
                reqType = "READ";
                break;
              case WRITE:
                reqType = "WRITE";
                break;
              case PRECHARGE:
                reqType = "PRE";
                break;
              case POWERDOWN_PDA:
                reqType = "PDA";
                break;
              case POWERDOWN_PDPF:
                reqType = "PDPF";
                break;
              case POWERDOWN_PDPS:
                reqType = "PDPS";
                break;
              case POWERUP:
                reqType = "PWRUP";
                break;
              case REFRESH:
                reqType = "REF";
                break;
              case BUS_READ:
                reqType = "BUSRD";
                break;
              case BUS_WRITE:
                reqType = "BUSWR";
                break;
              default:
                reqType = "NOP";
                break;
            }

            std::cout << "0x" << std::hex << std::setiosflags( std::ios_base::right ) << std::setw(8) 
                      << std::setfill('0') << req->address.GetPhysicalAddress() << std::dec 
                      << std::resetiosflags( std::ios_base::right )
                      << std::setw(6) << std::setfill(' ') << reqType << " "; 

            TracedRequest *tr = tracedRequests[addr];

            while( 1 )
            {
                std::cout << tr->moduleName;
                std::cout << ((tr->type == TracedIssue) ? "[I]" : "[C]");

                if( tr->next != NULL ) std::cout << " -> ";
                if( tr->next == NULL ) break;

                tr = tr->next;
            }

            std::cout << std::endl;
        }

        if( detectDeadlocks )
        {
            TracedRequest *tr = tracedRequests[addr];
#ifndef NDEBUG
            bool timerRemoved = GetEventQueue()->RemoveEvent( tr->deadlockEvent, tr->deadlockTimer );
            assert( timerRemoved );
#else
            GetEventQueue()->RemoveEvent( tr->deadlockEvent, tr->deadlockTimer );
#endif
        }

        tracedRequests.erase(addr);
    }

    return true;
}


void RequestTracer::Callback( void *data )
{
    /* 
     *  Normally you'd want a struct/class for data so you could 
     *  identify the data type. In this case, the type is ALWAYS
     *  a deadlock found message, so we don't need that here.
     */
    NVMainRequest *req = static_cast<NVMainRequest *>(data);

    /* Determine type of request for output neatness. */
    std::string reqType;
    
    switch( req->type )
    {
      case ACTIVATE:
        reqType = "Activate";
        break;
      case READ:
        reqType = "Read";
        break;
      case WRITE:
        reqType = "Write";
        break;
      case PRECHARGE:
        reqType = "Prechange";
        break;
      case POWERDOWN_PDA:
        reqType = "Active Powerdown";
        break;
      case POWERDOWN_PDPF:
        reqType = "Precharge Powerdown (Fast)";
        break;
      case POWERDOWN_PDPS:
        reqType = "Precharge Powerdown (Slow)";
        break;
      case POWERUP:
        reqType = "Powerup";
        break;
      case REFRESH:
        reqType = "Refresh";
        break;
      case BUS_READ:
        reqType = "Bus Read";
        break;
      case BUS_WRITE:
        reqType = "Bus Write";
        break;
      case NOP:
        reqType = "No Operation";
        break;
      default:
        reqType = "Unknown";
        break;
    }

    std::cerr << "RequestTracer: Deadlock detected! Request address is "
              << std::hex << req->address.GetPhysicalAddress() << std::dec
              << " request type is `" << reqType << "'. Raising SIGSTOP. "
              << "You may want to hook a debugger to this process at this point."
              << std::endl << std::endl << "The output chain is: " << std::endl;
    
    std::cerr << "0x" << std::hex << std::setiosflags( std::ios_base::right ) << std::setw(8) 
              << std::setfill('0') << req->address.GetPhysicalAddress() << std::dec 
              << std::resetiosflags( std::ios_base::right ) << " "; 

    uint64_t addr = reinterpret_cast<uint64_t>(req);
    TracedRequest *tr = tracedRequests[addr];
    std::string lastModule;

    while( 1 )
    {
        lastModule = tr->moduleName;
        std::cerr << tr->moduleName;
        std::cerr << ((tr->type == TracedIssue) ? "[I]" : "[C]");

        if( tr->next != NULL ) std::cout << " -> ";
        if( tr->next == NULL ) break;

        tr = tr->next;
    }

    std::cerr << std::endl << std::endl;

    std::cerr << "Check the " << lastModule << " code?" << std::endl;

    raise( SIGSTOP );
    exit(1);
}


void RequestTracer::Cycle( ncycle_t )
{
}


/*
 *  Note: This code uses some gcc specific stuff.
 *
 *  If this causes some compile errors change this #if
 *  to "#if 1". This won't affect functionality of this
 *  module, but the names of the modules in the request
 *  flow will be less human readable.
 *
 *  See also: "man c++filt"
 */
#if 0
std::string RequestTracer::Demangle( const char* cxxname )
{
    return std::string(name);
}
#else
std::string RequestTracer::Demangle( const char* cxxname )
{
    std::string demangled;
    int status;
    char *res = abi::__cxa_demangle( cxxname, NULL, NULL, &status );
    
    if( status == 0 )
    {
        demangled = std::string( res );
        if( demangled.substr(0,5) == "NVM::" )
        {
            demangled = demangled.substr(5,std::string::npos);
        }
    }
    else
    {
        demangled = std::string( cxxname );
    }

    free( res );
    
    return demangled;
}
#endif


RequestTracer::TracedRequest::TracedRequest( )
{
    // Initialize pointers and PODs
    next = NULL;
}


RequestTracer::TracedRequest::~TracedRequest( )
{
}



