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

#include <stdio.h>

#include "proto.H"
#include "household.H"
#include "producer.H"
#include "battery.H"
#include "globals.H"

Battery::Battery (Household *hh, SolarModule *sm)
{
    count++;
    capacity = (double)hh->residents *
                   get_random_number (config->battery.min_capacity_per_resident,
                                      config->battery.max_capacity_per_resident);
    charge = get_random_number (0., 1.) * capacity;
    efficiency_charging = get_random_number (config->battery.min_eff_charging, config->battery.max_eff_charging);
    efficiency_discharging = get_random_number (config->battery.min_eff_discharging, config->battery.max_eff_discharging);
    max_power_charging = config->battery.max_power_charging * capacity;
    max_power_discharging = config->battery.max_power_discharging * capacity;
    is_solar_battery = (sm != NULL);
    allow_grid_charge = !is_solar_battery; // Batteries in a non-solar household are
                                           // always charged from the grid
    min_price = -1;
    retail_price = config->battery.min_price + get_random_number (0., 1.) * (config->battery.max_price - config->battery.min_price);
    level_costs = (retail_price * capacity + config->battery.installation_costs) / (config->battery.avg_lifetime * 365 * capacity);
    household = hh;
    sg_enabled = config->battery.smartgrid_enabled > 0
                 && get_random_number (0., 100.) <= config->battery.smartgrid_enabled;
    is_solar_charging = false;
}


void Battery::simulate (double time, double power_household, double solar_power, double feed_to_grid)
{
    static const double factor = config->timestep_size/3600.;
    double delta_power, limit = 0.;
    static FILE *param_fp = NULL;   // strategy 4 only
    static double offset[12];       // strategy 4 only

    power_charging = 0.;
    power_discharging = 0.;

    if (is_solar_battery)
    {
        delta_power = solar_power - power_household;
        if (delta_power > 0.)    // Surplus production
        {
            is_solar_charging = true;
            switch (config->battery_charging.strategy)
            {
                case 0: // charge batteries whenever possible
                    limit = delta_power;
                    break;
                case 1: // delay charging until feed_to_grid drops down to zero (with a focus on peak-shaving)
                    if (delta_power > config->battery_charging.feed_in_limit * household->solar_module->nominal_power)
                    {
                        limit = delta_power - config->battery_charging.feed_in_limit * household->solar_module->nominal_power;
                    }
                    else if (feed_to_grid <= 0.)
                    {
                        limit = delta_power;
                    }
                    else is_solar_charging = false;
                    break;
                case 2: // similar like method 1, but with a focus on battery charging after feed_to_grid has become zero
                    if (feed_to_grid > 0. && delta_power > config->battery_charging.feed_in_limit * household->solar_module->nominal_power)
                    {
                        limit = delta_power - config->battery_charging.feed_in_limit * household->solar_module->nominal_power;
                    }
                    else if (feed_to_grid <= 0.)
                    {
                        limit = delta_power;
                    }
                    else is_solar_charging = false;
                    break;
                case 3: // Batteries with a charge below a given threshold (e.g. 10% of the capacity) get loaded
                        // according to strategy 0. Above that threshold switch to strategy 2.
                    if (charge/capacity < config->battery_charging.precharge_threshold) // strategy 0
                    {
                        limit = delta_power;
                    }
                    else // strategy 2
                    {
                        if (feed_to_grid > 0. && delta_power > config->battery_charging.feed_in_limit * household->solar_module->nominal_power)
                        {
                            limit = delta_power - config->battery_charging.feed_in_limit * household->solar_module->nominal_power;
                        }
                        else if (feed_to_grid <= 0.)
                        {
                            limit = delta_power;
                        }
                        else is_solar_charging = false;
                    }
                    break;
                case 4: // method without the need for a forecast
                    if (delta_power > config->battery_charging.feed_in_limit * household->solar_module->nominal_power)
                    {
                        limit = delta_power - config->battery_charging.feed_in_limit * household->solar_module->nominal_power;
                    }
                    else
                    {
                        if (!param_fp)
                        {
                            open_file (&param_fp, "param", "r");
                            for (int i=0; i<12; i++) fscanf (param_fp, "%lf", offset+i);
                        }
                        /*switch (sim_clock->month)
                        {
                            case 1: offset = 0.; break;
                            case 2: offset = 0.9; break;
                            case 3: offset = 2.8; break;
                            case 4: offset = 3.7; break;
                            case 5: offset = 3.9; break;
                            case 6: offset = 5.2; break;
                            case 7: offset = 4.8; break;
                            case 8: offset = 4.4; break;
                            case 9: offset = 2.9; break;
                            case 10: offset = 1.3; break;
                            case 11: offset = 0.; break;
                            case 12: offset = 0.; break;
                         // EV = 70.876680 with seed = 1
                        }*/
                        if (sim_clock->daytime > (sim_clock->sunrise + offset[sim_clock->month-1]*3600))
                        {
                            limit = delta_power;
                        }
                        else is_solar_charging = false;
                    }
                    break;
                default:;
            }
            if (is_solar_charging)
            {
                power_charging = fmin (limit, max_power_charging);
                // We have to check, whether the battery has enough capacity left
                if (charge + power_charging*efficiency_charging*factor > capacity)
                {
                    // Not enough room to store all => adapt power_charging
                    power_charging = (capacity - charge)/(efficiency_charging*factor);
                    charge = capacity;
                }
                else charge += power_charging*efficiency_charging*factor;
            }
        }
        else if (delta_power <= 0.)  // not enough solar power to keep up with consumption
        {
            // Decide whether we want to draw power from the battery or
            // save that power for later, when the price for electricity from the
            // grid is maybe higher compared to now.
            // When the current price is really low, we can even charge the battery
            // with power from the grid.

            if (   allow_grid_charge
                && fabs (household->producer->price (GRID, time) - min_price) < k_float_compare_eps
                && !household->reduce_consumption)
            {
                is_solar_charging = false;
                power_charging = max_power_charging; // Charge battery with grid power

                // Enough capacity left?
                if (charge + power_charging*efficiency_charging*factor > capacity)
                {
                    // Not enough room to store all => adapt power_charging
                    power_charging = (capacity - charge)/(efficiency_charging*factor);
                    charge = capacity;
                }
                else charge += power_charging*efficiency_charging*factor;
                power_from_grid_total += power_charging;
                power_from_grid_total_integral += power_charging;
                household->increase_power (power_charging, 0.);
            }
            else if (household->producer->price (GRID, time) > min_price + level_costs)
            {
                power_discharging = fmin (-delta_power, max_power_discharging);

                // Check whether the battery has enough charge left
                if (charge - power_discharging/efficiency_discharging*factor < 0.)
                {
                    power_discharging = charge * efficiency_discharging / factor;
                    charge = 0.;
                }
                else charge -= power_discharging/efficiency_discharging*factor;
            }
        }
    }
    else  // It's a non-solar battery
    {
        // Non-solar batteries are charged whenever the electricity tariff is very low,
        // and discharged when the tariff is high. If it is a smartgrid enabled battery and
        // voltage control has sent a 'raise consumption' signal, then charging is forced

        if ((sg_enabled && household->raise_consumption)
            ||
            fabs (household->producer->price (GRID, time) - min_price) < k_float_compare_eps)
        {
            power_charging = max_power_charging; // Charge battery with grid power

            // Enough capacity left?
            if (charge + power_charging*efficiency_charging*factor > capacity)
            {
                // Not enough room to store all => adapt power_charging
                power_charging = (capacity - charge)/(efficiency_charging*factor);
                charge = capacity;
            }
            else charge += power_charging*efficiency_charging*factor;
            power_from_grid_total += power_charging;
            power_from_grid_total_integral += power_charging;
            household->increase_power (power_charging, 0.);
        }
        else if ((sg_enabled && household->reduce_consumption)
                  ||
                  household->producer->price (GRID, time) > min_price + level_costs)
        {
            power_discharging = fmin (power_household, max_power_discharging);

            // Check whether the battery has enough charge left
            if (charge - power_discharging/efficiency_discharging*factor < 0.)
            {
                power_discharging = charge * efficiency_discharging / factor;
                charge = 0.;
            }
            else charge -= power_discharging/efficiency_discharging*factor;
        }
    }
    // Update integral values
    power_charging_total += power_charging;
    loss_charging_total += power_charging * (1. - efficiency_charging);
    power_discharging_total += power_discharging;
    loss_discharging_total += power_discharging * (1./efficiency_discharging - 1.);
    charge_total += 100. * charge / capacity;

    // Calculate the minimum price for grid-electricity for the next 24 hours
    // and decide whether to set the battery in grid-charge-mode.

    if (   ((is_solar_battery && config->battery.allow_grid_charge_solar) || !is_solar_battery)
        && almost_equal (sim_clock->daytime, sim_clock->sunset))
    {
        min_price = household->producer->min_price_in_time_interval (time, time+24*3600);

        // Solar batteries check the weatherforecast for the following day. If it's
        // going to be a day with little or no sun, we could load the battery from
        // the grid when the price is really low.
        if (is_solar_battery) allow_grid_charge = !household->solar_prediction(1); // adverse weather conditions 1 day ahead
    }
}


