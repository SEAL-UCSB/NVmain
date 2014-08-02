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

#include "Endurance/RowModel/RowModel.h"

using namespace NVM;

RowModel::RowModel( )
{
    /*
     *  Clear the life map, which will hold all of the endurance
     *  values for each of our rows. Do this to ensure it didn't
     *  happen to be allocated somewhere that thinks it contains 
     *  values.
     */
    life.clear( );
}

RowModel::~RowModel( )
{
    /*
     *  Nothing to do here. We do not own the *config pointer, so
     *  don't delete that.
     */
}

void RowModel::SetConfig( Config *conf, bool createChildren )
{
    Params *params = new Params( );
    params->SetParams( conf );
    SetParams( params );

    SetGranularity( p->COLS * 8 );

    EnduranceModel::SetConfig( conf, createChildren );
}

ncycles_t RowModel::Read( NVMainRequest *request )
{
    uint64_t row;
    ncycles_t rv = 0;

    /*
     *  For our simple row model, we just set the key equal to the row.
     */
    request->address.GetTranslatedAddress( &row, NULL, NULL, NULL, NULL, NULL );

    if( IsDead( row ) )
    {
        rv = -(rv + 1);
    }

    return rv;
}

ncycles_t RowModel::Write( NVMainRequest *request, NVMDataBlock& /*oldData*/ ) 
{
    NVMAddress address = request->address;

    /*
     *  The default life map is an stl map< uint64_t, uint64_t >. 
     *  You may map row and col to this map_key however you want.
     *  It is up to you to ensure there are no collisions here.
     */
    uint64_t row;
    ncycles_t rv = 0;

    /*
     *  For our simple row model, we just set the key equal to the row.
     */
    address.GetTranslatedAddress( &row, NULL, NULL, NULL, NULL, NULL );
    
    /*
     *  If using the default life map, we can call the DecrementLife
     *  function which will check if the map_key already exists. If so,
     *  the life value is decremented (write count incremented). Otherwise 
     *  the map_key is inserted with a write count of 1.
     */
    if( !DecrementLife( row ) )
        rv = -1;

    return rv;
}
