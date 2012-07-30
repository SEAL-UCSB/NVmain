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

#include "Interconnect/OnChipBus/OnChipBus.h"

#include <sstream>
#include <iostream>


using namespace NVM;


OnChipBus::OnChipBus( )
{
  nextOp = NULL;
  conf = NULL;
  ranks = NULL;
  configSet = false;
  numRanks = 0;
  currentCycle = 0;
  syncValue = 0.0f;
}


OnChipBus::~OnChipBus( )
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


void OnChipBus::SetConfig( Config *c )
{
  std::stringstream formatter;

  conf = c;
  configSet = true;

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



bool OnChipBus::IssueCommand( MemOp *mop )
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
  if( nextOp != NULL )
    {
      std::cerr << "Warning: Only one command can be issued per cycle. Check memory controller code." << std::endl;
      return false;
    }

  nextOp = mop;

  return true;
}


bool OnChipBus::IsIssuable( MemOp *mop, ncycle_t delay )
{
  uint64_t opRank;
  uint64_t opBank;
  uint64_t opRow;
  uint64_t opCol;

  /*
   *  Can happen since the memory controller runs at a
   *  higher frequency than the bus.
   */
  if( nextOp != NULL )
    return false;
  
  mop->GetAddress( ).GetTranslatedAddress( &opCol, &opRow, &opBank, &opRank, NULL );
  
  return ranks[opRank]->IsIssuable( mop, delay );
}


ncycle_t OnChipBus::GetNextActivate( uint64_t rank, uint64_t bank )
{
  if( rank < numRanks )
    return ranks[rank]->GetNextActivate( bank );

  return 0;
}


ncycle_t OnChipBus::GetNextRead( uint64_t rank, uint64_t bank )
{
  if( rank < numRanks )
    return ranks[rank]->GetNextRead( bank );

  return 0;
}


ncycle_t OnChipBus::GetNextWrite( uint64_t rank, uint64_t bank )
{
  if( rank < numRanks )
    return ranks[rank]->GetNextWrite( bank );

  return 0;
}


ncycle_t OnChipBus::GetNextPrecharge( uint64_t rank, uint64_t bank )
{
  if( rank < numRanks )
    return ranks[rank]->GetNextPrecharge( bank );

  return 0;
}


ncycle_t OnChipBus::GetNextRefresh( uint64_t rank, uint64_t bank )
{
  if( rank < numRanks )
    return ranks[rank]->GetNextRefresh( bank );

  return 0;
}


void OnChipBus::PrintStats( )
{
  if( !configSet || numRanks == 0 )
    {
      std::cerr << "Error: No statistics to print. Memory system was not configured!" << std::endl;
      return;
    }

  for( ncounter_t i = 0; i < numRanks; i++ )
    {
      ranks[i]->PrintStats( );
    }
}


void OnChipBus::Cycle( )
{
  float cpuFreq;
  float busFreq;

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
              std::cout << "OnChipBus got unknown op.\n";
            }
          //std::cout << "Issuing command " << nextOp->GetOperation( ) << std::endl;
          
          nextOp->GetRequest( )->issueInterconnect = this;

          ranks[opRank]->AddCommand( nextOp );

          /*
           *  To preserve rank-to-rank switching time, we need to notify the
           *  other ranks what the command sent to opRank was.
           */
          for( ncounter_t i = 0; i < numRanks; i++ )
            if( (uint64_t)(i) != opRank )
              ranks[i]->Notify( nextOp->GetOperation( ) );

          nextOp = NULL;
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

}

