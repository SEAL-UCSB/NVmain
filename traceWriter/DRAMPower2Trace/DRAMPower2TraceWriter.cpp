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
*******************************************************************************/

#include "traceWriter/DRAMPower2Trace/DRAMPower2TraceWriter.h"
#include "src/Params.h"
#include "include/NVMHelpers.h"

using namespace NVM;

DRAMPower2TraceWriter::DRAMPower2TraceWriter( ) : lastCommand(0)
{

}

DRAMPower2TraceWriter::~DRAMPower2TraceWriter( )
{

}

void DRAMPower2TraceWriter::Init( Config *conf )
{
    Params *p = new Params( );
    p->SetParams( conf );

    if( conf->KeyExists( "DRAMPower2XML" ) )
    {
        std::string xmlFileName = conf->GetString( "DRAMPower2XML" );

        if( xmlFileName[0] != '/' )
        {
            xmlFileName  = NVM::GetFilePath( conf->GetFileName() );
            xmlFileName += conf->GetString( "DRAMPower2XML" );
        }

        xmlFile.open( xmlFileName.c_str(),
                      std::ofstream::out | std::ofstream::trunc );

        if( !xmlFile.is_open( ) )
        {
            std::cerr << "DRAMPower2TraceWriter: Could not open file " << conf->GetString( "DRAMPower2XML" ) << std::endl;
            return;
        }

        xmlFile << "<!DOCTYPE memspec SYSTEM \"memspec.dtd\">\n"
                << "<memspec>\n"
                << "  <parameter id=\"memoryId\" type=\"string\" value=\"NVMain_DRAM\" />\n"
                << "  <parameter id=\"memoryType\" type=\"string\" value=\"DDR3\" />\n"
                << "  <memarchitecturespec>\n"
                << "    <parameter id=\"nbrOfBanks\" type=\"uint\" value=\"" << p->BANKS << "\" />\n"
                << "    <parameter id=\"dataRate\" type=\"uint\" value=\"" << p->RATE << "\" />\n"
                << "    <parameter id=\"burstLength\" type=\"uint\" value=\"8\" />\n"
                << "  </memarchitecturespec>\n"
                << "  <memtimingspec>\n"
                << "      <parameter id=\"clkMhz\" type=\"double\" value=\"" << p->CLK << "\" />\n"
                << "      <parameter id=\"RC\" type=\"uint\" value=\"" << p->tRP + p->tRAS << "\" />\n"
                << "      <parameter id=\"RCD\" type=\"uint\" value=\"" << p->tRCD << "\" />\n"
                << "      <parameter id=\"RL\" type=\"uint\" value=\"" << p->tCAS << "\" />\n"
                << "      <parameter id=\"RP\" type=\"uint\" value=\"" << p->tRP << "\" />\n"
                << "      <parameter id=\"RFC\" type=\"uint\" value=\"" << p->tRFC << "\" />\n"
                << "      <parameter id=\"RAS\" type=\"uint\" value=\"" << p->tRAS << "\" />\n"
                << "      <parameter id=\"WL\" type=\"uint\" value=\"" << p->tCWD << "\" />\n"
                << "      <parameter id=\"AL\" type=\"uint\" value=\"" << p->tAL << "\" />\n"
                << "      <parameter id=\"DQSCK\" type=\"uint\" value=\"0\" />\n"
                << "      <parameter id=\"RTP\" type=\"uint\" value=\"" << p->tRTP << "\" />\n"
                << "      <parameter id=\"WR\" type=\"uint\" value=\"" << p->tWR << "\" />\n"
                << "      <parameter id=\"XP\" type=\"uint\" value=\"" << p->tXP << "\" />\n"
                << "      <parameter id=\"XPDLL\" type=\"uint\" value=\"" << p->tXPDLL << "\" />\n"
                << "      <parameter id=\"XS\" type=\"uint\" value=\"" << p->tXS << "\" />\n"
                << "      <parameter id=\"XSDLL\" type=\"uint\" value=\"" << p->tXSDLL << "\" />\n"
                << "  </memtimingspec>\n"
                << "  <mempowerspec>\n"
                << "      <parameter id=\"idd0\" type=\"double\" value=\"" << p->EIDD0 << "\" />\n"
                << "      <parameter id=\"idd2p0\" type=\"double\" value=\"" << p->EIDD2P0 << "\" />\n"
                << "      <parameter id=\"idd2p1\" type=\"double\" value=\"" << p->EIDD2P1 << "\" />\n"
                << "      <parameter id=\"idd2n\" type=\"double\" value=\"" << p->EIDD2N << "\" />\n"
                << "      <parameter id=\"idd3p0\" type=\"double\" value=\"" << p->EIDD3P << "\" />\n"
                << "      <parameter id=\"idd3p1\" type=\"double\" value=\"" << p->EIDD3P << "\" />\n"
                << "      <parameter id=\"idd3n\" type=\"double\" value=\"" << p->EIDD3N << "\" />\n"
                << "      <parameter id=\"idd4w\" type=\"double\" value=\"" << p->EIDD4W << "\" />\n"
                << "      <parameter id=\"idd4r\" type=\"double\" value=\"" << p->EIDD4R << "\" />\n"
                << "      <parameter id=\"idd5\" type=\"double\" value=\"" << p->EIDD5B << "\" />\n"
                << "      <parameter id=\"idd6\" type=\"double\" value=\"" << p->EIDD6 << "\" />\n"
                << "      <parameter id=\"vdd\" type=\"double\" value=\"" << p->Voltage << "\" />\n"
                << "      <parameter id=\"clk_period\" type=\"double\" value=\"" << 1000.0 / static_cast<double>(p->CLK) << "\" />\n"
                << "  </mempowerspec>\n"
                << "</memspec>\n";

        xmlFile.close( );
    }

    delete p;
}