void Battery::adapt_capacity (double consumption)
{
    double relative_charge = charge/capacity;
    capacity = config->battery.capacity_in_days * consumption / 365.;
    charge = relative_charge * capacity;
    max_power_charging = config->battery.max_power_charging * capacity;
    max_power_discharging = config->battery.max_power_discharging * capacity;
    level_costs = (retail_price * capacity + config->battery.installation_costs) / (config->battery.avg_lifetime * 365 * capacity);
}


void Battery::print (FILE *fp)
{
    fprintf (fp, " %.3lf %.3lf %.3lf",
             capacity, efficiency_charging, efficiency_discharging);
}

void Battery::print_summary (FILE *fp, const char name[])
{
    if (count)
    {
        double factor = config->timestep_size/3600.;
        fprintf (fp, "%20s %17.3lf kWh\n", name, power_from_grid_total_integral*factor);
    }
}

void Battery::charge_from_neighbour (double *above)
{
    const double factor = config->timestep_size/3600.;
    is_solar_charging = true;
    power_charging = fmin (*above, max_power_charging);
    if (charge + power_charging*efficiency_charging*factor > capacity)
    {
        power_charging = (capacity - charge)/(efficiency_charging*factor);
        charge = capacity;
    }
    else charge += power_charging*efficiency_charging*factor;
    *above -= power_charging;
}

double Battery::charging_power_limit()
{
    const double factor = config->timestep_size/3600.;
    double limit;
    limit = max_power_charging - power_charging;
    if (charge + limit*efficiency_charging*factor > capacity)
    {
        limit = (capacity - charge)/(efficiency_charging*factor);
    }
    return limit;
}
