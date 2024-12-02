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

#ifndef BATTERY_H
#define BATTERY_H

#include <stdio.h>

#include "types.H"
#include "constants.H"
#include "household.H"
#include "solarmodule.H"

class Battery
{
    friend class Household;
private:
    double capacity;                       // battery capacity [kWh]
    double charge;                         // current battery charge [kWh]
    double efficiency_charging;            // efficiency of charging
    double efficiency_discharging;         // efficiency of discharging
    double max_power_charging;             // max. power while charging
    double max_power_discharging;          // max. power while discharging
    bool   is_solar_battery;               // TRUE if it's a solar battery
    bool   allow_grid_charge;              // is charging with power from grid allowed?
    bool   sg_enabled;                     // is the battery smart grid enabled?
    double min_price;                      // minimum price of electricity today
                                           // calculated at the beginning of each day
    double retail_price;                   // battery retail price in Euro per kWh capacity
    double level_costs;                    // levelized costs of the storage system
                                           // this is the price for power drawn from
                                           // the battery in Euro/kWh

    class  Household *household;           // pointer to the household this
                                           // module belongs to

public:
    static double charge_total;                   // total charge of all batteries    [kWh]
    static double power_charging_total;           // total power put into batteries   [kW]
    static double power_discharging_total;        // total power drawn from batteries [kW]
    static double power_from_grid_total;          // total power from grid            [kW]
    static double power_from_grid_total_integral; // total power from grid over time  [kW]
    static double loss_charging_total;            // total losses while charging      [kW]
    static double loss_discharging_total;         // total losses while discharging   [kW]
    static int count;                             // total number of batteries
    double power_charging;                        // [kW]
    double power_discharging;                     // [kW]
    bool   is_solar_charging;                     // battery gets charged with PV power?

    Battery (class Household *hh, class SolarModule *m);
    void simulate (double time, double power_household, double solar_power, double feed_to_grid);
    void adapt_capacity (double consumption);
    void print (FILE *fp);
    double charging_power_limit (void);
    void charge_from_neighbour (double *above);
    static void print_summary (FILE *fp, const char name[]);
};

#ifdef MAIN_MODULE
double Battery::charge_total = 0.;
double Battery::power_charging_total = 0.;
double Battery::power_discharging_total = 0.;
double Battery::power_from_grid_total = 0.;
double Battery::power_from_grid_total_integral = 0.;
double Battery::loss_charging_total = 0.;
double Battery::loss_discharging_total = 0.;
int    Battery::count = 0;
#endif

#endif
