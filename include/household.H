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

#ifndef HOUSEHOLD_H
#define HOUSEHOLD_H

#include "types.H"
#include "constants.H"
#include "element.H"

class Household
{
    friend class WashingMachine;
    friend class TumbleDryer;
    friend class Heating;
    friend class SolarCollector;
    friend class HeatStorage;
    friend class HeatPump;
    friend class AirConditioner;
    friend class E_Vehicle;
    friend class Battery;
    friend class Powerflow;
    friend class Fridge;
    friend class Freezer;

    // Pointers to this household's appliances, which are arranged in lists
    class AirConditioner *aircon;
    class Boiler *boiler;
    class CirculationPump *circpump;
    class Computer *computer;
    class ElectricStove *e_stove;
    class GasStove *gas_stove;
    class Dishwasher *dishwasher;
    class E_Vehicle *e_vehicle;
    class Freezer *freezer;
    class Fridge *fridge;
    class Heating *heating;
    class HeatPump *heatpump;
    class Light *light;
    class TumbleDryer *tumble_dryer;
    class TV *tv;
    class Vacuum *vacuum;
    class WashingMachine *wmachine;
    class SolarModule *solar_module;
    class SolarCollector *solar_collector;
    class Battery *battery;
    class HeatSource *heat_source;
    class HeatStorage *heat_storage;
    // The number of appliances of each kind
    int num_aircons;
    int num_boilers;
    int num_circpumps;
    int num_computers;
    int num_e_stoves;
    int num_gas_stoves;
    int num_dishwashers;
    int num_evehicles;
    int num_freezers;
    int num_fridges;
    int num_heatings;
    int num_heatpumps;
    int num_lamps;
    int num_dryers;
    int num_tvs;
    int num_vacuums;
    int num_wmachines;

    double power_from_grid;         // part of the power above which is drawn from the grid [kW]
    double power_to_grid;           // the solarmodule's overproduction, which can't
                                    // be used immediately (or can't be stored in a battery)
                                    // is fed into the grid [kW]
    double consumption;             // electrical power integral over time [kWh]
    double consumption_solar;       // part of the solar production that is used by the household [kWh]
    double consumption_battery;     // energy drawn from the battery [kWh]
    double consumption_cooking;     // energy used for cooking [kWh]
    double consumption_SH;          // energy used for space heating [kWh]
    double consumption_DHW;         // energy used for domestic hot water [kWh]
    double consumption_prev_day;    // consumption of the previous day
    double income_year;             // annual income received for power_to_grid
    double costs_year;              // annual price to pay for power_from_grid
    double sr_ss_consumption;       // consumption between sunrise and sunset [kWh]
    int occupation;                 // student, fulltime, parttime or retired
    double delta_laundry;
    int at_home[10][2];

    double max_power[3];
    double timestamp_at_mp[3];
    double sol_power_at_mp[3];
    double last_update_mp;

    double max_power_from_grid[3];
    double timestamp_at_mpfg[3];
    double sol_power_at_mpfg[3];
    double power_at_mpfg[3];
    double last_update_mpfg;

    double max_heat_power;      // maximum power of the heating system
    double max_cool_power;      // maximum power of the cooling system
    double temp_int_air;        // room temperature
    double temp_int_air_prev;   // room temperature previous timestep
    double temp_int_set_H;      // set temperature for heating
    double temp_int_set_C;      // set temperature for cooling
    double heat_loss_DHW;       // used in the calculation of the DHW demand
    double probability[1440];   // used to randomly select a start time for each DHW activity
    double probability_sum;     // used to randomly select a start time for each DHW activity
    int dhw_schedule[1440];     // stores start points for DHW activities
    int dhw_schedule_pos;       // current position in the schedule
    Timer *first_timer;         // head of the list of timers
    double *a_matrix;           // system matrix used in the calculation of the operative_temperature
    double *b_vector;           // right-hand side of the linear system
    int *offsets;               // used in the calculation of the operative temperature
    double area_tot;            // total area of walls, floor and ceiling
    bool reduce_heat;           // reduced heating during night-time

    int energy_class;
    int num_elements;           // number of building elements (walls, doors, windows,...)
    int num_nodes;              // number of element nodes (5 per wall, 2 per door or window)
    Element **elements;         // list of building elements

    double *distance;           // matrix of distances between HOME, WORK, ...
    bool shopping_done;

    double feed_to_grid;

    static Household *hh;       // pointer to the first household
    static double with_solar_costs[k_max_residents+1];
    static double without_solar_costs[k_max_residents+1];
    static double income_total[k_max_residents+1];
    static int global_count;
    static int local_count;
    static int first_number;
    static bool batteries_active;
    static int num_vacation;    // number of households on vacation

