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
#include "heatsource.H"
#include "heatstorage.H"
#include "proto.H"
#include "globals.H"
#include "types.H"


HeatPump::HeatPump (Household *hh)
{
    count[hh->residents]++;
    next_app = first_app;
    first_app = this;
    household = hh;
    efficiency = get_random_number (config->heatpump.min_eff, config->heatpump.max_eff);
    temp_hot = get_random_number (config->heatpump.min_temperature, config->heatpump.max_temperature);
    max_heat_power = hh->area * config->heatpump.kW_per_m2;
    heat_sum = 0.;
    status = OFF;
}


void HeatPump::simulate()
{
    double heat_power = 0., reduced_max_heat_power, cop;
    double power_SH = 0., power_DHW = 0.;
    HeatSourceType type = household->heat_source_type;

    if (type == HEAT_PUMP)
    {
        // Space heating
        if (household->heat_demand_SH > 0.)
        {
            heat_power = (household->heat_demand_SH > max_heat_power) ? max_heat_power : household->heat_demand_SH;
            cop = efficiency * (273.15 + temp_hot) / (temp_hot - location->temperature);  // coefficient of performance
            power_SH = heat_power / cop;
            household->increase_consumption_SH (power_SH * config->timestep_size/3600.);
            household->increase_consumption_SH_tot_int (power_SH * config->timestep_size/3600., HEAT_PUMP);
            status = ON;
        }
        // Domestic hot water
        if (household->has_boiler() == false)
        {
            heat_sum += household->heat_demand_DHW;
            if (heat_sum > 0.)
            {
                // max. available heat power reduced by heat power already used for space heating
                reduced_max_heat_power = max_heat_power - heat_power;
                heat_power = (heat_sum > reduced_max_heat_power) ? reduced_max_heat_power : heat_sum;
                cop = efficiency * (273.15 + temp_hot) / (temp_hot - location->temperature);  // coefficient of performance
                power_DHW = heat_power / cop;
                household->increase_consumption_DHW (power_DHW * config->timestep_size/3600.);
                household->increase_consumption_DHW_tot_int (power_DHW * config->timestep_size/3600., HEAT_PUMP);
                heat_sum -= heat_power;
                status = ON;
            }
        }
        power.real = power_SH + power_DHW;
        power.reactive = sqrt ((power.real/config->heatpump.power_factor)*(power.real/config->heatpump.power_factor)
                               - power.real*power.real);
    }
    else if (   (type == SOLAR_COLLECTOR
                 && household->heat_storage->is_low)
             || (type == SOLAR_COLLECTOR
                 && !household->heat_storage->is_high
                 && ((int)sim_clock->daytime/3600 >= 22 || (int)sim_clock->daytime/3600 <= 6)))
    {
        heat_power = max_heat_power;
        cop = efficiency * (273.15 + temp_hot) / (temp_hot - location->temperature);  // coefficient of performance
        power.real = heat_power / cop;
        power.reactive = sqrt ((power.real/config->heatpump.power_factor)*(power.real/config->heatpump.power_factor)
                               - power.real*power.real);
        household->heat_storage->increase_stored_heat (heat_power);
        status = ON;
    }
    else status = OFF;

    if (status == ON)
    {
        household->increase_power (power.real, power.reactive);
        power_total[0] += power.real;
        power_total[household->residents] += power.real;
        increase_consumption();
    }
}


void HeatPump::correction_term (void)
{
    class HeatPump *app = first_app;
    class Household *hh;
    double fraction_SH, fraction_DHW;

    while (app)
    {
        hh = app->household;
        if (hh->heat_source_type == SOLAR_COLLECTOR)
        {
            fraction_SH = hh->heat_storage->power_integral_SH / (hh->heat_storage->power_integral_SH + hh->heat_storage->power_integral_DHW);
            hh->increase_consumption_SH_tot_int (app->consumption * fraction_SH, HEAT_PUMP);
            fraction_DHW = hh->heat_storage->power_integral_DHW / (hh->heat_storage->power_integral_SH + hh->heat_storage->power_integral_DHW);
            hh->increase_consumption_DHW_tot_int (app->consumption * fraction_DHW, HEAT_PUMP);
        }
        app = app->next_app;
    }
}