void DRAMPower2TraceWriter::SetTraceFile( std::string file )
{
    // Note: This function assumes an absolute path is given, otherwise
    // the current directory is used. 

    traceFile = file;

    trace.open( traceFile.c_str( ) );

    if( !trace.is_open( ) )
    {
        std::cout << "Warning: Could not open trace file " << file
                  << ". Output will be suppressed." << std::endl;
    }
}

std::string DRAMPower2TraceWriter::GetTraceFile( )
{
    return traceFile;
}

/* The DRAMPower2 model only supports single devices, so we need per rank traces. */
bool DRAMPower2TraceWriter::GetPerChannelTraces( )
{
    return false;
}

bool DRAMPower2TraceWriter::GetPerRankTraces( )
{
    return true;
}

bool DRAMPower2TraceWriter::SetNextAccess( TraceLine *nextAccess )
{
    bool rv = false;

    if( trace.is_open( ) )
    {
        WriteTraceLine( trace, nextAccess );
        rv = trace.good();
    }

    if( this->GetEcho() )
    {
        WriteTraceLine( std::cout, nextAccess );
        rv = true;
    }

    return rv;
}

void DRAMPower2TraceWriter::WriteTraceLine( std::ostream& stream, TraceLine *line )
{
    NVMAddress addr = line->GetAddress( );

    assert( addr.IsTranslated() );

    /* The example trace seems to show relative time, but absolulte time provides correct result? */
    lastCommand = 0;

    switch( line->GetOperation() )
    {
        case ACTIVATE:
        {
            stream << (line->GetCycle() - lastCommand) << ",ACT," << addr.GetBank() << std::endl;
            lastCommand = line->GetCycle();
            break;
        }
        case READ:
        {
            stream << (line->GetCycle() - lastCommand) << ",RD," << addr.GetBank() << std::endl;
            lastCommand = line->GetCycle();
            break;
        }
        case READ_PRECHARGE:
        {
            stream << (line->GetCycle() - lastCommand) << ",RDA," << addr.GetBank() << std::endl;
            lastCommand = line->GetCycle();
            break;
        }
        case WRITE:
        {
            stream << (line->GetCycle() - lastCommand) << ",WR," << addr.GetBank() << std::endl;
            lastCommand = line->GetCycle();
            break;
        }
        case WRITE_PRECHARGE:
        {
            stream << (line->GetCycle() - lastCommand) << ",WRA," << addr.GetBank() << std::endl;
            lastCommand = line->GetCycle();
            break;
        }
        case PRECHARGE:
        {
            stream << (line->GetCycle() - lastCommand) << ",PRE," << addr.GetBank() << std::endl;
            lastCommand = line->GetCycle();
            break;
        }
        case PRECHARGE_ALL:
        {
            //stream << (line->GetCycle() - lastCommand) << ",PREA,0" << std::endl;
            // TODO: The PRECHARGE_ALL request generated before refresh is meant to precharge all
            // subarrays -- We will need a different command for precharging all banks
            stream << (line->GetCycle() - lastCommand) << ",PRE," << addr.GetBank() << std::endl;
            lastCommand = line->GetCycle();
            break;
        }
        case REFRESH:
        {
            stream << (line->GetCycle() - lastCommand) << ",REF,0" << std::endl;
            lastCommand = line->GetCycle();
            break;
        }
        case POWERDOWN_PDA:
        {
            stream << (line->GetCycle() - lastCommand) << ",PDN_F_ACT,0" << std::endl;
            lastCommand = line->GetCycle();
            pdState = DRAMPower2TraceWriter::PDN_F_ACT;
            break;
        }
        case POWERDOWN_PDPF:
        {
            stream << (line->GetCycle() - lastCommand) << ",PDN_F_PRE,0" << std::endl;
            lastCommand = line->GetCycle();
            pdState = DRAMPower2TraceWriter::PDN_F_PRE;
            break;
        }
        case POWERDOWN_PDPS:
        {
            stream << (line->GetCycle() - lastCommand) << ",PDN_S_PRE,0" << std::endl;
            lastCommand = line->GetCycle();
            pdState = DRAMPower2TraceWriter::PDN_S_PRE;
            break;
        }
        case POWERUP:
        {
            if( pdState == DRAMPower2TraceWriter::PDN_F_ACT )
            {
                stream << (line->GetCycle() - lastCommand) << ",PUP_ACT,0" << std::endl;
            }
            else if( pdState == DRAMPower2TraceWriter::PDN_F_PRE || 
                     pdState == DRAMPower2TraceWriter::PDN_S_PRE )
            {
                stream << (line->GetCycle() - lastCommand) << ",PDN_S_PRE,0" << std::endl;
            }
            else
            {
                std::cerr << "DRAMPower2TraceWriter: Unknown powerdown state" << std::endl;
            }
            pdState = DRAMPower2TraceWriter::PUP;

            lastCommand = line->GetCycle();
        }
            break;
        default:
            break;
    }
}

