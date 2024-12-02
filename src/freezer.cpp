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
#include "producer.H"
#include "proto.H"
#include "globals.H"


Freezer::Freezer (Household *hh)
{
    count[hh->residents]++;
    next_app = first_app;
    first_app = this;
    household = hh;
    smart = false;
    sg_enabled = config->freezer.smartgrid_enabled > 0
                 && get_random_number (0., 100.) <= config->freezer.smartgrid_enabled;

    delta_t_rise = config->freezer.delta_t_rise_factor
                   * normal_distributed_random (config->freezer.delta_t_rise_mean, config->freezer.delta_t_rise_sigma)
                   * config->timestep_size / 60.;
    delta_t_drop = config->freezer.delta_t_drop_factor
                   * normal_distributed_random (config->freezer.delta_t_drop_mean, config->freezer.delta_t_drop_sigma)
                   * config->timestep_size / 60.;

    target_temperature = get_random_number (config->freezer.min_temperature+1,
                                            config->freezer.max_temperature-1);
    temperature = get_random_number (target_temperature-1.0-0.5*delta_t_drop,
                                     target_temperature+1.0+0.5*delta_t_rise);

    if (get_random_number (1, 100) <= 100./(1.+ delta_t_drop/delta_t_rise))
    {
        status = ON;
    }
    else
    {
        status = OFF;
    }
    num_energy_classes = config->freezer.num_energy_classes;
    energy_class = random_energy_class (config->freezer.energy_classes);

    if (config->energy_classes_2021)
    {
        /* The energy class dependent figures below and the formula to
        calculate the power are from the following source:
        COMMISSION DELEGATED REGULATION (EU) 2019/2016 of 11 March 2019
        It can be downloaded here:
        https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX:32019R2016
        */
        double EEI[] = {31, 41, 51, 64, 80, 100, 125, 155};
        double index = get_random_number (EEI[energy_class], EEI[energy_class+1]);
        // only 1 compartment with volume Vc assumed
        double Vc = config->freezer.Vc_per_resident * household->residents;
        double C = 1.0;
        double D = 1.0;
        double Ac = (1.0 + 1.1) / 2.0;
        double Bc = (1.0 + 1.05) / 2.0;
        double Nc = 138.0;
        double rc = 1.80;
        double Mc = 0.15;
        double SAE = C * D * Ac * Bc * (Nc + Vc * rc * Mc);
        power.real = config->freezer.factor_1 * index/100. * SAE/(365.*24.); // runs approx. 10 minutes per hour
    }
    else
    {
        /* The energy class dependent figures below and the formula to
        calculate the power are from the following source:
        COMMISSION DELEGATED REGULATION (EU) No 1060/2010 of 28 September 2010
        It can be downloaded here:
        http://eur-lex.europa.eu/legal-content/EN/TXT/PDF/?uri=CELEX:32010R1060&from=EN
        */
        double EEI[] = {0.175, 0.275, 0.375, 0.485, 0.650, 0.850, 1.025, 1.175, 1.375, 1.575};
        double Tc = config->freezer.Tc;
        double Vc = config->freezer.Vc_per_resident * household->residents;
        double FF = 1.;
        double CC = 1.;
        double BI = 1.;
        double Veq = Vc * (25.-Tc) * 0.05 * FF * CC * BI;
        double CH = 50.;
        double M, N;
        if (get_random_number (1, 100) <= config->freezer.mn_percentage) { M = 0.539; N = 315; }
        else                                                             { M = 0.472; N = 286; }
        double SAEc = Veq * M + N + CH;
        power.real = config->freezer.factor_1 * EEI[energy_class] * SAEc / (365.*24.); // runs approx. 10 minutes per hour
    }
    power.reactive = sqrt ((power.real/config->freezer.power_factor)*(power.real/config->freezer.power_factor)
                           - power.real*power.real);
}


void Freezer::simulate (double time)
{
    double future;  // some point of time in the future [s]

    if (status == OFF) 
    {
        temperature += delta_t_rise;
        if (temperature > household->temp_int_air) temperature = household->temp_int_air;
    }
    else temperature -= delta_t_drop;

    // Smartgrid enabled freezers are turned on when there is overvoltage in the grid
    if (sg_enabled && household->raise_consumption)
    {
        if (temperature > config->freezer.min_temperature) status = ON;
        else status = OFF;
    }
    // Smartgrid enabled freezers are turned off when there is undervoltage in the grid
    else if (sg_enabled && household->reduce_consumption)
    {
        if (temperature < config->freezer.max_temperature) status = OFF;
        else status = ON;
    }
    else
    {
        future = time + config->timestep_size * (temperature - config->freezer.min_temperature) / delta_t_drop;
        // Smartgrid enabled freezers can take advantage of a low energy tariff
        if (   sg_enabled
            && config->control == PRICE
            && Household::producer->price (GRID, time) < Household::producer->price (GRID, future)
            && temperature > config->freezer.min_temperature)
        {
            status = ON;
        }
        else // This is the freezer's default behaviour, which depends on the temperature only
        {
            if (temperature > target_temperature + 1. && status == OFF)
            {
                status = ON;
                delta_t_drop = config->freezer.delta_t_drop_factor
                               * normal_distributed_random (config->freezer.delta_t_drop_mean, config->freezer.delta_t_drop_sigma)
                               * config->timestep_size / 60.;
            }
            else if (temperature < target_temperature - 1. && status == ON)
            {
                status = OFF;
                delta_t_rise = config->freezer.delta_t_rise_factor
                               * normal_distributed_random (config->freezer.delta_t_rise_mean, config->freezer.delta_t_rise_sigma)
                               * config->timestep_size / 60.;
            }
        }
    }
    if (status == ON)
    {
        household->increase_power (power.real, power.reactive);
        power_total[0] += power.real;
        power_total[household->residents] += power.real;
        increase_consumption();
        household->heat_loss_app += power.real;
    }
}


int Freezer::create_smart_list (Freezer ***list)
{
    Freezer *fz = first_app;
    int i, num = 0;
    while (fz)
    {
        if (fz->sg_enabled) num++;
        fz = fz->next_app;
    }
    try
    {
        *list = new class Freezer* [num];
    }
    catch (...)
    {
        fprintf (stderr, "Cannot allocate memory\n");
        exit (1);
    }
    fz = first_app;
    i = 0;
    while (fz)
    {
        if (fz->sg_enabled) (*list)[i++] = fz;
        fz = fz->next_app;
    }
    return num;
}


void Freezer::turn_off()
{
    if (status == ON && temperature < config->freezer.max_temperature)
    {
        status = OFF;
        household->decrease_power (power.real, power.reactive);
        power_total[0] -= power.real;
        power_total[household->residents] -= power.real;
        decrease_consumption();
        household->heat_loss_app -= power.real;
    }
}

void Freezer::turn_on()
{
    if (status == OFF && temperature > config->freezer.min_temperature)
    {
        status = ON;
        household->increase_power (power.real, power.reactive);
        power_total[0] += power.real;
        power_total[household->residents] += power.real;
        increase_consumption();
        household->heat_loss_app += power.real;
    }
}
