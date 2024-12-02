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

#include "heatsource.H"
#include "proto.H"
#include "globals.H"
#include "types.H"
#include "household.H"


HeatSource::HeatSource (Household *hh)
{
    household = hh;
    count[hh->heat_source_type]++;
    max_heat_power = 20.;
    heat_sum = 0.;
}

void HeatSource::simulate()
{
    double consumption = 0., heat_power;

    HeatSourceType type = household->heat_source_type;

    // Space heating
    if (household->heat_demand_SH > 0.)
    {
        heat_power_SH_total[type][0] += household->heat_demand_SH;
        heat_power_SH_total[type][household->residents] += household->heat_demand_SH;
        consumption = household->heat_demand_SH * config->timestep_size/3600.;
        household->increase_consumption_SH (consumption);
        household->increase_consumption_SH_tot_int (consumption, type);
    }
    // Domestic hot water
    if (household->has_boiler() == false)
    {
        heat_sum += household->heat_demand_DHW;
        if (heat_sum > 0.)
        {
            heat_power = (heat_sum >= max_heat_power) ? max_heat_power : heat_sum;
            heat_power_DHW_total[type][0] += heat_power;
            heat_power_DHW_total[type][household->residents] += heat_power;
            consumption = heat_power * config->timestep_size/3600.;
            household->increase_consumption_DHW (consumption);
            household->increase_consumption_DHW_tot_int (consumption, type);
            heat_sum -= heat_power;
        }
    }
}
