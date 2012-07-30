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
#include <limits>

#include "src/Bank.h"
#include "src/MemoryController.h"
#include "Endurance/EnduranceModelFactory.h"
#include <signal.h>


using namespace NVM;



#define MAX(a,b) (( a > b ) ? a : b )


Bank::Bank( )
{
  conf = NULL;

  nextActivate = 0;
  nextPrecharge = 0;
  nextRead = 0;
  nextWrite = 0;
  nextPowerDown = 0;
  nextPowerDownDone = 0;
  nextPowerUp = 0;
  nextCommand = CMD_NOP;

  state = BANK_CLOSED;
  lastActivate = 0;
  openRow = 0;

  currentCycle = 0;

  bankGraph = new GenericBus( );

  bankGraph->SetGraphLabel( "BANK      " );

  bankEnergy = 0.0f;
  backgroundEnergy = 0.0f;
  activeEnergy = 0.0f;
  burstEnergy = 0.0f;
  refreshEnergy = 0.0f;

  powerCycles = 0;
  feCycles = 0;
  seCycles = 0;
  dataCycles = 0;
  activeCycles = 0;
  utilization = 0.0f;
  writeCycle = false;
  writeMode = WRITE_THROUGH;
  idleTimer = 0;

  reads = 0;
  writes = 0;
  activates = 0;

  actWaits = 0;
  actWaitTime = 0;

  bankId = -1;

  refreshUsed = false;
  refreshRows = 1024;

  psInterval = 0;
}



Bank::~Bank( )
{
  
}



void Bank::SetConfig( Config *c )
{
  conf = c;

  /*
   *  We need to create an endurance model on a bank-by-bank basis.
   */
  endrModel = EnduranceModelFactory::CreateEnduranceModel( conf->GetString( "EnduranceModel" ) );
  if( endrModel )
    endrModel->SetConfig( conf );

  bankGraph->SetConfig( conf );

  if( conf->GetString( "InitPD" ) == "true" )
    state = BANK_PDPF;

  if( conf->GetString( "UseRefresh" ) == "true" )
    {
      refreshUsed = true;
      refreshRows = conf->GetValue( "RefreshRows" );
      nextRefresh = currentCycle + ((conf->GetValue( "tRFI" )) / (conf->GetValue( "ROWS" ) / refreshRows));
      nextRefreshDone = 0;
    }
}



bool Bank::PowerDown( BankState pdState )
{
  bool returnValue = false;

  if( nextPowerDown <= currentCycle && ( state == BANK_OPEN || state == BANK_CLOSED ) )
    {
      /*
       *  The power down state (pdState) will be determined by the device class, which
       *  will check to see if all the banks are idle or not, and if fast exit is used.
       */
      state = pdState;

      nextPowerUp = MAX( nextPowerUp, currentCycle + conf->GetValue( "tPD" ) );
      nextActivate = MAX( nextActivate, currentCycle + conf->GetValue( "tPD" ) + conf->GetValue( "tXP" ) );
      if( pdState == BANK_PDPFWAIT || pdState == BANK_PDAWAIT )
        nextRead = MAX( nextRead, currentCycle + conf->GetValue( "tPD" ) + conf->GetValue( "tXP" ) );
      else
        nextRead = MAX( nextRead, currentCycle + conf->GetValue( "tPD" ) + conf->GetValue( "tXPDLL" ) );
      nextWrite = MAX( nextWrite, currentCycle + conf->GetValue( "tPD" ) + conf->GetValue( "tXP" ) );
      nextPrecharge = MAX( nextPrecharge, currentCycle + conf->GetValue( "tPD" ) + conf->GetValue( "tXP" ) );


      bankGraph->SetLabel( currentCycle, currentCycle + conf->GetValue( "tPD" ), 'D' );

      
      switch( nextCommand )
        {
        case CMD_PDPF:
          nextCommand = CMD_NOP;
          break;

        case CMD_NOP:
          nextCommand = CMD_NOP;
          break;

        default:
          std::cout << "Warning: PowerDown: Unknown bulk command: " << nextCommand << std::endl;
          nextCommand = CMD_NOP;
          break;
        }
     

      returnValue = true;
    }

  return returnValue;
}



