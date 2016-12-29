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

#include "src/EventQueue.h"
#include "src/NVMObject.h"
#include "src/Config.h"
#include "NVM/nvmain.h"

#include <limits>
#include <assert.h>

using namespace NVM;

void Event::SetRecipient( NVMObject *r )
{
    std::vector<NVMObject_hook *>& children = r->GetParent( )->GetTrampoline( )->GetChildren( );
    std::vector<NVMObject_hook *>::iterator it;
    NVMObject_hook *hook = NULL;

    for( it = children.begin(); it != children.end(); it++ )
    {
        if( (*it)->GetTrampoline() == r )
        {
            hook = (*it);
            break;
        }
    }

    assert( hook != NULL );

    recipient = hook;
}

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

void EventQueue::InsertEvent( EventType type, NVMObject *recipient, ncycle_t when, void *data, int priority )
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

    InsertEvent( type, hook, NULL, when, data, priority );
}

void EventQueue::InsertEvent( EventType type, NVMObject_hook *recipient, ncycle_t when, void *data, int priority )
{
    InsertEvent( type, recipient, NULL, when, data, priority );
}

void EventQueue::InsertEvent( EventType type, NVMObject *recipient, NVMainRequest *req, ncycle_t when, void *data, int priority )
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

    InsertEvent( type, hook, req, when, data, priority );
}

void EventQueue::InsertEvent( EventType type, NVMObject_hook *recipient, NVMainRequest *req, ncycle_t when, void *data, int priority )
{
    /* Create our event */
    Event *event = new Event( );

    event->SetType( type );
    event->SetRecipient( recipient );
    event->SetRequest( req );
    event->SetCycle( when );
    event->SetData( data );

    InsertEvent( event, when, priority );
}

void EventQueue::InsertEvent( Event *event, ncycle_t when, int priority )
{
    event->SetCycle( when );

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

        EventList::iterator it;
        bool inserted = false;

        for( it = eventList.begin(); it != eventList.end(); it++ )
        {
            if( (*it)->GetPriority( ) > priority )
            {
                eventList.insert( it, event );
                inserted = true;
                break;
            }
        }

        if( !inserted )
        {
            eventList.push_back( event );
        }
    }
}


void EventQueue::InsertCallback( NVMObject *recipient, CallbackPtr method,
                                 ncycle_t when, void *data, int priority )
{
    Event *event = new Event( );

    event->SetType( EventCallback );
    event->SetRecipient( recipient );
    event->SetData( data );
    event->SetCycle( when );
    event->SetPriority( priority );
    event->SetCallback( method );

    InsertEvent( event, when, priority );
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

        if( eventMap.empty() )
        {
            nextEventCycle = std::numeric_limits<uint64_t>::max( );
        }
        else
        {
            nextEventCycle = eventMap.begin()->first;
        }
    }

    return rv;
}


Event *EventQueue::FindEvent( EventType type, NVMObject *recipient, NVMainRequest *req, ncycle_t when ) const
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

    return FindEvent( type, hook, req, when );
}


Event *EventQueue::FindEvent( EventType type, NVMObject_hook *recipient, NVMainRequest *req, ncycle_t when ) const
{
    Event *rv = NULL;

    if (eventMap.count(when) == 0) {
        return rv;
    } else {
        const EventList& eventList = eventMap.find(when)->second;

        EventList::const_iterator it;
        for( it = eventList.begin(); it != eventList.end(); it++ )
        {
            if( (*it)->GetType( ) == type && (*it)->GetRecipient( ) == recipient
                && (*it)->GetRequest( ) == req )
            {
                rv = (*it);
            }
        }
        return rv;
    }
}


