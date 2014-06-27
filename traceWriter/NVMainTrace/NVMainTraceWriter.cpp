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

#include "traceWriter/NVMainTrace/NVMainTraceWriter.h"

using namespace NVM;

NVMainTraceWriter::NVMainTraceWriter( )
{

}

NVMainTraceWriter::~NVMainTraceWriter( )
{

}

void NVMainTraceWriter::SetTraceFile( std::string file )
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

    /* Write version number of this writer. */
    trace << "NVMV1" << std::endl;
}

std::string NVMainTraceWriter::GetTraceFile( )
{
    return traceFile;
}

bool NVMainTraceWriter::SetNextAccess( TraceLine *nextAccess )
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

void NVMainTraceWriter::WriteTraceLine( std::ostream& stream, TraceLine *line )
{
    NVMDataBlock& data = line->GetData( );
    NVMDataBlock& oldData = line->GetOldData( );

    /* Only print reads or writes. */
    if( line->GetOperation() != READ && line->GetOperation() != WRITE )
        return;

    /* Print memory cycle. */
    stream << line->GetCycle( ) << " ";

    /* Print the operation type */
    if( line->GetOperation( ) == READ )
        stream << "R ";
    else if( line->GetOperation( ) == WRITE )
        stream << "W ";

    /* Print address */
    stream << std::hex << "0x" << line->GetAddress( ).GetPhysicalAddress( ) 
           << std::dec << " ";

    /* Print data. */
    stream << data << " ";

    /* Print previous data. */
    stream << oldData << " ";

    /* Print the thread ID */
    stream << line->GetThreadId( ) << std::endl;
}

