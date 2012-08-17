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

#include <iostream>
#include <sstream>

#include "src/Rank.h"


using namespace NVM;




#define MAX(a,b) ((a > b) ? a : b)


std::string GetFilePath( std::string file );


Rank::Rank( )
{
  // NOTE: make sure this doesn't cause unnecessary tRRD delays at start, k?
  lastActivate[0] = -10000;
  lastActivate[1] = -10000;
  lastActivate[2] = -10000;
  lastActivate[3] = -10000;

  FAWindex = 0;

  currentCycle = 0;

  conf = NULL;

  backgroundEnergy = 0.0f;

  psInterval = 0;
}


Rank::~Rank( )
{
  if( devices != NULL )
    {
      for( ncounter_t i = 0; i < deviceCount; i++ )
        for( ncounter_t j = 0; j < bankCount; j++ )
          {
            Bank *bank = devices[i].GetBank( j );

            delete bank;
          }

      delete [] devices;
    }
}


void Rank::SetConfig( Config *c )
{
  conf = c;

  bankCount = conf->GetValue( "BANKS" );
  deviceWidth = conf->GetValue( "DeviceWidth" );
  busWidth = conf->GetValue( "BusWidth" );

  if( conf->GetValue( "BANKS" ) == -1 || conf->GetValue( "DeviceWidth" ) == -1
      || conf->GetValue( "BusWidth" ) == -1 )
    {
      std::cout << "Rank: `BANKS', `DeviceWidth', or `BusWidth' configuration value "
    << "does not exist in configuration!" << std::endl;
    }

 
  /* Calculate the number of devices needed. */
  deviceCount = busWidth / deviceWidth;
  if( busWidth % deviceWidth != 0 )
    {
      std::cout << "NVMain: device width is not a multiple of the bus width!\n";
      deviceCount++;
    }

  std::cout << "Creating " << bankCount << " banks in all " << deviceCount << " devices.\n";

  devices = new Device[deviceCount];
  
  for( ncounter_t i = 0; i < deviceCount; i++ )
    {
      for( ncounter_t j = 0; j < bankCount; j++ )
        {
          Bank *newBank = new Bank( );
          std::stringstream formatter;
          ncycle_t nextRefresh;

          newBank->SetConfig( c );

          if( c->KeyExists( "UseRefresh" ) && c->GetString( "UseRefresh" ) == "true" ) 
            {
              nextRefresh = ((conf->GetValue( "tRFI" )) / (conf->GetValue( "ROWS" ) / conf->GetValue( "RefreshRows" )));
              nextRefresh = static_cast<ncycle_t>(static_cast<float>(nextRefresh) * (static_cast<float>(j + 1) / static_cast<float>(bankCount)));
              newBank->SetNextRefresh( nextRefresh );
            }

          formatter << j;
          newBank->SetName( formatter.str( ) );
          newBank->SetId( (int)i );
          formatter.str( "" );

          devices[i].AddBank( newBank );

          formatter << statName << ".bank" << j;
          newBank->StatName( formatter.str( ) );

          NVMNetNode *nodeInfo = new NVMNetNode( NVMNETDESTTYPE_BANK, 0, 0, j );
          NVMNetNode *parentInfo = new NVMNetNode( NVMNETDESTTYPE_RANK, 0, 0, 0 );
          newBank->AddParent( this, parentInfo );
          AddChild( newBank, nodeInfo );
        }
    }



  cmdBus = new GenericBus( );
  dataBus = new GenericBus( );

  cmdBus->SetGraphLabel( "RANK      " );
  dataBus->SetGraphLabel( "RANK      " );

  cmdBus->SetConfig( conf );
  dataBus->SetConfig( conf );

  /*
   *  We'll say you can't do anything until the command has time to issue on the bus.
   */
  nextRead = conf->GetValue( "tCMD" );
  nextWrite = conf->GetValue( "tCMD" );
  nextActivate = conf->GetValue( "tCMD" );
  nextPrecharge = conf->GetValue( "tCMD" );

  nextOp = NULL;


  fawWaits = 0;
  rrdWaits = 0;
  actWaits = 0;

  fawWaitTime = 0;
  rrdWaitTime = 0;
  actWaitTime = 0;
}