bool Bank::PowerUp( NVMainRequest *request )
{
  bool returnValue = false;

  if( nextPowerUp <= currentCycle && ( state == BANK_PDPF || state == BANK_PDPS || state == BANK_PDA ) )
    {
      nextPowerDown = MAX( nextPowerDown, currentCycle + conf->GetValue( "tXP" ) );
      nextActivate = MAX( nextActivate, currentCycle + conf->GetValue( "tXP" ) );
      if( state == BANK_PDPS )
        nextRead = MAX( nextRead, currentCycle + conf->GetValue( "tXPDLL" ) );
      else
        nextRead = MAX( nextRead, currentCycle + conf->GetValue( "tXP" ) );
      nextWrite = MAX( nextWrite, currentCycle + conf->GetValue( "tXP" ) );
      nextPrecharge = MAX( nextPrecharge, currentCycle + conf->GetValue( "tXP" ) );

      /*
       *  While technically the bank is being "powered up" we will just reset
       *  the previous state. For energy calculations, the bank is still considered
       *  to be consuming background power while powering up/down. Thus, we need
       *  a powerdown wait, but no power up wait.
       */
      if( state == BANK_PDA || state == BANK_PDAWAIT )
        state = BANK_OPEN;
      else
        state = BANK_CLOSED;


      if( state == BANK_PDPS )
        bankGraph->SetLabel( currentCycle, currentCycle + conf->GetValue( "tXPDLL" ), 'U' );
      else
        bankGraph->SetLabel( currentCycle, currentCycle + conf->GetValue( "tXP" ), 'U' );


      lastOperation = *request;

      switch( request->bulkCmd )
        {
        case CMD_PU_ACT_READ_PRE_PDPF:
          nextCommand = CMD_ACT_READ_PRE_PDPF;
          break;

        case CMD_PU_ACT_WRITE_PRE_PDPF:
          nextCommand = CMD_ACT_WRITE_PRE_PDPF;
          break;

        case CMD_PU_ACT_READ_PRE:
          nextCommand = CMD_ACTREADPRE;
          break;

        case CMD_PU_ACT_WRITE_PRE:
          nextCommand = CMD_ACTWRITEPRE;
          break;

        case CMD_NOP:
          nextCommand = CMD_NOP;
          break;

        default:
          std::cout << "Warning: PowerUp: Unknown bulk command: " << request->bulkCmd << std::endl;
          nextCommand = CMD_NOP;
          break;
        }


      returnValue = true;
    }

  return returnValue;
}


bool Bank::Activate( NVMainRequest *request )
{
  uint64_t activateRow;
  bool returnValue = false;

  request->address.GetTranslatedAddress( &activateRow, NULL, NULL, NULL, NULL );

  if( nextActivate <= currentCycle && state == BANK_CLOSED )
    {
      nextActivate = MAX( nextActivate, currentCycle + MAX( conf->GetValue( "tRCD" ), conf->GetValue( "tRAS" ) ) 
                          + conf->GetValue( "tRP" ) );
      nextPrecharge = MAX( nextPrecharge, currentCycle + MAX( conf->GetValue( "tRCD" ), conf->GetValue( "tRAS" ) ) );
      nextRead = MAX( nextRead, currentCycle + conf->GetValue( "tRCD" ) - conf->GetValue( "tAL" ) );
      nextWrite = MAX( nextWrite, currentCycle + conf->GetValue( "tRCD" ) - conf->GetValue( "tAL" ) );
      nextPowerDown = MAX( nextPowerDown, currentCycle + conf->GetValue( "tRCD" ) + 1 );
      nextPowerDownDone = MAX( nextPowerDownDone, currentCycle + conf->GetValue( "tRCD" )
                   + conf->GetValue( "tPD" ) + 1 );


      if( request->issueInterconnect != NULL && bankId == 0 )
        {
          notifyComplete.insert(std::pair<NVMainRequest *, int>( request, conf->GetValue( "tRCD" ) ) );
        }


      openRow = activateRow;
      state = BANK_OPEN;
      writeCycle = false;

      lastActivate = currentCycle;

      /* Add to bank's total energy. */
      if( conf->KeyExists( "EnergyModel" ) && conf->GetString( "EnergyModel" ) == "current" )
        {
          uint64_t tRC = conf->GetValue( "tRAS" ) + conf->GetValue( "tRCD" );


          bankEnergy += (float)((conf->GetValue( "EIDD0" ) * tRC) 
                      - ((conf->GetValue( "EIDD3N" ) * conf->GetValue( "tRAS" ))
                      +  (conf->GetValue( "EIDD2N" ) * conf->GetValue( "tRP" ))));

          activeEnergy +=  (float)((conf->GetValue( "EIDD0" ) * tRC) 
                        - ((conf->GetValue( "EIDD3N" ) * conf->GetValue( "tRAS" ))
                        +  (conf->GetValue( "EIDD2N" ) * conf->GetValue( "tRP" )) ));
        }
      else
        {
          bankEnergy += conf->GetEnergy( "Erd" );
        }

      bankGraph->SetLabel( currentCycle, currentCycle + conf->GetValue( "tRCD" ), 'A' );


      lastOperation = *request;

      switch( request->bulkCmd )
        {
        case CMD_ACTREADPRE:
          nextCommand = CMD_READPRE;
          break;

        case CMD_ACTREAD2PRE:
          nextCommand = CMD_READ2PRE;
          break;

        case CMD_ACTREAD3PRE:
          nextCommand = CMD_READ3PRE;
          break;

        case CMD_ACTREAD4PRE:
          nextCommand = CMD_READ4PRE;
          break;

        case CMD_ACTWRITEPRE:
          nextCommand = CMD_WRITEPRE;
          break;

        case CMD_ACTWRITE2PRE:
          nextCommand = CMD_WRITE2PRE;
          break;

        case CMD_ACTWRITE3PRE:
          nextCommand = CMD_WRITE3PRE;
          break;

        case CMD_ACTWRITE4PRE:
          nextCommand = CMD_WRITE4PRE;
          break;

        case CMD_ACT_READ_PRE_PDPF:
          nextCommand = CMD_READ_PRE_PDPF;
          break;
          
        case CMD_ACT_WRITE_PRE_PDPF:
          nextCommand = CMD_WRITE_PRE_PDPF;
          break;

        case CMD_NOP:
          nextCommand = CMD_NOP;
          break;

        default:
          std::cout << "Warning: Activate: Unknown bulk command: " << request->bulkCmd << std::endl;
          nextCommand = CMD_NOP;
          break;
        }


      activates++;

      returnValue = true;
    }
  else if( state == BANK_OPEN )
    {
      std::cout << "NVMain: Bank: Attempted to activate open row!" << std::endl;
    }

  return returnValue;
}



