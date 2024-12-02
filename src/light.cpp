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

#include "appliance.H"
#include "household.H"
#include "proto.H"
#include "globals.H"


Light::Light (Household *hh)
{
    count[hh->residents]++;
    next_app = first_app;
    first_app = this;
    household = hh;
    status = OFF;
    timer = 0;
    time_1 = time_2 = 0.;
    num_energy_classes = config->light.num_energy_classes;
    energy_class = random_energy_class (config->light.energy_classes);

    if (config->energy_classes_2021)
    {
        /* The energy class dependent figures below and the formula to
        calculate the power are from the following source:
        COMMISSION DELEGATED REGULATION (EU) 2019/2015 of 11 March 2019
        It can be downloaded here:
        https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX%3A32019R2015
        */
        double eta_TM[] = {10, 85, 110, 135, 160, 185, 210, 235};  // [lm/W]
        double eta = get_random_number (eta_TM[num_energy_classes-energy_class-1],
                                        eta_TM[num_energy_classes-energy_class]);
        double luminous_flux = normal_distributed_random_with_limits (config->light.luminous_flux_mean,
                                                                      config->light.luminous_flux_sigma,
                                                                      config->light.luminous_flux_min,
                                                                      config->light.luminous_flux_max);
        double F_TM = (1.0 + 0.926 + 1.176 + 1.089) * 0.25;  // use the mean value
        power.real = (luminous_flux * F_TM / eta) * 0.001;
    }
    else
    {
        /* The energy class dependent figures below and the formula to
        calculate the power are from the following source:
        COMMISSION DELEGATED REGULATION (EU) No 874/2012 of 12 July 2012
        It can be downloaded here:
        http://eur-lex.europa.eu/legal-content/EN/TXT/PDF/?uri=CELEX:32012R0874&from=EN
        */
        double EEI[] = {0.100, 0.140, 0.205, 0.420, 0.700, 0.875, 1.000};
        double luminous_flux = normal_distributed_random_with_limits (config->light.luminous_flux_mean,
                                                                      config->light.luminous_flux_sigma,
                                                                      config->light.luminous_flux_min,
                                                                      config->light.luminous_flux_max);
        double power_ref;
        if (luminous_flux < 1300) power_ref = 0.88 * sqrt (luminous_flux) + 0.049 * luminous_flux;
        else                      power_ref = 0.07341 * luminous_flux;
        power.real = EEI[energy_class] * power_ref / 1000.;
    }
    power.reactive = sqrt ((power.real/config->light.power_factor)*(power.real/config->light.power_factor)
                           - power.real*power.real);
}


void Light::simulate()
{
    double daytime = sim_clock->daytime;

    timer--;

    // At the beginning of each day decide when to turn on the lamps

   if (sim_clock->midnight)
    {
        double rnd;
        // Morning
	    time_1 = household->wakeup;
	    rnd = normal_distributed_random (sim_clock->sunrise, config->light.sigma_morning);
        duration_1 = rnd - time_1;
        if (duration_1 < 0) time_1 = DBL_MAX;
	    // Evening
	    time_2 = normal_distributed_random (sim_clock->sunset, config->light.sigma_evening);
        duration_2 = household->bedtime - time_2;
        if (duration_2 < 0) time_2 = DBL_MAX;
    }
    if (almost_equal (daytime, time_1))
    {
        status = ON;
        timer = duration_1 / config->timestep_size;
    }
    if (almost_equal (daytime, time_2))
    {
        status = ON;
        timer = duration_2 / config->timestep_size;
    }
    if (timer == 0) status = OFF;

    if (status == ON)
    {
        household->increase_power (power.real, power.reactive);
        power_total[0] += power.real;
        power_total[household->residents] += power.real;
        increase_consumption();
        household->heat_loss_app += power.real*0.95;
    }
}
