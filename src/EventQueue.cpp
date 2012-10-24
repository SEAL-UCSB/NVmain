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


#include "src/EventQueue.h"

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
    // Clean up events?
}



void EventQueue::InsertEvent( EventType type, NVMObject *recipient, ncycle_t when )
{
    InsertEvent( type, recipient, NULL, when );
}


void EventQueue::InsertEvent( EventType type, NVMObject *recipient, NVMainRequest *req, ncycle_t when )
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

                break;
              }
          }
      }

    return rv;
}


void EventQueue::Loop( )
{
    currentCycle++;

    if( nextEventCycle == currentCycle )
      Process( );
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
          case EventUnknown:
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
        nextEventCycle = eventMap.begin()->first; // map is sorted by keys, so this works out.
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

