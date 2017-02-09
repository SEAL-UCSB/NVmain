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
    BusWidth = 64;
    DeviceWidth = 8;
    CLK = 666;
    RATE = 2;
    CPUFreq = 2000;

    EIDD0 = 85;
    EIDD1 = 54;
    EIDD2P0 = 30;
    EIDD2P1 = 30;
    EIDD2N = 37;
    EIDD3P = 35;
    EIDD3N = 40;
    EIDD4R = 160;
    EIDD4W = 165;
    EIDD5B = 200;
    EIDD6 = 12;
    //Ewrpb = 0.000202;
    // Values from DRAMPower2 tool
    Erd = 3.405401;
    Eopenrd = 1.081080;
    Ewr = 1.023750;
    Ewrpb = Ewr / 512.0; // Estimated value
    Eref = 38.558533;
    Eactstdby = 0.090090;
    Eprestdby = 0.083333;
    // TODO: Update pda, pdpf, pdps
    Epda = 0.000000;
    Epdpf = 0.000000;
    Epdps = 0.000000;
    Voltage = 1.5;

    /* 
     * Default to 30 ohms for read. This means 60 ohms for 
     * pull up and pull down. 
     */
    Rtt_nom  = 30; 
    /*
     * Default to 120 ohms for write. This means 240 ohms for 
     * pull up and pull down. 
     */
    Rtt_wr   = 60; 
    /* 
     * Default to 75 ohms for termination at the controller. 
     * This means 150 ohms for pull up and pull down. 
     */
    Rtt_cont = 75; 

    /* Default 1.5 Volts */
    Vddq = 1.5; 
    /* Default 0 Volts */
    Vssq = 0.0; 

    RanksPerDIMM = 1;

    EnduranceModel = "NullModel";
    DataEncoder = "default";
    EnergyModel = "current";

    UseLowPower = true;
    PowerDownMode = "FASTEXIT";
    InitPD = false;

    PrintGraphs = false;
    PrintAllDevices = false;
    PrintConfig = false;

    PrintPreTrace = false;
    EchoPreTrace = false;

    RefreshRows = 4;
    UseRefresh = true;
    StaggerRefresh = false;
    UsePrecharge = true;

    OffChipLatency = 10;

    PeriodicStatsInterval = 0;

    ROWS = 65536;
    COLS = 32;
    CHANNELS = 2;
    RANKS = 2;
    BANKS = 8;
    RAW = 4;
    MATHeight = ROWS;
    RBSize = COLS;

    tAL = 0;
    tBURST = 4;
    tCAS = 10;
    tCCD = 4;
    tCMD = 1;
    tCWD = 7;
    tRAW = 20;
    tOST = 1;
    tPD = 6;
    tRAS = 24;
    tRCD = 9;
    tRDB = 2;
    tREFW = 42666667;
    tRFC = 107;
    tRP = 9;
    tRRDR = 5;
    tRRDW = 5;
    tPPD = 0;
    tRTP = 5;
    tRTRS = 1;
    tWP = 0;
    tWR = 10;
    tWTR = 5;
    tXP = 6;
    tXPDLL = 17;
    tXS = 5;
    tXSDLL = 512;

    tRDPDEN = 24;
    tWRPDEN = 19;
    tWRAPDEN = 22;
    ClosePage = 1;
    ScheduleScheme = 1;
    HighWaterMark = 32;
    LowWaterMark = 16;
    BanksPerRefresh = BANKS;
    DelayedRefreshThreshold = 1;
    AddressMappingScheme = "R:SA:RK:BK:CH:C";

    MemoryPrefetcher = "none";
    PrefetchBufferSize = 32;

    programMode = ProgramMode_SRMS;
    MLCLevels = 1;
    WPVariance = 1;
    UniformWrites = true; // Disable MLC by default
    WriteAllBits = true;

    Ereset = 0.054331;
    Eset = 0.101581;
    tWP0 = 40;
    tWP1 = 60;

    nWP00 = 0;
    nWP01 = 7;
    nWP10 = 5;
    nWP11 = 1;

    WPMaxVariance = 2;

    WritePausing = false;
    PauseThreshold = 0.4;
    MaxCancellations = 4;
    pauseMode = PauseMode_Normal;

    DeadlockTimer = 10000000;

    debugOn = false;
    debugClasses.clear();
}