bool Rank::Activate( NVMainRequest *request )
{
  uint64_t activateBank;

  request->address.GetTranslatedAddress( NULL, NULL, &activateBank, NULL, NULL );

  if( activateBank >= bankCount )
    {
      std::cerr << "Rank: Attempted to activate non-existant bank " << activateBank << std::endl;
      return false;
    }
  
  /*
   *  Ensure that the time since the last bank activation is >= tRRD. This is to limit
   *  power consumption.
   */
  if( nextActivate <= currentCycle 
      && (ncycles_t)lastActivate[FAWindex] + (ncycles_t)conf->GetValue( "tRRDR" ) <= (ncycles_t)currentCycle
      && (ncycles_t)lastActivate[(FAWindex + 1)%4] + (ncycles_t)conf->GetValue( "tFAW" ) <= (ncycles_t)currentCycle )
    {
      for( ncounter_t i = 0; i < deviceCount; i++ )
        devices[i].GetBank( activateBank )->Activate( request );

      FAWindex = (FAWindex + 1) % 4;
      lastActivate[FAWindex] = (ncycles_t)currentCycle;
    }
  else
    {
      std::cerr << "Rank: Activation FAILED! Did you check IsIssuable?" << std::endl;
    }

  return true;
}


bool Rank::Read( NVMainRequest *request )
{
  uint64_t readRow, readBank;

  request->address.GetTranslatedAddress( &readRow, NULL, &readBank, NULL, NULL );

  if( readBank >= bankCount )
    {
      std::cerr << "Attempted to read non-existant bank: " << readBank << "!" << std::endl;
      return false;
    }


  if( nextRead > currentCycle )
    {
      return false;
    }


  /*
   *  We need to check for bank conflicts. If there are no conflicts, we can issue the 
   *  command as long as the bus will be ready and the column can decode that fast. The
   *  bank code ensures the second criteria.
   */
  if( !devices[0].GetBank( readBank )->WouldConflict( readRow ) )
    {
      for( ncounter_t i = 0; i < deviceCount; i++ )
        devices[i].GetBank( readBank )->Read( request );

      nextRead = MAX( nextRead, currentCycle + MAX( conf->GetValue( "tBURST" ), conf->GetValue( "tCCD" ) ) );
      nextWrite = MAX( nextWrite, currentCycle + conf->GetValue( "tCAS" ) + conf->GetValue( "tBURST" ) +
           conf->GetValue( "tRTRS" ) - conf->GetValue( "tCWD" ) );
      nextActivate = MAX( nextActivate, currentCycle + conf->GetValue( "tRRDR" ) );
    }
  else
    {
      std::cerr << "Rank: Read FAILED! Did you check IsIssuable?" << std::endl;
    }
  

  return true;
}


bool Rank::Write( NVMainRequest *request )
{
  uint64_t writeRow, writeBank;

  request->address.GetTranslatedAddress( &writeRow, NULL, &writeBank, NULL, NULL );

  if( writeBank >= bankCount )
    {
      std::cerr << "Attempted to write non-existant bank: " << writeBank << "!" << std::endl;
      return false;
    }


  if( nextWrite > currentCycle )
    {
      return false;
    }


  if( !devices[0].GetBank( writeBank )->WouldConflict( writeRow ) )
    {
      for( ncounter_t i = 0; i < deviceCount; i++ )
        devices[i].GetBank( writeBank )->Write( request );

      nextRead = MAX( nextRead, currentCycle + conf->GetValue( "tCWD" ) + conf->GetValue( "tBURST" )
              + conf->GetValue( "tWTR" ) );
      nextWrite = MAX( nextWrite, currentCycle + MAX( conf->GetValue( "tBURST" ), conf->GetValue( "tCCD" ) ) );
      nextActivate = MAX( nextActivate, currentCycle + conf->GetValue( "tRRDW" ) );

    }
  else
    {
      std::cerr << "Rank: Write FAILED! Did you check IsIssuable?" << std::endl;
    }

  
  return true;
}


bool Rank::Precharge( NVMainRequest *request )
{
  uint64_t prechargeBank;

  request->address.GetTranslatedAddress( NULL, NULL, &prechargeBank, NULL, NULL );

  if( prechargeBank >= bankCount )
    {
      std::cerr << "Rank: Attempted to precharge non-existant bank: " << prechargeBank << std::endl;
      return false;
    }

  /*
   *  There are no rank-level constraints on precharges. If the bank says timing
   *  was met we can send the command to the bank.
   */
  for( ncounter_t i = 0; i < deviceCount; i++ )
    devices[i].GetBank( prechargeBank )->Precharge( request );

  return true;
}


