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
#include "producer.H"
#include "globals.H"
#include "proto.H"


E_Vehicle::E_Vehicle (Household *hh)
{
    static int num = 0;
    number = num++;
    count[hh->residents]++;
    next_app = first_app;
    first_app = this;
    household = hh;
    status = OFF;
    position = HOME;
    last_position = HOME;
    destination = HOME;
    arrival_time = -1000;
    departure_time = -1000;
    soc_gradient = 0.2;
    soc_midpoint = 0.75;
    idle_time = -1;
    can_charge_at_work = get_random_number (1, 100) < 5;
    charging_is_possible = true;
    smart = false;
    sg_enabled = config->e_vehicle.smartgrid_enabled > 0
                 && get_random_number (0., 100.) <= config->e_vehicle.smartgrid_enabled;
    model_index = get_random_number (0, num_models-1);
    battery_charge = config->e_vehicle.models[model_index].battery_capacity;
    power.real = 0.;
    power.reactive = 0.;
}


void E_Vehicle::simulate()
{
    double daytime = sim_clock->daytime;
    static const double factor = config->timestep_size/3600.;
    int rnd;
    double speed, duration;
    double temp_factor, energy_drive;
    bool begin_charging;
    double p_charge, soc;
    double battery_capacity = config->e_vehicle.models[model_index].battery_capacity;

    if (almost_equal (daytime, household->wakeup))   // make a decision for the first trip of the day
    {
        if (sim_clock->weekday == SATURDAY)
        {
            switch (household->occupation)
            {
                case PARTTIME:
                case FULLTIME:
                    rnd = get_random_number (1, 100);
                    if (rnd < 25)
                    {
                        departure_time = daytime + config->e_vehicle.departure_delay;
                        destination = WORK;
                    }
                    else if (rnd < 50)
                    {
                        departure_time = get_random_number (daytime + 3600, 61200.);
                        destination = SHOP;
                    }
                    else
                    {
                        departure_time = get_random_number (daytime + 3600, fmin (household->bedtime-7200, 79200.));
                        destination = RECREATION;
                    }
                    break;
                case STUDENT:
                case RETIRED:
                    rnd = get_random_number (1, 100);
                    if (rnd < 25)
                    {
                        departure_time = daytime + 7200;
                        destination = SHOP;
                    }
                    else if (rnd < 50)
                    {
                        departure_time = normal_distributed_random (54000, 7200);   // 15:00, 2h
                        destination = SHOP;
                    }
                    else
                    {
                        departure_time = get_random_number (daytime+1800, fmin (household->bedtime-7200, 79200.));
                        destination = RECREATION;
                    }
                    break;
            }
        }
        else if (sim_clock->weekday == SUNDAY || sim_clock->holiday)
        {
            switch (household->occupation)
            {
                case STUDENT:
                case PARTTIME:
                case FULLTIME:
                case RETIRED:
                    rnd = get_random_number (1, 100);
                    if (rnd < 10) departure_time = daytime + 1800;
                    else
                    {
                        departure_time = normal_distributed_random (16*3600, 2*3600);
                        if (departure_time < daytime) departure_time = get_random_number (daytime+1800, fmin (household->bedtime-7200, 79200.));
                    }
                    destination = RECREATION;
                    break;
            }
        }
        else // Monday - Friday
        {
            switch (household->occupation)
            {
                case STUDENT:
                case PARTTIME:
                    if (get_random_number (1, 100) < 50)
                    {
                        departure_time = daytime + config->e_vehicle.departure_delay;
                        destination = WORK;
                    }
                    else
                    {
                        departure_time = daytime + get_random_number (1800, 43200-(int)daytime);
                        if (get_random_number (1, 100) < 50) destination = WORK; else destination = SHOP;
                    }
                    break;
                case FULLTIME:
                    // first trip in the morning is usually from home to work
                    departure_time = daytime + config->e_vehicle.departure_delay;
                    destination = WORK;
                    break;
                case RETIRED:
                    rnd = get_random_number (1, 100);
                    if (rnd < 25)
                    {
                        departure_time = daytime + 7200;
                        destination = SHOP;
                    }
                    else if (rnd < 50)
                    {
                        departure_time = normal_distributed_random (54000, 7200);   // 15:00, 2h
                        destination = SHOP;
                    }
                    else
                    {
                        departure_time = get_random_number (daytime+1800, fmin (household->bedtime-7200, 79200.));
                        destination = RECREATION;
                    }
                    break;
            }
        }
    }

    if (almost_equal (daytime, departure_time))
    {
        status = DRIVING;
        last_position = position;
        idle_time = -1;
        charging_is_possible = false;

        // Calculate the arrival time depending on speed and distance
        if (location->type == URBAN) speed = get_random_number (40., 50.);
        else speed = get_random_number (50., 80.);
        if (destination == RECREATION || position == RECREATION) distance = get_random_number (5., 50.);
        else distance = household->distance[position*NUM_DESTINATIONS+destination];
        duration = 3600. * distance / speed;  // time in seconds
        arrival_time = daytime + duration;
        if (arrival_time >= k_seconds_per_day) arrival_time -= k_seconds_per_day;
    }
    else if (almost_equal (daytime, arrival_time))
    {
        position = destination;
        if (position == HOME) arr_counter++;
        if (position == SHOP) household->shopping_done = true;

        // calculate the energy used for driving depending on the nominal value of the car
        // and the ambient temperature (air conditioning!)

        if (location->temperature < 15) temp_factor = 1.12 - 0.01 * location->temperature;
        else if (location->temperature <= 20) temp_factor = 1;
        else temp_factor = 0.63 + 0.02 * location->temperature;

        energy_drive = config->e_vehicle.models[model_index].consumption_per_100km/100. * distance * temp_factor;

        // update the battery charge
        battery_charge -= energy_drive;
        if (battery_charge < 0.) battery_charge = 0.;

        // charging decision depending on the position (availability of a charger) and the
        // current battery charge.
        switch (position)
        {
            case HOME: charging_is_possible = true; break;
            case WORK: charging_is_possible = can_charge_at_work; break;
            case SHOP: charging_is_possible = (get_random_number (1, 100) < 5); break;
            default: charging_is_possible = false;
        }
        if (charging_is_possible)
        {
            // Find out whether the battery should be charged right now.
            soc = battery_charge / battery_capacity;
            if (smart)
            {
                // If the car is a smart one, the decision is left to the car/wallbox.
                // A smart car parked at home will try to take advantage of a high solar
                // production.
                if (position == HOME)
                {
                    if (household->has_enough_solar_power (power.real))
                    {
                        begin_charging = (soc < 0.95);
                    }
                    else begin_charging = (soc < 0.60);
                }
                else
                {
                    // Charge only when really low on battery
                    begin_charging = (soc < 0.40);
                }
            }
            else
            {
                // In this case the decision is made by the driver, and is based on the
                // current SOC (battery charge) and on personal habits (expressed by
                // the factors soc_gradient and soc_midpoint).
                p_charge = fmin ((1. - 1./(1.+exp (-soc_gradient*(soc-soc_midpoint)))), 1.);
                begin_charging = (p_charge > 0. && get_random_number (0., 1.) <= p_charge);
            }
        }
        else begin_charging = false;

        if (begin_charging) status = CHARGING; else status = IDLE;

        // Determine the next destination and departure time

        switch (position)
        {
            case HOME:
                if (household->occupation == RETIRED)
                {
                    if (sim_clock->weekday == SUNDAY || sim_clock->holiday)
                    {
                    }
                    else
                    {
                    }
                }
                else // household with at least one person working or being in education
                {
                    if (sim_clock->weekday == SUNDAY || sim_clock->holiday)
                    {
                    }
                    else if (sim_clock->weekday == SATURDAY)
                    {
                    }
                    else
                    {
                    }
                }
                break;
            case WORK:
                if (household->occupation == FULLTIME)
                {
                    departure_time = daytime + get_random_number (32400, 3600);  // 9h, 1h
                }
                else
                {
                    departure_time = daytime + get_random_number (18000, 3600);  // 5h, 1h
                }
                destination = HOME;
                break;
            case SHOP:
                departure_time = daytime + get_random_number (3600, 1800);  // 1h, 0.5h
                destination = HOME;
                break;
            default:
                departure_time = daytime + get_random_number (7200, 3600);  // 2h, 1h
                destination = HOME;
                break;
        }
        if (departure_time >= k_seconds_per_day) departure_time -= k_seconds_per_day;

    }   // end if daytime == arrival_time

    // Override above rules in case of grid over-/undervoltage. This effects all
    // smartgrid enabled cars
    if (sg_enabled)
    {
        if (household->reduce_consumption)
        {
            if (status == CHARGING) status = FORCED_IDLE;
        }
        else if (household->raise_consumption)
        {
            if (status == IDLE && charging_is_possible && battery_charge < battery_capacity) status = FORCED_CHARGING;
        }
        else  // continue with whatever you did before the raise/reduce signal
        {
            if (status == FORCED_IDLE) status = CHARGING;
            if (status == FORCED_CHARGING) status = IDLE;
        }
    }

    if (status == CHARGING || status == FORCED_CHARGING)
    {
        double soc, interval_length, gradient;
        int index;

        // the current power depends on the charging curve of the battery
        soc = battery_charge / battery_capacity;
        interval_length = 1./(k_num_curve_points-1.);
        index = (int)(soc / interval_length);
        gradient = (config->e_vehicle.models[model_index].charging_curve[index+1] - config->e_vehicle.models[model_index].charging_curve[index]) / interval_length;
        power.real = (config->e_vehicle.models[model_index].charging_curve[index] + gradient * (soc - index*interval_length))
        * config->e_vehicle.models[model_index].max_charge_power_AC;
        battery_charge += power.real * factor;
        if (battery_charge >= battery_capacity)
        {
            battery_charge = battery_capacity;
            status = IDLE;
            idle_time = 0.;  // start the idle timer
        }
        //if (position == HOME)
        {
            household->increase_power (power.real, power.reactive);
            power_total[0] += power.real;
            power_total[household->residents] += power.real;
            increase_consumption();
        }
    }
    else if (status == IDLE)  // take self discharge into account
    {
        if (idle_time >= 0)  // the car has been idling since the last full charge
        {
            if (idle_time <= 24)
            {
                // non-linear discharge during the first 24h after charging was completed
                battery_charge = battery_capacity * (0.95 + 0.05/(idle_time+1));
            }
            else if (idle_time <= 30*24)
            {
                // linear discharge between 0.95 and 0.9 SOC
                battery_charge = battery_capacity * (0.95 - 0.05/(29*24) * (idle_time - 24));
            }
            else battery_charge = battery_capacity * 0.9;
            idle_time += factor;
        }
    }
}


