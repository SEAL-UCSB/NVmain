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

#include "MemControl/DRAMCache/DRAMCache.h"
#include "include/NVMHelpers.h"
#include "NVM/nvmain.h"

#include <iostream>
#include <assert.h>


using namespace NVM;



DRAMCache::DRAMCache( Interconnect *memory, AddressTranslator *translator )
{
  translator->GetTranslationMethod( )->SetOrder( 5, 1, 4, 3, 2 );

  SetMemory( memory );
  SetTranslator( translator );

  std::cout << "Created a DRAMCache!" << std::endl;

  drcChannels = NULL;

  numChannels = 0;
}



DRAMCache::~DRAMCache( )
{

}



void DRAMCache::SetConfig( Config *conf )
{
  /*
   *  Initialize off-chip memory;
   */
  std::string configFile;
  Config *mainMemoryConfig;

  configFile  = NVM::GetFilePath( conf->GetFileName( ) );
  configFile += conf->GetString( "MM_CONFIG" );

  mainMemoryConfig = new Config( );
  mainMemoryConfig->Read( configFile );

  mainMemory = new NVMain( );
  mainMemory->SetConfig( mainMemoryConfig, "offChipMemory" );
  mainMemory->SetParent( this ); // TODO: Somehow this needs to have all the basic DRCs as parents..

  /*
   *  Initialize DRAM Cache channels;
   */
  if( conf->KeyExists( "DRC_CHANNELS" ) )
    numChannels = static_cast<ncounter_t>( conf->GetValue( "DRC_CHANNELS" ) );
  else
    numChannels = 1;


  drcChannels = new BasicDRC*[numChannels];
  for( ncounter_t i = 0; i < numChannels; i++ )
    {
      drcChannels[i] = new BasicDRC( GetMemory(), GetTranslator() );
      drcChannels[i]->SetMainMemory( mainMemory );

      drcChannels[i]->SetID( i );
      drcChannels[i]->StatName( this->statName ); 

      drcChannels[i]->SetParent( this );
      AddChild( drcChannels[i] );

      drcChannels[i]->SetConfig( conf );
    }

  MemoryController::SetConfig( conf );
}



bool DRAMCache::IssueAtomic( NVMainRequest *req )
{
  uint64_t chan;

  req->address.GetTranslatedAddress( NULL, NULL, NULL, NULL, &chan );

  assert( chan < numChannels );

  return drcChannels[chan]->IssueAtomic( req );
}


bool DRAMCache::IssueCommand( NVMainRequest *req )
{
  uint64_t chan;

  req->address.GetTranslatedAddress( NULL, NULL, NULL, NULL, &chan );

  assert( chan < numChannels );

  return drcChannels[chan]->IssueCommand( req );
}


bool DRAMCache::RequestComplete( NVMainRequest *req )
{
  bool rv = false;

  if( req->owner == this )
    {
      delete req;
      rv = true;
    }
  else
    {
      /* 
       *  We handle DRC and NVMain source requests. If the request is 
       *  somewhere in the DRC hierarchy, send to the BasicDRC, otherwise
       *  back to NVMain.
       */
      bool drcRequest = false;

      for( ncounter_t i = 0; i < numChannels; i++ )
        {
          if( req->owner == drcChannels[i] )
            {
              drcRequest = true;
              break;
            }
        }

      if( drcRequest )
        {
          uint64_t chan;

          req->address.GetTranslatedAddress( NULL, NULL, NULL, NULL, &chan );

          rv = drcChannels[chan]->RequestComplete( req );
        }
      else
        {
          rv = GetParent( )->RequestComplete( req );
        }
    }

  return rv;
}


void DRAMCache::Cycle( ncycle_t steps )
{
  uint64_t i;

  for( i = 0; i < numChannels; i++ )
    {
      drcChannels[i]->Cycle( steps );
    }

  mainMemory->Cycle( steps );
}



void DRAMCache::PrintStats( )
{
  uint64_t i;

  for( i = 0; i < numChannels; i++ )
    {
      drcChannels[i]->PrintStats( );
    }

  mainMemory->PrintStats( );
}