bool Rank::PowerUp( NVMainRequest *request )
{
  bool returnValue = false;
  uint64_t puBank;

  request->address.GetTranslatedAddress( NULL, NULL, &puBank, NULL, NULL );

  if( puBank >= bankCount )
    {
      std::cerr << "Rank: Attempted to powerup non-existant bank: " << puBank << std::endl;
      return false;
    }

  returnValue = true;
  for( ncounter_t i = 0; i < deviceCount; i++ )
    if( !devices[i].GetBank( puBank )->PowerUp( request ) )
      {
        if( i != 0 )
          std::cerr << "Rank: Error partial power up failure!" << std::endl;
        returnValue = false;
      }

  return returnValue;
}


bool Rank::Refresh( NVMainRequest *request )
{
  uint64_t reBank;

  request->address.GetTranslatedAddress( NULL, NULL, &reBank, NULL, NULL );

  if( reBank >= bankCount )
    {
      std::cerr << "Rank: Attempted to refresh non-existant bank: " << reBank << std::endl;
      return false;
    }

  for( ncounter_t i = 0; i < deviceCount; i++ )
    devices[i].GetBank( reBank )->Refresh( );

  return true;
}


ncycle_t Rank::GetNextActivate( uint64_t bank )
{
  return MAX( MAX( nextActivate,
       devices[0].GetBank( bank )->GetNextActivate( ) ),
        (ncycle_t)MAX( lastActivate[FAWindex] + conf->GetValue( "tRRDR" ),
       lastActivate[(FAWindex + 1)%4] + conf->GetValue( "tFAW" ) ) 
        );
}


ncycle_t Rank::GetNextRead( uint64_t bank )
{
  return MAX( nextRead, devices[0].GetBank( bank )->GetNextRead( ) );
}


ncycle_t Rank::GetNextWrite( uint64_t bank )
{
  return MAX( nextWrite, devices[0].GetBank( bank )->GetNextWrite( ) );
}


ncycle_t Rank::GetNextPrecharge( uint64_t bank )
{
  return MAX( nextPrecharge, devices[0].GetBank( bank )->GetNextPrecharge( ) );
}


ncycle_t Rank::GetNextRefresh( uint64_t bank )
{
  return devices[0].GetBank( bank )->GetNextRefresh( );
}