Event *EventQueue::FindCallback( NVMObject *recipient, CallbackPtr method, ncycle_t when, void *data, int priority ) const
{
    Event *rv = NULL;

    if( eventMap.count(when) != 0 )
    {
        const EventList& eventList = eventMap.find(when)->second;

        EventList::const_iterator it;
        for( it = eventList.begin(); it != eventList.end(); it++ )
        {
            if( (*it)->GetRecipient()->GetTrampoline() == recipient
                && (*it)->GetCallback() == method
                && (*it)->GetData() == data 
                && (*it)->GetPriority() == priority )
            {
                rv = (*it);
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


void EventQueue::Loop( ncycle_t steps )
{
    /* Special case. */
    if( steps == 0 && nextEventCycle == currentCycle )
    {
        Process( );
        return;
    }

    ncycle_t stepCycles = steps;

    while( stepCycles > 0 )
    {
        /* No events in this step amount, just change current cycle. */
        if( nextEventCycle > currentCycle + stepCycles )
        {
            currentCycle += stepCycles;
            break;
        }

        ncycle_t currentSteps = nextEventCycle - currentCycle;

        currentCycle += currentSteps;
        stepCycles -= currentSteps;

        /* Process will update nextEventCycle for the next loop iteration. */
        Process( );
    }
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
            {
                CallbackPtr cb = (*it)->GetCallback( );
                NVMObject *thisPtr = (*it)->GetRecipient( )->GetTrampoline( );
                (*thisPtr.*cb)( (*it)->GetData() );
                break;
            }

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

void EventQueue::SetFrequency( double freq )
{
    frequency = freq;
}

double EventQueue::GetFrequency( )
{
    return frequency;
}

ncycle_t EventQueue::GetNextEvent( )
{
    return nextEventCycle;
}

ncycle_t EventQueue::GetCurrentCycle( )
{
    return currentCycle;
}

void EventQueue::SetCurrentCycle( ncycle_t curCycle )
{
    currentCycle = curCycle;
}


GlobalEventQueue::GlobalEventQueue( )
{
    currentCycle = 0;
}

GlobalEventQueue::~GlobalEventQueue( )
{

}

void GlobalEventQueue::AddSystem( NVMain *subSystem, Config *config )
{
    double subSystemFrequency = config->GetEnergy( "CLK" ) * 1000000.0;
    EventQueue *queue = subSystem->GetEventQueue( );

    assert( subSystemFrequency <= frequency );

    /* 
     *  The CLK value in the config file is the frequency this subsystem should run at.
     *  We aren't doing and checks here to make sure the input side (i.e. CPUFreq) is
     *  corrent since we don't know what it should be.
     */
    eventQueues.insert( std::pair<EventQueue*, double>(queue, subSystemFrequency) );
    queue->SetFrequency( subSystemFrequency );

    std::cout << "NVMain: GlobalEventQueue: Added a memory subsystem running at "
              << config->GetEnergy( "CLK" ) << "MHz. My frequency is "
              << (frequency / 1000000.0) << "MHz." << std::endl;
}

void GlobalEventQueue::Cycle( ncycle_t steps )
{
    EventQueue *nextEventQueue;
    ncycle_t iterationSteps = 0;

    while( iterationSteps <= steps )
    {
        ncycle_t nextEvent = GetNextEvent( &nextEventQueue );

        ncycle_t globalQueueSteps = 0;
        if( nextEvent > currentCycle )
        {
            globalQueueSteps = nextEvent - currentCycle;
        }

        /* Next event occurs after the current number of steps. */
        if( globalQueueSteps > (steps - iterationSteps))
        {
            currentCycle += steps - iterationSteps;
            Sync( );
            break;
        }

        ncycle_t localQueueSteps = nextEventQueue->GetNextEvent( ) - nextEventQueue->GetCurrentCycle( );
        nextEventQueue->Loop( localQueueSteps );

        currentCycle += globalQueueSteps;
        iterationSteps += globalQueueSteps;

        Sync( );
    }
}

/* Set frequency of global event queue in Hz. */
void GlobalEventQueue::SetFrequency( double freq )
{
    frequency = freq;
}

double GlobalEventQueue::GetFrequency( )
{
    return frequency;
}

ncycle_t GlobalEventQueue::GetNextEvent( EventQueue **eq )
{
    std::map<EventQueue *, double>::const_iterator iter;
    ncycle_t nextEventCycle = std::numeric_limits<ncycle_t>::max( );

    if( eq != NULL )
        *eq = NULL;

    for( iter = eventQueues.begin( ); iter != eventQueues.end( ); iter++ )
    {
        /* 
         *  If there is no event, we must skip frequency alignment to prevent
         *  underflow causing an invalid nextEventCycle.
         */
        if( iter->first->GetNextEvent( ) == std::numeric_limits<ncycle_t>::max( ) )
            continue;

        double frequencyMultiplier = frequency / iter->second;
        double globalEventCycle = iter->first->GetNextEvent( ) * frequencyMultiplier;

        if( static_cast<ncycle_t>(globalEventCycle) < nextEventCycle )
        {
            nextEventCycle = static_cast<ncycle_t>(globalEventCycle);
            if( eq != NULL )
                *eq = iter->first;
        }
    }

    return nextEventCycle;
}

ncycle_t GlobalEventQueue::GetCurrentCycle( )
{
    return currentCycle;
}

void GlobalEventQueue::Sync( )
{
    std::map<EventQueue *, double>::const_iterator iter;
    for( iter = eventQueues.begin( ); iter != eventQueues.end( ); iter++ )
    {
        double frequencyMultiplier = frequency / iter->second;
        double setCycle = static_cast<double>(currentCycle) / frequencyMultiplier;
        ncycle_t stepCount = static_cast<ncycle_t>(setCycle) - iter->first->GetCurrentCycle( );

        if( static_cast<ncycle_t>(setCycle) > iter->first->GetCurrentCycle( ) )
        {
            iter->first->Loop( stepCount );
        }
    }
}