bool Bank::Read( NVMainRequest *request )
{
  uint64_t readRow, readCol;
  bool returnValue = false;

  request->address.GetTranslatedAddress( &readRow, &readCol, NULL, NULL, NULL );

  if( nextRead <= currentCycle && state == BANK_OPEN && readRow == openRow )
    {
      nextPrecharge = MAX( nextPrecharge, currentCycle + conf->GetValue( "tAL" ) + conf->GetValue( "tBURST" )
               + conf->GetValue( "tRTP" ) - conf->GetValue( "tCCD" ) );
      nextRead = MAX( nextRead, currentCycle + MAX( conf->GetValue( "tBURST" ), conf->GetValue( "tCCD" ) ) );
      nextWrite = MAX( nextWrite, currentCycle + conf->GetValue( "tCAS" ) + conf->GetValue( "tBURST" ) 
               + 2 - conf->GetValue( "tCWD" ) );
      nextActivate = MAX( nextActivate, lastActivate + conf->GetValue( "tRRDR" ) );
      nextPowerDown = MAX( nextPowerDown, currentCycle + conf->GetValue( "tAL" ) + conf->GetValue( "tBURST" )
               + conf->GetValue( "tCAS" ) + 1 );
      nextPowerDownDone = MAX( nextPowerDownDone, currentCycle + conf->GetValue( "tAL" ) + conf->GetValue( "tBURST" )
                   + conf->GetValue( "tCAS" ) + conf->GetValue( "tPD" ) + 1 );
      

      bankGraph->SetLabel( currentCycle, currentCycle + conf->GetValue( "tBURST" ), 'R' );
      dataCycles += conf->GetValue( "tBURST" );


      if( request->issueInterconnect != NULL && bankId == 0 )
        {
          notifyComplete.insert(std::pair<NVMainRequest *, int>( request, conf->GetValue( "tBURST" ) + conf->GetValue( "tCAS" ) ) );
        }


      if( conf->KeyExists( "EnergyModel" ) && conf->GetString( "EnergyModel" ) == "current" )
        {
          bankEnergy += (float)((conf->GetValue( "EIDD4R" ) - conf->GetValue( "EIDD3N" )) * conf->GetValue( "tBURST" ));

          burstEnergy += (float)((conf->GetValue( "EIDD4R" ) - conf->GetValue( "EIDD3N" )) * conf->GetValue( "tBURST" ));
        }
      else
        {
          bankEnergy += conf->GetEnergy( "Eopenrd" );

          burstEnergy += conf->GetEnergy( "Eopenrd" );
        }

      /*
       *  There's no reason to track data if endurance is not modeled.
       */
      if( conf->GetSimInterface( ) != NULL && endrModel != NULL )
        {
          NVMDataBlock dataBlock;

          /*
           *  In a trace-based simulation, or a live simulation where simulation is
           *  started in the middle of execution, some data being read maybe have never
           *  been written to memory. In this case, we will store the value, since the
           *  data is always correct from either the simulator or the value in the trace,
           *  which corresponds to the actual value that was read. 
           *
           *  Since the read data is already in the request, we don't actually need to
           *  copy it there.
           */
          if( !conf->GetSimInterface( )->GetDataAtAddress( request->address.GetPhysicalAddress( ), &dataBlock ) )
            {
              //std::cout << "Setting data block for 0x" << std::hex << request->address.GetPhysicalAddress( )
              //          << std::dec << " to " << request->data << std::endl;
              conf->GetSimInterface( )->SetDataAtAddress( request->address.GetPhysicalAddress( ), request->data );
            }
        }


      switch( request->bulkCmd )
        {
        case CMD_READPRE:
          nextCommand = CMD_PRE;
          break;

        case CMD_READ2PRE:
          nextCommand = CMD_READPRE;
          break;

        case CMD_READ3PRE:
          nextCommand = CMD_READ2PRE;
          break;

        case CMD_READ4PRE:
          nextCommand = CMD_READ3PRE;
          break;

        case CMD_READ_PRE_PDPF:
          nextCommand = CMD_PRE_PDPF;
          break;

        case CMD_NOP:
          nextCommand = CMD_NOP;
          break;

        default:
          std::cout << "Warning: Read: Unknown bulk command: " << request->bulkCmd << std::endl;
          nextCommand = CMD_NOP;
          break;
        }


      reads++;
      
      returnValue = true;
    }


  return returnValue;
}



