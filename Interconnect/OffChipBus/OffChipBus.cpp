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

#include "Interconnect/OffChipBus/OffChipBus.h"
#include "src/MemoryController.h"
#include "Ranks/RankFactory.h"
#include "src/EventQueue.h"

#include <sstream>
#include <cassert>

using namespace NVM;

OffChipBus::OffChipBus( )
{
    conf = NULL;
    configSet = false;
    numRanks = 0;
    syncValue = 0.0f;
}

OffChipBus::~OffChipBus( )
{
}

void OffChipBus::SetConfig( Config *c, bool createChildren )
{
    Params *params = new Params( );
    params->SetParams( c );
    SetParams( params );

    conf = c;
    configSet = true;

    offChipDelay = p->OffChipLatency;

    numRanks = p->RANKS;

    if( createChildren )
    {
        /* When selecting a child, use the rank field from the decoder. */
        AddressTranslator *incAT = DecoderFactory::CreateDecoderNoWarn( c->GetString( "Decoder" ) );
        TranslationMethod *method = GetParent()->GetTrampoline()->GetDecoder()->GetTranslationMethod();
        incAT->SetTranslationMethod( method );
        incAT->SetDefaultField( RANK_FIELD );
        incAT->SetConfig( c, createChildren );
        SetDecoder( incAT );

        for( ncounter_t i = 0; i < numRanks; i++ )
        {
            std::stringstream formatter;

            Rank *nextRank = RankFactory::CreateRankNoWarn( c->GetString( "RankType" ) );

            formatter.str( "" );
            formatter << StatName( ) << ".rank" << i;
            nextRank->StatName( formatter.str( ) );

            nextRank->SetParent( this );
            AddChild( nextRank );

            /* SetConfig recursively. */
            nextRank->SetConfig( conf, createChildren );
            nextRank->RegisterStats( );
        }
    }

    SetDebugName( "OffChipBus", c );
}

bool OffChipBus::RequestComplete( NVMainRequest *request )
{
    GetEventQueue( )->InsertEvent( EventResponse, GetParent( ), 
            request, GetEventQueue()->GetCurrentCycle() + offChipDelay ); 

    return true;
}

bool OffChipBus::IssueCommand( NVMainRequest *req )
{
    ncounter_t opRank;
    bool success = false;

    req->address.GetTranslatedAddress( NULL, NULL, NULL, &opRank, NULL, NULL );

    assert( GetChild( req )->IsIssuable( req ) );

    success = GetChild( req )->IssueCommand( req );

    /*
     *  To preserve rank-to-rank switching time, we need to notify the
     *  other ranks what the command sent was.
     */
    if( success )
    {
        for( ncounter_t childIdx = 0; childIdx < GetChildCount( ); childIdx++ )
          if( GetChild( req ) != GetChild( childIdx ) )
            GetChild( childIdx )->Notify( req );
    }

    return success;
}

bool OffChipBus::IsIssuable( NVMainRequest *req, FailReason *reason )
{
    return GetChild( req )->IsIssuable( req, reason );
}

void OffChipBus::CalculateStats( )
{
    for( ncounter_t childIdx = 0; childIdx < GetChildren().size(); childIdx++ )
    {
        GetChild(childIdx)->CalculateStats( );
    }
}

void OffChipBus::Cycle( ncycle_t steps )
{
    for( ncounter_t childIdx = 0; childIdx < GetChildren().size(); childIdx++ )
    {
        GetChild(childIdx)->Cycle( steps );
    }
}

