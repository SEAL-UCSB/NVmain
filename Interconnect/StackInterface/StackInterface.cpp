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

#include "Interconnect/StackInterface/StackInterface.h"

#include <sstream>
#include <iostream>


using namespace NVM;


StackInterface::StackInterface( )
{
  conf = NULL;
  ranks = NULL;
  configSet = false;
  numRanks = 0;
  currentCycle = 0;
  syncValue = 0.0f;
  MLR_value = 100;
  MLW_value = 100;
  firstTry = 0;
  secondTry = 0;
  issuedReqs = 0;
  completedReqs = 0;
  stackRequests.clear( );
  completedRequests.clear( );
}


StackInterface::~StackInterface( )
{
  if( numRanks > 0 )
    {
      for( ncounter_t i = 0; i < numRanks; i++ )
	{
	  delete ranks[i];
	}

      delete [] ranks;
    }
}


void StackInterface::SetConfig( Config *c )
{
  std::stringstream formatter;

  conf = c;
  configSet = true;
  MLR_value = conf->GetValue( "tRCD" ) + conf->GetValue( "tBURST" ) + conf->GetValue( "tRTP" ) + 
    conf->GetValue( "tRP" );
  MLW_value = conf->GetValue( "tRCD" ) + conf->GetValue( "tBURST" ) + conf->GetValue( "tCWD" ) +
    conf->GetValue( "tWR" ) + conf->GetValue( "tRP" );

  std::cout << "MLR = " << MLR_value << ". MLW = " << MLW_value << std::endl;

  numRanks = conf->GetValue( "RANKS" );

  ranks = new Rank * [numRanks];
  for( ncounter_t i = 0; i < numRanks; i++ )
    {
      ranks[i] = new Rank( );

      formatter.str( "" );
      formatter << statName << ".rank" << i;
      ranks[i]->StatName( formatter.str( ) );

      ranks[i]->SetConfig( conf );

      formatter.str( "" );
      formatter << i;
      ranks[i]->SetName( formatter.str( ) );

      ranks[i]->SetParent( this );
      AddChild( ranks[i] );
    }
}


void StackInterface::SetMLRValue( unsigned int mlr )
{
  MLR_value = mlr;
  
  std::cout << "MLR changed by MC to " << MLR_value << std::endl;
}


void StackInterface::SetMLWValue( unsigned int mlw )
{
  MLW_value = mlw;
  
  std::cout << "MLW changed by MC to " << MLW_value << std::endl;
}


unsigned int StackInterface::GetMLValue( BulkCommand /*cmd*/ )
{
  unsigned int retval = MLR_value;

  /*
  if( cmd == CMD_ACTREADPRE || cmd == CMD_PU_ACT_READ_PRE_PDPF )
    retval = MLR_value;
  else if( cmd == CMD_ACTWRITEPRE || cmd == CMD_PU_ACT_WRITE_PRE_PDPF )
    retval = MLW_value;
  else
    std::cout << "StackInterface: Can't determine ML value for unknown bulk command: "
	      << cmd << std::endl;
  */

  return retval;
}
 


bool StackInterface::IssueCommand( NVMainRequest *nreq )
{
  if( !configSet || numRanks == 0 )
    {
      std::cerr << "Error: Issued command before memory system was configured!" << std::endl;
      return false;
    }

  /*
   * The next operation should be NULL after Cycle() is called, so if it's not NULL, it must have been
   * set by this function. In this wrapper, only one operation is allowed to be issued, as there is
   * presumably only a single bus per channel.
   */
  if( !IsIssuable( nreq ) )
    {
      std::cerr << "Warning: Only one command can be issued per cycle. Check memory controller code." << std::endl;
      return false;
    }


  /*
   *  Add the issue request to the front of the queue...
   */
  StackRequest *req = new StackRequest;

  req->slot = 0;
  req->memReq = nreq;

  stackRequests.push_front( req );

  /*
   *  Now find a slot for the backup request...
   */
  req = new StackRequest;

  req->memReq = nreq;
  req->slot = GetMLValue( req->memReq->bulkCmd );


  std::deque<StackRequest *>::iterator iter;
  for( iter = stackRequests.begin( ); iter != stackRequests.end( ); iter++ )
    {
      /* If the slot at ML_value is already used, try the next one. */
      if( (*iter)->slot == req->slot )
        req->slot++;

      /* If we find a slot past our desired slot, break. We will insert here. */
      if( (*iter)->slot > req->slot )
        break;
    }

  /* 
   *  This *should* insert _at_ the iter location, aka, it will be before 
   *  the entry with a higher slot number.
   */
  stackRequests.insert( iter, req );

  issuedReqs++;

  return true;
}


