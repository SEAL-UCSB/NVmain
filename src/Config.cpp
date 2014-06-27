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

#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <assert.h>
#include <limits>
#include "src/Config.h"

using namespace NVM;

Config::Config( )
{
    simPtr = NULL;
    useDebugLog = false;
}


Config::~Config( )
{
}

Config::Config(const Config& conf)
{
    std::map<std::string, std::string>::iterator it;
    std::map<std::string, std::string> tmpMap (conf.values);

    for( it = tmpMap.begin(); it != tmpMap.end(); it++ )
    {
        values.insert( std::pair<std::string, std::string>( it->first, it->second ) );
    }

    fileName = conf.fileName;
    simPtr = conf.simPtr;

    std::vector<std::string> tmpVec(conf.hookList);
    std::vector<std::string>::iterator vit;

    for( vit = tmpVec.begin(); vit != tmpVec.end(); vit++ )
    {
        hookList.push_back( (*vit) );
    }
    SetDebugLog( );
}


std::string Config::GetFileName( )
{
    return this->fileName;
}


void Config::Read( std::string filename )
{
    std::string line;
    std::ifstream configFile( filename.c_str( ) );
    std::map<std::string, std::string>::iterator i;
    std::string subline;

    this->fileName = filename;

    if( configFile.is_open( ) ) 
    {
        while( !configFile.eof( ) ) 
        {
            getline( configFile, line );

            /* Ignore blank lines and comments beginning with ';'. */

            /* find the first character that is not space, tab, return */
            size_t cPos = line.find_first_not_of( " \t\r\n" );

            /* if not found, the line is empty. just skip it */
            if( cPos == std::string::npos )
                continue;
            /*
             * else, check whether the first character is the comment flag. 
             * if so, skip it 
             */
            else if( line[cPos] == ';' )
                continue;
            /* else, remove the redundant white space and the possible comments */
            else
            {
                /* find the position of the first ';' */
                size_t colonPos = line.find_first_of( ";" );

                /* if there is no ';', extract all */
                if( colonPos == std::string::npos )
                {
                    subline = line.substr( cPos );
                }
                else
                {
                    /* colonPos must be larger than cPos */
                    assert( colonPos > cPos );

                    /* extract the useful message from the line */
                    subline = line.substr( cPos, (colonPos - cPos) );
                }
            }

            /* parse the parameters and values */
            char *cline;
            cline = new char[subline.size( ) + 1];
            strcpy( cline, subline.c_str( ) );
            
            char *tokens = strtok( cline, " " );
            
            std::string ty = std::string( tokens );
            
            tokens = strtok( NULL, " " );
            
            i = values.find( ty );
            if( i != values.end( ) )
                values.erase( i );

            if( tokens != NULL )
            {
                /* Treat hooks specially. */
                if( ty == "AddHook" )
                {
                    hookList.push_back( tokens );
                }
                else
                {
                    values.insert( std::pair<std::string, 
                            std::string>( ty, tokens ) );
                }
            }
            else
            {
                std::cout << "Config: Missing value for key " << ty << std::endl;
                values.insert( std::pair<std::string, std::string>( ty, "" ) );
            }
        }
    }
    else
    {
        std::cout << "NVMain: Could not read configuration file: " 
            << filename << std::endl;
        exit(1);
    }

    SetDebugLog( );
}

bool Config::KeyExists( std::string key )
{
    std::map<std::string, std::string>::iterator i;

    if( values.empty( ) )
        return false;

    i = values.find( key );

    if( i == values.end( ) )
        return false;

    return true;
}


void Config::GetString( std::string key, std::string& value )
{
    if( !KeyExists( key ) && !warned.count( key ) )
    {   
        std::cout << "Config: Warning: Key " << key << " is not set. Using '" << value 
                  << "' as the default. Please configure this value if this is wrong." << std::endl;
        warned.insert( key );
    }
    else if( KeyExists( key ) )
    {
        value = GetString( key );
    }
}


std::string Config::GetString( std::string key )
{
    std::map<std::string, std::string>::iterator i;
    std::string value;

    if( values.empty( ) )
    {
        std::cerr << "Configuration has not been read yet." << std::endl;
        return "";
    }

    i = values.find( key );

    if( !KeyExists( key ) && !warned.count( key ) )
    {
        std::cout << "Config: Warning: Key " << key << " is not set. Using '' as the default. Please configure this value if this is wrong." << std::endl;
        warned.insert( key );
    }


    /*
     *  Find returns map::end if the element is not found. We will use -1 as
     *  the error code. Functions calling this function should check for -1
     *  for possible configuration file problems.
     *
     *  If the key is found, return the second element (the key value).
     */
    if( i == values.end( ) )
        value = "";
    else
        value = i->second;

    return value;
}


void Config::SetString( std::string key, std::string value )
{
    values.insert( std::pair<std::string, std::string>( key, value ) );
}

void Config::GetValueUL( std::string key, uint64_t& value )
{
    if( !KeyExists( key ) && !warned.count( key ) )
    {
        std::cout << "Config: Warning: Key " << key << " is not set. Using '" << value
                  << "' as the default. Please configure this value if this is wrong." << std::endl;
        warned.insert( key );
    }
    else if( KeyExists( key ) )
    {
        value = GetValueUL( key );
    }
}

