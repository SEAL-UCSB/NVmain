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

#include "Endurance/Distributions/Normal.h"
#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace NVM;

NormalDistribution::NormalDistribution( )
{
    nextEndurance = 0.0f;
}

NormalDistribution::NormalDistribution( Config *conf )
{
    config = conf;

    if( conf->GetValue( "EnduranceDistMean" ) == -1 )
    {
        std::cout << "EnduranceDistMean parameter not found for normal \
            distribution!!\n";
        mean = 1000000;
    }
    else
    {
        mean = conf->GetValue( "EnduranceDistMean" );
    }


    if( conf->GetValue( "EnduranceDistVariance" ) == -1 )
    {
        std::cout << "EnduranceDistVariance parameter not found for normal \
            distribution!!\n";
        variance = 100000;
    }
    else
    {
        variance = conf->GetValue( "EnduranceDistVariance" );
    }

    nextEndurance = 0.0f;
}

uint64_t NormalDistribution::GetEndurance( )
{
    double x1, x2;
    double y1, y2;
    double w;

    /*
     *  This method will generate two results at the same time. Instead of
     *  throwing one of the results out, store it and return it here. Once
     *  returned, reset the result to 0.
     */
    if( nextEndurance != 0.0f )
    {
        w = nextEndurance;
        nextEndurance = 0.0f;

        return (uint64_t)w;
    }

    /*
     *  rand() is a uniform distribution. Use the Box-Muller method to
     *  convert the output from rand() to a normal distribution.
     */
    w = 2.0f;
    while( w >= 1.0f )
    {
        x1 = 2.0 * ( (double)(rand( ) % 1000000) / 1000000.0f ) - 1.0f;
        x2 = 2.0 * ( (double)(rand( ) % 1000000) / 1000000.0f ) - 1.0f;
        w = x1 * x1 + x2 * x2;
    }

    w = sqrt( ( -2.0f * log( w ) ) / w );
    y1 = x1 * w;
    y2 = x2 * w;

    nextEndurance = static_cast<double>(mean) + y2 
                        * static_cast<double>(variance);
    
    return mean + static_cast<uint64_t>(y1 * static_cast<double>(variance));
}