/*
 *  In this stack interface implementation, we say a request is issuable
 *  if the current slot is empty.
 */
bool StackInterface::IsIssuable( NVMainRequest * /*req*/, ncycle_t /*delay*/ )
{
  /*
   *  If there are no requests in the queue, or the first slot is not
   *  being used, we can issue
   */
  if( stackRequests.empty( ) || stackRequests.front()->slot != 0 )
    return true;

  return false;
}



bool StackInterface::GetCompletedRequest( StackRequest **req )
{
  /*
   *  If the completed requests queue is empty, or the first request is
   *  not in the current cycle, return no request completed this cycle.
   */
  if( completedRequests.empty( ) || completedRequests.front()->slot != 0 )
    return false;

  *req = completedRequests.front( );
  completedRequests.erase( completedRequests.begin( ) );

  std::deque<StackRequest *>::iterator iter;

  for( iter = completedRequests.begin( ); iter != completedRequests.end( ); ++iter )
    {
      if( (*iter)->slot == 0 )
        std::cout << "StackInterface: Warning: Multiple requests ended in same slot!" << std::endl;
    }

  return true;
}


void StackInterface::PrintStats( )
{
  if( !configSet || numRanks == 0 )
    {
      std::cerr << "Error: No statistics to print. Memory system was not configured!" << std::endl;
      return;
    }

  for( ncounter_t i = 0; i < numRanks; i++ )
    {
      std::cout << "Rank " << i << " statisics: " << std::endl << std::endl;
      std::cout << " --- Requests complated on first try: " << firstTry << std::endl;
      std::cout << " --- Requests complated on second try: " << secondTry << std::endl;
      std::cout << " --- Requests issued: " << issuedReqs << std::endl;
      std::cout << " --- Completed requests: " << completedReqs << std::endl;

      ranks[i]->PrintStats( );
    }
}


