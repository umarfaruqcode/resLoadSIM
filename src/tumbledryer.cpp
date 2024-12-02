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
#include "producer.H"
#include "proto.H"
#include "configuration.H"


TumbleDryer::TumbleDryer (Household *hh)
{
    count[hh->residents]++;
    next_app = first_app;
    first_app = this;
    household = hh;
    status = OFF;
    timer = 0;
    laundry = 0.;
    sg_enabled = config->dryer.smartgrid_enabled > 0
                 && get_random_number (0., 100.) <= config->dryer.smartgrid_enabled;
    capacity = config->dryer.capacity[household->residents-1];
    num_energy_classes = config->dryer.num_energy_classes;
    energy_class = random_energy_class (config->dryer.energy_classes);

    if (config->energy_classes_2021)
    {
        /* The energy class dependent figures below and the formula to
        calculate the power are from the following source:
        COMMISSION DELEGATED REGULATION (EU) No 392/2012 of 1 March 2012
        It can be downloaded here:
        https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX%3A32012R0392
        */
        double EEI[] = {18, 24, 32, 42, 65, 76, 85, 95};
        double index = get_random_number (EEI[energy_class], EEI[energy_class+1]);
        double SAEc = 140. * pow (capacity, 0.8);
        power.real = index * SAEc / (100. * 160. * config->dryer.hours_per_cycle);
    }
    else
    {
        /* The energy class dependent figures below and the formula to
        calculate the power are from the following source:
        COMMISSION DELEGATED REGULATION (EU) No 392/2012 of 1 March 2012
        It can be downloaded here:
        http://eur-lex.europa.eu/legal-content/EN/TXT/PDF/?uri=CELEX:32012R0392&from=EN
        */
        double EEI[] = {0.200, 0.280, 0.370, 0.535, 0.705, 0.805, 0.895};
        double SAEc = 140. * pow (capacity, 0.8);
        power.real = EEI[energy_class] * SAEc / 160.;  // 1 hour per cycle assumed
    }
    power.reactive = sqrt ((power.real/config->dryer.power_factor)*(power.real/config->dryer.power_factor)
                           - power.real*power.real);
}


void TumbleDryer::simulate (double time)
{
    const double seconds_per_cycle = config->dryer.hours_per_cycle * 3600.;
    double daytime = sim_clock->daytime;
    int best_start, best_end;

    timer--;

    // If the machine is idle
    // and there is some laundry waiting to be dried
    // and someone is at home and not asleep
    if (   timer < 0
        && laundry > 0
        && household->residents_at_home (daytime)
        && (daytime < household->bedtime_old || (daytime > household->wakeup && daytime < household->bedtime)) )
    {
        // A smartgrid enabled machine under price control will wait for the best price
        // if the user is willing to wait
        if (   sg_enabled
            && config->control == PRICE
            && get_random_number (1, 100) <= (100 - config->dryer.ignore_price))
        {
            household->producer->next_best_price_interval (time, time+k_seconds_per_day, &best_start, &best_end);
            if ((best_end - best_start) >= seconds_per_cycle)
            {
                timer = (int)((best_start + get_random_number (0., best_end - best_start - seconds_per_cycle))/config->timestep_size) + 1;
            }
            else
            {
                household->producer->next_best_price_interval (time+best_start, time+best_start+k_seconds_per_day, &best_start, &best_end);
                timer = (int)(best_start/config->timestep_size) + 1;
            }
        }
        else // start next timestep
        {
            timer = 1;
        }
    }
    if (timer == 0)  // It's time to switch
    {
        if (status == OFF)
        {
            if (stop)
            {
                // It's peak time, so let us delay the start
                timer = (int)(config->dryer.peak_delay / config->timestep_size);
            }
            else
            {
                status = ON;
                timer = (int)(seconds_per_cycle / config->timestep_size);
                if (laundry > capacity) laundry -= capacity; else laundry = 0.;
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
        household->heat_loss_app += power.real*0.1;
    }
}
