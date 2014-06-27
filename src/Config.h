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

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <vector>
#include <string>
#include <map>
#include <set>
#include <fstream>

#include "src/SimInterface.h"

namespace NVM {

class Config 
{
  public:
    Config ();
    ~Config ();

    Config(const Config& conf);
    
    void Read( std::string filename );
    std::string GetFileName( );

    uint64_t GetValueUL( std::string key );
    void     GetValueUL( std::string Key, uint64_t& value );
    int  GetValue( std::string key );
    void GetValue( std::string key, int& value );
    void SetValue( std::string key, std::string value );

    double GetEnergy( std::string key );
    void   GetEnergy( std::string key, double &energy );
    void   SetEnergy( std::string key, std::string energy );

    std::string GetString( std::string key );
    void  GetString( std::string key, std::string& value );
    void  SetString( std::string key, std::string );

    bool  GetBool( std::string key );
    void  GetBool( std::string key, bool& value );
    void  SetBool( std::string key, bool value );

    bool KeyExists( std::string key );

    std::vector<std::string>& GetHooks( );

    void Print( );

    /*
     *  Any special class to get information from the underlying
     *  simulator can be set here.
     */
    void SetSimInterface( SimInterface *simPtr );
    SimInterface *GetSimInterface( );

    void SetDebugLog( );
    std::ostream *GetDebugLog( );

  private:
    std::string fileName;
    std::map<std::string, std::string> values;
    std::set<std::string> warned;
    std::vector<std::string> hookList;
    SimInterface *simPtr;
    std::ofstream debugLogFile;
    bool useDebugLog;

};

};

#endif