Params::~Params( )
{
}

void Params::ConvertTiming( Config *conf, std::string param, ncycle_t& value )
{
    if( conf->KeyExists( param ) )
    {
        value = ConvertTiming( conf, param );
    }
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
    c->GetValueUL( "BusWidth", BusWidth );
    c->GetValueUL( "DeviceWidth", DeviceWidth );
    c->GetValueUL( "CLK", CLK );
    c->GetValueUL( "RATE", RATE );
    c->GetValueUL( "CPUFreq", CPUFreq );

    c->GetEnergy( "EIDD0", EIDD0 );
    c->GetEnergy( "EIDD1", EIDD1 );
    c->GetEnergy( "EIDD2P0", EIDD2P0 );
    c->GetEnergy( "EIDD2P1", EIDD2P1 );
    c->GetEnergy( "EIDD2N", EIDD2N );
    c->GetEnergy( "EIDD3P", EIDD3P );
    c->GetEnergy( "EIDD3N", EIDD3N );
    c->GetEnergy( "EIDD4R", EIDD4R );
    c->GetEnergy( "EIDD4W", EIDD4W );
    c->GetEnergy( "EIDD5B", EIDD5B );
    c->GetEnergy( "EIDD6", EIDD6 );
    c->GetEnergy( "Eopenrd", Eopenrd );
    c->GetEnergy( "Erd", Erd );
    c->GetEnergy( "Eref", Eref );
    c->GetEnergy( "Ewr", Ewr );
    c->GetEnergy( "Ewrpb", Ewrpb );
    c->GetEnergy( "Eactstdby", Eactstdby );
    c->GetEnergy( "Eprestdby", Eprestdby );
    c->GetEnergy( "Epda", Epda );
    c->GetEnergy( "Epdpf", Epdpf );
    c->GetEnergy( "Epdps", Epdps );
    c->GetEnergy( "Voltage", Voltage );

    c->GetValue( "Rtt_nom", Rtt_nom );
    c->GetValue( "Rtt_wr", Rtt_wr );
    c->GetValue( "Rtt_cont", Rtt_cont );
    c->GetEnergy( "VDDQ", Vddq );
    c->GetEnergy( "VSSQ", Vssq );

    c->GetValue( "RanksPerDIMM", RanksPerDIMM );

    c->GetString( "EnduranceModel", EnduranceModel );
    c->GetString( "DataEncoder", DataEncoder );
    c->GetString( "EnergyModel", EnergyModel );

    c->GetBool( "UseLowPower", UseLowPower );
    c->GetString( "PowerDownMode", PowerDownMode );
    c->GetBool( "InitPD", InitPD );

    c->GetBool( "PrintGraphs", PrintGraphs );
    c->GetBool( "PrintAllDevices", PrintAllDevices );
    c->GetBool( "PrintConfig", PrintConfig );

    c->GetBool( "PrintPreTrace", PrintPreTrace );
    c->GetBool( "EchoPreTrace", EchoPreTrace );

    c->GetValueUL( "RefreshRows", RefreshRows );
    c->GetBool( "UseRefresh", UseRefresh );
    c->GetBool( "StaggerRefresh", StaggerRefresh );
    c->GetBool( "UsePrecharge", UsePrecharge );

    c->GetValueUL( "OffChipLatency", OffChipLatency );

    c->GetValueUL( "PeriodicStatsInterval", PeriodicStatsInterval );

    c->GetValueUL( "ROWS", ROWS );
    c->GetValueUL( "COLS", COLS );
    c->GetValueUL( "CHANNELS", CHANNELS );
    c->GetValueUL( "RANKS", RANKS );
    c->GetValueUL( "BANKS", BANKS );
    c->GetValueUL( "RAW", RAW );
    c->GetValueUL( "MATHeight", MATHeight );
    c->GetValueUL( "RBSize", RBSize );

    ConvertTiming( c, "tAL", tAL );
    ConvertTiming( c, "tBURST", tBURST );
    ConvertTiming( c, "tCAS", tCAS );
    ConvertTiming( c, "tCCD", tCCD );
    ConvertTiming( c, "tCMD", tCMD );
    ConvertTiming( c, "tCWD", tCWD );
    ConvertTiming( c, "tRAW", tRAW );
    ConvertTiming( c, "tOST", tOST );
    ConvertTiming( c, "tPD", tPD );
    ConvertTiming( c, "tRAS", tRAS );
    ConvertTiming( c, "tRCD", tRCD );
    ConvertTiming( c, "tRDB", tRDB );
    ConvertTiming( c, "tREFW", tREFW );
    ConvertTiming( c, "tRFC", tRFC );
    ConvertTiming( c, "tRP", tRP );
    ConvertTiming( c, "tRRDR", tRRDR );
    ConvertTiming( c, "tRRDW", tRRDW );
    ConvertTiming( c, "tPPD", tPPD );
    ConvertTiming( c, "tRTP", tRTP );
    ConvertTiming( c, "tRTRS", tRTRS );
    ConvertTiming( c, "tWP", tWP );
    ConvertTiming( c, "tWR", tWR );
    ConvertTiming( c, "tWTR", tWTR );
    ConvertTiming( c, "tXP", tXP );
    ConvertTiming( c, "tXPDLL", tXPDLL );
    ConvertTiming( c, "tXS", tXS );
    ConvertTiming( c, "tXSDLL", tXSDLL );

    c->GetValueUL( "tRDPDEN", tRDPDEN );
    c->GetValueUL( "tWRPDEN", tWRPDEN );
    c->GetValueUL( "tWRAPDEN", tWRAPDEN );
    c->GetValueUL( "ClosePage", ClosePage );
    c->GetValue( "ScheduleScheme", ScheduleScheme );
    c->GetValue( "HighWaterMark", HighWaterMark );
    c->GetValue( "LowWaterMark", LowWaterMark );
    c->GetValueUL( "BanksPerRefresh", BanksPerRefresh );
    c->GetValueUL( "DelayedRefreshThreshold", DelayedRefreshThreshold );
    c->GetString( "AddressMappingScheme", AddressMappingScheme );

    c->GetString( "MemoryPrefetcher", MemoryPrefetcher );
    c->GetValueUL( "PrefetchBufferSize", PrefetchBufferSize );

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
    c->GetValueUL( "MLCLevels", MLCLevels );
    c->GetValueUL( "WPVariance",  WPVariance );
    c->GetBool( "UniformWrites", UniformWrites );
    c->GetBool( "WriteAllBits", WriteAllBits );

    c->GetEnergy( "Ereset", Ereset );
    c->GetEnergy( "Eset", Eset );
    ConvertTiming( c, "tWP0", tWP0 );
    ConvertTiming( c, "tWP1", tWP1 );

    c->GetValueUL( "nWP00", nWP00 );
    c->GetValueUL( "nWP01", nWP01 );
    c->GetValueUL( "nWP10", nWP10 );
    c->GetValueUL( "nWP11", nWP11 );

    c->GetValueUL( "WPMaxVariance", WPMaxVariance );

    c->GetValueUL( "DeadlockTimer", DeadlockTimer );

    c->GetBool( "EnableDebug", debugOn );
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

    c->GetBool( "WritePausing", WritePausing );
    c->GetEnergy( "PauseThreshold", PauseThreshold );
    c->GetValueUL( "MaxCancellations", MaxCancellations );
    if( c->KeyExists( "PauseMode" ) )
    {
        if( c->GetString( "PauseMode" ) == "Normal" )
            pauseMode = PauseMode_Normal;
        else if( c->GetString( "PauseMode" ) == "IIWC" )
            pauseMode = PauseMode_IIWC;
        else if( c->GetString( "PauseMode" ) == "Optimal" )
            pauseMode = PauseMode_Optimal;
        else
            std::cout << "Unknown PauseMode: " << c->GetString( "PauseMode" )
                      << ". Defaulting to Normal" << std::endl;
    }
}