double OffChipBus::CalculateIOPower( bool isRead, unsigned int bitValue )
{
    unsigned int Rtt_nom, Rtt_wr, Rtt_cont;
    double VDDQ, VSSQ;
    unsigned int ranksPerDimm;
    double Pdq; 

    Pdq = 0.0f;

    Rtt_nom  = p->Rtt_nom;
    Rtt_wr   = p->Rtt_wr;
    Rtt_cont = p->Rtt_cont;

    VDDQ = p->Vddq;
    VSSQ = p->Vssq;

    ranksPerDimm = p->RanksPerDIMM;

    if( ranksPerDimm == 1 )
    {
        if( isRead )
        {
            /* 
             * Simple resistor network. Calculate the bus voltage, then current, 
             * and finally power. 
             */
            double Rttpu, Rttpd, Rdevice, Rs, Ron;
            double Vread;

            Rs = 15.0;  /* Series resistance of device. */
            Ron = 34.0; /* Resistance of device output driver. */

            Rttpu = static_cast<double>(Rtt_nom * 2.0);
            Rttpd = static_cast<double>(Rtt_nom * 2.0);
            Rdevice = Rs + Ron;

            Vread = (bitValue == 0) ? VSSQ : VDDQ;

            /* Bus voltage equation */
            double Vbus;

            Vbus = ( (VDDQ / Rttpu) + (VSSQ / Rttpd) + (Vread / Rdevice) ) 
                / ( (1.0f / Rttpu) + (1.0f / Rttpd) + (1.0f / Rdevice) );

            /* Bus current equation. */
            double Ibus, Ipu, Ipd;

            Ipu = (VDDQ - Vbus) / Rttpu; /* Current through controller pull up */
            Ipd = (Vbus - VSSQ) / Rttpd; /* Current through controller pull down */
            Ibus = (Vbus - Vread) / Rdevice; /* Current sourced/sinked by device */

            /* Power calculation equation. */
            Pdq = Ipu * Ipu * Rttpu + Ipd * Ipd * Rttpd + Ibus * Ibus * Rdevice;
        }
        else
        {
            double Rttpu, Rttpd, Rdevice, Rs, Ron;
            double Vwrite;

            Rs = 15.0;  /* Series resistance of device. */
            Ron = 34.0; /* Resistance of device output driver. */

            Rttpu = static_cast<double>(Rtt_wr * 2.0);
            Rttpd = static_cast<double>(Rtt_wr * 2.0);
            Rdevice = Rs + Ron;

            Vwrite = (bitValue == 0) ? VSSQ : VDDQ;

            /* Bus voltage equation. */
            double Vbus;

            Vbus = ( (VDDQ / Rttpu) + (VSSQ / Rttpd) + (Vwrite / Rdevice) ) 
                / ( (1.0f / Rttpu) + (1.0f / Rttpd) + (1.0f / Rdevice) );

            /* Bus current equation. */
            double Ibus, Ipu, Ipd;

            Ipu = (VDDQ - Vbus) / Rttpu;
            Ipd = (Vbus - VSSQ) / Rttpd;
            Ibus = (Vbus - Vwrite) / Rdevice;

            /* Power calculation equation. */
            Pdq = Ipu * Ipu * Rttpu + Ipd * Ipd * Rttpd + Ibus * Ibus * Rdevice;
        }
    }
    /*
     *  For 3 and 4 ranks per dimm, we assume one rank is terminated, and 
     *  the remaining 1 or 2 ranks have ODT off (i.e., High-Z) 
     */
    else if( ranksPerDimm == 2 || ranksPerDimm == 3 || ranksPerDimm == 4 ) 
    {
        if( isRead )
        {
            /* 
             * Calculate using delta-wye transformation and then solving for 
             * bus and terminated rank voltages, followed by currents and power. 
             */
            double R1, R2, R3, R4, R5;
            double Rttpu, Rttpd, Rothpu, Rothpd, Rdevice, Rs, Ron;

            Rs = 15.0f;  /* Series resistance of device. */
            Ron = 34.0f; /* Resistance of device output driver. */

            Rdevice = Rs + Ron;

            /* Pull-up/Pull-down at controller. */
            Rttpu = static_cast<double>(Rtt_cont) * 2.0f;
            Rttpd = static_cast<double>(Rtt_cont) * 2.0f;

            /* Pull-up/Pull-down at terminated rank. */
            Rothpu = static_cast<double>(Rtt_nom) * 2.0f;
            Rothpd = static_cast<double>(Rtt_nom) * 2.0f;

            if( bitValue == 0 )
            {
                R1 = Rttpu;
                /* Device resistors are in parallel with controller pull-down. */
                R2 = 1.0f / ( (1.0f / Rttpu) + (1.0f / Rdevice) ); 
            }
            else
            {
                /* Device resistors are in parallel with controller pull-up. */
                R1 = 1.0f / ( (1.0f / Rttpu) + (1.0f / Rdevice) ); 
                R2 = Rttpd;
            }

            R3 = Rs;
            R4 = Rothpu;
            R5 = Rothpd;

            /* Delta-wye transformation */
            double RP, RB, RC;
            RP = R3*R4 + R4*R5 + R5*R3;
            RB = RP / R5;
            RC = RP / R4;

            /* Combine parallel resistors. */
            double RX, RY;
            RX = 1.0f / ( (1.0f / R1) + (1.0f / RB) );
            RY = 1.0f / ( (1.0f / R2) + (1.0f / RC) );

            /* Bus voltage calculation. */
            double Vbus, Ibus;

            Ibus = (VDDQ - VSSQ) / (RX + RY);
            Vbus = VSSQ + Ibus * RY;

            /* Voltage at terminated rank. */
            double Vterm;

            Vterm = -1.0f * R3 * ( ((VDDQ - Vbus) / R1) 
                    - ((Vbus - VSSQ) / R2) ) + Vbus;

            /* Current through each resistor. */
            double I1, I2, I3, I4, I5;

            I1 = (VDDQ - Vbus) / R1;
            I2 = (Vbus - VSSQ) / R2;
            I3 = (Vbus - Vterm) / R3;
            I4 = (VDDQ - Vterm) / R4;
            I5 = (Vterm - VSSQ) / R5;

            /* Power calculation equation. */
            Pdq = I1 * I1 * R1 + I2 * I2 * R2 + I3 * I3 * R3 + I4 * I4 * R4 
                + I5 * I5 * R5;
        }
        else
        {
            /* 
             * Calculate using two delta-wye transformations and then solving 
             * for bus and termated rank voltages, followed by currents and power 
             */
            double Rothpu, Rothpd, Rttpu, Rttpd, Rs, Ron;
            double Vwrite;

            Rttpu = static_cast<double>(Rtt_wr) * 2.0f;
            Rttpd = static_cast<double>(Rtt_wr) * 2.0f;
            Rothpu = static_cast<double>(Rtt_nom) * 2.0f;
            Rothpd = static_cast<double>(Rtt_nom) * 2.0f;

            Rs = 15.0f;  /* Series resistance of DRAM device. */
            Ron = 34.0f; /* Controller output driver resistance. */

            Vwrite = (bitValue == 0) ? VSSQ : VDDQ;

            /* Do delta-wye transforms. */
            double RAL, RBL, RAR, RBR;
            double RPL, RPR;

            RPL = Rothpu*Rothpd + Rothpd*Rs + Rs*Rothpu;
            RAL = RPL / Rothpd;
            RBL = RPL / Rothpu;

            RPR = Rttpu*Rttpd + Rttpd*Rs + Rs*Rttpu;
            RAR = RPR / Rttpd;
            RBR = RPR / Rttpu;

            /* Calculate bus voltage. */
            double Vbus;

            Vbus = ( (VSSQ / RBL) + (VSSQ / RBR) + (VDDQ / RAL) + (VDDQ / RAR) 
                    + (Vwrite / Ron) ) / ( (1.0f / RBL) + (1.0f / RBR) + (1.0f / RAL) 
                    + (1.0f / RAR) + (1.0f / Ron) );

            /* Calculate terminated node voltages. */
            double Vterm, Voterm;

            Vterm  = ( (VDDQ / Rttpu) + (VSSQ / Rttpd) + (Vbus / Rs) ) 
                / ( (1.0f / Rttpu) + (1.0f / Rttpd) + (1.0f / Rs) );
            Voterm = ( (VDDQ / Rothpu) + (VSSQ / Rothpd) + (Vbus / Rs ) ) 
                / ( (1.0f / Rothpu) + (1.0f / Rothpd) + (1.0f / Rs) );

            /* Calculate resistor currents. */
            double Ittpu, Ittpd, Irs1, Irs2, Iothpu, Iothpd, Ibus;

            Ittpu = (VDDQ - Vterm) / Rttpu;
            Ittpd = (Vterm - VSSQ) / Rttpd;
            Iothpu = (VDDQ - Voterm) / Rothpu;
            Iothpd = (Voterm - VSSQ) / Rothpd;
            Irs1 = (Vbus - Vterm) / Rs;
            Irs2 = (Vbus - Voterm) / Rs;
            Ibus = (Vwrite - Vbus) / Ron;

            /* Calculate total power. */
            Pdq = Ittpu*Ittpu*Rttpu + Ittpd*Ittpd*Rttpd + Iothpu*Iothpu*Rothpu 
                + Iothpd*Iothpd*Rothpd + Irs1*Irs1*Rs + Irs2*Irs2*Rs 
                + Ibus*Ibus*Ron;
        }
    }

    return Pdq;
}