bool Bank::Write( NVMainRequest *request )
{
  uint64_t writeRow, writeCol;
  bool returnValue = false;

  request->address.GetTranslatedAddress( &writeRow, &writeCol, NULL, NULL, NULL );

  if( nextWrite <= currentCycle && state == BANK_OPEN && writeRow == openRow )
    {
      nextPrecharge = MAX( nextPrecharge, currentCycle + conf->GetValue( "tAL" ) + conf->GetValue( "tCWD" )
               + conf->GetValue( "tBURST" ) + conf->GetValue( "tWR" ) );
      nextRead = MAX( nextRead, currentCycle + conf->GetValue( "tCWD" ) 
              + conf->GetValue( "tBURST" ) + conf->GetValue( "tWTR" ) );
      nextWrite = MAX( nextWrite, currentCycle + MAX( conf->GetValue( "tBURST" ), conf->GetValue( "tCCD" ) ) );
      nextPowerDown = MAX( nextPowerDown, currentCycle + conf->GetValue( "tAL" ) + conf->GetValue( "tBURST" )
               + conf->GetValue( "tWR" ) + conf->GetValue( "tCWD" ) + 1 );
      nextPowerDownDone = MAX( nextPowerDownDone, currentCycle + conf->GetValue( "tAL" ) + conf->GetValue( "tBURST" )
                   + conf->GetValue( "tWR" ) + conf->GetValue( "tCWD" ) + conf->GetValue( "tPD" ) + 1 );

      bankGraph->SetLabel( currentCycle, currentCycle + conf->GetValue( "tBURST" ), 'W' );
      dataCycles += conf->GetValue( "tBURST" );


      if( request->issueInterconnect != NULL && bankId == 0 )
        {
          notifyComplete.insert(std::pair<NVMainRequest *, int>( request, conf->GetValue( "tBURST" ) + conf->GetValue( "tCWD" ) ) );
        }


      if( conf->KeyExists( "EnergyModel" ) && conf->GetString( "EnergyModel" ) == "current" )
        {
          bankEnergy += (float)((conf->GetValue( "EIDD4W" ) - conf->GetValue( "EIDD3N" )) * conf->GetValue( "tBURST" ));

          burstEnergy += (float)((conf->GetValue( "EIDD4W" ) - conf->GetValue( "EIDD3N" )) * conf->GetValue( "tBURST" ));
        }
      else
        {
          bankEnergy += conf->GetEnergy( "Ewr" ); // - conf->GetEnergy( "Ewrpb" );

          burstEnergy += conf->GetEnergy( "Ewr" ); // - conf->GetEnergy( "Ewrpb" );
        }

      writeCycle = true;

      writes++;
      
      if( endrModel && bankId == 0 )
        {
          NVMDataBlock oldData;

          if( conf->GetSimInterface( ) != NULL )
            {
              /*
               *  If the old data is not there, we will assume the data is 0.
               */
              uint64_t wordSize;
              uint64_t blockMask;
              uint64_t blockAddr;
              bool hardError;

              wordSize = conf->GetValue( "BusWidth" );
              wordSize *= conf->GetValue( "tBURST" ) * conf->GetValue( "RATE" );
              wordSize /= 8;

              blockMask = ~(wordSize - 1);

              blockAddr = request->address.GetPhysicalAddress( ) & blockMask;
              //std::cout << "Address is 0x" << std::hex << request->address.GetPhysicalAddress() << " block mask is 0x" << blockMask << " block address is 0x" << blockAddr << std::dec << std::endl;

              if( !conf->GetSimInterface( )->GetDataAtAddress( request->address.GetPhysicalAddress( ), &oldData ) )
                {
                  // maybe increment a counter for this...
                  //std::cout << "Zeroing write to not-previously-read data 0x" << std::hex
                  //          << request->address.GetPhysicalAddress( ) << std::dec << std::endl;

                  for( int i = 0; i < (int)(conf->GetValue( "BusWidth" ) / 8); i++ )
                    oldData.SetByte( i, 0 );
                }
          
              /*
               *  Write the new data...
               */
              conf->GetSimInterface( )->SetDataAtAddress( request->address.GetPhysicalAddress( ), request->data );
     
              //std::cout << "Changing data at address 0x" << std::hex << request->address.GetPhysicalAddress( )
              //          << std::dec << " from " << oldData << " to " << request->data << std::endl;

              /*
               *  Model the endurance 
               */
              hardError = !endrModel->Write( request->address, oldData, request->data );

              if( hardError )
                {
                  std::cout << "WARNING: Write to 0x" << std::hex << request->address.GetPhysicalAddress( )
                            << std::dec << " resulted in a hard error! " << std::endl;

                  NVMNetMessage *netMsg = new NVMNetMessage( );

                  netMsg->SetDestination( NVMNETDEST_LOCAL_MC );
                  netMsg->SetAddress( request->address );
                  netMsg->SetMessage( NVMNETMSGTYPE_HARD_ERROR );
                  netMsg->SetDirection( NVMNETDIR_BCAST );

                  SendMessage( netMsg ); 
                }
            }
          else
            {
              std::cerr << "Warning: Endurance modeled without simulator interface for data tracking!" << std::endl;
            }
        }

      switch( request->bulkCmd )
        {
        case CMD_WRITEPRE:
          nextCommand = CMD_PRE;
          break;

        case CMD_WRITE2PRE:
          nextCommand = CMD_WRITEPRE;
          break;

        case CMD_WRITE3PRE:
          nextCommand = CMD_WRITE2PRE;
          break;

        case CMD_WRITE4PRE:
          nextCommand = CMD_WRITE3PRE;
          break;

        case CMD_WRITE_PRE_PDPF:
          nextCommand = CMD_PRE_PDPF;
          break;

        case CMD_NOP:
          nextCommand = CMD_NOP;
          break;

        default:
          std::cout << "Warning: Write: Unknown bulk command: " << request->bulkCmd << std::endl;
          nextCommand = CMD_NOP;
          break;
        }



      returnValue = true;
    }



  return returnValue;
}



