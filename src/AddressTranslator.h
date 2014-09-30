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

#ifndef __ADDRESSTRANSLATOR_H__
#define __ADDRESSTRANSLATOR_H__

#include "src/TranslationMethod.h"
#include "src/Config.h"
#include "src/Stats.h"
#include "include/NVMainRequest.h"

namespace NVM {

typedef enum 
{ 
    NO_FIELD, 
    ROW_FIELD, 
    COL_FIELD, 
    BANK_FIELD, 
    RANK_FIELD, 
    CHANNEL_FIELD,
    SUBARRAY_FIELD,
} TranslationField;

class AddressTranslator
{
  public:
    AddressTranslator( );
    virtual ~AddressTranslator( );

    virtual void SetConfig( Config * /*config*/, bool /*createChildren*/ = true ) { }

    void SetBusWidth( int );
    void SetBurstLength( int );
    void SetTranslationMethod( TranslationMethod *m );
    TranslationMethod *GetTranslationMethod( );
    
    virtual void Translate( uint64_t address, uint64_t *row, uint64_t *col, uint64_t *bank, 
                            uint64_t *rank, uint64_t *channel, uint64_t *subarray );
    virtual void Translate( NVMainRequest *request, uint64_t *row, uint64_t *col, uint64_t *bank, 
                            uint64_t *rank, uint64_t *channel, uint64_t *subarray );

    virtual uint64_t ReverseTranslate( const uint64_t& row, const uint64_t& col, 
                                       const uint64_t& bank, const uint64_t& rank, 
                                       const uint64_t& channel, const uint64_t& subarray );

    virtual uint64_t Translate( uint64_t address );
    virtual uint64_t Translate( NVMainRequest *request );
    virtual void SetDefaultField( TranslationField f ); 

    void SetStats( Stats *stats );
    Stats *GetStats( );

    void StatName( std::string name );
    std::string StatName( );

    virtual void RegisterStats( ) { } 
    virtual void CalculateStats( ) { }

    virtual void CreateCheckpoint( std::string /*dir*/ ) { }
    virtual void RestoreCheckpoint( std::string /*dir*/ ) { }

  private:
    TranslationMethod *method;
    TranslationField defaultField;
    int busWidth;
    int burstLength;
    int lowColBits;

    Stats *stats;
    std::string statName;

  protected:
    uint64_t Divide( uint64_t partSize, MemoryPartition partition );
    uint64_t Modulo( uint64_t partialAddr, MemoryPartition partition );
    void FindOrder( int order, MemoryPartition *p );
};


};


#endif


