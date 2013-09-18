/*******************************************************************************
* Copyright (c) 2012-2013, The Microsystems Design Labratory (MDL)
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

#include "src/EventQueue.h"
#include "src/NVMObject.h"

#include <limits>
#include <assert.h>

using namespace NVM;

EventQueue::EventQueue( )
{
    eventMap.clear( );
    lastEventCycle = 0;
    nextEventCycle = std::numeric_limits<ncycle_t>::max();
    currentCycle = 0;
}

EventQueue::~EventQueue( )
{
}

void EventQueue::InsertEvent( EventType type, NVMObject *recipient, ncycle_t when )
{
    /* The parent has our hook in the children list, we need to find this. */
    std::vector<NVMObject_hook *>& children = recipient->GetParent( )->GetTrampoline( )->GetChildren( );
    std::vector<NVMObject_hook *>::iterator it;
    NVMObject_hook *hook = NULL;

    for( it = children.begin(); it != children.end(); it++ )
    {
        if( (*it)->GetTrampoline() == recipient )
        {
            hook = (*it);
            break;
        }
    }

    assert( hook != NULL );

    InsertEvent( type, hook, NULL, when );
}

void EventQueue::InsertEvent( EventType type, NVMObject_hook *recipient, ncycle_t when )
{
    InsertEvent( type, recipient, NULL, when );
}

void EventQueue::InsertEvent( EventType type, NVMObject *recipient, NVMainRequest *req, ncycle_t when )
{
    /* The parent has our hook in the children list, we need to find this. */
    std::vector<NVMObject_hook *>& children = recipient->GetParent( )->GetTrampoline( )->GetChildren( );
    std::vector<NVMObject_hook *>::iterator it;
    NVMObject_hook *hook = NULL;

    for( it = children.begin(); it != children.end(); it++ )
    {
        if( (*it)->GetTrampoline() == recipient )
        {
            hook = (*it);
            break;
        }
    }

    assert( hook != NULL );

    InsertEvent( type, hook, req, when );
}

void EventQueue::InsertEvent( EventType type, NVMObject_hook *recipient, NVMainRequest *req, ncycle_t when )
{
    /* Create our event */
    Event *event = new Event( );

    event->SetType( type );
    event->SetRecipient( recipient );
    event->SetRequest( req );

    /* If this event time is before our previous nextEventCycle, change it. */
    if( when < nextEventCycle )
    {
        nextEventCycle = when;
    }

    /* If there are no events at this time, create a new mapping. */ 
    if( eventMap.count( when ) == 0 )
    {
        EventList eventList;

        eventList.push_back( event );

        eventMap.insert( std::pair<ncycle_t, EventList>( when, eventList ) );
    }
    /* Otherwise append this event to the event list for this cycle. */
    else
    {
        EventList& eventList = eventMap[when];

        eventList.push_back( event );
    }
}

void EventQueue::InsertEvent( Event *event, ncycle_t when )
{
    /* If this event time is before our previous nextEventCycle, change it. */
    if( when < nextEventCycle )
    {
        nextEventCycle = when;
    }

    /* If there are no events at this time, create a new mapping. */ 
    if( eventMap.count( when ) == 0 )
    {
        EventList eventList;

        eventList.push_back( event );

        eventMap.insert( std::pair<ncycle_t, EventList>( when, eventList ) );
    }
    /* Otherwise append this event to the event list for this cycle. */
    else
    {
        EventList& eventList = eventMap[when];

        eventList.push_back( event );
    }
}


bool EventQueue::RemoveEvent( Event *event, ncycle_t when )
{
    bool rv = false;

    if( eventMap.count( when ) == 0 )
    {
        rv = false;
    }
    else
    {
        EventList& eventList = eventMap[when];

        EventList::iterator it;
        for( it = eventList.begin(); it != eventList.end(); it++ )
        {
            if( (*it) == event )
            {
                eventList.erase( it );

                rv = true;

                /* If the list is empty now, we can also erase the map entry. */
                if( eventList.empty() )
                    eventMap.erase( when );

                break;
            }
        }
    }

    return rv;
}


void EventQueue::Loop( )
{
    /* 
     * Loop() is called by NVMain::Cycle() in nvmain.cc, where the whole
     * memory system inserts new events into the mainEventQueue. We need to
     * guarantee that the event that is inserted in current cycle and must
     * be handled in current cycle can be processed.
     */  
    if( nextEventCycle == currentCycle )
        Process( );

    currentCycle++;
}


void EventQueue::Process( )
{
    /* Process all the events at the next cycle, and figure out the next next cycle. */
    assert( eventMap.count( nextEventCycle ) );

    EventList& eventList = eventMap[nextEventCycle];
    EventList::iterator it;

    for( it = eventList.begin( ); it != eventList.end( ); it++ )
    {
        switch( (*it)->GetType( ) )
        {
            case EventCycle:
                (*it)->GetRecipient( )->Cycle( nextEventCycle - lastEventCycle );
                break;

            case EventIdle:
                // TODO: Add this
                break;

            case EventRequest:
                // TODO: Add this
                break;

            case EventResponse:
                (*it)->GetRecipient( )->RequestComplete( (*it)->GetRequest( ) );
                break;

            case EventCallback:
                (*it)->GetRecipient( )->Callback( (*it)->GetData() );

            case EventUnknown:
                // TODO: Add this
            default:
                break;
        }

        /* Free event data */
        delete (*it);
    }

    eventMap.erase( nextEventCycle );

    /* Figure out the next cycle. */
    if( eventMap.empty( ) )
    {
        lastEventCycle = nextEventCycle;
        nextEventCycle = std::numeric_limits<ncycle_t>::max();
    }
    else
    {
        lastEventCycle = nextEventCycle;
        /* map is sorted by keys, so this works out. */
        nextEventCycle = eventMap.begin()->first; 
    }
}

ncycle_t EventQueue::GetNextEvent( )
{
    return nextEventCycle;
}

ncycle_t EventQueue::GetCurrentCycle( )
{
    return currentCycle;
}