bool Bank::Precharge( NVMainRequest *request )
{
  bool returnValue = false;


  if( nextPrecharge <= currentCycle && state == BANK_OPEN  )
    {
      nextActivate = MAX( nextActivate, currentCycle + conf->GetValue( "tRP" ) );
      nextPowerDown = MAX( nextPowerDown, currentCycle + conf->GetValue( "tRP" ) );
      nextPowerDownDone = MAX( nextPowerDownDone, currentCycle + conf->GetValue( "tRP" ) 
                   + conf->GetValue( "tPD" ) );


      if( request->issueInterconnect != NULL && bankId == 0 )
        {
          notifyComplete.insert(std::pair<NVMainRequest *, int>( request, conf->GetValue( "tRP" ) ) );
        }


      bankGraph->SetLabel( currentCycle, currentCycle + conf->GetValue( "tRP" ), 'P' );

      switch( nextCommand )
        {
        case CMD_PRE:
          nextCommand = CMD_NOP;
          break;

        case CMD_PRE_PDPF:
          nextCommand = CMD_PDPF;
          break;

        case CMD_NOP:
          nextCommand = CMD_NOP;
          break;

        default:
          std::cout << "Warning: Prechange: Unknown bulk command: " << nextCommand << std::endl;
          nextCommand = CMD_NOP;
          break;
        }

      state = BANK_CLOSED;

      returnValue = true;
    }


  return returnValue;
}


bool Bank::Refresh( )
{
  bool returnValue = false;

  if( nextRefresh <= currentCycle && state == BANK_CLOSED )
    {
      nextActivate = MAX( nextActivate, currentCycle + refreshRows * conf->GetValue( "tRCD" ) );

      state = BANK_REFRESHING;
      nextRefreshDone = currentCycle + refreshRows * conf->GetValue( "tRCD" );
       
      refreshRowIndex = (refreshRowIndex + refreshRows) % conf->GetValue( "ROWS" );

      /*
       *  tRFI is the minimum refresh time for all rows. If we don't refresh the entire
       *  bank we need to calculate when the next refresh will occur.. 
       */
      nextRefresh = currentCycle + ((conf->GetValue( "tRFI" )) / (conf->GetValue( "ROWS" ) / refreshRows));
      
      if( conf->KeyExists( "EnergyModel" ) && conf->GetString( "EnergyModel" ) == "current" )
        {
          bankEnergy += (float)((conf->GetValue( "EIDD5B" ) - conf->GetValue( "EIDD3N" )) * conf->GetValue( "tRCD" ) * refreshRows); 

          refreshEnergy += (float)((conf->GetValue( "EIDD5B" ) - conf->GetValue( "EIDD3N" )) * conf->GetValue( "tRCD" ) * refreshRows); 
        }
      else
        {
          bankEnergy += conf->GetEnergy( "Eref" );

          refreshEnergy += conf->GetEnergy( "Eref" );
        }

      returnValue = true;
    }

  return returnValue;
}


