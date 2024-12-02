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

#include <stdlib.h>

#include "proto.H"
#include "household.H"
#include "solarmodule.H"
#include "globals.H"


SolarModule::SolarModule (class Household *hh)
{
    static bool first = true;

    if (first)
    {
        first = false;
        for (int i=0; i<=k_max_residents; i++)
        {
            real_power_total[i] = 0.;
            reactive_power_total[i] = 0.;
            apparent_power_total[i] = 0.;
        }
    }
    household = hh;
    if (config->solar_module.production_ratio)
    {
        // With a given production ratio (solar production / consumption)
        // the nominal power can be calculated once the consumption is known.
        // The consumption is known after a pre-run simulation of a whole year.
        // During the pre-run the nominal power is set to 1. After the pre-run
        // and immediately before the real simulation starts, the nominal power
        // is updated.
        nominal_power = 1.;
    }
    else
    {
        // Set the nominal power to a meaningful random value depending on the
        // household size and the number of residents.
        nominal_power = household->residents
                        * get_random_number (config->solar_module.min_area, config->solar_module.max_area)
                        * get_random_number (config->solar_module.min_eff, config->solar_module.max_eff);
    }

//  HANNOVER  nominal_power = 5.4;

    production_integral = 0.;
    production_prev_day = 0.;
    count++;
}


void SolarModule::simulate()
{
    static const double factor = config->timestep_size/3600.;
    int r = household->residents;

    power.real = 0.;
    power.reactive = 0.;
    if (almost_equal (sim_clock->daytime, sim_clock->sunrise)) production_prev_day = 0.;
    if (sim_clock->daytime >= sim_clock->sunrise && sim_clock->daytime <= sim_clock->sunset)
    {
        power.real = location->irradiance * nominal_power;
        power.real *= (1.-config->solar_module.system_loss*0.01)*0.001; // power output in kW after taking system losses into account
        power.reactive = sqrt ((power.real/config->solar_module.power_factor)*(power.real/config->solar_module.power_factor)
                           - power.real*power.real);
        real_power_total[0] += power.real;
        real_power_total[r] += power.real;
        power_total_integral += power.real;
        production_integral += power.real*factor;
        production_prev_day += power.real*factor;

        reactive_power_total[0] += power.reactive;
        reactive_power_total[r] += power.reactive;

        apparent_power_total[0] = sqrt (real_power_total[0]*real_power_total[0] + reactive_power_total[0]*reactive_power_total[0]);
        apparent_power_total[r] = sqrt (real_power_total[r]*real_power_total[r] + reactive_power_total[r]*reactive_power_total[r]);
    }
}


void SolarModule::reset_production()
{
    production_integral = 0.;
}

void SolarModule::print (FILE *fp)
{
    fprintf (fp, " %.3lf %.3lf", nominal_power, production_integral);
}

void SolarModule::adapt_size (double consumption)
{
    nominal_power = config->solar_module.production_ratio * consumption / production_integral;
}

double SolarModule::calc_future_power_output (double daytime, int days_in_the_future)
{
    //  ####TBI
    double pwr = 0.;
    return pwr;
}


double SolarModule::production_forecast()
{
    static const double factor = 0.001 * (1.-config->solar_module.system_loss/100.) * config->timestep_size/3600.;
    return location->irradiance_integral * factor * nominal_power;
}
