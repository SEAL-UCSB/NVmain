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


#ifndef __NVMAIN_EVENTQUEUE_H__
#define __NVMAIN_EVENTQUEUE_H__


#include <map>


#include "include/NVMTypes.h"
#include "src/NVMObject.h"
#include "include/NVMainRequest.h"


namespace NVM {


class Event;
typedef std::list<Event *> EventList;


enum EventType { EventUnknown,
                 EventCycle,
                 EventIdle,    /* Automatic event */
                 EventRequest,
                 EventResponse
};


class Event
{
 public:
  Event() : type(EventUnknown), recipient(NULL) {}
  ~Event() {}

  void SetType( EventType e ) { type = e; }
  void SetRecipient( NVMObject *r ) { recipient = r; }
  void SetRequest( NVMainRequest *r ) { request = r; }

  EventType GetType( ) { return type; }
  NVMObject *GetRecipient( ) { return recipient; }
  NVMainRequest *GetRequest( ) { return request; }

 private:
  EventType type;         /* Type of event (which callback to invoke). */
  NVMObject *recipient;   /* Who to callback. */
  NVMainRequest *request; /* Request causing event. */

};


class EventQueue
{
 public:
  EventQueue();
  ~EventQueue();


  void InsertEvent( EventType type, NVMObject *recipient, NVMainRequest *req, ncycle_t when );
  void InsertEvent( EventType type, NVMObject *recipient, ncycle_t when );
  void InsertEvent( Event *event, ncycle_t when );
  bool RemoveEvent( Event *event, ncycle_t when );
  void Process( );
  void Loop( );

  ncycle_t GetNextEvent( );
  ncycle_t GetCurrentCycle( );


 private:
  ncycle_t nextEventCycle;
  ncycle_t lastEventCycle;
  ncycle_t currentCycle; 

  std::map< ncycle_t, EventList> eventMap; 


};



};


#endif