bool Bank::IsIssuable( MemOp *mop, ncycle_t delay )
{
  uint64_t opRank;
  uint64_t opBank;
  uint64_t opRow;
  uint64_t opCol;
  bool rv = true;

  mop->GetAddress( ).GetTranslatedAddress( &opRow, &opCol, &opBank, &opRank, NULL );

  if( nextCommand != CMD_NOP )
    return false;
    

  if( mop->GetOperation( ) == ACTIVATE )
    {
      if( nextActivate > (currentCycle+delay) || state != BANK_CLOSED )
        rv = false;

      if( refreshUsed && (currentCycle+delay) >= nextRefresh )
        rv = false;

      if( rv == false )
        {
          if( nextActivate > (currentCycle+delay) )
            {
              //std::cout << "Bank: Can't activate for " << nextActivate - currentCycle << " cycles." << std::endl;
              actWaits++;
              actWaitTime += nextActivate - (currentCycle+delay);
            }
        }
    }
  else if( mop->GetOperation( ) == READ )
    {
      if( nextRead > (currentCycle+delay) || state != BANK_OPEN || opRow != openRow )
        rv = false;
    }
  else if( mop->GetOperation( ) == WRITE )
    {
      if( nextWrite > (currentCycle+delay) || state != BANK_OPEN || opRow != openRow )
        rv = false;
    }
  else if( mop->GetOperation( ) == PRECHARGE )
    {
      if( nextPrecharge > (currentCycle+delay) || state != BANK_OPEN )
        rv = false;
    }
  else if( mop->GetOperation( ) == POWERDOWN_PDA || mop->GetOperation( ) == POWERDOWN_PDPF
       || mop->GetOperation( ) == POWERDOWN_PDPS )
    {
      if( nextPowerDown > (currentCycle+delay) || ( state != BANK_OPEN && state != BANK_CLOSED ) )
        rv = false;
    }
  else if( mop->GetOperation( ) == POWERUP )
    {
      if( nextPowerUp > (currentCycle+delay) || ( state != BANK_PDPF && state != BANK_PDPS && state != BANK_PDA ) )
        rv = false;
    }
  else if( mop->GetOperation( ) == REFRESH )
    {
      if( nextRefresh > (currentCycle+delay) || state != BANK_CLOSED )
        rv = false;
    }
  else
    {
      std::cout << "Bank: IsIssuable: Unknown operation: " << mop->GetOperation( ) << std::endl;
      rv = false;
    }

  return rv;
}


bool Bank::WouldConflict( uint64_t checkRow )
{
  bool returnValue = true;

  if ( state == BANK_OPEN && checkRow == openRow )
    returnValue = false;

  return returnValue;
}



BankState Bank::GetState( ) 
{
  return state;
}


float Bank::GetPower( )
{
  /*
   *
   */
  float simulationTime = (float)((float)currentCycle / ((float)conf->GetValue( "CLK" ) * 1000000.0f));
  float power = 0.0f;

  if( simulationTime == 0.0f )
    return 0.0f;

  if( conf->KeyExists( "EnergyModel" ) && conf->GetString( "EnergyModel" ) == "current" )
    {
      power = ((backgroundEnergy / (float)currentCycle) * conf->GetEnergy( "Voltage" )) / 1000.0f;
      power = ((activeEnergy / (float)currentCycle) * conf->GetEnergy( "Voltage" )) / 1000.0f;
      power = ((refreshEnergy / (float)currentCycle) * conf->GetEnergy( "Voltage" )) / 1000.0f;
      power = ((bankEnergy / (float)currentCycle) * conf->GetEnergy( "Voltage" )) / 1000.0f;
    }
  else
    power = ((bankEnergy / 1000000.0f) / simulationTime);

  return power;
}



void Bank::SetName( std::string name )
{
  std::string graphLabel;

  graphLabel = "BANK ";

  for( unsigned int i = 0; i < 6; i++ )
    {
      if( i >= name.length( ) )
        graphLabel += ' ';
      else
        graphLabel += name[i];
    }

  bankGraph->SetGraphLabel( graphLabel );
}


// Corresponds to physical bank id e.g., if this bank logically spans multiple devices, the id corresponds to the device, NOT the logical bank id within a single device.
void Bank::SetId( int id )
{
  bankId = id;
}