    template <class AP> void add_appliance (AP **first);
    void add_solar_module();
    void add_solar_collector();
    void add_heat_source();
    void add_heat_storage();
    void add_battery();
    void add_tv (int rank);
    void simulate_1st_pass (double time);
    void simulate_2nd_pass (double time, bool main_simulation);
    void simulate_3rd_pass (double time, bool main_simulation);
    void add_timer (double duration, double mass_flow);
    void construct_building (void);
    double operative_temperature (double phi_HC);
    void space_heating_and_cooling_demand (void);
    double heat_transfer_ventilation (void);

public:
    static class Producer *producer;
    static int count[k_max_residents+1];
    static double consumption_min[k_max_residents+1];
    static double consumption_max[k_max_residents+1];
    static double consumption_sum[k_max_residents+1];
    static double consumption_square[k_max_residents+1];
    static double consumption_cooking_total;                // energy used for cooking in all households [kWh]
    static double consumption_SH_total_integral [NUM_HEAT_SOURCE_TYPES];  // energy used for space heating in all households integrated over time [kWh]
    static double consumption_DHW_total_integral [NUM_HEAT_SOURCE_TYPES]; // energy used for domestic hot water in all households integrated over time [kWh]
    static double real_power_total[k_max_residents+1];      // real power of all households [kW]
    static double reactive_power_total[k_max_residents+1];  // reactive power of all households [kVAR]
    static double apparent_power_total[k_max_residents+1];  // apparent power of all households [kVA]
    static double power_hot_water[k_max_residents+1];   // power for hot water of all households [kW]
    static double power_from_grid_total;                // power drawn from grid over all households [kW]
    static double power_to_grid_total;                  // surplus production of all households [kW]
    static double power_to_grid_total_integral;         // surplus production of all households over time [kW]
    static double production_used_total;                // part of the solar production that is used immediately
                                                        // to power the appliances and to load the battery [kW]
    static double power_above_limit_total;              // surplus production of all households, above feed in limit [kW]
    static double power_above_limit_total_integral;     // surplus production of all households over time, above feed in limit [kW]

    int number;     // household ID
    Power power;    // sum of the power of all appliances in this hh which are currently turned on [kW]
    int residents;  // number of people living in this household
    double wakeup;  // wakeup time in seconds
    double bedtime; // bedtime in seconds
    double bedtime_old; // bedtime calculated at the previous day
    double area;    // the household's area in m^2
    int vacuum_interval;
    double laundry;
    int vacation;                // >0 if the household residents are on vacation
    bool reduce_consumption;     // indicates whether this household should REDUCE its consumption.
    bool raise_consumption;      // indicates whether this household should RAISE its consumption.
    double rc_timestamp;         // time at which the household received a signal to reduce its consumption

    HeatSourceType heat_source_type;   // what kind of heat source is used for SH and DHW
    double heat_demand_DHW;     // heat demand for domestic hot water (DHW)
    double heat_demand_SH;      // heat demand for space heating (SH)
    double heat_loss_app;       // total heat loss of all currently active appliances
    double cool_demand;         // energy demand for room cooling

    Household();
    ~Household();
    static void allocate_memory (int num_households);
    static void deallocate_memory();
    static Household* get_household_ptr (int id);
    static void simulate_forerun();
    static void simulate();
    static void reset_integrals();
    static void calc_consumption();
    static void calc_consumption_SH (double con[]);
    static void calc_consumption_DHW (double con[]);
    static double median (int res);
    static double std_deviation (int res);
    static void print_distribution (FILE *fp, int res);
    static void print_costs (int year);
    static void print_heat_consumption (int year);
    //static void print_debug_heat_consumption (int year);
    static void print_debug_info (int num);
    static void print (FILE *fp1[], FILE *fp2[]);
    static void print_max (FILE *fp1[], FILE *fp2[]);
    static void adapt_pv_module_size();
    static void adapt_battery_capacity();
    static void smartification();
    static double Pd (int list[], int list_length);
    static double Qd (int list[], int list_length);
    static double solar_collector_SH (void);
    static double solar_collector_DHW (void);
    static void activate_batteries() { batteries_active = true; }
    static void deactivate_batteries() { batteries_active = false; }
    static void shared_battery_charging (double *above);
    static void update_vacation (void);
    void get_interval (double *begin, double *end, double limit_1, int rank);
    double residents_at_home_duration (double start_time, int tv_rank);
    int residents_at_home (double daytime);
    double get_random_start_time (double begin, double end);
    double return_time (int tv_rank);
    void decrease_laundry (double amount) { laundry -= amount; }
    void increase_power (double real, double reactive);
    void decrease_power (double real, double reactive);
    bool has_enough_solar_power (double power);
    bool solar_prediction (int days_in_the_future);
    void schedule (DHW_activity act, int start_time);
    void increase_consumption_SH (double value) { consumption_SH += value; };
    void increase_consumption_DHW (double value) { consumption_DHW += value; };
    void increase_consumption_SH_tot_int (double value, int type) { consumption_SH_total_integral[type] += value; };
    void increase_consumption_DHW_tot_int (double value, int type) { consumption_DHW_total_integral[type] += value; };
    void increase_consumption_cooking (double value) { consumption_cooking += value; };
    bool has_boiler (void) { return num_boilers > 0; };
};

#ifdef MAIN_MODULE
double Household::real_power_total[k_max_residents+1];
double Household::reactive_power_total[k_max_residents+1];
double Household::apparent_power_total[k_max_residents+1];
double Household::power_hot_water[k_max_residents+1];
class Producer* Household::producer = NULL;
double Household::with_solar_costs[k_max_residents+1];
double Household::without_solar_costs[k_max_residents+1];
double Household::income_total[k_max_residents+1];
int Household::count[k_max_residents+1];
double Household::consumption_min[k_max_residents+1];
double Household::consumption_max[k_max_residents+1];
double Household::consumption_sum[k_max_residents+1];
double Household::consumption_square[k_max_residents+1];
double Household::consumption_cooking_total = 0.;
double Household::consumption_SH_total_integral[NUM_HEAT_SOURCE_TYPES];
double Household::consumption_DHW_total_integral[NUM_HEAT_SOURCE_TYPES];
class Household* Household::hh = NULL;
int Household::local_count;
int Household::global_count;
int Household::first_number = 1;
double Household::power_from_grid_total = 0.;
double Household::power_to_grid_total = 0.;
double Household::power_to_grid_total_integral = 0.;
double Household::power_above_limit_total = 0.;
double Household::power_above_limit_total_integral = 0.;
double Household::production_used_total = 0.;
bool Household::batteries_active = true;
int Household::num_vacation = 0;
#endif

#endif
