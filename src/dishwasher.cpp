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
#include "producer.H"
#include "globals.H"


Dishwasher::Dishwasher (Household *hh)
{
    count[hh->residents]++;
    next_app = first_app;
    first_app = this;
    household = hh;
    status = OFF;
    timer = 0;
    smart = smart_mode = false;
    sg_enabled = config->dishwasher.smartgrid_enabled > 0
                 && get_random_number (0., 100.) <= config->dishwasher.smartgrid_enabled;
    num_energy_classes = config->dishwasher.num_energy_classes;
    energy_class = random_energy_class (config->dishwasher.energy_classes);

    if (config->energy_classes_2021)
    {
        /* The energy class dependent figures below and the formula to
        calculate the power are from the following source:
        COMMISSION DELEGATED REGULATION (EU) 2019/2017 of 11 March 2019
        It can be downloaded here:
        https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX%3A32019R2017
        */
        double EEI[] = {26, 32, 38, 44, 50, 56, 62, 68};
        double index = get_random_number (EEI[energy_class], EEI[energy_class+1]);
        int place_settings = config->dishwasher.place_settings[household->residents-1];
        double SPEC; // a reference value for the standard programme consumption
        if (place_settings >= 10) SPEC = 0.025 * place_settings + 1.350;
        else                      SPEC = 0.090 * place_settings + 0.450;
        power.real = index * SPEC / (100. * config->dishwasher.hours_per_cycle);
    }
    else
    {
        /* The energy class dependent figures below and the formula to
        calculate the power are from the following source:
        COMMISSION DELEGATED REGULATION (EU) No 1059/2010 of 28 September 2010
        It can be downloaded here:
        http://eur-lex.europa.eu/legal-content/EN/TXT/PDF/?uri=CELEX:32010R1059&from=EN
        */
        double EEI[] = {0.475, 0.530, 0.595, 0.670, 0.755, 0.850, 0.955};
        int place_settings = config->dishwasher.place_settings[household->residents-1];
        double SAEc;
        if (place_settings >= 10) SAEc = config->dishwasher.SAEc_big[0] + config->dishwasher.SAEc_big[1] * place_settings;
        else                      SAEc = config->dishwasher.SAEc_small[0] + config->dishwasher.SAEc_small[1] * place_settings;
        power.real = (EEI[energy_class] * SAEc) / (config->dishwasher.factor * config->dishwasher.hours_per_cycle);
    }
    power.reactive = sqrt ((power.real/config->dishwasher.power_factor)*(power.real/config->dishwasher.power_factor)
                           - power.real*power.real);
}


void Dishwasher::simulate (double time)
{
    const double seconds_per_cycle = config->dishwasher.hours_per_cycle * 3600.;
    int probability;
    int intervals[20], num_intervals, i;
    double daytime = sim_clock->daytime;
    double begin, length;

    timer--;

    // Decide whether to turn on the machine, and when.
    // This is done every day at the same time and only for machines,
    // which are not running already or have their timer set to wait
    // for the best price.
    if (sim_clock->midnight && timer < 0)
    {
        probability = config->dishwasher.probability[0] + config->dishwasher.probability[1] * household->residents;
        // Do we need the dishwasher today?
        if (get_random_number (1, 100) <= probability)
        {
            // A smart machine will try to utilize a predicted surplus production
            // of the solarmodules.
            if (smart && household->solar_prediction(0)) smart_mode = true;

            // A smart grid enabled machine can wait for the best price, if the
            // user is willing to wait.
            else if (sg_enabled && config->control == PRICE)
            {
                // Check, whether we want to pay attention to the price:
                if (get_random_number (1, 100) <= config->dishwasher.ignore_price)
                {
                    // Ok, we don't care about money. Let's do it sometime
                    // during the day.
                    if (get_random_number (1, 100) <= config->dishwasher.fraction)
                        timer = normal_distributed_random (config->dishwasher.timer_1_mean,
                                                           config->dishwasher.timer_1_sigma) / config->timestep_size;
                    else
                        timer = normal_distributed_random (config->dishwasher.timer_2_mean,
                                                           config->dishwasher.timer_2_sigma) / config->timestep_size;
                }
                else
                {
                    // In this case the machine checks for the best price in the
                    // near future:
                    Household::producer->best_price (time, config->dishwasher.preview_length, &num_intervals, intervals);
                    // It is possible that there is more than one interval with the
                    // best price. The values stored in 'intervals' are in minutes.
                    i = num_intervals * get_random_number (0., 0.99);
                    begin = intervals[i*2] * 60;
                    length = intervals[i*2+1] * 60;
                    if (begin < daytime) begin += k_seconds_per_day;
                    if (seconds_per_cycle > length)
                        timer = (begin-daytime) / config->timestep_size;
                    else
                        timer = (begin-daytime + get_random_number (0., length-seconds_per_cycle))
                        / config->timestep_size;
                }
            }
            // In all other cases the machine is turned on sometime during the day.
            else
            {
                if (get_random_number (1, 100) <= config->dishwasher.fraction)
                    timer = normal_distributed_random (config->dishwasher.timer_1_mean,
                                                       config->dishwasher.timer_1_sigma) / config->timestep_size;
                else
                    timer = normal_distributed_random (config->dishwasher.timer_2_mean,
                                                       config->dishwasher.timer_2_sigma) / config->timestep_size;
            }
        }
    }

    if (smart_mode)
    {
        if (household->has_enough_solar_power (power.real))
        {
            status = ON;
            timer = seconds_per_cycle / config->timestep_size;
            smart_mode = false;
        }
        else if (sim_clock->daytime > sim_clock->sunset)
        {
            timer = normal_distributed_random (config->dishwasher.timer_3_mean,
                                               config->dishwasher.timer_3_sigma) / config->timestep_size;
            smart_mode = false;
        }
    }

    if (timer == 0)  // time to switch
    {
        if (status == OFF)
        {
            if (sg_enabled && stop)
            {
                // it's peak time, so let us delay the start
                timer = config->dishwasher.peak_delay / config->timestep_size;
            }
            else
            {
                status = ON;
                timer = seconds_per_cycle / config->timestep_size;
            }
        }
        else status = OFF;
    }
    if (status == ON)
    {
        household->increase_power (power.real, power.reactive);
        power_total[0] += power.real;
        power_total[household->residents] += power.real;
        increase_consumption();
        household->heat_loss_app += power.real*0.25;
    }
}