void Bank::PrintStats( )
{
  float idealBandwidth;


  idealBandwidth = (float)(conf->GetValue( "CLK" ) * conf->GetValue( "MULT" ) * 
                   conf->GetValue( "RATE" ) * conf->GetValue( "BPC" ));

  if( activeCycles != 0 )
    utilization = (float)((float)dataCycles / (float)activeCycles);
  else
    utilization = 0.0f;


  if( conf->KeyExists( "EnergyModel" ) && conf->GetString( "EnergyModel" ) == "current" )
    {
      std::cout << "i" << psInterval << "." << statName << ".current " << bankEnergy << "\t; mA" << std::endl;
      std::cout << "i" << psInterval << "." << statName << ".current.background " << backgroundEnergy << "\t; mA" << std::endl;
      std::cout << "i" << psInterval << "." << statName << ".current.active " << activeEnergy << "\t; mA" << std::endl;
      std::cout << "i" << psInterval << "." << statName << ".current.burst " << burstEnergy << "\t; mA" << std::endl;
      std::cout << "i" << psInterval << "." << statName << ".current.refresh " << refreshEnergy << "\t; mA" << std::endl;
    }
  else
    {
      std::cout << "i" << psInterval << "." << statName << ".energy " << bankEnergy << "\t; nJ" << std::endl; 
      std::cout << "i" << psInterval << "." << statName << ".energy.background " << backgroundEnergy << "\t; nJ" << std::endl;
      std::cout << "i" << psInterval << "." << statName << ".energy.active " << activeEnergy << "\t; nJ" << std::endl;
      std::cout << "i" << psInterval << "." << statName << ".energy.burst " << burstEnergy << "\t; nJ" << std::endl;
      std::cout << "i" << psInterval << "." << statName << ".energy.refresh " << refreshEnergy << "\t; nJ" << std::endl;
    }
  
  std::cout << "i" << psInterval << "." << statName << ".power " << GetPower( ) << "\t; W per bank per device" << std::endl
            << "i" << psInterval << "." << statName << ".bandwidth " << (utilization * idealBandwidth) << "\t; MB/s "
            << "i" << psInterval << "." << statName << "(" << dataCycles << " data cycles in " << activeCycles << " cycles)" << std::endl
            << "i" << psInterval << "." << statName << ".utilization " << utilization << std::endl;
  std::cout << "i" << psInterval << "." << statName << ".reads " << reads << std::endl
            << "i" << psInterval << "." << statName << ".writes " << writes << std::endl
            << "i" << psInterval << "." << statName << ".activates " << activates << std::endl;
  std::cout << "i" << psInterval << "." << statName << ".activeCycles " << powerCycles << std::endl
            << "i" << psInterval << "." << statName << ".fastExitCycles " << feCycles << std::endl
            << "i" << psInterval << "." << statName << ".slowExitCycles " << seCycles << std::endl;

  if( endrModel )
    {
      if( endrModel->GetWorstLife( ) == std::numeric_limits< uint64_t >::max( ) )
        std::cout << "i" << psInterval << "." << statName << ".worstCaseEndurance N/A" << std::endl
                  << "i" << psInterval << "." << statName << ".averageEndurance N/A" << std::endl;
      else
        std::cout << "i" << psInterval << "." << statName << ".worstCaseEndurance " << (endrModel->GetWorstLife( )) << std::endl
                  << "i" << psInterval << "." << statName << ".averageEndurance " << endrModel->GetAverageLife( ) << std::endl;

      endrModel->PrintStats( );
    }

  std::cout << "i" << psInterval << "." << statName << ".actWaits " << actWaits << std::endl
            << "i" << psInterval << "." << statName << ".actWaits.totalTime " << actWaitTime << std::endl
            << "i" << psInterval << "." << statName << ".actWaits.averageTime " << (double)((double)actWaitTime / (double)actWaits) << std::endl;

  psInterval++;
}


bool Bank::Idle( )
{
  if( nextPrecharge <= currentCycle && nextActivate <= currentCycle
      && nextRead <= currentCycle && nextWrite <= currentCycle )
    {
      return true;
    }

  return false;
}