uint64_t Config::GetValueUL( std::string key )
{
    std::map<std::string, std::string>::iterator i;
    uint64_t value;

    if( values.empty( ) )
    {
        std::cerr << "Configuration has not been read yet." << std::endl;
        return std::numeric_limits<uint64_t>::max( );
    }

    i = values.find( key );

    if( !KeyExists( key ) && !warned.count( key ) )
    {
        std::cout << "Config: Warning: Key " << key << " is not set. Using '-1' as the default. Please configure this value if this is wrong." << std::endl;
        warned.insert( key );
    }

    /*
     *  Find returns map::end if the element is not found. We will use -1 as
     *  the error code. Functions calling this function should check for -1
     *  for possible configuration file problems.
     *
     *  If the key is found, return the second element (the key value).
     */
    if( i == values.end( ) )
        value = std::numeric_limits<uint64_t>::max( );
    else
        value = strtoul( i->second.c_str( ), NULL, 10 );

    return value;
}

void Config::GetValue( std::string key, int& value )
{
    if( !KeyExists( key ) && !warned.count( key ) )
    {
        std::cout << "Config: Warning: Key " << key << " is not set. Using '" << value
                  << "' as the default. Please configure this value if this is wrong." << std::endl;
        warned.insert( key );
    }
    else if( KeyExists( key ) )
    {
        value = GetValue( key );
    }
}

int Config::GetValue( std::string key )
{
    std::map<std::string, std::string>::iterator i;
    int value;

    if( values.empty( ) )
    {
        std::cerr << "Configuration has not been read yet." << std::endl;
        return -1;
    }

    i = values.find( key );

    if( !KeyExists( key ) && !warned.count( key ) )
    {
        std::cout << "Config: Warning: Key " << key << " is not set. Using '-1' as the default. Please configure this value if this is wrong." << std::endl;
        warned.insert( key );
    }

    /*
     *  Find returns map::end if the element is not found. We will use -1 as
     *  the error code. Functions calling this function should check for -1
     *  for possible configuration file problems.
     *
     *  If the key is found, return the second element (the key value).
     */
    if( i == values.end( ) )
        value = -1;
    else
        value = atoi( i->second.c_str( ) );

    return value;
}

void Config::SetValue( std::string key, std::string value )
{
    std::map<std::string, std::string>::iterator i;

    i = values.find( key );

    if( i != values.end( ) )
        values.erase( i );

    values.insert( std::pair<std::string, std::string>( key, value ) );
}

void Config::GetEnergy( std::string key, double& value )
{
    if( !KeyExists( key ) && !warned.count( key ) )
    {
        std::cout << "Config: Warning: Key " << key << " is not set. Using '" << value
                  << "' as the default. Please configure this value if this is wrong." << std::endl;
        warned.insert( key );
    }
    else if( KeyExists( key ) )
    {
        value = GetEnergy( key );
    }
}

double Config::GetEnergy( std::string key )
{
    std::map<std::string, std::string>::iterator i;
    double value;

    if( values.empty( ) )
    {
        std::cerr << "Configuration has not been read yet." << std::endl;
        return -1;
    }

    i = values.find( key );

    if( !KeyExists( key ) && !warned.count( key ) )
    {
        std::cout << "Config: Warning: Key " << key << " is not set. Using '-1.0' as the default. Please configure this value if this is wrong." << std::endl;
        warned.insert( key );
    }

    /*
     *  Find returns map::end if the element is not found. We will use -1 as
     *  the error code. Functions calling this function should check for -1
     *  for possible configuration file problems.
     *
     *  If the key is found, return the second element (the key value).
     */
    if( i == values.end( ) )
        value = -1.0;
    else
        value = (double)atof( i->second.c_str( ) );

    return value;
}

void Config::SetEnergy( std::string key, std::string energy )
{
    values.insert( std::pair<std::string, std::string>( key, energy ) );
}

void Config::GetBool( std::string key, bool& value )
{
    if( !KeyExists( key ) && !warned.count( key ) )
    {
        std::string defaultValue = (value ? "true" : "false");
        std::cout << "Config: Warning: Key " << key << " is not set. Using '" << defaultValue
                  << "' as the default. Please configure this value if this is wrong." << std::endl;
        warned.insert( key );
    }
    else if( KeyExists( key ) )
    {
        value = GetBool( key );
    }
}

bool Config::GetBool( std::string key )
{
    bool rv = false;

    if( !KeyExists( key ) && !warned.count( key ) )
    {
        std::cout << "Config: Warning: Key " << key << " is not set. Using 'false' as the default. Please configure this value if this is wrong." << std::endl;
        warned.insert( key );
    }

    if( KeyExists( key ) && GetString( key ) == "true" )
        rv = true;

    return rv;
}

void Config::SetBool( std::string key, bool value )
{
    if( value )
        SetString( key, "true" );
    else
        SetString( key, "false" );
}

std::vector<std::string>& Config::GetHooks( )
{
    return hookList;
}


void Config::Print( )
{
    std::map<std::string, std::string>::iterator i;

    for( i = values.begin( ); i != values.end( ); ++i) 
    {
        std::cout << (i->first) << " = " << (i->second) << std::endl;
    }
}

void Config::SetSimInterface( SimInterface *ptr )
{
    simPtr = ptr;
}

SimInterface *Config::GetSimInterface( )
{
    return simPtr;
}

void Config::SetDebugLog( )
{
    if( this->KeyExists( "DebugLog" ) )
    {
        std::string debugLogFilename = this->GetString( "DebugLog" );
        debugLogFile.open( debugLogFilename.c_str() );
        if( !debugLogFile.is_open() )
        {
            std::cerr << "NVMain: Could not open debug log file: " << debugLogFilename << std::endl;
            exit(1);
        }
        std::cout << &debugLogFile << std::endl;
        std::cout << "Printing debug information to '" << debugLogFilename << "'" << std::endl;
        useDebugLog = true;
    }
}

std::ostream *Config::GetDebugLog( )
{
    if( useDebugLog )
        return &debugLogFile;

    return &std::cerr;
}

