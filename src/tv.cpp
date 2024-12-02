/*---------------------------------------------------------------------------
 _____   ______  ______         _____   _____   _____   ______ _____  _____
|_____/ |______ |_____  |      |     | |_____| |     \ |_____    |   |  |  |
|    \_ |______ ______| |_____ |_____| |     | |_____/ ______| __|__ |  |  |

|.....................|  The Residential Load Simulator
|.......*..*..*.......|
|.....*.........*.....|  Authors: Christoph Troyer
|....*...........*....|           Heinz Wilkening
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


TV::TV (Household *hh, int tv_rank)
{
    static const double factor = 0.254*0.254*(16.*9.)/(16.*16.+9.*9.);
    double area, diag;

    count[hh->residents]++;
    next_app = first_app;
    first_app = this;
    status = OFF;
    household = hh;
    rank = tv_rank;
    timer = time_1 = time_2 = 0.;
    num_energy_classes = config->tv.num_energy_classes;
    energy_class = (random_energy_class (config->tv.energy_classes) + rank - 1);
    if (energy_class >= num_energy_classes) energy_class = (num_energy_classes-1);

    // Calculating the TV area via the diagonal of TV (16:9 ratio assumed)
    // Diag in inch, area in dm^2
    if      (rank == 1) diag = config->tv.diagonal_1; // inch
    else if (rank == 2) diag = config->tv.diagonal_2; // inch
    else                diag = config->tv.diagonal_3; // inch
    area = factor * diag * diag;

    if (config->energy_classes_2021)
    {
        /* The energy class dependent figures below and the formula to
        calculate the power are from the following source:
        Commission Delegated Regulation (EU) 2019/2013 of 11 March 2019
        It can be downloaded here:
        https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=uriserv:OJ.L_.2019.315.01.0001.01.ENG&toc=OJ:L:2019:315:TOC
        */
        double EEI_label[] = {0.20, 0.30, 0.40, 0.50, 0.60, 0.75, 0.90, 1.05};
        double index = get_random_number (EEI_label[energy_class], EEI_label[energy_class+1]);
        power.real = (index * (3.*(90.+(0.025+0.0035*(area-11.)+4.))+3.) - 1.) * 0.001;
    }
    else
    {
        /* The energy class dependent figures below and the formula to
        calculate the power are from the following source:
        Commission Delegated Regulation (EU) No 1062/2010  of 28 September 2010
        It can be downloaded here:
        http://eur-lex.europa.eu/legal-content/EN/TXT/PDF/?uri=CELEX:32010R1062&from=EN
        */
        double EEI[] = {0.080, 0.130, 0.195, 0.265, 0.360, 0.510, 0.700, 0.850, 0.950, 1.050};
        power.real = EEI[energy_class] * (24. + area * 4.3224) / 1000.;
    }
    power.reactive = sqrt ((power.real/config->tv.power_factor)*(power.real/config->tv.power_factor)
                           - power.real*power.real);
    avg_duration = config->tv.avg_duration[household->residents-1];
}


void TV::simulate()
{
    double corr_factor = 1.0;
    double tv_duration;
    double daytime = sim_clock->daytime;
    int weekday = sim_clock->weekday;
    int rnd;
    double rahd, rt;

    timer--;

    // At the beginning of each day decide for how long and when
    // to turn on the TV

    if (sim_clock->midnight)
    {
        if (weekday == SATURDAY || weekday == SUNDAY || sim_clock->holiday)
        {
            // 20 % longer tv_duration during the weekend
            tv_duration = normal_distributed_random (config->tv.factor_mean_we * avg_duration,
                                                     config->tv.factor_sigma_we * avg_duration);
        }
        else
        {
            tv_duration = normal_distributed_random (config->tv.factor_mean * avg_duration,
                                                     config->tv.factor_sigma * avg_duration);
        }
        if      (weekday == SUNDAY || sim_clock->holiday)   duration_1 = tv_duration * config->tv.duration_factor_sun;
        else if (weekday == SATURDAY) duration_1 = tv_duration * config->tv.duration_factor_sat;
        else                          duration_1 = tv_duration * config->tv.duration_factor;
        duration_2 = tv_duration - duration_1;

        rnd = get_random_number (1, 100);
        if (weekday == SUNDAY || sim_clock->holiday)
        {
            if (rnd < config->tv.random_sun[0])
                time_1 = household->wakeup;
            else if (rnd >= config->tv.random_sun[0] && rnd < config->tv.random_sun[1])
                time_1 = household->wakeup +  config->tv.delay_sun[0];
            else if (rnd >= config->tv.random_sun[1] && rnd < config->tv.random_sun[2])
                time_1 = household->wakeup + config->tv.delay_sun[1];
            else
                time_1 = household->wakeup + config->tv.delay_sun[2];
        }
        else if (weekday == SATURDAY)
        {
            if (rnd < config->tv.random_sat[0])
                time_1 = household->wakeup;
            else if (rnd >= config->tv.random_sat[0] && rnd < config->tv.random_sat[1])
                time_1 = household->wakeup +  config->tv.delay_sat[0];
            else if (rnd >= config->tv.random_sat[1] && rnd < config->tv.random_sat[2])
                time_1 = household->wakeup + config->tv.delay_sat[1];
            else
                time_1 = household->wakeup + config->tv.delay_sat[2];
        }
        else
        {
            if (rnd < config->tv.random[0])
                time_1 = household->wakeup;
            else if (rnd >= config->tv.random[0] && rnd < config->tv.random[1])
                time_1 = household->wakeup +  config->tv.delay[0];
            else if (rnd >= config->tv.random[1] && rnd < config->tv.random[2])
                time_1 = household->wakeup + config->tv.delay[1];
            else
                time_1 = household->wakeup + config->tv.delay[2];
        }
        rahd = household->residents_at_home_duration (time_1, rank);
        if (duration_1 > rahd)
        {
            duration_1 = rahd;
            duration_2 = tv_duration - duration_1;
        }
        rt = household->return_time (rank);
        if (fabs (rt - DBL_MAX) < k_float_compare_eps) time_2 = normal_distributed_random (config->tv.time_2_mean,
                                                                                           config->tv.time_2_sigma);
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
        household->heat_loss_app += power.real * corr_factor;
    }
}
