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


GasStove::GasStove (Household *hh)
{
    count[hh->residents]++;
    next_app = first_app;
    first_app = this;
    household = hh;
    timer = 0;
    status = OFF;
    power.real = config->stove.power[household->residents-1];
    power.reactive = 0.;
    time_1 = time_2 = time_3 = 0.;
}


void GasStove::simulate()
{
    double daytime = sim_clock->daytime;
    int weekday = sim_clock->weekday;
    int percent;

    timer--;

    if (sim_clock->midnight)
    {
        if (get_random_number (1, 100) <= config->stove.duration_1_percent)
        {
            time_1 = household->wakeup + config->stove.time_offset;
            duration_1 = normal_distributed_random_with_limits (config->stove.rnd_duration_1[0],
                                                                config->stove.rnd_duration_1[1],
                                                                config->stove.rnd_duration_1[2],
                                                                config->stove.rnd_duration_1[3]);
            household->schedule (COOK, time_1/60.);
        }
        else time_1 = DBL_MAX;


        if (weekday == SUNDAY || sim_clock->holiday)
            percent = config->stove.duration_2_percent_sunday;
        else if (weekday == SATURDAY)
            percent = config->stove.duration_2_percent_saturday;
        else
            percent = config->stove.duration_2_percent;
        if (get_random_number (1, 100) <= percent)
        {
            duration_2 = normal_distributed_random_with_limits (config->stove.rnd_duration_2[0],
                                                                config->stove.rnd_duration_2[1],
                                                                config->stove.rnd_duration_2[2],
                                                                config->stove.rnd_duration_2[3]);
            time_2 = normal_distributed_random (config->stove.time_2_mean, config->stove.time_2_sigma);
            if (household->residents_at_home_duration (time_2, 1) < duration_2) time_2 = DBL_MAX;
            else household->schedule (COOK, time_2/60.);
        }
        else time_2 = DBL_MAX;

        if (fabs (time_2 - DBL_MAX) < k_float_compare_eps)
        {
            duration_3 = normal_distributed_random_with_limits (config->stove.rnd_duration_3[0],
                                                                config->stove.rnd_duration_3[1],
                                                                config->stove.rnd_duration_3[2],
                                                                config->stove.rnd_duration_3[3]);
            time_3 = normal_distributed_random (config->stove.time_3_mean, config->stove.time_3_sigma);
            if (household->residents_at_home_duration (time_3, 1) < duration_3) time_3 = DBL_MAX;
            else household->schedule (COOK, time_3/60.);
        }
        else time_3 = DBL_MAX;
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
    if (almost_equal (daytime, time_3))
    {
        status = ON;
        timer = duration_3 / config->timestep_size;
    }
    if (timer == 0) status = OFF;

    if (status == ON)
    {
        power_total[0] += power.real;
        power_total[household->residents] += power.real;
        household->heat_loss_app += power.real*0.25;
        household->increase_consumption_cooking (power.real * config->timestep_size/3600.);
    }
}
