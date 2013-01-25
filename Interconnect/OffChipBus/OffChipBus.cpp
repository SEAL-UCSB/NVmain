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
  conf = NULL;
  ranks = NULL;
  configSet = false;
  numRanks = 0;
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

  Params *params = new Params( );
  params->SetParams( c );
  SetParams( params );

  conf = c;
  configSet = true;

  if( p->OffChipLatency_set )
    offChipDelay = p->OffChipLatency;
  else
    offChipDelay = 10;

  numRanks = p->RANKS;

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

      /* SetConfig recursively. */
      ranks[i]->SetConfig( conf );
    }
}



bool OffChipBus::RequestComplete( NVMainRequest *request )
{
  GetEventQueue( )->InsertEvent( EventResponse, GetParent( )->GetTrampoline( ), request, GetEventQueue()->GetCurrentCycle() + offChipDelay ); 

  return true;
}


bool OffChipBus::IssueCommand( NVMainRequest *req )
{
  uint64_t opRank;
  uint64_t opBank;
  uint64_t opRow;
  uint64_t opCol;
  bool success = false;

  if( !configSet || numRanks == 0 )
    {
      std::cerr << "Error: Issued command before memory system was configured!"
                << std::endl;
      return false;
    }

  req->address.GetTranslatedAddress( &opCol, &opRow, &opBank, &opRank, NULL );

  if( ranks[opRank]->IsIssuable( req ) )
    {
      if( req->type == 0 )
        {
          std::cout << "OffChipBus got unknown op." << std::endl;
        }

      success = GetChild( req )->IssueCommand( req );

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


bool OffChipBus::IsIssuable( NVMainRequest *req, FailReason *reason )
{
  uint64_t opRank;
  uint64_t opBank;
  uint64_t opRow;
  uint64_t opCol;

  req->address.GetTranslatedAddress( &opCol, &opRow, &opBank, &opRank, NULL );

  return ranks[opRank]->IsIssuable( req, reason );
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


void OffChipBus::Cycle( ncycle_t )
{
}


float OffChipBus::CalculateIOPower( bool isRead, unsigned int bitValue )
{
  unsigned int Rtt_nom, Rtt_wr, Rtt_cont;
  float VDDQ, VSSQ;
  unsigned int ranksPerDimm;
  float Pdq; 

  Pdq = 0.0f;

  if( p->Rtt_nom_set && p->Rtt_wr_set && p->Rtt_cont_set && p->Vddq_set && p->Vssq_set )
    {
      Rtt_nom  = p->Rtt_nom;
      Rtt_wr   = p->Rtt_wr;
      Rtt_cont = p->Rtt_cont;

      VDDQ = p->Vddq;
      VSSQ = p->Vssq;
    }
  else
    {
      Rtt_nom  = 30; /* Default to 30 ohms for read. This means 60 ohms for pull up and pull down. */
      Rtt_wr   = 60; /* Default to 120 ohms for write. This means 240 ohms for pull up and pull down. */
      Rtt_cont = 75; /* Default to 75 ohms for termination at the controller. This means 150 ohms for pull up and pull down. */

      VDDQ = 1.5; /* Default 1.5 Volts */
      VSSQ = 0.0; /* Default 0 Volts */
    }

  if( p->RanksPerDIMM_set )
    {
      ranksPerDimm = p->RanksPerDIMM;
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

          Vbus = ( (VDDQ / Rttpu) + (VSSQ / Rttpd) + (Vread / Rdevice) ) / ( (1.0f / Rttpu) + (1.0f / Rttpd) + (1.0f / Rdevice) );

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

          Vbus = ( (VDDQ / Rttpu) + (VSSQ / Rttpd) + (Vwrite / Rdevice) ) / ( (1.0f / Rttpu) + (1.0f / Rttpd) + (1.0f / Rdevice) );

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

          Rs = 15.0f;  /* Series resistance of device. */
          Ron = 34.0f; /* Resistance of device output driver. */

          Rdevice = Rs + Ron;

          /* Pull-up/Pull-down at controller. */
          Rttpu = static_cast<float>(Rtt_cont) * 2.0f;
          Rttpd = static_cast<float>(Rtt_cont) * 2.0f;

          /* Pull-up/Pull-down at terminated rank. */
          Rothpu = static_cast<float>(Rtt_nom) * 2.0f;
          Rothpd = static_cast<float>(Rtt_nom) * 2.0f;

          if( bitValue == 0 )
            {
              R1 = Rttpu;
              R2 = 1.0f / ( (1.0f / Rttpu) + (1.0f / Rdevice) ); /* Device resistors are in parallel with controller pull-down. */
            }
          else
            {
              R1 = 1.0f / ( (1.0f / Rttpu) + (1.0f / Rdevice) ); /* Device resistors are in parallel with controller pull-up. */
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
          RX = 1.0f / ( (1.0f / R1) + (1.0f / RB) );
          RY = 1.0f / ( (1.0f / R2) + (1.0f / RC) );

          /* Bus voltage calculation. */
          float Vbus, Ibus;

          Ibus = (VDDQ - VSSQ) / (RX + RY);
          Vbus = VSSQ + Ibus * RY;

          /* Voltage at terminated rank. */
          float Vterm;

          Vterm = -1.0f * R3 * ( ((VDDQ - Vbus) / R1) - ((Vbus - VSSQ) / R2) ) + Vbus;

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

          Rttpu = static_cast<float>(Rtt_wr) * 2.0f;
          Rttpd = static_cast<float>(Rtt_wr) * 2.0f;
          Rothpu = static_cast<float>(Rtt_nom) * 2.0f;
          Rothpd = static_cast<float>(Rtt_nom) * 2.0f;

          Rs = 15.0f;  /* Series resistance of DRAM device. */
          Ron = 34.0f; /* Controller output driver resistance. */

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

