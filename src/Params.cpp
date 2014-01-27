/*******************************************************************************
* Copyright (c) 2012-2014, The Microsystems Design Labratory (MDL)
* Department of Computer Science and Engineering, The Pennsylvania State University
* All rights reserved.
* 
* This source code is part of NVMain - A cycle accurate timing, bit accurate
* energy simulator for both volatile (e.g., DRAM) and non-volatile memory
* (e.g., PCRAM). The source code is free and you can redistribute and/or
* modify it by providing that the following conditions are met:
* 
*  1) Redistributions of source code must retain the above copyright notice,
*     this list of conditions and the following disclaimer.
* 
*  2) Redistributions in binary form must reproduce the above copyright notice,
*     this list of conditions and the following disclaimer in the documentation
*     and/or other materials provided with the distribution.
* 
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
* 
* Author list: 
*   Matt Poremba    ( Email: mrp5060 at psu dot edu 
*                     Website: http://www.cse.psu.edu/~poremba/ )
*   Tao Zhang       ( Email: tzz106 at cse dot psu dot edu
*                     Website: http://www.cse.psu.edu/~tzz106 )
*******************************************************************************/

#include "src/Params.h"
#include "include/NVMHelpers.h"

#include <cmath>
#include <cstdlib>
#include <sstream>
#include <iostream>

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
    MATHeight_set = false;
    RBSize_set = false;
    tWP_set = false;

    /* Defaults */
    PrintPreTrace = false;
    EchoPreTrace = false;
    PrintConfig = false;

    StaggerRefresh = false;

    ROWS = 32768;

    tWP = 0;

    /* MLC params. */
    UniformWrites = true; // Disable MLC by default
    programMode = ProgramMode_SRMS;
    MLCLevels = 1;
    WPVariance = 1;
    Ereset = 0.3;
    Eset = 0.2;
    tWP0 = 100;
    tWP1 = 50;
    nWP00 = 0;
    nWP01 = 7;
    nWP10 = 5;
    nWP11 = 1;
    nWPVar = 2;

    WritePausing = false;
    PauseThreshold = 0.4;

    DeadlockTimer = 10000000;

    debugOn = false;
    debugClasses.clear();
}

Params::~Params( )
{
}

