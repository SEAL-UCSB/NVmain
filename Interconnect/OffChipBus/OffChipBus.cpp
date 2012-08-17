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


float OffChipBus::CalculateIOPower( bool isRead, unsigned int bitValue )
{
  unsigned int Rtt_nom, Rtt_wr, Rtt_cont;
  float VDDQ, VSSQ;
  unsigned int ranksPerDimm;
  float Pdq; 

  Pdq = 0.0f;

  if( conf->KeyExists( "Rtt_nom" ) && conf->KeyExists( "Rtt_wr" ) && conf->KeyExists( "Rtt_cont" )
      && conf->KeyExists( "Vddq" ) && conf->KeyExists( "Vssq" ) )
    {
      Rtt_nom  = conf->GetValue( "Rtt_nom" );
      Rtt_wr   = conf->GetEnergy( "Rtt_wr" );
      Rtt_cont = conf->GetEnergy( "Rtt_conf" );

      VDDQ = conf->GetEnergy( "Vddq" );
      VSSQ = conf->GetEnergy( "Vssq" );
    }
  else
    {
      Rtt_nom  = 30; /* Default to 30 ohms for read. This means 60 ohms for pull up and pull down. */
      Rtt_wr   = 60; /* Default to 120 ohms for write. This means 240 ohms for pull up and pull down. */
      Rtt_cont = 75; /* Default to 75 ohms for termination at the controller. This means 150 ohms for pull up and pull down. */

      VDDQ = 1.5; /* Default 1.5 Volts */
      VSSQ = 0.0; /* Default 0 Volts */
    }

  if( conf->KeyExists( "RanksPerDIMM" ) )
    {
      ranksPerDimm = conf->GetValue( "RanksPerDIMM" );
    }
  else
    {
      ranksPerDimm = 1;
    }


  if( ranksPerDimm == 1 )
    {
      if( isRead )
        {
          /* Simple resistor network. Calculate the bus voltage, then current, and finally power. */
          float Rttpu, Rttpd, Rdevice, Rs, Ron;
          float Vread;

          Rs = 15.0;  /* Series resistance of device. */
          Ron = 34.0; /* Resistance of device output driver. */

          Rttpu = static_cast<float>(Rtt_nom * 2.0);
          Rttpd = static_cast<float>(Rtt_nom * 2.0);
          Rdevice = Rs + Ron;

          Vread = (bitValue == 0) ? VSSQ : VDDQ;

          /* Bus voltage equation */
          float Vbus;

          Vbus = ( (VDDQ / Rttpu) + (VSSQ / Rttpd) + (Vread / Rdevice) ) / ( (1.0 / Rttpu) + (1.0 / Rttpd) + (1.0 / Rdevice) );

          /* Bus current equation. */
          float Ibus, Ipu, Ipd;

          Ipu = (VDDQ - Vbus) / Rttpu; /* Current through controller pull up. */
          Ipd = (Vbus - VSSQ) / Rttpd; /* Current through controller pull down. */
          Ibus = (Vbus - Vread) / Rdevice; /* Current sourced/sinked by device. */

          /* Power calculation equation. */
          Pdq = Ipu * Ipu * Rttpu + Ipd * Ipd * Rttpd + Ibus * Ibus * Rdevice;
        }
      else
        {
          float Rttpu, Rttpd, Rdevice, Rs, Ron;
          float Vwrite;

          Rs = 15.0;  /* Series resistance of device. */
          Ron = 34.0; /* Resistance of device output driver. */

          Rttpu = static_cast<float>(Rtt_wr * 2.0);
          Rttpd = static_cast<float>(Rtt_wr * 2.0);
          Rdevice = Rs + Ron;

          Vwrite = (bitValue == 0) ? VSSQ : VDDQ;

          /* Bus voltage equation. */
          float Vbus;

          Vbus = ( (VDDQ / Rttpu) + (VSSQ / Rttpd) + (Vwrite / Rdevice) ) / ( (1.0 / Rttpu) + (1.0 / Rttpd) + (1.0 / Rdevice) );

          /* Bus current equation. */
          float Ibus, Ipu, Ipd;

          Ipu = (VDDQ - Vbus) / Rttpu;
          Ipd = (Vbus - VSSQ) / Rttpd;
          Ibus = (Vbus - Vwrite) / Rdevice;

          /* Power calculation equation. */
          Pdq = Ipu * Ipu * Rttpu + Ipd * Ipd * Rttpd + Ibus * Ibus * Rdevice;
        }
    }
  /*
   *  For 3 and 4 ranks per dimm, we assume one rank is terminated, and the remaining 1 or 2 ranks have ODT off (i.e., High-Z) 
   */
  else if( ranksPerDimm == 2 || ranksPerDimm == 3 || ranksPerDimm == 4 ) 
    {
      if( isRead )
        {
          /* Calculate using delta-wye transformation and then solving for bus and terminated rank voltages, followed by currents and power. */
          float R1, R2, R3, R4, R5;
          float Rttpu, Rttpd, Rothpu, Rothpd, Rdevice, Rs, Ron;

          Rs = 15.0;  /* Series resistance of device. */
          Ron = 34.0; /* Resistance of device output driver. */

          Rdevice = Rs + Ron;

          /* Pull-up/Pull-down at controller. */
          Rttpu = static_cast<float>(Rtt_cont * 2.0);
          Rttpd = static_cast<float>(Rtt_cont * 2.0);

          /* Pull-up/Pull-down at terminated rank. */
          Rothpu = static_cast<float>(Rtt_nom * 2.0);
          Rothpd = static_cast<float>(Rtt_nom * 2.0);

          if( bitValue == 0 )
            {
              R1 = Rttpu;
              R2 = 1.0 / ( (1.0 / Rttpu) + (1.0 / Rdevice) ); /* Device resistors are in parallel with controller pull-down. */
            }
          else
            {
              R1 = 1.0 / ( (1.0 / Rttpu) + (1.0 / Rdevice) ); /* Device resistors are in parallel with controller pull-up. */
              R2 = Rttpd;
            }

          R3 = Rs;
          R4 = Rothpu;
          R5 = Rothpd;

          /* Delta-wye transformation */
          float RP, RA, RB, RC;
          RP = R3*R4 + R4*R5 + R5*R3;
          RA = RP / R3;
          RB = RP / R5;
          RC = RP / R4;

          /* Combine parallel resistors. */
          float RX, RY;
          RX = 1.0 / ( (1.0 / R1) + (1.0 / RB) );
          RY = 1.0 / ( (1.0 / R2) + (1.0 / RC) );

          /* Bus voltage calculation. */
          float Vbus, Ibus;

          Ibus = (VDDQ - VSSQ) / (RX + RY);
          Vbus = VSSQ + Ibus * RY;

          /* Voltage at terminated rank. */
          float Vterm;

          Vterm = -1.0 * R3 * ( ((VDDQ - Vbus) / R1) - ((Vbus - VSSQ) / R2) ) + Vbus;

          /* Current through each resistor. */
          float I1, I2, I3, I4, I5;

          I1 = (VDDQ - Vbus) / R1;
          I2 = (Vbus - VSSQ) / R2;
          I3 = (Vbus - Vterm) / R3;
          I4 = (VDDQ - Vterm) / R4;
          I5 = (Vterm - VSSQ) / R5;

          /* Power calculation equation. */
          Pdq = I1 * I1 * R1 + I2 * I2 * R2 + I3 * I3 * R3 + I4 * I4 * R4 + I5 * I5 * R5;
        }
      else
        {
          /* Calculate using two delta-wye transformations and then solving for bus and termated rank voltages, followed by currents and power. */
          float Rothpu, Rothpd, Rttpu, Rttpd, Rs, Ron;
          float Vwrite;

          Rttpu = static_cast<float>(Rtt_wr * 2.0);
          Rttpd = static_cast<float>(Rtt_wr * 2.0);
          Rothpu = static_cast<float>(Rtt_nom * 2.0);
          Rothpd = static_cast<float>(Rtt_nom * 2.0);

          Rs = 15.0;  /* Series resistance of DRAM device. */
          Ron = 34.0; /* Controller output driver resistance. */

          Vwrite = (bitValue == 0) ? VSSQ : VDDQ;

          /* Do delta-wye transforms. */
          float RAL, RBL, RCL, RAR, RBR, RCR;
          float RPL, RPR;

          RPL = Rothpu*Rothpd + Rothpd*Rs + Rs*Rothpu;
          RAL = RPL / Rothpd;
          RBL = RPL / Rothpu;
          RCL = RPL / Rs;

          RPR = Rttpu*Rttpd + Rttpd*Rs + Rs*Rttpu;
          RAR = RPR / Rttpd;
          RBR = RPR / Rttpu;
          RCR = RPR / Rs;

          /* Calculate bus voltage. */
          float Vbus;

          Vbus = ( (VSSQ / RBL) + (VSSQ / RBR) + (VDDQ / RAL) + (VDDQ / RAR) + (Vwrite / Ron) )
               / ( (1.0f / RBL) + (1.0f / RBR) + (1.0f / RAL) + (1.0f / RAR) + (1.0f / Ron) );

          /* Calculate terminated node voltages. */
          float Vterm, Voterm;

          Vterm  = ( (VDDQ / Rttpu) + (VSSQ / Rttpd) + (Vbus / Rs) ) / ( (1.0f / Rttpu) + (1.0f / Rttpd) + (1.0f / Rs) );
          Voterm = ( (VDDQ / Rothpu) + (VSSQ / Rothpd) + (Vbus / Rs ) ) / ( (1.0f / Rothpu) + (1.0f / Rothpd) + (1.0f / Rs) );


          /* Calculate resistor currents. */
          float Ittpu, Ittpd, Irs1, Irs2, Iothpu, Iothpd, Ibus;

          Ittpu = (VDDQ - Vterm) / Rttpu;
          Ittpd = (Vterm - VSSQ) / Rttpd;
          Iothpu = (VDDQ - Voterm) / Rothpu;
          Iothpd = (Voterm - VSSQ) / Rothpd;
          Irs1 = (Vbus - Vterm) / Rs;
          Irs2 = (Vbus - Voterm) / Rs;
          Ibus = (Vwrite - Vbus) / Ron;

          /* Calculate total power. */
          Pdq = Ittpu*Ittpu*Rttpu + Ittpd*Ittpd*Rttpd + Iothpu*Iothpu*Rothpu + Iothpd*Iothpd*Rothpd
              + Irs1*Irs1*Rs + Irs2*Irs2*Rs + Ibus*Ibus*Ron;
        }
    }


  return Pdq;
}

