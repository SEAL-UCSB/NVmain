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

#ifndef __SRC_STATS_H__
#define __SRC_STATS_H__


#define AddStat(STAT)                       \
        {                                   \
            _AddStat(STAT, "")              \
        }
#define AddUnitStat(STAT, UNITS)            \
        {                                   \
            _AddStat(STAT, UNITS)           \
        }
#define _AddStat(STAT, UNITS)                                               \
        {                                                                   \
            int *__resetValue = new int [sizeof(STAT)];                     \
            memcpy(__resetValue, static_cast<void *>(&STAT), sizeof(STAT)); \
            this->GetStats()->addStat(static_cast<void *>(&(STAT)),         \
                                      static_cast<void *>(__resetValue),    \
                                      typeid(STAT).name(),                  \
                                      sizeof(STAT),                         \
                                      StatName() + "." + #STAT,             \
                                      UNITS);                               \
        }
#define RemoveStat(STAT) (this->GetStats()->removeStat(static_cast<void *>(&STAT)))



#include <ostream>
#include <typeinfo>
#include <vector>
#include <cstring>

#include "include/NVMTypes.h"

namespace NVM {


class StatBase
{
  public:
    StatBase( ) { }
    ~StatBase( ) { }

    void Reset( );
    void Print( std::ostream& stream, ncounter_t psInterval );

    std::string GetName( ) { return name; }
    void SetName( std::string n ) { name = n; }

    void* GetValue( ) { return value; }
    void SetValue( void *val ) { value = val; }

    std::string GetUnits( ) { return units; }
    void SetUnits( std::string u ) { units = u; }

    void SetResetValue( void *rval ) { resetValue = rval; }
    void *GetResetValue( ) { return resetValue; }

    void SetStatType( std::string st, size_t ts ) { statType = st; typeSize = ts; }
    size_t GetTypeSize( ) { return typeSize; }
    std::string GetTypeName() { return statType; }

  private:
    std::string name, statType, units;
    size_t typeSize;
    void *resetValue;
    void *value;
};

class Stats
{
  public:
    Stats( );
    ~Stats( );

    void addStat( void *stat, void *resetValue, std::string statType, size_t typeSize, std::string name, std::string units );
    void removeStat( void *stat );

    void PrintAll( std::ostream& );
    void ResetAll( );

  private: 
    std::vector<StatBase *> statList;
    ncounter_t psInterval;
};


};


#endif


