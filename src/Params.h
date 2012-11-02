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
#ifndef __NVM_PARAMS_H__
#define __NVM_PARAMS_H__


#include "src/Config.h"
#include "include/NVMTypes.h"


namespace NVM {


class Params
{
public:
  Params( );
  ~Params( );


  void SetParams( Config *c );

  ncounter_t BPC;
  ncounter_t BusWidth;
  ncounter_t DeviceWidth;
  ncounter_t CLK;
  ncounter_t MULT;
  ncounter_t RATE;
  ncounter_t CPUFreq;

  float EIDD0;
  float EIDD2N;
  float EIDD3N;
  float EIDD4R;
  float EIDD4W;
  float EIDD5B;
  float Eclosed;
  float Eopen;
  float Eopenrd;
  float Erd;
  float Eref;
  float Ewr;
  float Voltage;

  unsigned int Rtt_nom;
  unsigned int Rtt_wr;
  unsigned int Rtt_cont;
  float Vddq;
  float Vssq;
  bool Rtt_nom_set;
  bool Rtt_wr_set;
  bool Rtt_cont_set;
  bool Vddq_set;
  bool Vssq_set;

  unsigned int RanksPerDIMM;
  bool RanksPerDIMM_set;

  std::string EnduranceModel;
  std::string EnergyModel;
  bool EnergyModel_set;

  bool InitPD;
  bool PrintGraphs;
  bool PrintAllDevices;
  bool PrintAllDevices_set;

  bool PrintPreTrace;
  bool EchoPreTrace;

  ncounter_t RefreshRows;
  bool UseRefresh;
  bool UseRefresh_set;
  bool StaggerRefresh;
  bool StaggerRefresh_set;

  ncounter_t OffChipLatency;
  bool OffChipLatency_set;

  ncounter_t PeriodicStatsInterval;
  bool PeriodicStatsInterval_set;

  ncounter_t ROWS;
  ncounter_t COLS;
  ncounter_t CHANNELS;
  ncounter_t RANKS;
  ncounter_t BANKS;

  ncycle_t tAL;
  ncycle_t tBURST;
  ncycle_t tCAS;
  ncycle_t tCCD;
  ncycle_t tCMD;
  ncycle_t tCWD;
  ncycle_t tFAW;
  ncycle_t tOST;
  ncycle_t tPD;
  ncycle_t tRAS;
  ncycle_t tRCD;
  ncycle_t tRFI;
  ncycle_t tRFC;
  ncycle_t tRP;
  ncycle_t tRRDR;
  ncycle_t tRRDW;
  ncycle_t tRTP;
  ncycle_t tRTRS;
  ncycle_t tWR;
  ncycle_t tWTR;
  ncycle_t tXP;
  ncycle_t tXPDLL;


};



};


#endif

