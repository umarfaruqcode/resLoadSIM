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


Vacuum::Vacuum (Household *hh)
{
    count[hh->residents]++;
    next_app = first_app;
    first_app = this;
    household = hh;
    status = OFF;
    timer = 0;
    days_since_last_usage = get_random_number (0, household->vacuum_interval - 1);

    if (config->energy_classes_2021)
    {
        /* The formula to calculate the power is from the following source:
        Commission Delegated Regulation (EU) No 666/2013 of 8 July 2013
        The document can be downloaded here:
        https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX:32013R0666
        */
        double AE = get_random_number (7., 61.);
        double dpu_c = get_random_number (0.69, 0.91);
        double dpu_hf = get_random_number (0.940, 1.110);
        double area = 0.3 * 0.5 * 10;  // test area in m^2
        double t = 5./3600.;           // 5 seconds
        power.real = 2. * AE * area * 0.001 / (17.4 * (0.8/(dpu_c-0.2)+0.8/(dpu_hf-0.2)) * t);
    }
    else
    {
        /* The energy class dependent figures below and the formula to
        calculate the power are from the following source:
        Commission Delegated Regulation (EU) No 665/2013  of 3 May 2013
        It can be downloaded here:
        http://eur-lex.europa.eu/LexUriServ/LexUriServ.do?uri=OJ:L:2013:192:0001:0023:EN:PDF
        */
        double AE[] = {7., 13., 19., 25., 31., 37., 43., 49., 55., 61.};
        double dpu_c[] = {0.91, 0.91, 0.91, 0.91, 0.89, 0.85, 0.81, 0.77, 0.73, 0.69};
        double dpu_hf[] = {1.110, 1.110, 1.110, 1.110, 1.095, 1.065, 1.035, 1.005, 0.975, 0.940};
        double area = 0.3 * 0.5 * 10;  // test area in m^2
        double t = 5./3600.;           // 5 seconds
        energy_class = random_energy_class (config->vacuum.energy_classes);
        power.real = 2. * AE[energy_class] * area * 0.001 / (17.4 * (0.8/(dpu_c[energy_class]-0.2)+0.8/(dpu_hf[energy_class]-0.2)) * t);
    }
    power.reactive = sqrt ((power.real/config->vacuum.power_factor)*(power.real/config->vacuum.power_factor)
                           - power.real*power.real);
}


void Vacuum::simulate()
{
    double corr_factor = 1.0, duration;

    timer--;

//  Do we want to vacuum clean today?
    if (sim_clock->midnight && status == OFF)
    {
        days_since_last_usage++;
        if (days_since_last_usage == household->vacuum_interval)
        {
            timer = (int)(get_random_number (config->vacuum.timer_min, config->vacuum.timer_max) / config->timestep_size);
        }
        else
        {
            timer = -1;
        }
    }

    if (timer == 0)   // It's time to switch
    {
        if (status == OFF)
        {
            status = ON;
            duration = config->vacuum.timer_factor * household->area; 
            if (duration < config->timestep_size)   // timestep size too big --> run with less power to get the same consumption 
            {
                timer = 1;
                corr_factor = duration / config->timestep_size;
            } 
            else timer = (int)(duration / config->timestep_size + 0.5);
        }
        else
        {
            status = OFF;
            timer = -1;  // Don't turn it on again today
            days_since_last_usage = 0;
        }
    }
    if (status == ON)
    {
        household->increase_power (power.real * corr_factor, power.reactive * corr_factor * corr_factor);
        power_total[0] += power.real * corr_factor;
        power_total[household->residents] += power.real * corr_factor;
        increase_consumption (corr_factor);
        household->heat_loss_app += power.real*0.5*corr_factor;
    }
}
