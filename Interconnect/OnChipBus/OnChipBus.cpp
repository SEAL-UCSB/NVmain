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
  conf = NULL;
  ranks = NULL;
  configSet = false;
  numRanks = 0;
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

  Params *params = new Params( );
  params->SetParams( c );
  SetParams( params );

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

      formatter.str( "" );
      formatter << i;
      ranks[i]->SetName( formatter.str( ) );

      ranks[i]->SetParent( this );
      AddChild( ranks[i] );

      /* SetConfig recursively */
      ranks[i]->SetConfig( conf ); 
    }
}



bool OnChipBus::IssueCommand( NVMainRequest *req )
{
  uint64_t opRank;
  uint64_t opBank;
  uint64_t opRow;
  uint64_t opCol;
  bool success = false;

  if( !configSet || numRanks == 0 )
    {
      std::cerr << "Error: Issued command before memory system was configured!" << std::endl;
      return false;
    }

  req->address.GetTranslatedAddress( &opCol, &opRow, &opBank, &opRank, NULL );

  if( ranks[opRank]->IsIssuable( req ) )
    {
      if( req->type == 0 )
        {
          std::cout << "OnChipBus got unknown op.\n";
        }
      //std::cout << "Issuing command " << req->type << std::endl;
      
      success = ranks[opRank]->IssueCommand( req );

      /*
       *  To preserve rank-to-rank switching time, we need to notify the
       *  other ranks what the command sent to opRank was.
       */
      if( success )
        {
          for( ncounter_t i = 0; i < numRanks; i++ )
            if( (uint64_t)(i) != opRank )
              ranks[i]->Notify( req->type );
        }
    }

  return success;
}


bool OnChipBus::IsIssuable( NVMainRequest *req, ncycle_t delay )
{
  uint64_t opRank;
  uint64_t opBank;
  uint64_t opRow;
  uint64_t opCol;

  req->address.GetTranslatedAddress( &opCol, &opRow, &opBank, &opRank, NULL );
  
  return ranks[opRank]->IsIssuable( req, delay );
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


void OnChipBus::Cycle( ncycle_t )
{
}

