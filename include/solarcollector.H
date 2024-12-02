/*---------------------------------------------------------------------------
 _____   ______  ______         _____   _____   _____   ______ _____  _____
|_____/ |______ |_____  |      |     | |_____| |     \ |_____    |   |  |  |
|    \_ |______ ______| |_____ |_____| |     | |_____/ ______| __|__ |  |  |

|.....................|  The Residential Load Simulator
|.......*..*..*.......|
|.....*.........*.....|  Authors: Christoph Troyer
|....*...........*....|
|.....*.........*.....|
|.......*..*..*.......|
|.....................|

Copyright (c) 2021 European Union

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software without
   specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
OF THE POSSIBILITY OF SUCH DAMAGE.

---------------------------------------------------------------------------*/

#ifndef SOLAR_COLLECTOR_H
#define SOLAR_COLLECTOR_H

#include "constants.H"
#include "household.H"
#include "types.H"


class SolarCollector
{
    friend class HeatStorage;
private:
    double area;                    // collector area [m2]
    double temp_loop_in;            // inlet temperature to the collector loop [°C]
    double a1, a2;                  // first and second order heat loss coefficients
    double IAM;                     // incident angle modifier
    double mass_flow;               // mass flow rate of collector loop [kg/s/m2]
    class  Household *household;    // pointer to the household this collector belongs to
public:
    static int count;               // total number of solar collectors
    static double power_total[k_max_residents+1];  // heat power output of all collectors in 1 timestep [kW]
    static double power_total_integral;            // heat power output of all collectors for the whole simulation
    double heat_to_storage_integral;               // amount of heat sent to the storage
    SolarCollector (class Household *hh);
    void simulate();
};

#ifdef MAIN_MODULE
double SolarCollector::power_total[k_max_residents+1];
double SolarCollector::power_total_integral = 0.;
int    SolarCollector::count = 0;
#endif

#endif
