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
#include "proto.H"
#include "globals.H"

Computer::Computer (Household *hh)
{
    count[hh->residents]++;
    next_app = first_app;
    first_app = this;
    household = hh;
    power.real = config->computer.power;
    power.reactive = sqrt ((power.real/config->computer.power_factor)*(power.real/config->computer.power_factor)
                           - power.real*power.real);
    status = OFF;
    time_1 = time_2 = timer = 0.;
}


void Computer::simulate()
{
    double corr_factor = 1.0;
    double total_duration;  // in seconds
    double daytime = sim_clock->daytime;
    int weekday = sim_clock->weekday;
    int rnd;
    double rahd, rt;
    
    timer--;
    if (sim_clock->midnight)
    {
        total_duration = normal_distributed_random (config->computer.duration_mean, config->computer.duration_sigma);
        if (weekday == SUNDAY || sim_clock->holiday) duration_1 = total_duration * config->computer.duration_fraction_sunday;
        else if (weekday == SATURDAY) duration_1 = total_duration * config->computer.duration_fraction_saturday;
        else                          duration_1 = total_duration * config->computer.duration_fraction;
        duration_2 = total_duration - duration_1;

        rnd = get_random_number (1, 100);
        if (weekday == SUNDAY || sim_clock->holiday)
        {
            if (rnd < config->computer.rnd_sunday[0])
                time_1 = household->wakeup;
            else if (rnd >= config->computer.rnd_sunday[0] && rnd < config->computer.rnd_sunday[1])
                time_1 = household->wakeup + config->computer.time_offset_sunday[0];
            else if (rnd >= config->computer.rnd_sunday[1] && rnd < config->computer.rnd_sunday[2])
                time_1 = household->wakeup + config->computer.time_offset_sunday[1];
            else
                time_1 = household->wakeup + config->computer.time_offset_sunday[2];
        }
        else if (weekday == SATURDAY)
        {
            if (rnd < config->computer.rnd_saturday[0])
                time_1 = household->wakeup;
            else if (rnd >= config->computer.rnd_saturday[0] && rnd < config->computer.rnd_saturday[1])
                time_1 = household->wakeup + config->computer.time_offset_saturday[0];
            else if (rnd >= config->computer.rnd_saturday[1] && rnd < config->computer.rnd_saturday[2])
                time_1 = household->wakeup + config->computer.time_offset_saturday[1];
            else
                time_1 = household->wakeup + config->computer.time_offset_saturday[2];
        }
        else
        {
            if (rnd < config->computer.rnd[0])
                time_1 = household->wakeup;
            else if (rnd >= config->computer.rnd[0] && rnd < config->computer.rnd[1])
                time_1 = household->wakeup + config->computer.time_offset[0];
            else if (rnd >= config->computer.rnd[1] && rnd < config->computer.rnd[2])
                time_1 = household->wakeup + config->computer.time_offset[1];
            else
                time_1 = household->wakeup + config->computer.time_offset[2];
        }
        rahd = household->residents_at_home_duration (time_1, 1);
        if (duration_1 > rahd)
        {
            duration_1 = rahd;
            duration_2 = total_duration - duration_1;
        }
        rt = household->return_time (1);
        if (fabs (rt - DBL_MAX) < k_float_compare_eps) time_2 = normal_distributed_random (config->computer.time_2_mean,
                                                                                           config->computer.time_2_sigma);
        else time_2 = rt;
    }
    if (almost_equal (daytime, time_1))
    {
        status = ON;
        if (duration_1 < config->timestep_size)
        {
            timer = 1;
            corr_factor = duration_1 / config->timestep_size;
        }
        else timer = (int)(duration_1 / config->timestep_size + 0.5);
    }
    if (almost_equal (daytime, time_2))
    {
        status = ON;
        if (duration_2 < config->timestep_size)
        {
            timer = 1;
            corr_factor = duration_2 / config->timestep_size;
        }
        else timer = (int)(duration_2 / config->timestep_size + 0.5);
    }
    if (timer == 0) status = OFF;

    if (status == ON)
    {
        household->increase_power (power.real * corr_factor, power.reactive * corr_factor * corr_factor);
        power_total[0] += power.real * corr_factor;
        power_total[household->residents] += power.real * corr_factor;
        increase_consumption (corr_factor);
    }
}
