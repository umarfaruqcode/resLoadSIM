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
#include "proto.H"
#include "globals.H"


Heating::Heating (Household *hh)
{
    count[hh->residents]++;
    next_app = first_app;
    first_app = this;
    household = hh;
    max_heat_power = hh->area * config->heating.kW_per_m2;
    status = OFF;
    sg_enabled = config->heating.smartgrid_enabled > 0
                 && get_random_number (0., 100.) <= config->heating.smartgrid_enabled;
}


void Heating::simulate()
{
    // First check whether this household has received a 'reduce consumption' signal
    // due to an overloaded grid. In this case we turn off the heating immediately.

    if ((sg_enabled && household->reduce_consumption) || household->vacation > 0)
    {
        status = OFF;
        return;
    }

    // The electrical heating is used as an auxiliary heat source.
    // It is turned on only when the heat pump does not deliver enough heat power.
    if (household->heat_demand_SH > household->heatpump->max_heat_power)  // heat pump doesn't deliver enough heat
    {
        status = ON;
        power.real = household->heat_demand_SH - household->heatpump->max_heat_power;
        power.reactive = sqrt ((power.real/config->heating.power_factor)*(power.real/config->heating.power_factor)
                               - power.real*power.real);
    }
    else
    {
        status = OFF;
    }

    if (status == ON)
    {
        household->increase_power (power.real, power.reactive);
        power_total[0] += power.real;
        power_total[household->residents] += power.real;
        increase_consumption();
        household->increase_consumption_SH (power.real * config->timestep_size/3600.);
    }
}
