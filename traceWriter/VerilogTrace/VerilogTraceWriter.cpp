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

#include "traceWriter/VerilogTrace/VerilogTraceWriter.h"

using namespace NVM;

VerilogTraceWriter::VerilogTraceWriter( ) : lastCommand(0)
{

}

VerilogTraceWriter::~VerilogTraceWriter( )
{

}

void VerilogTraceWriter::Init( Config *conf )
{
    deviceWidth = conf->GetValue( "DeviceWidth" );

    if( deviceWidth != 4 && deviceWidth % 8 != 0 )
    {
        std::cout << "VerilogTraceWriter: Don't know how to write a device "
                  << " with width " << deviceWidth << std::endl;
        exit(0);
    }
}

void VerilogTraceWriter::SetTraceFile( std::string file )
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

std::string VerilogTraceWriter::GetTraceFile( )
{
    return traceFile;
}

/* The verilog model only supports single devices, so we need per rank traces. */
bool VerilogTraceWriter::GetPerChannelTraces( )
{
    return false;
}

bool VerilogTraceWriter::GetPerRankTraces( )
{
    return true;
}

bool VerilogTraceWriter::SetNextAccess( TraceLine *nextAccess )
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

void VerilogTraceWriter::WriteTraceLine( std::ostream& stream, TraceLine *line )
{
    NVMDataBlock& data = line->GetData( );
    NVMAddress addr = line->GetAddress( );

    assert( addr.IsTranslated() );

    switch( line->GetOperation() )
    {
        case ACTIVATE:
        {
            stream << "        nop(" << (line->GetCycle() - lastCommand) << ");" << std::endl;
            lastCommand = line->GetCycle();

            stream << "        activate(" << addr.GetBank() << ", " << addr.GetRow() 
                   << ");" << std::endl;
            break;
        }
        case READ:
        case READ_PRECHARGE:
        {
            stream << "        nop(" << (line->GetCycle() - lastCommand) << ");" << std::endl;
            lastCommand = line->GetCycle();

            stream << "        read(" << addr.GetBank() << ", " << addr.GetCol() 
                   << ", " << ((line->GetOperation() == READ_PRECHARGE) ? "1" : "0") 
                   << ", 0);" << std::endl;
            break;
        }
        case WRITE:
        case WRITE_PRECHARGE:
        {
            stream << "        nop(" << (line->GetCycle() - lastCommand) << ");" << std::endl;
            lastCommand = line->GetCycle();

            stream << "        write(" << addr.GetBank() << ", " << addr.GetCol() 
                   << ", " << ((line->GetOperation() == WRITE_PRECHARGE) ? "1" : "0") 
                   << ", 0, 0, {";

            // Assuming BL8 write
            for( ncounter_t burstIdx = 0; burstIdx < 8; burstIdx++ )
            {
                if( deviceWidth == 8 || deviceWidth == 16 )
                {
                    for( ncounter_t byteIdx = 0; byteIdx < (deviceWidth / 8); byteIdx++ )
                    {
                        uint8_t nextByte = data.GetByte(burstIdx * (deviceWidth / 8) + byteIdx);

                        if( byteIdx != 0 || burstIdx != 0 ) stream << ", ";

                        stream << "{8'h" << std::hex << (uint32_t)nextByte << std::dec << "}";
                    }
                }
                else if( deviceWidth == 4 )
                {
                    if( burstIdx % 2 == 0 )
                    {
                        uint8_t nextByte = data.GetByte(burstIdx / 2);

                        if( burstIdx != 0 ) stream << ", ";

                        stream << "{8'h" << std::hex << (uint32_t)nextByte << std::dec << "}";
                    }
                }
            }

            stream << "});" << std::endl; 
            break;
        }
        case PRECHARGE:
        case PRECHARGE_ALL:
        {
            stream << "        nop(" << (line->GetCycle() - lastCommand) << ");" << std::endl;
            lastCommand = line->GetCycle();

            stream << "        precharge(" << addr.GetBank() << ", "
                   << ((line->GetOperation() == PRECHARGE_ALL) ? "1" : "0")
                   << ");" << std::endl;
            break;
        }
        case REFRESH:
        {
            stream << "        nop(" << (line->GetCycle() - lastCommand) << ");" << std::endl;
            lastCommand = line->GetCycle();

            stream << "        refresh;" << std::endl;
            break;
        }
        case POWERDOWN_PDA:
        case POWERDOWN_PDPF:
        case POWERDOWN_PDPS:
        {
            stream << "        power_down(" << (line->GetCycle() - lastCommand) 
                   << ");" << std::endl;
            lastCommand = line->GetCycle();
            break;
        }
        case POWERUP:
            break;
        default:
            break;
    }
}

