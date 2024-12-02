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
#include "configuration.H"
#include "proto.H"
#include "globals.H"


CirculationPump::CirculationPump (Household *hh)
{
    count[hh->residents]++;
    next_app = first_app;
    first_app = this;
    household = hh;
    power.real = config->circpump.power_per_size * household->area;
    power.reactive = sqrt ((power.real/config->circpump.power_factor)*(power.real/config->circpump.power_factor)
                           - power.real*power.real);
    timer = 1;

    if (config->circpump.controlled < 0)   // kludge to save an additional flag
    {
        non_stop_operation = true;  // runs 24h per day, all year round
        status = ON;
    }
    else
    {
        non_stop_operation = false;
        status = OFF;
        is_controlled = config->circpump.controlled > 0
                        && get_random_number (0., 100.) <= config->circpump.controlled;

        first_day = (int) normal_distributed_random_with_limits (config->circpump.rnd_first_day[0],
                                                                 config->circpump.rnd_first_day[1],
                                                                 config->circpump.rnd_first_day[2],
                                                                 config->circpump.rnd_first_day[3]);
        first_month = config->circpump.first_month;
        if (first_day > 30)
        {
            first_day -= 30;
            first_month = (first_month+1)%12;
        }
        last_day = (int) normal_distributed_random_with_limits (config->circpump.rnd_last_day[0],
                                                                config->circpump.rnd_last_day[1],
                                                                config->circpump.rnd_last_day[2],
                                                                config->circpump.rnd_last_day[3]);
        last_month = config->circpump.last_month;
        if (last_day > 30)
        {
            last_day -= 30;
            last_month = (last_month+1)%12;
        }
    }
}


void CirculationPump::simulate()
{
    double duration, corr_factor = 1.0;

    if (non_stop_operation);
    else if (   (sim_clock->month == last_month && sim_clock->day > last_day)
             || (sim_clock->month > last_month && sim_clock->month < first_month)
             || (sim_clock->month == first_month && sim_clock->day < first_day)
             || sim_clock->daytime > config->circpump.time_1
             || sim_clock->daytime < config->circpump.time_2)  status = OFF;
    else if (is_controlled)
    {
        timer--;
        if (timer == 0)
        {
            if (status == OFF)
            {
                status = ON;
                duration = get_random_number (config->circpump.rnd_time_on[0], config->circpump.rnd_time_on[1]);
            }
            else
            {
                status = OFF;
                duration = get_random_number (config->circpump.rnd_time_off[0], config->circpump.rnd_time_off[1]);
            }
            if (duration < config->timestep_size) 
            {
                timer = 1;
                corr_factor = (config->circpump.rnd_time_on[0]+config->circpump.rnd_time_on[1])
                              / (config->circpump.rnd_time_on[0]+config->circpump.rnd_time_on[1]+config->circpump.rnd_time_off[0]+config->circpump.rnd_time_off[1]);
                status = ON;
            }
            else timer = (int)(duration / config->timestep_size + 0.5);
        }
    }
    else status = ON;  // pump runs 24h per day

    if (status == ON)
    {
        household->increase_power (power.real * corr_factor, power.reactive * corr_factor * corr_factor);
        power_total[0] += power.real * corr_factor;
        power_total[household->residents] += power.real * corr_factor;
        increase_consumption (corr_factor);
    }
}
