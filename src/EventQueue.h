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

#ifndef __NVMAIN_EVENTQUEUE_H__
#define __NVMAIN_EVENTQUEUE_H__

#include <map>
#include <list>
#include "include/NVMTypes.h"
#include "include/NVMainRequest.h"

namespace NVM {

class Event;
class NVMObject_hook;
class Config;
class NVMain;

typedef std::list<Event *> EventList;
typedef void (NVMObject::*CallbackPtr)(void*);

enum EventType { EventUnknown,
                 EventCycle,
                 EventIdle,    /* Automatic event */
                 EventRequest,
                 EventResponse,
                 EventCallback
};

class Event
{
  public:
    Event() : type(EventUnknown), recipient(NULL), request(NULL), data(NULL), cycle(0), priority(0) {}
    ~Event() {}

    void SetType( EventType e ) { type = e; }
    void SetRecipient( NVMObject *r );
    void SetRecipient( NVMObject_hook *r ) { recipient = r; }
    void SetRequest( NVMainRequest *r ) { request = r; }
    void SetData( void *d ) { data = d; }
    void SetCycle( ncycle_t c ) { cycle = c; }
    void SetPriority( int p ) { priority = p; }
    void SetCallback( CallbackPtr m ) { method = m; }

    EventType GetType( ) { return type; }
    NVMObject_hook *GetRecipient( ) { return recipient; }
    NVMainRequest *GetRequest( ) { return request; }
    void *GetData( ) { return data; }
    ncycle_t GetCycle( ) { return cycle; }
    int GetPriority( ) { return priority; }
    CallbackPtr GetCallback( ) { return method; }

 private:
    EventType type;              /* Type of event (which callback to invoke). */
    NVMObject_hook *recipient;   /* Who to callback. */
    NVMainRequest *request;      /* Request causing event. */
    void *data;                  /* Generic data to pass to callback. */
    ncycle_t cycle;
    int priority;
    CallbackPtr method;
};


class EventQueue
{
  public:
    EventQueue();
    ~EventQueue();

    void InsertEvent( EventType type, NVMObject_hook *recipient, NVMainRequest *req, ncycle_t when, void *data = NULL, int priority = 0 );
    void InsertEvent( EventType type, NVMObject *recipient, NVMainRequest *req, ncycle_t when, void *data = NULL, int priority = 0 );
    void InsertEvent( EventType type, NVMObject_hook *recipient, ncycle_t when, void *data = NULL, int priority = 0 );
    void InsertEvent( EventType type, NVMObject *recipient, ncycle_t when, void *data = NULL, int priority = 0 );
    void InsertEvent( Event *event, ncycle_t when, int priority = 0 );

    void InsertCallback( NVMObject *recipient, CallbackPtr method, ncycle_t when, void *data = NULL, int priority = 0 );

    Event *FindEvent( EventType type, NVMObject *recipient, NVMainRequest *req, ncycle_t when ) const;
    Event *FindEvent( EventType type, NVMObject_hook *recipient, NVMainRequest *req, ncycle_t when ) const;

    Event *FindCallback( NVMObject *recipient, CallbackPtr method, ncycle_t when, void *data = NULL, int priority = 0 ) const;

    bool RemoveEvent( Event *event, ncycle_t when );

    void Process( );
    void Loop( );
    void Loop( ncycle_t steps );

    void SetFrequency( double freq );
    double GetFrequency( );

    ncycle_t GetNextEvent( );
    ncycle_t GetCurrentCycle( );
    void SetCurrentCycle( ncycle_t curCycle );

  private:
    ncycle_t nextEventCycle;
    ncycle_t lastEventCycle;
    ncycle_t currentCycle; 
    double frequency;

    std::map< ncycle_t, EventList> eventMap; 
};


class GlobalEventQueue
{
  public:
    GlobalEventQueue();
    ~GlobalEventQueue();

    void AddSystem( NVMain *subSystem, Config *config );
    void Cycle( ncycle_t steps );

    void SetFrequency( double freq );
    double GetFrequency( );

    ncycle_t GetNextEvent( EventQueue **eq = NULL );
    ncycle_t GetCurrentCycle( );

  private:
    ncycle_t currentCycle;
    double frequency;

    std::map<EventQueue *, double> eventQueues;

    void Sync( );

};

};

#endif
