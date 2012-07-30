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

#include <sstream>

#include "src/MemoryControllerManager.h"


using namespace NVM;




MemoryControllerManager::MemoryControllerManager( )
{
  config = NULL;
}


MemoryControllerManager::~MemoryControllerManager( )
{

}


void MemoryControllerManager::SendMessage( MemoryControllerMessage *msg )
{
  MemoryControllerMessage *queueMsg;
  int latency;
  std::stringstream formatter;

  /* Don't assume ownership of the pointer... Makes it easier for the sender to free */
  queueMsg = new MemoryControllerMessage( );

  queueMsg->src = msg->src;
  queueMsg->dest = msg->dest;
  queueMsg->latency = msg->latency;
  queueMsg->message = msg->message;

  /* Calculate latency if default is used. */
  if( queueMsg->latency == -1 )
    {
      /* If config is not set, use a hardcoded default. */
      if( !this->config )
        {
          queueMsg->latency = 1;
        }
      /* Otherwise get latency from config... */
      else
        {
          /* 
           *  First, check for a pairwise latency between the source and destination.
           *  in the config file, this is something like MemCtlLatency(0,1) for the
           *  latency between memory controllers 0 and 1.
           */
          formatter << "MemCtlLatency(" << queueMsg->src << "," << queueMsg->dest << ")";
          
          if( ( latency = this->config->GetValue( formatter.str( ) ) ) == -1 )
            {
              /*
               *  If a pairwise latency wasn't found, use the default MemCtlLatency.
               */
              if( ( latency = this->config->GetValue( "MemCtlLatency" ) ) != -1 )
                queueMsg->latency = latency;
              /* If niether are found, use a hardcoded default. */
              else
                queueMsg->latency = 1;
            }
          else
            {
              queueMsg->latency = latency;
            }
        }
    }


  /* Place the message in the queue. */
  messages.push_back( queueMsg );
}


int MemoryControllerManager::RecvMessage( MemoryControllerMessage *msg )
{
  std::vector< MemoryControllerMessage * >::iterator iter;
  unsigned int dest;
  int rv;

  /* Just in case.. */
  if( !msg )
    return MSG_NOT_FOUND;

  /* Assume a message can not be found. */
  rv = MSG_NOT_FOUND;
  dest = msg->dest;

  /*
   *  Look for message with the same destination as the pointer passed and latency
   *  of 0 (meaning the message has arrived at the destination).
   */
  for( iter = this->messages.begin( ); iter != this->messages.end( ); ++iter )
    {
      if( (*iter)->dest == dest && (*iter)->latency == 0 )
        {
          /*
           *  Copy the contents of the enqueued message to the given pointer.
           */
          msg->src = (*iter)->src;
          msg->dest = (*iter)->dest;
          msg->latency = (*iter)->latency;
          msg->message = (*iter)->message;

          /*
           *  Delete the pointer allocated in SendMessage() and remove from queue.
           */
          delete (*iter);
          this->messages.erase( iter );
          iter--;

          /*
           *  Change return code to message found and return.
           */
          rv = MSG_FOUND;
          break;
        }
    }

  return rv;
}


void MemoryControllerManager::AddController( MemoryController *controller )
{
  std::vector< MemoryController * >::iterator iter;

  /* Just return if the controller is already in the list. */
  for( iter = this->controllers.begin( ); iter != this->controllers.end( ); ++iter )
    {
      if( (*iter) == controller )
        return;
    }

  this->controllers.push_back( controller );
}


void MemoryControllerManager::SetConfig( Config *conf )
{
  this->config = conf;
}


Config *MemoryControllerManager::GetConfig( )
{
  return this->config;
}


void MemoryControllerManager::Cycle( )
{
  std::vector< MemoryControllerMessage * >::iterator iter;

  for( iter = this->messages.begin( ); iter != this->messages.end( ); ++iter )
    {
      if( (*iter)->latency > 0 )
        (*iter)->latency--;
    }
}