bool Rank::IsIssuable( MemOp *mop, ncycle_t delay )
{
  uint64_t opRank;
  uint64_t opBank;
  uint64_t opRow;
  uint64_t opCol;
  bool rv;
  
  mop->GetAddress( ).GetTranslatedAddress( &opRow, &opCol, &opBank, &opRank, NULL );

  rv = true;

  if( mop->GetOperation( ) == ACTIVATE )
    {
      if( nextActivate > (currentCycle+delay) 
          || lastActivate[FAWindex] + static_cast<ncycles_t>(conf->GetValue( "tRRDR" )) > static_cast<ncycles_t>(currentCycle+delay)
          || lastActivate[(FAWindex + 1)%4] + static_cast<ncycles_t>(conf->GetValue( "tFAW" )) > static_cast<ncycles_t>(currentCycle+delay) )
        rv = false;
      else
        rv = devices[0].GetBank( opBank )->IsIssuable( mop, delay );

      if( rv == false )
        {
          if( nextActivate > (currentCycle+delay) )
            {
              //std::cout << "Rank: Can't activate " << mop->GetAddress( ).GetPhysicalAddress( )
              //  << " for " << nextActivate - currentCycle << " more cycles." << std::endl;
            
              actWaits++;
              actWaitTime += nextActivate - (currentCycle+delay);
            }
          if( lastActivate[FAWindex] + static_cast<ncycles_t>(conf->GetValue( "tRRDR" )) > static_cast<ncycles_t>(currentCycle+delay) )
            {
              //std::cout << "Rank: Can't activate " << mop->GetAddress( ).GetPhysicalAddress( )
              //  << " due to tRRDR until " << lastActivate[FAWindex] + conf->GetValue( "tRRDR" )
              //- currentCycle << " more cycles." << std::endl;
            
              rrdWaits++;
              rrdWaitTime += lastActivate[FAWindex] + conf->GetValue( "tRRDR" ) - (currentCycle+delay);
            }
          if( lastActivate[(FAWindex + 1)%4] + static_cast<ncycles_t>(conf->GetValue( "tFAW" )) > static_cast<ncycles_t>(currentCycle+delay) )
            {
              //std::cout << "Rank: Can't activate " << mop->GetAddress( ).GetPhysicalAddress( )
              //  << " due to tFAW until " << lastActivate[(FAWindex + 1)%4] + conf->GetValue( "tFAW" )
              //- currentCycle << " more cycles." << std::endl;

              fawWaits++;
              fawWaitTime += lastActivate[(FAWindex +1)%4] + conf->GetValue( "tFAW" ) - (currentCycle+delay);
            }
        }
    }
  else if( mop->GetOperation( ) == READ )
    {
      if( nextRead > (currentCycle+delay) || devices[0].GetBank( opBank )->WouldConflict( opRow ) )
        rv = false;
      else
        rv = devices[0].GetBank( opBank )->IsIssuable( mop, delay );
    }
  else if( mop->GetOperation( ) == WRITE )
    {
      if( nextWrite > (currentCycle+delay) || devices[0].GetBank( opBank )->WouldConflict( opRow ) )
        rv = false;
      else
        rv = devices[0].GetBank( opBank )->IsIssuable( mop, delay );
    }
  else if( mop->GetOperation( ) == PRECHARGE )
    {
      if( nextPrecharge > (currentCycle+delay) )
        rv = false;
      else
        rv = devices[0].GetBank( opBank )->IsIssuable( mop, delay );
    }
  else if( mop->GetOperation( ) == POWERDOWN_PDA || mop->GetOperation( ) == POWERDOWN_PDPF
     || mop->GetOperation( ) == POWERDOWN_PDPS )
    {
      rv = devices[0].CanPowerDown( mop->GetOperation( ) );
    }
  else if( mop->GetOperation( ) == POWERUP )
    {
      rv = devices[0].CanPowerUp( opBank );
    }
  else if( mop->GetOperation( ) == REFRESH )
    {
      rv = devices[0].GetBank( opBank )->IsIssuable( mop, delay );
    }
  /*
   *  Can't issue unknown operations.
   */
  else
    {
      rv = false;
    }

  return rv;
}



void Rank::AddCommand( MemOp *mop )
{
  if( !IsIssuable( mop ) )
    {
      std::cout << "NVMain: Rank: Warning: Command can not be issued!\n";
    }
  else
    {
      nextOp = mop;
    }
}




/* 
 *  Other ranks should notify us when they read/write so we can ensure minimum timings are met.
 */
void Rank::Notify( OpType op )
{
  /*
   *  We only care if other ranks are reading/writing (to avoid bus contention)
   */
  if( op == READ )
    {
      nextRead = MAX( nextRead, currentCycle + conf->GetValue( "tBURST" ) 
          + conf->GetValue( "tRTRS" ) );
    }
  else if( op == WRITE )
    {
      nextWrite = MAX( nextWrite, currentCycle + conf->GetValue( "tBURST" )
           + conf->GetValue( "tOST" ) );
      nextRead = MAX( nextRead, currentCycle + conf->GetValue("tBURST" )
          + conf->GetValue( "tCWD" ) + conf->GetValue( "tRTRS" )
          - conf->GetValue( "tCAS" ) );
    }
}


