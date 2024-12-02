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

#include "proto.H"
#include "household.H"
#include "solarcollector.H"
#include "heatstorage.H"
#include "globals.H"

HeatStorage::HeatStorage (Household *hh)
{
    static bool first = true;

    if (first)
    {
        first = false;
        for (int i=0; i<=k_max_residents; i++) power_total[i] = 0.;
    }
    household = hh;
    count++;
    // assume cold water temperature 10°C
    capacity = config->heat_storage.liter_per_m2 * household->solar_collector->area
               * (config->heat_storage.max_temperature-10.) * k_heat_capacity_H2O / 3600.;  // [kWh]

    stored_heat = get_random_number (0.1, 1.) * capacity;  // [kWh]

    max_heat_power = config->heat_storage.max_heat_power;  // [kW]
    heat_sum = 0.;  // accumulated heat demand for DHW
    // Assume that the heat losses are somewhere between 2.0 and 5.0 kWh per day
    heat_loss = get_random_number (2.0, 5.0) * config->timestep_size / (24. * 3600.); // [kWh]
}


void HeatStorage::simulate()
{
    static const double factor = config->timestep_size/3600.;
    double heat_power, reduced_max_heat_power;
    double power_SH = 0., power_DHW = 0.;

    // Space heating
    if (household->heat_demand_SH > 0.)
    {
        power_SH = (household->heat_demand_SH > max_heat_power) ? max_heat_power : household->heat_demand_SH;
        if (stored_heat/factor < power_SH) power_SH = stored_heat/factor; // storage almost empty
        stored_heat -= power_SH * factor + heat_loss;
        household->increase_consumption_SH (power_SH * config->timestep_size/3600.);
        household->increase_consumption_SH_tot_int (power_SH * config->timestep_size/3600., SOLAR_COLLECTOR);
    }
    // Domestic hot water
    if (household->has_boiler() == false)
    {
        heat_sum += household->heat_demand_DHW;
        if (heat_sum > 0.)  // draw energy from the storage
        {
            // max. available heat power reduced by heat power already used for space heating
            reduced_max_heat_power = max_heat_power - power_SH;
            power_DHW = (heat_sum > reduced_max_heat_power) ? reduced_max_heat_power : heat_sum;
            if (stored_heat/factor < power_DHW) power_DHW = stored_heat/factor; // storage almost empty
            stored_heat -= power_DHW * factor + heat_loss;
            heat_sum -= power_DHW;
            household->increase_consumption_DHW (power_DHW * config->timestep_size/3600.);
            household->increase_consumption_DHW_tot_int (power_DHW * config->timestep_size/3600., SOLAR_COLLECTOR);
        }
    }
    heat_power = power_SH + power_DHW;
    is_low = (stored_heat < 0.1 * capacity);
    is_high = (stored_heat > 0.9 * capacity);

    power_total[0] += heat_power;
    power_total[household->residents] += heat_power;
    power_integral_SH += household->heat_demand_SH;
    power_integral_DHW += household->heat_demand_DHW;
    stored_heat_total += stored_heat;
}


double HeatStorage::increase_stored_heat (double heat_power_input)
{
    double value = heat_power_input * config->timestep_size/3600.;
    if (stored_heat + value > capacity) value = capacity - stored_heat;
    stored_heat += value;
    return value;
}
