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

#include "appliance.H"
#include "household.H"
#include "solarcollector.H"
#include "heatstorage.H"
#include "proto.H"
#include "globals.H"
#include "types.H"

// The values used for initializing the collector are partially from prEN 15316-4-3 Annex A

SolarCollector::SolarCollector (class Household *hh)
{
    static bool first = true;

    if (first)
    {
        first = false;
        for (int i=0; i<=k_max_residents; i++) power_total[i] = 0.;
    }
    household = hh;
    count++;
    // assumption:
    // required area for DHW:  1.5 m2 per resident
    // required area for space heating support:  1.25 m2 per resident
    //                                          + 0.5 m2 for every 10 m2 household area
    area =   config->solar_collector.area_factor_1 * household->residents
           + config->solar_collector.area_factor_2 * household->area;
    int collector_type = get_random_number (1,4);
    // first order heat loss coefficient a1, and incident angle modifier IAM
    switch (collector_type)
    {
        case 1: a1 = 15.; IAM = 1.0; break;
        case 2: a1 = 3.5; IAM = 0.94; break;
        case 3: a1 = 1.8; IAM = 1.0; break;
        case 4: a1 = 1.8; IAM = 0.97; break;
    }
    a2 = 0.;    // second order heat loss coefficient
    mass_flow = area * get_random_number (config->solar_collector.min_flow_rate,
                                          config->solar_collector.max_flow_rate) / 3600.;  // mass flow rate [kg/s]
    heat_to_storage_integral = 0.;
}

// Calculations of efficiency and heat output according to prEN 15316-4-3

void SolarCollector::simulate()
{
    static const double factor = mass_flow * k_heat_capacity_H2O*k_heat_capacity_H2O * 1000.;
    static const double H_sol_loop = 5.0 + 0.5 * area; // heat loss coefficient of collector loop piping [W/K]
//    double temp_star;     // reduced temperature difference collector
    double temp_col_avg;  // average collector water temperature
    double eff_col;       // collector efficiency
    double heat_sol_out;
    double heat_loss;
    double heat_sol_loop_out;   // heat power output of the solar collector

    if (location->irradiance > 0.)
    {
        temp_col_avg = 60. + 0.4 * location->irradiance * area / factor;

//    for (int i=0; i<4; i++)
//    {
//        temp_star = (temp_col_avg - location->temperature) / location->irradiance;
//        eff_col = config->solar_collector.eff_0 * IAM - a1 * temp_star - a2 * temp_star * temp_star * location->irradiance;
        eff_col = config->solar_collector.eff_0;
        heat_sol_out = eff_col * location->irradiance * area * 0.001;            // [kW]
        heat_loss = H_sol_loop * (temp_col_avg - location->temperature) * 0.001;  // [kW]
        heat_sol_loop_out = heat_sol_out - heat_loss;
        if (heat_sol_loop_out < 0.) heat_sol_loop_out = 0.;
//        temp_sol_loop_in = 60.; // a more sophisticated model for the storage will return this value
//        temp_col_avg = 0.5 * (temp_sol_loop_in_prev + temp_sol_loop_in) + heat_sol_loop_out / factor;
//        temp_sol_loop_in_prev = temp_sol_loop_in;
//    }
        heat_to_storage_integral += household->heat_storage->increase_stored_heat (heat_sol_loop_out);
        power_total[0] += heat_sol_loop_out;
        power_total[household->residents] += heat_sol_loop_out;
        power_total_integral += heat_sol_loop_out;
    }
}
