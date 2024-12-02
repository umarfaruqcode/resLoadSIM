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

#ifndef APPLIANCE_H
#define APPLIANCE_H

#include <float.h>
#include <stdio.h>
#ifdef PARALLEL
#include <mpi.h>
#endif

#include "types.H"
#include "constants.H"
#include "household.H"
#include "proto.H"
#include "globals.H"


template <class AP>
class Appliance_CRTP
{
private:
    static void calc_consumption();
    static double median (int res);
    static double std_deviation (int res);
    static double consumption_min[k_max_residents+1];
    static double consumption_max[k_max_residents+1];
    static double consumption_sum[k_max_residents+1];
    static double consumption_square[k_max_residents+1];
    static int hh_count[k_max_residents+1];

protected:
    Status status;             // either ON or OFF
    double consumption;        // energy consumption [kWh]
    static int num_energy_classes;
    static int count[k_max_residents+1];
    int energy_class;
    class Household *household;  // pointer to the household
                                 // this appliance belongs to
public:
    static double power_total[k_max_residents+1];  // [kW]
    static AP *first_app;
    AP *next_app;
    Power power;               // instantaneous power [kW]

    Appliance_CRTP() { count[0]++; consumption = 0.; energy_class = 0; }
    static void print_EEI (FILE *fp, AP *head, int num);
    static void print_consumption (FILE *fp, const char name[]);
    static double print_summary (FILE *fp, const char name[]);
    static void reset_consumption() {AP *app=first_app; while (app) {app->consumption=0.; app=app->next_app;}}
    static int  global_count()
    {
#ifdef PARALLEL
        MPI_Allreduce (MPI_IN_PLACE, count, k_max_residents+1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
#endif
        return count[0];
    }
    void increase_consumption() { consumption += power.real*config->timestep_size/3600.; }
    void increase_consumption (double corr_factor) { consumption += corr_factor*power.real*config->timestep_size/3600.; }
    void decrease_consumption() { consumption -= power.real*config->timestep_size/3600.; }
};
template <class AP> int Appliance_CRTP<AP>::count[k_max_residents+1];
template <class AP> int Appliance_CRTP<AP>::hh_count[k_max_residents+1];
template <class AP> double Appliance_CRTP<AP>::power_total[k_max_residents+1];
template <class AP> double Appliance_CRTP<AP>::consumption_min[k_max_residents+1];
template <class AP> double Appliance_CRTP<AP>::consumption_max[k_max_residents+1];
template <class AP> double Appliance_CRTP<AP>::consumption_sum[k_max_residents+1];
template <class AP> double Appliance_CRTP<AP>::consumption_square[k_max_residents+1];
template <class AP> AP* Appliance_CRTP<AP>::first_app = NULL;
template <class AP> int Appliance_CRTP<AP>::num_energy_classes = 1;


class E_Vehicle : public Appliance_CRTP<E_Vehicle>
{
private:
    int number;
    bool sg_enabled;
    bool smart;
    bool can_charge_at_work;
    bool charging_is_possible;
    int model_index;
    int last_position;
    int position;
    int destination;
    double idle_time;           // uninterrupted time since last full charge in hours
    double distance;
    double departure_time;
    double arrival_time;
    double battery_charge;
    double soc_gradient;        // used for charging decision; midpoint gradient of the logistic function
    double soc_midpoint;        // used for charging decision; the corresponding midpoint SOC
public:
    static int num_models;
    static double arr_counter;
    E_Vehicle (Household *hh);
    void simulate();
    static int create_smart_list (E_Vehicle ***list);
    void turn_off();
    void turn_on();
    void make_smart() { smart = config->e_vehicle.smart > 0
        && get_random_number (0., 100.) <= config->e_vehicle.smart; }
};

#ifdef MAIN_MODULE
int E_Vehicle::num_models = 0;
double E_Vehicle::arr_counter = 0;
#endif


class Fridge : public Appliance_CRTP<Fridge>
{
private:
    double temperature;
    double target_temperature;
    double delta_t_rise;
    double delta_t_drop;
    bool sg_enabled;
    bool smart;
    //bool smart_mode;
public:
    Fridge (Household *hh);
    void simulate (double time);
    static int create_smart_list (Fridge ***list);
    void turn_off();
    void turn_on();
    double operator-(Fridge f2) { return temperature - f2.temperature; }
    void make_smart() { smart = config->fridge.smart > 0
        && get_random_number (0., 100.) <= config->fridge.smart; }
};


class Freezer : public Appliance_CRTP<Freezer>
{
private:
    double temperature;
    double target_temperature;
    double delta_t_rise;
    double delta_t_drop;
    bool sg_enabled;
    bool smart;
    //bool smart_mode;
public:
    Freezer (Household *hh);
    void simulate (double time);
    static int create_smart_list (Freezer ***list);
    void turn_off();
    void turn_on();
    double operator-(Freezer f2) { return temperature - f2.temperature; }
    void make_smart() { smart = config->freezer.smart > 0
        && get_random_number (0., 100.) <= config->freezer.smart; }
};


class ElectricStove : public Appliance_CRTP<ElectricStove>
{
private:
    double time_1, time_2, time_3;
    double duration_1, duration_2, duration_3;
    int timer;
public:
    ElectricStove (Household *hh);
    void simulate();
};


class GasStove : public Appliance_CRTP<GasStove>
{
private:
    double time_1, time_2, time_3;
    double duration_1, duration_2, duration_3;
    int timer;
public:
    GasStove (Household *hh);
    void simulate();
};


class Boiler : public Appliance_CRTP<Boiler>
{
private:
//    double efficiency;      // efficiency depending on energy efficiency class
    double heat_sum;        // the amount of heat which has to be provided by this boiler
    double heat_loss_sum;
    double heat_loss;
public:
    Boiler (Household *hh);
    void simulate();
};


class TV : public Appliance_CRTP<TV>
{
private:
    int rank;
    double avg_duration;
    double time_1, time_2;
    double duration_1, duration_2;
    int timer;
public:
    TV (Household *hh, int rank);
    void simulate();
};


class AirConditioner : public Appliance_CRTP<AirConditioner>
{
private:
    double efficiency;
public:
    double max_cool_power;
    AirConditioner (Household *hh);
    void simulate();
};


class Computer : public Appliance_CRTP<Computer>
{
private:
    double time_1, time_2;
    double duration_1, duration_2;
    int timer;
public:
    Computer (Household *hh);
    void simulate();
};


class WashingMachine : public Appliance_CRTP<WashingMachine>
{
private:
    double capacity;     // the maximum load in kg
    bool   sg_enabled;   // controllable by the producer in a smart grid ?
    bool   smart;        // can the appliance control itself ?
    bool   smart_mode;
    int    timer;
    double seconds_per_cycle;
    double consumption_per_cycle;
public:
    static bool stop;
    static double **variable_load;
    WashingMachine (Household *hh);
    void simulate (double time);
    void make_smart() { smart = config->wmachine.smart > 0
                                && get_random_number (0., 100.) <= config->wmachine.smart; }
};

#ifdef MAIN_MODULE
bool WashingMachine::stop = false;
double **WashingMachine::variable_load = NULL;
#endif


class Dishwasher : public Appliance_CRTP<Dishwasher>
{
private:
    bool   sg_enabled;   // controllable by the producer in a smart grid ?
    bool   smart;        // can the appliance control itself ?
    bool   smart_mode;
    int    timer;
public:
    static bool stop;
    Dishwasher (Household *hh);
    void simulate (double time);
    void make_smart() { smart = config->dishwasher.smart > 0
                                && get_random_number (0., 100.) <= config->dishwasher.smart; }
};

#ifdef MAIN_MODULE
bool  Dishwasher::stop = false;
#endif


class Light : public Appliance_CRTP<Light>
{
private:
    double time_1, time_2;
    double duration_1, duration_2;
    int timer;
public:
    Light (Household *hh);
    void simulate();
};


class Heating : public Appliance_CRTP<Heating>
{
private:
    bool sg_enabled;
public:
    double max_heat_power;
    Heating (Household *hh);
    void simulate();
};


class HeatPump : public Appliance_CRTP<HeatPump>
{
private:
    double efficiency;
    double temp_hot;
    double heat_sum;
public:
    double max_heat_power;
    HeatPump (Household *hh);
    void simulate();
    static void correction_term (void);
};


class Vacuum : public Appliance_CRTP<Vacuum>
{
private:
    int days_since_last_usage;
    int timer;
public:
    Vacuum (Household *hh);
    void simulate();
};


class TumbleDryer : public Appliance_CRTP<TumbleDryer>
{
private:
    double capacity;
    double laundry;
    int timer;
    bool sg_enabled;
public:
    static bool stop;
    TumbleDryer (Household *hh);
    void simulate (double time);
    void add_laundry (double value) { laundry += value; }
};

#ifdef MAIN_MODULE
bool TumbleDryer::stop = false;
#endif


class CirculationPump : public Appliance_CRTP<CirculationPump>
{
private:
    bool is_controlled;
    bool non_stop_operation;
    int first_month, first_day;
    int last_month, last_day;
    int timer;
public:
    CirculationPump (Household *hh);
    void simulate();
};

#endif