void StackInterface::Cycle( )
{
  float cpuFreq;
  float busFreq;
  NVMainRequest *nextReq;

  /* GetEnergy is used since it returns a float. */
  cpuFreq = conf->GetEnergy( "CPUFreq" );
  busFreq = conf->GetEnergy( "CLK" );

  /* busFreq should be <= cpuFreq. */
  syncValue += (float)( busFreq / cpuFreq );

  /*  
   *  Since the CPU runs faster than the bus, if the bus
   *  is not ready, don't cycle the ranks yet.
   */
  if( syncValue >= 1.0f )
    syncValue -= 1.0f;
  else
    return;

  currentCycle++;

  if( !stackRequests.empty( ) && (stackRequests.front())->slot == 0 )
    nextReq = (stackRequests.front())->memReq;
  else
    nextReq = NULL;
  
  if( nextReq != NULL )
    {
      uint64_t opRank;
      uint64_t opBank;
      uint64_t opRow;
      uint64_t opCol;

      nextReq->address.GetTranslatedAddress( &opCol, &opRow, &opBank, &opRank, NULL );

      /*
       *  If the bank is ready to issue, issue and delete backup slot.
       */
      if( ranks[opRank]->IsIssuable( nextReq ) )
        {
          if( nextReq->type == 0 )
            {
              std::cout << "StackInterface got unknown op.\n";
            }

          ranks[opRank]->IssueCommand( nextReq );

          /*
           *  To preserve rank-to-rank switching time, we need to notify the
           *  other ranks what the command sent to opRank was.
           */
          for( ncounter_t i = 0; i < numRanks; i++ )
            if( (uint64_t)(i) != opRank )
              ranks[i]->Notify( nextReq->type );

          /*
           *  Put the request in the completed queue.
           */
          StackRequest *req = new StackRequest;
          
          req->memReq = nextReq;
          req->status = ACK_ACK;
          req->slot = GetMLValue( req->memReq->bulkCmd );

          std::deque<StackRequest *>::iterator iter;
          for( iter = completedRequests.begin( ); iter != completedRequests.end( ); iter++ )
            {
              if( (*iter)->slot == req->slot )
                req->slot++;
            
              if( (*iter)->slot > req->slot )
                break;
            }

          completedRequests.insert( iter, req );

          completedReqs++;

          /*
           *  Delete this request and any backup slots.
           */
          unsigned int deleteCount;
          
          deleteCount = 0;
          iter = stackRequests.begin( );
          while( iter != stackRequests.end( ) )
            {
              if( (*iter)->memReq == nextReq )
                {
                  deleteCount++;
                  delete (*iter);
                  iter = stackRequests.erase( iter );
                }
              else
                {
                  iter++;
                }
            }

          /*
           *  Take stats on number of requests completed in 1st/2nd slot
           */
          if( deleteCount == 2 )
            {
              /* Completed first try. */
              firstTry++;
            }
          else if( deleteCount == 1 )
            {
              secondTry++;
            }

          
          nextReq = NULL;
        }
      /*
       *  Bank is not ready... 
       */
      else
        {
          //std::cout << "Couldn't issue request to " << nextOp->GetAddress( ).GetPhysicalAddress( )
          //	    << std::endl;

          /*
           *  If this is the last request, mark it as complete and NACK'd
           */
          std::deque<StackRequest *>::iterator iter;
          unsigned int requestCount = 0;

          for( iter = stackRequests.begin( ); iter != stackRequests.end( ); ++iter )
            {
              if( (*iter)->memReq == nextReq )
                requestCount++;
            }


          /* Request count of 1 means only the backup request is found. */
          if( requestCount == 1 )
            {
              StackRequest *req = new StackRequest;

              req->status = ACK_NACK;
              req->memReq = nextReq;
              req->slot = GetMLValue( req->memReq->bulkCmd );

              for( iter = completedRequests.begin( ); iter != completedRequests.end( ); iter++ )
                {
                  if( (*iter)->slot == req->slot )
                    req->slot++;
                  
                  if( (*iter)->slot > req->slot )
                    break;
                }

              completedRequests.insert( iter, req );
            }

          /* Delete the first request found. */
          iter = stackRequests.begin( );

          while( iter != stackRequests.end( ) )
            {
              if( (*iter)->memReq == nextReq )
                {
                  delete (*iter);
                  iter = stackRequests.erase( iter );
                  break;
                }
              else
                {
                  iter++;
                }
            }
        }
    }
  else
    {
      // Issue a NOP
    }

  for( ncounter_t i = 0; i < numRanks; i++ )
    {
      ranks[i]->Cycle( );
    }


  /* Decrement the counter of each request. */
  std::deque<StackRequest *>::iterator iter;

  if( !stackRequests.empty( ) )
    {
      iter = stackRequests.begin( );

      while( iter != stackRequests.end( ) )
        {
          /* This shouldn't happen, but just to be safe, delete any requests at 0. */
          if( (*iter)->slot == 0 )
            {
              iter = stackRequests.erase( iter );
            }
          else
            {
              (*iter)->slot--;
              ++iter;
            }
        }
    }

  if( !completedRequests.empty( ) )
    {
      for( iter = completedRequests.begin( ); iter != completedRequests.end( ); ++iter )
        {
          if( (*iter)->slot == 0 )
            std::cout << "Warning: Completed request was not checked by memory controller!" << std::endl;
          
          (*iter)->slot--;
        }
    }
}