void Rank::Cycle( )
{
  bool success;

  /* Wait for the next operation. */
  if( nextOp != NULL )
    {
      success = false;
      
      switch( nextOp->GetOperation( ) )
        {
        case ACTIVATE:
          success = this->Activate( nextOp->GetRequest( ) );
          break;
        
        case READ:
          success = this->Read( nextOp->GetRequest( ) );
          break;
        
        case WRITE:
          success = this->Write( nextOp->GetRequest( ) );
          break;
        
        case PRECHARGE:
          success = this->Precharge( nextOp->GetRequest( ) );
          break;

        case POWERUP:
          success = this->PowerUp( nextOp->GetRequest( ) );
          break;
      
        case REFRESH:
          success = this->Refresh( nextOp->GetRequest( ) );
          break;

        default:
          std::cout << "NVMain: Rank: Unknown operation in command queue! " << nextOp->GetOperation( ) << std::endl;
          break;  
      }

      /* If the command is successfully issues, erase it and draw some pictures. */
      if( success )
        {
          cmdBus->SetBusy( currentCycle - conf->GetValue( "tCMD" ), currentCycle );
          if( nextOp->GetOperation( ) == WRITE )
            dataBus->SetBusy( currentCycle + conf->GetValue( "tCWD" ), currentCycle +
                  conf->GetValue( "tCWD" ) + conf->GetValue( "tBURST" ) );
          else if( nextOp->GetOperation( ) == READ )
            dataBus->SetBusy( currentCycle + conf->GetValue( "tCAS" ), currentCycle +
                  conf->GetValue( "tCAS" ) + conf->GetValue( "tBURST" ) );
          else if( nextOp->GetBulkCmd( ) == CMD_ACTREADPRE )
            dataBus->SetBusy( currentCycle + conf->GetValue( "tRCD" ) + conf->GetValue( "tCAS" ), 
                  currentCycle + conf->GetValue( "tRCD" ) + conf->GetValue( "tCAS" )
                  + conf->GetValue( "tBURST" ) );
          else if( nextOp->GetBulkCmd( ) == CMD_ACTWRITEPRE )
            dataBus->SetBusy( currentCycle + conf->GetValue( "tRCD" ) + conf->GetValue( "tCWD" ),
                  currentCycle + conf->GetValue( "tRCD" ) + conf->GetValue( "tCWD" )
                  + conf->GetValue( "tBURST" ) );
          else if( nextOp->GetBulkCmd( ) == CMD_ACT_READ_PRE_PDPF )
            dataBus->SetBusy( currentCycle + conf->GetValue( "tRCD" ) + conf->GetValue( "tCAS" ), 
                  currentCycle + conf->GetValue( "tRCD" ) + conf->GetValue( "tCAS" )
                  + conf->GetValue( "tBURST" ) );
          else if( nextOp->GetBulkCmd( ) == CMD_ACT_WRITE_PRE_PDPF )
            dataBus->SetBusy( currentCycle + conf->GetValue( "tRCD" ) + conf->GetValue( "tCWD" ),
                  currentCycle + conf->GetValue( "tRCD" ) + conf->GetValue( "tCWD" )
                  + conf->GetValue( "tBURST" ) );
          else if( nextOp->GetBulkCmd( ) == CMD_PU_ACT_READ_PRE_PDPF )
            dataBus->SetBusy( currentCycle + conf->GetValue( "tXP" ) + conf->GetValue( "tRCD" )
                  + conf->GetValue( "tCAS" ), currentCycle + conf->GetValue( "tXP" )
                  + conf->GetValue( "tRCD" ) + conf->GetValue( "tCAS" )
                  + conf->GetValue( "tBURST" ) );
          else if( nextOp->GetBulkCmd( ) == CMD_PU_ACT_WRITE_PRE_PDPF )
            dataBus->SetBusy( currentCycle + conf->GetValue( "tXP" ) + conf->GetValue( "tRCD" )
                  + conf->GetValue( "tCWD" ), currentCycle + conf->GetValue( "tXP" )
                  + conf->GetValue( "tRCD" ) + conf->GetValue( "tCWD" )
                  + conf->GetValue( "tBURST" ) );
          else if( nextOp->GetBulkCmd( ) == CMD_PU_ACT_READ_PRE )
            dataBus->SetBusy( currentCycle + conf->GetValue( "tXP" ) + conf->GetValue( "tRCD" )
                  + conf->GetValue( "tCAS" ), currentCycle + conf->GetValue( "tXP" )
                  + conf->GetValue( "tRCD" ) + conf->GetValue( "tCAS" )
                  + conf->GetValue( "tBURST" ) );
          else if( nextOp->GetBulkCmd( ) == CMD_PU_ACT_WRITE_PRE )
            dataBus->SetBusy( currentCycle + conf->GetValue( "tXP" ) + conf->GetValue( "tRCD" )
                  + conf->GetValue( "tCWD" ), currentCycle + conf->GetValue( "tXP" )
                  + conf->GetValue( "tRCD" ) + conf->GetValue( "tCWD" )
                  + conf->GetValue( "tBURST" ) );


          nextOp = NULL;
        }
    }


  /*
   *  Do rank level power calculations
   */
  // if all banks are closed, add EIDD2N
  // if any bank is open, add EIDD3N
  bool allIdle = true;
  for( ncounter_t i = 0; i < bankCount; i++ )
    {
      if( devices[0].GetBank( i )->GetState( ) == BANK_OPEN )
        {
          allIdle = false;
          break;
        }
    }

  if( allIdle )
    {
      if( conf->KeyExists( "EnergyModel" ) && conf->GetString( "EnergyModel" ) == "current" )
        {
          backgroundEnergy += (float)conf->GetValue( "EIDD2N" );
        }
      else
        {
          backgroundEnergy += conf->GetEnergy( "Eclosed" );
        }
    }
  else
    {
      if( conf->KeyExists( "EnergyModel" ) && conf->GetString( "EnergyModel" ) == "current" )
        {
          backgroundEnergy += (float)conf->GetValue( "EIDD3N" );
        }
      else
        {
          backgroundEnergy += conf->GetEnergy( "Eopen" );
        }
    }


  /*
   *  Update current cycle for all banks and this rank.
   */
  currentCycle++;

  cmdBus->Cycle( );
  dataBus->Cycle( );


  for( ncounter_t i = 0; i < deviceCount; i++ )
    {
      devices[i].Cycle( );
    }

  if( currentCycle % 50 == 0 && conf &&
      conf->GetString( "PrintGraphs" ) == "true" )
    std::cout << std::endl;
}