int E_Vehicle::create_smart_list (E_Vehicle ***list)
{
    E_Vehicle *ev = first_app;
    int i, num = 0;
    while (ev)
    {
        if (ev->sg_enabled) num++;
        ev = ev->next_app;
    }
    try
    {
        *list = new class E_Vehicle* [num];
    }
    catch (...)
    {
        fprintf (stderr, "Cannot allocate memory\n");
        exit (1);
    }
    ev = first_app;
    i = 0;
    while (ev)
    {
        if (ev->sg_enabled) (*list)[i++] = ev;
        ev = ev->next_app;
    }
    return num;
}


void E_Vehicle::turn_off()
{
    static const double factor = config->timestep_size/3600.;
    if (status == CHARGING || status == FORCED_CHARGING)  // can override a raise consumption signal
    {
        status = FORCED_IDLE;
        battery_charge -= power.real * factor;
        household->decrease_power (power.real, power.reactive);
        power_total[0] -= power.real;
        power_total[household->residents] -= power.real;
        decrease_consumption();
    }
}

void E_Vehicle::turn_on()
{/*
    double battery_capacity = config->e_vehicle.models[model_index].battery_capacity;
    static const double factor = config->timestep_size/3600.;
    if (status == IDLE && charging_is_possible && battery_charge < battery_capacity)
    {
        double soc, gradient, interval_length;
        int index;
        status = FORCED_CHARGING;
        soc = battery_charge / battery_capacity;
        interval_length = 1./(k_num_curve_points-1.);
        index = (int)(soc / interval_length);
        gradient = (config->e_vehicle.models[model_index].charging_curve[index+1] - config->e_vehicle.models[model_index].charging_curve[index]) / interval_length;
        power.real = (config->e_vehicle.models[model_index].charging_curve[index] + gradient * (soc - index*interval_length))
        * config->e_vehicle.models[model_index].max_charge_power_AC;
        battery_charge += power.real * factor;
        if (battery_charge >= battery_capacity)
        {
            battery_charge = battery_capacity;
            status = IDLE;
            idle_time = 0.;  // start the idle timer
        }
        household->increase_power (power.real, power.reactive);
        power_total[0] += power.real;
        power_total[household->residents] += power.real;
        increase_consumption();
    }*/
}