void Bank::Cycle( )
{
  /*
   *  Check for implicit commands to issue.
   */
  if( nextCommand != CMD_NOP )
    {
      MemOp mop;
      BulkCommand bulkCmd;

      mop.SetAddress( lastOperation.address );
      
      switch( nextCommand )
        {
        case CMD_PDPF:
          mop.SetOperation( POWERDOWN_PDPF );
          lastOperation.type = POWERDOWN_PDPF;
          break;

        case CMD_ACT_READ_PRE_PDPF:
        case CMD_ACT_WRITE_PRE_PDPF:
        case CMD_ACTREADPRE:
        case CMD_ACTWRITEPRE:
          mop.SetOperation( ACTIVATE );
          lastOperation.type = ACTIVATE;
          break;

        case CMD_PRE:
        case CMD_PRE_PDPF:
          mop.SetOperation( PRECHARGE );
          lastOperation.type = PRECHARGE;
          break;

        case CMD_READPRE:
        case CMD_READ2PRE:
        case CMD_READ3PRE:
        case CMD_READ4PRE:
        case CMD_READ_PRE_PDPF:
          mop.SetOperation( READ );
          lastOperation.type = READ;
          break;

        case CMD_WRITEPRE:
        case CMD_WRITE2PRE:
        case CMD_WRITE3PRE:
        case CMD_WRITE4PRE:
        case CMD_WRITE_PRE_PDPF:
          mop.SetOperation( WRITE );
          lastOperation.type = WRITE;
          break;

        default:
          std::cout << "Warning: Invalid nextCommand: " << nextCommand << std::endl;
          break;
        }
      

      /*
       *  IsIssuable returns false if nextCommand != CMD_NOP. This is meant for other
       *  bulk commands being issued from the interconnect. Here it is temporarily
       *  set to CMD_NOP before calling IsIssuable.
       */
      bulkCmd = nextCommand;
      nextCommand = CMD_NOP;
      if( IsIssuable( &mop ) )
        {
          nextCommand = bulkCmd;
          lastOperation.bulkCmd = bulkCmd;

          switch( mop.GetOperation( ) )
            {
            case PRECHARGE:
              Precharge( &lastOperation );
              break;

            case READ:
              Read( &lastOperation );
              break;

            case WRITE:
              Write( &lastOperation );
              break;

            case ACTIVATE:
              Activate( &lastOperation );
              break;

            case POWERDOWN_PDA:
              PowerDown( BANK_PDAWAIT );
              break;

            case POWERDOWN_PDPF:
              PowerDown( BANK_PDPFWAIT );
              break;
              
            case POWERDOWN_PDPS:
              PowerDown( BANK_PDPSWAIT );
              break;
          
            case REFRESH:
              Refresh( );
              break;

            default:
              break;
            }
        }
      else
        {
          nextCommand = bulkCmd;
        }
    }

  
  /*
  if( idleTimer > 10 && state == BANK_CLOSED )
    {
      MemOp testOp;

      testOp.SetOperation( POWERDOWN_PDPF );
      if( IsIssuable( &testOp ) )
        PowerDown( BANK_PDPSWAIT );
    }
  */

  /*
   *  Check if a power down wait state is complete.
   */
  if( nextPowerDownDone <= currentCycle )
    {
      if( state == BANK_PDPFWAIT )
        state = BANK_PDPF;
      else if( state == BANK_PDPSWAIT )
        state = BANK_PDPS;
      else if( state == BANK_PDAWAIT )
        state = BANK_PDA;
    }

  /*
   *  If we are in a powered-down state, count the number of cycles spent in each state.
   */
  if( state == BANK_PDPF || state == BANK_PDA )
    feCycles++;
  else if( state == BANK_PDPS )
    seCycles++;
  else
    powerCycles++;


  /*
   *  Check if a refresh is complete.
   */
  if( state == BANK_REFRESHING && nextRefreshDone <= currentCycle )
    state = BANK_CLOSED;


  /*
   *  Automatically refresh if we need to.
   */
  if( state == BANK_CLOSED && nextRefresh <= currentCycle && refreshUsed )
    {
      Refresh( );
    }


  /*
   *  The output graph labeller only has a buffer of about
   *  100 characters, so we need to do something like this
   *  for long operations like refresh...
   */
  if( state == BANK_REFRESHING )
    {
      bankGraph->SetLabel( currentCycle, currentCycle + 1, 'F' );
    }


  currentCycle++;

  /*
   *  Count non-idle cycles for utilization calculations
   */
  if( !Idle( ) )
    {
      activeCycles++;
    }
  else
    {
      idleTimer++;
    }



  /* Inhibit some graph output by default. */
  if( conf->KeyExists( "PrintAllDevices" ) && conf->GetString( "PrintAllDevices" ) == "true" )
    {
      bankGraph->Cycle( );
    }
  else
    {
      if( bankId == 0 )
        bankGraph->Cycle( );
    }




  /* Notify memory controllers of completed commands. */
  std::map<NVMainRequest *, int>::iterator it;

  for( it = notifyComplete.begin( ); it != notifyComplete.end( ); it++ )
    {
      if( it->second == 0 )
        {
          it->first->issueInterconnect->RequestComplete( it->first );
          notifyComplete.erase( it );
        }
      else
        {
          it->second = it->second - 1;
        }
    }
}


