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

#ifndef __NVM_PARAMS_H__
#define __NVM_PARAMS_H__

#include "src/Config.h"
#include "src/Debug.h"
#include "include/NVMTypes.h"

#include <set>
#include <string>

namespace NVM {

enum ProgramMode {
    ProgramMode_SRMS,
    ProgramMode_SSMR
};

enum PauseMode {
    PauseMode_Normal,   ///< Normal pause mode: Wait until write pulse before read
    PauseMode_IIWC,     ///< Intra-Iteration Write Cancellation: allow cancel during write pulse
    PauseMode_Optimal   ///< Optimal: Same as IIWC, but consider iteration complete
};

class Params
{
  public:
    Params( );
    ~Params( );

    void SetParams( Config *c );

    ncounter_t BusWidth;
    ncounter_t DeviceWidth;
    ncounter_t CLK;
    ncounter_t RATE;
    ncounter_t CPUFreq;

    double EIDD0;
    double EIDD1;
    double EIDD2P0;
    double EIDD2P1;
    double EIDD2N;
    double EIDD3P;
    double EIDD3N;
    double EIDD4R;
    double EIDD4W;
    double EIDD5B;
    double EIDD6;
    double Eopenrd;
    double Erd;
    double Eref;
    double Ewr;
    double Ewrpb;
    double Eactstdby;
    double Eprestdby;
    double Epda;
    double Epdpf;
    double Epdps;
    double Voltage;

    int Rtt_nom;
    int Rtt_wr;
    int Rtt_cont;
    double Vddq;
    double Vssq;

    int RanksPerDIMM;

    std::string EnduranceModel;
    std::string DataEncoder;
    std::string EnergyModel;

    bool UseLowPower;
    std::string PowerDownMode;
    bool InitPD;

    bool PrintGraphs;
    bool PrintAllDevices;
    bool PrintConfig;

    bool PrintPreTrace;
    bool EchoPreTrace;

    ncounter_t RefreshRows;
    bool UseRefresh;
    bool StaggerRefresh;
    bool UsePrecharge;

    ncounter_t OffChipLatency;

    ncounter_t PeriodicStatsInterval;

    ncounter_t ROWS;
    ncounter_t COLS;
    ncounter_t CHANNELS;
    ncounter_t RANKS;
    ncounter_t BANKS;
    ncounter_t RAW;
    ncounter_t MATHeight;
    ncounter_t RBSize;

    ncycle_t tAL;
    ncycle_t tBURST;
    ncycle_t tCAS;
    ncycle_t tCCD;
    ncycle_t tCMD;
    ncycle_t tCWD;
    ncycle_t tRAW;
    ncycle_t tOST;
    ncycle_t tPD;
    ncycle_t tRAS;
    ncycle_t tRCD;
    ncycle_t tRDB;
    ncycle_t tREFW;
    ncycle_t tRFC;
    ncycle_t tRP;
    ncycle_t tRRDR;
    ncycle_t tRRDW;
    ncycle_t tPPD;
    ncycle_t tRTP;
    ncycle_t tRTRS;
    ncycle_t tWP;
    ncycle_t tWR;
    ncycle_t tWTR;
    ncycle_t tXP;
    ncycle_t tXPDLL;
    ncycle_t tXS;
    ncycle_t tXSDLL;

    ncycle_t tRDPDEN; // interval between Read/ReadA and PowerDown
    ncycle_t tWRPDEN; // interval between Write and PowerDown
    ncycle_t tWRAPDEN; // interval between WriteA and PowerDown
    ncycle_t ClosePage; // enable close-page management policy
    int ScheduleScheme; // command scheduling policy 
    int HighWaterMark; // write drain high watermark
    int LowWaterMark; // write drain low watermark
    ncounter_t BanksPerRefresh; // the number of banks in a refresh (in lockstep)
    ncounter_t DelayedRefreshThreshold; // the threshold that indicates how many refresh can be delayed
    std::string AddressMappingScheme; // the address mapping scheme

    std::string MemoryPrefetcher;
    ncounter_t PrefetchBufferSize;

    ProgramMode programMode;
    ncounter_t MLCLevels;
    ncounter_t WPVariance;
    bool UniformWrites;
    bool WriteAllBits; // Set false to calculate write energy on a per-bit basis

    /* SLC energy */
    double Ereset; 
    double Eset; 
    ncycle_t tWP0;
    ncycle_t tWP1;

    /* 2-level MLC average program pulse count */
    ncycle_t nWP00;
    ncycle_t nWP01;
    ncycle_t nWP10;
    ncycle_t nWP11;

    /* 2-level MLC variance (01 and 10 only). */
    ncycle_t WPMaxVariance;

    /* Configurable deadlock timer. */
    ncycle_t DeadlockTimer;

    /* List of debug classes. */
    bool debugOn;
    std::set<std::string> debugClasses;

    bool WritePausing;
    double PauseThreshold;
    ncounter_t MaxCancellations;
    PauseMode pauseMode;

  private:
    void ConvertTiming( Config *conf, std::string param, ncycle_t& value );
    ncycle_t ConvertTiming( Config *conf, std::string param );
};

};

#endif