/*
 *  Assign a name to this rank (used in graph outputs)
 */
void Rank::SetName( std::string name )
{
  std::string graphLabel;

  graphLabel = "RANK ";

  for( unsigned int i = 0; i < 6; i++ )
    {
      if( i >= name.length( ) )
        graphLabel += ' ';
      else
        graphLabel += name[i];
    }

  cmdBus->SetGraphLabel( graphLabel );
  dataBus->SetGraphLabel( graphLabel );
}



void Rank::PrintStats( )
{
  float totalPower = 0.0f;
  float totalEnergy = 0.0f;
  ncounter_t reads, writes;

  reads = 0;
  writes = 0;
  for( ncounter_t i = 0; i < bankCount; i++ )
    {
      totalEnergy += devices[0].GetBank( i )->GetEnergy( );
      totalPower += devices[0].GetBank( i )->GetPower( );

      reads += devices[0].GetBank( i )->GetReads( );
      writes += devices[0].GetBank( i )->GetWrites( );
    }

  totalEnergy *= (float)deviceCount;
  totalPower *= (float)deviceCount;

  totalEnergy += backgroundEnergy;
  totalPower += (((backgroundEnergy / (float)currentCycle) * conf->GetEnergy( "Voltage" )) / 1000.0f) * (float)deviceCount;

  std::cout << "i" << psInterval << "." << statName << ".power " << totalPower << std::endl;
  std::cout << "i" << psInterval << "." << statName << ".energy " << totalEnergy << std::endl;
  std::cout << "i" << psInterval << "." << statName << ".backgroundEnergy " << backgroundEnergy << std::endl;
  std::cout << "i" << psInterval << "." << statName << ".reads " << reads << std::endl;
  std::cout << "i" << psInterval << "." << statName << ".writes " << writes << std::endl;

  std::cout << "i" << psInterval << "." << statName << ".actWaits " << actWaits << std::endl;
  std::cout << "i" << psInterval << "." << statName << ".actWaits.totalTime " << actWaitTime << std::endl;
  std::cout << "i" << psInterval << "." << statName << ".actWaits.averageTime " << (double)((double)actWaitTime / (double)actWaits) << std::endl;

  std::cout << "i" << psInterval << "." << statName << ".rrdWaits " << rrdWaits << std::endl;
  std::cout << "i" << psInterval << "." << statName << ".rrdWaits.totalTime " << rrdWaitTime << std::endl;
  std::cout << "i" << psInterval << "." << statName << ".rrdWaits.averageTime " << (double)((double)rrdWaitTime / (double)rrdWaits) << std::endl;

  std::cout << "i" << psInterval << "." << statName << ".fawWaits " << fawWaits << std::endl;
  std::cout << "i" << psInterval << "." << statName << ".fawWaits.totalTime " << fawWaitTime << std::endl;
  std::cout << "i" << psInterval << "." << statName << ".fawWaits.averageTime " << (double)((double)fawWaitTime / (double)fawWaits) << std::endl;


  for( ncounter_t i = 0; i < bankCount; i++ )
    {
      devices[0].GetBank( i )->PrintStats( );
    }

  psInterval++;
}



