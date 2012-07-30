/*
 *  this file is part of nvmain- a cycle accurate timing, bit-accurate
 *  energy simulator for non-volatile memory. originally developed by 
 *  matt poremba at the pennsylvania state university.
 *
 *  website: http://www.cse.psu.edu/~poremba/nvmain/
 *  email: mrp5060@psu.edu
 *
 *  ---------------------------------------------------------------------
 *
 *  if you use this software for publishable research, please include 
 *  the original nvmain paper in the citation list and mention the use 
 *  of nvmain.
 *
 */

#include "Interconnect/OffChipBus/OffChipBus.h"
#include "src/MemoryController.h"


#include <sstream>
#include <assert.h>


using namespace NVM;



OffChipBus::OffChipBus( )
{
  nextOp = NULL;
  conf = NULL;
  ranks = NULL;
  configSet = false;
  numRanks = 0;
  currentCycle = 0;
  syncValue = 0.0f;
}


OffChipBus::~OffChipBus( )
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


void OffChipBus::SetConfig( Config *c )
{
  std::stringstream formatter;

  conf = c;
  configSet = true;

  if( conf->KeyExists( "OffChipLatency" ) )
    offChipDelay = conf->GetValue( "OffChipLatency" );
  else
    offChipDelay = 10;

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

      NVMNetNode *nodeInfo = new NVMNetNode( NVMNETDESTTYPE_RANK, 0, i, 0 );
      NVMNetNode *parentInfo = new NVMNetNode( NVMNETDESTTYPE_INT, 0, 0, 0 );
      ranks[i]->AddParent( this, parentInfo );
      AddChild( ranks[i], nodeInfo );
    }
}



void OffChipBus::RequestComplete( NVMainRequest *request )
{
  if( request->issueController != NULL )
    {
      DelayedReq *dreq = new DelayedReq;

      dreq->req = request;
      dreq->delay = offChipDelay;

      delayQueue.push_back( dreq );
    }
}


bool OffChipBus::IssueCommand( MemOp *mop )
{
  if( !configSet || numRanks == 0 )
    {
      std::cerr << "Error: Issued command before memory system was configured!"
                << std::endl;
      return false;
    }


  /*
   *  Only one command can be issued per cycle. Make sure none of the
   *  delayed operations have a delay equal to the latency.
   */
  if( nextOp != NULL )
    {
      std::cerr << "Warning: Only one command can be issued per cycle. Check memory controller code."
                << std::endl;
    }

  nextOp = mop;

  return true;
}


bool OffChipBus::IsIssuable( MemOp *mop, ncycle_t delay )
{
  uint64_t opRank;
  uint64_t opBank;
  uint64_t opRow;
  uint64_t opCol;

  /*
   *  Only one command can be issued per cycle. Make sure none of the
   *  delayed operations have a delay equal to the latency.
   */
  if( nextOp != NULL )
    return false;

  mop->GetAddress( ).GetTranslatedAddress( &opCol, &opRow, &opBank, &opRank, NULL );

  return ranks[opRank]->IsIssuable( mop, delay+offChipDelay );
}


ncycle_t OffChipBus::GetNextActivate( uint64_t rank, uint64_t bank )
{
  if( rank < numRanks )
    return ranks[rank]->GetNextActivate( bank );

  return 0;
}


ncycle_t OffChipBus::GetNextRead( uint64_t rank, uint64_t bank )
{
  if( rank < numRanks )
    return ranks[rank]->GetNextRead( bank );

  return 0;
}


ncycle_t OffChipBus::GetNextWrite( uint64_t rank, uint64_t bank )
{
  if( rank < numRanks )
    return ranks[rank]->GetNextWrite( bank );

  return 0;
}


ncycle_t OffChipBus::GetNextPrecharge( uint64_t rank, uint64_t bank )
{
  if( rank < numRanks )
    return ranks[rank]->GetNextPrecharge( bank );

  return 0;
}


ncycle_t OffChipBus::GetNextRefresh( uint64_t rank, uint64_t bank )
{
  if( rank < numRanks )
    return ranks[rank]->GetNextRefresh( bank );

  return 0;
}


void OffChipBus::PrintStats( )
{
  if( !configSet || numRanks == 0 )
    {
      std::cerr << "Error: No statistics to print. Memory system was not configured!"
                << std::endl;
    }

  for( ncounter_t i = 0; i < numRanks; i++ )
    {
      ranks[i]->PrintStats( );
    }
}


void OffChipBus::Cycle( )
{
  float cpuFreq;
  float busFreq;

  /* GetEnergy returns a float */
  cpuFreq = conf->GetEnergy( "CPUFreq" );
  busFreq = conf->GetEnergy( "CLK" );

  
  syncValue += (float)( busFreq / cpuFreq );

  if( syncValue >= 1.0f )
    {
      syncValue -= 1.0f;
    }
  else
    {
      return;
    }


  currentCycle++;

  if( nextOp != NULL )
    {
      uint64_t opRank;
      uint64_t opBank;
      uint64_t opRow;
      uint64_t opCol;

      nextOp->GetAddress( ).GetTranslatedAddress( &opCol, &opRow, &opBank, &opRank, NULL );

      if( ranks[opRank]->IsIssuable( nextOp ) )
        {
          if( nextOp->GetOperation( ) == 0 )
            {
              std::cout << "OffChipBus got unknown op." << std::endl;
            }

          nextOp->GetRequest( )->issueInterconnect = this;

          ranks[opRank]->AddCommand( nextOp );

          for( ncounter_t i = 0; i < numRanks; i++ )
            if( (uint64_t)(i) != opRank )
              ranks[i]->Notify( nextOp->GetOperation( ) );
            
          nextOp = NULL;
        }
    }


  /* Complete delayed requests at 0, and decrement others. */
  std::list<DelayedReq *>::iterator it;

  for( it = delayQueue.begin( ); it != delayQueue.end( ); ++it )
    {
      if( (*it)->delay == 0 )
        {
          (*it)->req->issueController->RequestComplete( (*it)->req );
        }
    }

  delayQueue.remove_if( zero_delay() );

  for( it = delayQueue.begin( ); it != delayQueue.end( ); ++it )
    {
      (*it)->delay--;
    }

  /* Cycle the ranks. */
  for( ncounter_t i = 0; i < numRanks; i++ )
    {
      ranks[i]->Cycle( );
    }
}



