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


#include "Params.h"



using namespace NVM;


Params::Params( )
{
  PrintAllDevices_set = false;
  EnergyModel_set = false;
  UseRefresh_set = false;
  StaggerRefresh_set = false;
  OffChipLatency_set = false;
  PeriodicStatsInterval_set = false;
  Rtt_nom_set = false;
  Rtt_wr_set = false;
  Rtt_cont_set = false;
  Vddq_set = false;
  Vssq_set = false;

  /* Defaults */
  PrintPreTrace = false;
  EchoPreTrace = false;

  StaggerRefresh = false;
}


Params::~Params( )
{

}


/* This can be called whenever timings change. (Will not update the "next" vars) */
void Params::SetParams( Config *c )
{
    BPC = c->GetValue( "BPC" );
    BusWidth = c->GetValue( "BusWidth" );
    DeviceWidth = c->GetValue( "DeviceWidth" );
    CLK = c->GetValue( "CLK" );
    MULT = c->GetValue( "MULT" );
    RATE = c->GetValue( "RATE" );
    CPUFreq = c->GetValue( "CPUFreq" );

    EIDD0 = c->GetEnergy( "EIDD0" );
    EIDD2N = c->GetEnergy( "EIDD2N" );
    EIDD3N = c->GetEnergy( "EIDD3N" );
    EIDD4R = c->GetEnergy( "EIDD4R" );
    EIDD4W = c->GetEnergy( "EIDD4W" );
    EIDD5B = c->GetEnergy( "EIDD5B" );
    Eclosed = c->GetEnergy( "Eclosed" );
    Eopen = c->GetEnergy( "Eopen" );
    Eopenrd = c->GetEnergy( "Eopenrd" );
    Erd = c->GetEnergy( "Erd" );
    Eref = c->GetEnergy( "Eref" );
    Ewr = c->GetEnergy( "Ewr" );
    Voltage = c->GetEnergy( "Voltage" );

    Rtt_nom = (unsigned int)c->GetValue( "Rtt_nom" );
    Rtt_wr = (unsigned int)c->GetValue( "Rtt_wr" );
    Rtt_cont = (unsigned int)c->GetValue( "Rtt_cont" );
    Vddq = c->GetEnergy( "VDDQ" );
    Vssq = c->GetEnergy( "VSSQ" );
    Rtt_nom_set = c->KeyExists( "Rtt_nom" );
    Rtt_wr_set = c->KeyExists( "Rtt_wr" );
    Rtt_cont_set = c->KeyExists( "Rtt_cont" );
    Vddq_set = c->KeyExists( "VDDQ" );
    Vssq_set = c->KeyExists( "VSSQ" );

    RanksPerDIMM = (unsigned int)c->GetValue( "RanksPerDIMM" );
    RanksPerDIMM_set = c->KeyExists( "RanksPerDIMM" );

    EnduranceModel = c->GetString( "EnduranceModel" );
    EnergyModel = c->GetString( "EnergyModel" );
    EnergyModel_set = c->KeyExists( "EnergyModel" );

    InitPD = ( c->GetString( "InitPD" ) == "true" );
    PrintGraphs = ( c->GetString( "PrintGraphs" ) == "true" );
    PrintAllDevices = ( c->GetString( "PrintAllDevices" ) == "true" );
    PrintAllDevices_set = c->KeyExists( "PrintAllDevices" );

    PrintPreTrace = ( c->GetString( "PrintPreTrace" ) == "true" );
    EchoPreTrace = ( c->GetString( "EchoPreTrace" ) == "true" );

    RefreshRows = c->GetValue( "RefreshRows" );
    UseRefresh = ( c->GetString( "UseRefresh" ) == "true" );
    UseRefresh_set = ( c->KeyExists( "UseRefresh" ) );
    StaggerRefresh = ( c->GetString( "StaggerRefresh" ) == "true" );
    StaggerRefresh_set = ( c->KeyExists( "StaggerRefresh" ) );

    OffChipLatency = c->GetValue( "OffChipLatency" );
    OffChipLatency_set = c->KeyExists( "OffChipLatency" );

    PeriodicStatsInterval = c->GetValue( "PeriodicStatsInterval" );
    PeriodicStatsInterval_set = c->KeyExists( "PeriodicStatsInterval" );

    ROWS = c->GetValue( "ROWS" );
    COLS = c->GetValue( "COLS" );
    CHANNELS = c->GetValue( "CHANNELS" );
    RANKS = c->GetValue( "RANKS" );
    BANKS = c->GetValue( "BANKS" );

    tAL = c->GetValue( "tAL" );
    tBURST = c->GetValue( "tBURST" );
    tCAS = c->GetValue( "tCAS" );
    tCCD = c->GetValue( "tCCD" );
    tCMD = c->GetValue( "tCMD" );
    tCWD = c->GetValue( "tCWD" );
    tFAW = c->GetValue( "tFAW" );
    tOST = c->GetValue( "tOST" );
    tPD = c->GetValue( "tPD" );
    tRAS = c->GetValue( "tRAS" );
    tRCD = c->GetValue( "tRCD" );
    tRFI = c->GetValue( "tRFI" );
    tRFC = c->GetValue( "tRFC" );
    tRP = c->GetValue( "tRP" );
    tRRDR = c->GetValue( "tRRDR" );
    tRRDW = c->GetValue( "tRRDW" );
    tRTP = c->GetValue( "tRTP" );
    tRTRS = c->GetValue( "tRTRS" );
    tWR = c->GetValue( "tWR" );
    tWTR = c->GetValue( "tWTR" );
    tXP = c->GetValue( "tXP" );
    tXPDLL = c->GetValue( "tXPDLL" );

    tRDPDEN = c->GetValue( "tRDPDEN" );
    tWRPDEN = c->GetValue( "tWRPDEN" );
    tWRAPDEN = c->GetValue( "tWRAPDEN" );
    ClosePage = ( c->GetString( "ClosePage" ) == "true" );
    ScheduleScheme = c->GetValue ( "ScheduleScheme" );
    HighWaterMark = c->GetValue ( "HighWaterMark" );
    LowWaterMark = c->GetValue ( "LowWaterMark" );
}

