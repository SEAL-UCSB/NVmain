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


#include "Utils/AccessPredictor/PerfectPredictor/PerfectPredictor.h"
#include "NVM/nvmain.h"
#include "src/MemoryController.h"
#include "MemControl/DRAMCache/DRAMCache.h"

#include <iostream>
#include <cstdlib>
#include <cassert>



using namespace NVM;



PerfectPredictor::PerfectPredictor( )
{

}


PerfectPredictor::~PerfectPredictor( )
{

}


uint64_t PerfectPredictor::Translate( NVMainRequest *request )
{
    ncounter_t dest = GetMissDestination( );

    /* 
     * We assume the "hit" destination is some kind of cache for this to make sense.
     * For now, assume that it is a DRAM cache, although later this may be changed
     * to be any generic type of cache.
     *
     * This predictor only returns which route the request should take. We assume
     * that this module's parent has two children representing the multiple routes.
     * The parent module must set this module's parent on initialization.
     */
    assert( parent != NULL );

    /* Attempt the hit destination first. */
    if( GetParent()->GetTrampoline()->GetChild( GetHitDestination() )->IssueFunctional( request ) )
    {
        /* Request would be success to cache, issue there. */
        dest = GetHitDestination( ); 
    }
    else
    {
        /* Request would fail to cache, issue to backing memory. */
        dest = GetMissDestination( ); 
    }

    return dest;
}


//void PerfectPredictor::Cycle( ncycle_t /*steps*/ )
//{
//
//}
//
//
//bool PerfectPredictor::IssueAtomic( NVMainRequest *req )
//{
//    DRAMCache *drc = dynamic_cast<DRAMCache *>( hitController );
//
//    assert( drc != NULL );
//
//    return drc->IssueAtomic( req );
//}
//
//bool PerfectPredictor::IssueCommand( NVMainRequest *req )
//{
//    bool rv = false;
//
//    /* 
//     * We assume the "hit" destination is some kind of cache for this to make sense.
//     * For now, assume that it is a DRAM cache, although later this may be changed
//     * to be any generic type of cache.
//     */
//    /* TODO: Implement an IssueFunctional for NVMain. */
//    DRAMCache *drc = dynamic_cast<DRAMCache *>( hitController );
//    
//    assert( drc != NULL );
//
//    if( req->type == WRITE )
//    {
//        /*
//         *  Writes always hit, no prediction needed.
//         */
//        rv = drc->IssueCommand( req );
//    }
//    else if( drc->IssueFunctional( req ) )
//    {
//        //std::cout << "Request 0x" << std::hex << req->address.GetPhysicalAddress() << std::dec
//        //          << " predicted HIT." << std::endl;
//
//        rv = drc->IssueCommand( req );
//    }
//    else
//    {
//        if( missMemory != NULL )
//        {
//            rv = missMemory->IssueCommand( req );
//        }
//        else if( missController != NULL )
//        {
//            rv = missController->IssueCommand( req );
//        }
//        else
//        {
//            std::cout << "Error: No miss destination specified." << std::endl;
//            exit(1);
//        }
//
//        //std::cout << "Request 0x" << std::hex << req->address.GetPhysicalAddress() << std::dec
//        //          << " predicted MISS." << std::endl;
//
//        outstandingMisses.insert( req->address.GetPhysicalAddress( ) );
//    }
//
//    return rv;
//}
//
//bool PerfectPredictor::RequestComplete( NVMainRequest *req )
//{
//    bool rv = false;
//
//    /*
//     *  Check if the completed request was predicted as a miss. If so, we need to
//     *  install this in the hit destination as well.
//     */
//    if( req->owner == this )
//    {
//        //std::cout << "Request 0x" << std::hex << req->address.GetPhysicalAddress() << std::dec
//        //          << " fill complete." << std::endl;
//
//        delete req;
//        rv = true;
//    }
//    else
//    {
//        //std::cout << "Request 0x" << std::hex << req->address.GetPhysicalAddress() << std::dec
//        //          << " complete." << std::endl;
//
//        if( outstandingMisses.count( req->address.GetPhysicalAddress() ) != 0 )
//        {
//            //std::cout << "Request 0x" << std::hex << req->address.GetPhysicalAddress() << std::dec
//            //          << " starting fill." << std::endl;
//
//            NVMainRequest *fillReq = new NVMainRequest();
//
//            *fillReq = *req;
//            fillReq->owner = this;
//            fillReq->type = WRITE;
//
//            hitController->IssueCommand( fillReq );
//        }
//
//        rv = GetParent( )->RequestComplete( req );
//    }
//
//    return rv;
//}

