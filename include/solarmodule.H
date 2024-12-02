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

#ifndef SOLAR_MODULE_H
#define SOLAR_MODULE_H

#include "constants.H"
#include "household.H"
#include "types.H"


class SolarModule
{
private:
    class  Household *household;           // pointer to the household this
                                           // module belongs to
public:
    static double real_power_total[k_max_residents+1];      // real power output of all modules [kW]
    static double reactive_power_total[k_max_residents+1];  // reactive power output of all modules [kVAR]
    static double apparent_power_total[k_max_residents+1];  // apparent power output of all modules [kVA]
    static double power_total_integral;           // real power output of all modules over time [kW]
    static int count;                             // total number of solarmodules
    double nominal_power;                         // nominal power of the module [kW]
    double production_integral;                   // solar production over time [kWh]
    double production_prev_day;                   // solar production of the previous day [kWh]
    Power  power;                                 // current power output [kW]

    SolarModule (class Household *hh);
    void simulate();
    void print (FILE *fp);
    void adapt_size (double consumption);
    void reset_production();
    double calc_future_power_output (double daytime, int days_in_the_future);
    double production_forecast();
};

#ifdef MAIN_MODULE
double SolarModule::real_power_total[k_max_residents+1];
double SolarModule::reactive_power_total[k_max_residents+1];
double SolarModule::apparent_power_total[k_max_residents+1];
double SolarModule::power_total_integral = 0.;
int    SolarModule::count = 0;
#endif

#endif