ncycle_t Params::ConvertTiming( Config *conf, std::string param )
{
    if( !conf->KeyExists( param ) )
        return 0;

    std::string stringValue = conf->GetString( param );
    double numericValue = conf->GetEnergy( param ); // GetEnergy returns a float.
    double calculatedValue = numericValue; // Assume pre-calculated.
    ncounter_t clock = conf->GetValue( "CLK" );

    if( stringValue.length() > 2 )
    {
        std::string units = stringValue.substr( stringValue.length() - 2, std::string::npos );

        if( units == "ns" )
        {
            // CLK is in MHz, divide from 1000 to get period in ns.
            calculatedValue = static_cast<double>(numericValue) * (static_cast<double>(clock) / 1e3f);
        }
        else if( units == "us" )
        {
            // CLK is in MHz, divide from 1000 to get period in ns.
            calculatedValue = static_cast<double>(numericValue) * (static_cast<double>(clock));
        }
        else if( units == "ms" )
        {
            // CLK is in MHz, divide from 1000 to get period in ns.
            calculatedValue = static_cast<double>(numericValue) * (static_cast<double>(clock) * 1e3f);
        }
    }

    return static_cast<ncycle_t>( std::ceil( calculatedValue ) );
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
    EIDD2P0 = c->GetEnergy( "EIDD2P0" );
    EIDD2P1 = c->GetEnergy( "EIDD2P1" );
    EIDD2N = c->GetEnergy( "EIDD2N" );
    EIDD3P = c->GetEnergy( "EIDD3P" );
    EIDD3N = c->GetEnergy( "EIDD3N" );
    EIDD4R = c->GetEnergy( "EIDD4R" );
    EIDD4W = c->GetEnergy( "EIDD4W" );
    EIDD5B = c->GetEnergy( "EIDD5B" );
    EIDD6 = c->GetEnergy( "EIDD6" );
    Eclosed = c->GetEnergy( "Eclosed" );
    Eopen = c->GetEnergy( "Eopen" );
    Eopenrd = c->GetEnergy( "Eopenrd" );
    Erd = c->GetEnergy( "Erd" );
    Eref = c->GetEnergy( "Eref" );
    Ewr = c->GetEnergy( "Ewr" );
    Eleak = c->GetEnergy( "Eleak" );
    Epda = c->GetEnergy( "Epda" );
    Epdpf = c->GetEnergy( "Epdpf" );
    Epdps = c->GetEnergy( "Epdps" );
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

    UseLowPower = ( c->GetString( "UseLowPower" ) == "true" );
    PowerDownMode = c->GetString( "PowerDownMode" );
    InitPD = ( c->GetString( "InitPD" ) == "true" );

    PrintGraphs = ( c->GetString( "PrintGraphs" ) == "true" );
    PrintAllDevices = ( c->GetString( "PrintAllDevices" ) == "true" );
    PrintAllDevices_set = c->KeyExists( "PrintAllDevices" );
    PrintConfig = ( c->GetString( "PrintConfig" ) == "true" );

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
    RAW = c->GetValue( "RAW" );
    MATHeight = c->GetValue( "MATHeight" );
    MATHeight_set = c->KeyExists( "MATHeight" );
    RBSize = c->GetValue( "RBSize" );
    RBSize_set = c->KeyExists( "RBSize" );

    tAL = ConvertTiming( c, "tAL" );
    tBURST = ConvertTiming( c, "tBURST" );
    tCAS = ConvertTiming( c, "tCAS" );
    tCCD = ConvertTiming( c, "tCCD" );
    tCMD = ConvertTiming( c, "tCMD" );
    tCWD = ConvertTiming( c, "tCWD" );
    tRAW = ConvertTiming( c, "tRAW" );
    tOST = ConvertTiming( c, "tOST" );
    tPD = ConvertTiming( c, "tPD" );
    tRAS = ConvertTiming( c, "tRAS" );
    tRCD = ConvertTiming( c, "tRCD" );
    tREFW = ConvertTiming( c, "tREFW" );
    tRFC = ConvertTiming( c, "tRFC" );
    tRP = ConvertTiming( c, "tRP" );
    tRRDR = ConvertTiming( c, "tRRDR" );
    tRRDW = ConvertTiming( c, "tRRDW" );
    tRTP = ConvertTiming( c, "tRTP" );
    tRTRS = ConvertTiming( c, "tRTRS" );
    if( c->KeyExists( "tWP" ) ) { tWP = ConvertTiming( c, "tWP" ); tWP_set = true; }
    tWR = ConvertTiming( c, "tWR" );
    tWTR = ConvertTiming( c, "tWTR" );
    tXP = ConvertTiming( c, "tXP" );
    tXPDLL = ConvertTiming( c, "tXPDLL" );
    tXS = ConvertTiming( c, "tXS" );
    tXSDLL = ConvertTiming( c, "tXSDLL" );
    tWP0 = ConvertTiming( c, "tWP0" );
    tWP1 = ConvertTiming( c, "tWP1" );

    tRDPDEN = c->GetValue( "tRDPDEN" );
    tWRPDEN = c->GetValue( "tWRPDEN" );
    tWRAPDEN = c->GetValue( "tWRAPDEN" );
    ClosePage = c->GetValue( "ClosePage" );
    ScheduleScheme = c->GetValue ( "ScheduleScheme" );
    HighWaterMark = c->GetValue ( "HighWaterMark" );
    LowWaterMark = c->GetValue ( "LowWaterMark" );
    BanksPerRefresh = c->GetValue ( "BanksPerRefresh" );
    DelayedRefreshThreshold = c->GetValue ( "DelayedRefreshThreshold" );
    AddressMappingScheme = c->GetString ( "AddressMappingScheme" );

    if( c->KeyExists( "MLCLevels" ) ) MLCLevels = c->GetValue( "MLCLevels" );
    if( c->KeyExists( "WPVariance" ) ) WPVariance = c->GetValue( "WPVariance" );
    if( c->KeyExists( "UniformWrites" ) && c->GetString( "UniformWrites" ) == "false" )
        UniformWrites = false;
    if( c->KeyExists( "ProgramMode" ) )
    {
        if( c->GetString( "ProgramMode" ) == "SRMS" )
            programMode = ProgramMode_SRMS;
        else if( c->GetString( "ProgramMode" ) == "SSMR" )
            programMode = ProgramMode_SSMR;
        else
            std::cout << "Unknown ProgramMode: " << c->GetString( "ProgramMode" )
                      << ". Defaulting to SRMS" << std::endl;
    }

    if( c->KeyExists( "Ereset" ) ) Ereset = c->GetEnergy( "Ereset" );
    if( c->KeyExists( "Eset" ) )   Eset = c->GetEnergy( "Eset" );

    if( c->KeyExists( "nWP00" ) ) nWP00 = c->GetValue( "nWP00" );
    if( c->KeyExists( "nWP01" ) ) nWP01 = c->GetValue( "nWP01" );
    if( c->KeyExists( "nWP10" ) ) nWP10 = c->GetValue( "nWP10" );
    if( c->KeyExists( "nWP11" ) ) nWP11 = c->GetValue( "nWP11" );
    if( c->KeyExists( "nWPVar" ) ) nWPVar = c->GetValue( "nWPVar" );

    if( c->KeyExists( "DeadlockTimer" ) ) DeadlockTimer = c->GetValue( "DeadlockTimer" );

    if( c->KeyExists( "EnableDebug" ) && c->GetString( "EnableDebug" ) == "true" )
        debugOn = true;
    if( c->KeyExists( "DebugClasses" ) )
    {
        std::string debugClassList = c->GetString( "DebugClasses" );
        std::string debugClass;
        std::istringstream debugClassStream( debugClassList );

        while( std::getline( debugClassStream, debugClass, ',' ) )
        {
            // TODO: strip whitespace?
            std::cout << "Will print debug information from \"" << debugClass << ".\"" << std::endl;
            debugClasses.insert( debugClass );
        }
    }

    if( c->KeyExists( "WritePausing" ) && c->GetString( "WritePausing" ) == "true" )
        WritePausing = true;
    if( c->KeyExists( "PauseThrehold" ) )
        PauseThreshold = c->GetEnergy( "PauseThreshold" );

    /* Check for uninitialized parameters. */
    if( !MATHeight_set ) MATHeight = ROWS;
    if( !RBSize_set ) RBSize = COLS;
    
    if( !tWP_set ) tWP = 0;
}

