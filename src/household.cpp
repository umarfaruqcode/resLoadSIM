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
#include <stdio.h>
#include <string.h>
#ifdef PARALLEL
#   include <mpi.h>
#endif

#include "element.H"
#include "appliance.H"
#include "household.H"
#include "solarmodule.H"
#include "solarcollector.H"
#include "battery.H"
#include "producer.H"
#include "heatsource.H"
#include "heatstorage.H"
#include "proto.H"
#include "globals.H"

int compare_double (double *val_1, double *val_2);
static int compare_households_vacation (const void *h1, const void *h2);


Household::Household()
{
    double x;
    double percent;
    int num_hh_per_category[k_max_residents];
    int limit[k_max_residents];
    static int counter = 0;


    number = first_number + counter;
    counter++;

    reduce_consumption = false;
    raise_consumption = false;
    rc_timestamp = DBL_MAX;
    shopping_done = false;
    bedtime = k_seconds_per_day;
    feed_to_grid = 0.;
    vacation = 0;

    // Determine the number of residents
    int sum = 0;
    for (int i=0; i<k_max_residents-1; i++)
    { 
        num_hh_per_category[i] = global_count * config->household.size_distribution[i] / 100.0; 
        sum += num_hh_per_category[i];
    }
    num_hh_per_category[k_max_residents-1] = global_count - sum;
    limit[0] = num_hh_per_category[0];
    for (int i=1; i<k_max_residents; i++)
    { 
        limit[i] = limit[i-1] + num_hh_per_category[i]; 
    }
    for (int i=1; i<=k_max_residents; i++)
    {
        if (number <= limit[i-1])
        {
            residents = i;
            break;
        }
    }
    count[residents]++;
    
    // Prepare the limit array to be used later when appliances get added to the household
    for (int i=k_max_residents-1; i>0; i--)
    {
        limit[i] = limit[i-1];
    }
    limit[0] = 0;
    
    sr_ss_consumption = 0.;
    consumption_prev_day = 0.;
    consumption_solar = 0.;
    consumption_battery = 0.;
    for (int i=0; i<3; i++)
    {
        max_power[i] = 0.;
        max_power_from_grid[i] = 0.;
    }
    last_update_mp = last_update_mpfg = 0.;

    if (residents == 1)
    {
        if (config->household.retired_1 > 0 && get_random_number (0., 100.) <= config->household.retired_1) occupation = RETIRED;
        else occupation = get_random_number (0, 2);
    }
    else if (residents == 2)
    {
        if (config->household.retired_2 > 0 && get_random_number (0., 100.) <= config->household.retired_2) occupation = RETIRED;
        else occupation = get_random_number (0, 2);
    }
    else occupation = get_random_number (0, 2);

    temp_int_set_H = config->household.set_temperature_H_day;
    temp_int_set_C = config->household.set_temperature_C;
    temp_int_air = temp_int_air_prev = temp_int_set_H;  // assume that initially the internal air temperature = set temperature
    heat_loss_app = 0.;

    // Construct the building as a set of elements (floor, walls, windows, doors)
    // This is needed for the heat demand model based on ISO-52016-1

    energy_class = random_energy_class (config->household.energy_class);

    construct_building ();

    // Initialize variables used for the DHW model

    heat_demand_DHW = 0.;
    //heat_loss_DHW = 0.20;
    heat_loss_DHW = 0.; // heat losses are handled in the boilers' simulation
    for (int i=0; i<1440; i++) dhw_schedule[i] = DO_NOTHING;
    first_timer = NULL;

    // SOLARMODULE

    // If the power flow solver is used, then the info whether to add a solar module
    // is read from the case file extension.

    solar_module = NULL;
    if (!config->powerflow.step_size) // no power flow solver is used
    {
        percent = config->household.prevalence.solar_module[residents-1];
        if (percent > 0 && get_random_number (0., 100.) <= percent)
        {
            add_solar_module ();
        }
    }

    // BATTERY

    // If the power flow solver is used, then the info whether to add a battery
    // is read from the case file extension.

    battery = NULL;
    if (!config->powerflow.step_size) // no power flow solver is used
    {
        if (solar_module) percent = config->battery.frequency_solar;
        else              percent = config->battery.frequency_non_solar;
        if (percent > 0 && get_random_number (0., 100.) <= percent)
        {
            add_battery ();
        }
    }

    // AIR CONDITIONER
    aircon = NULL;
    num_aircons = 0;
    percent = config->household.prevalence.aircon[residents-1];
    if (percent > 0 && get_random_number (0., 100.) <= percent)
    {
        add_appliance (&aircon);
        num_aircons++;
    }

    // Heat source related initialization

    heatpump = NULL;
    num_heatpumps = 0;
    heating = NULL;
    num_heatings = 0;
    boiler = NULL;
    num_boilers = 0;
    solar_collector = NULL;
    heat_storage = NULL;
    heat_source = NULL;  // no oil, gas or district heating
    a_matrix = NULL;
    b_vector = NULL;
    offsets = NULL;
    area_tot = 0.;
    heat_demand_SH = 0.;

    x = get_random_number (0., 100.);
    double limit_heat_source = config->household.rnd_heat_source[0];
    for (int i=0; i<NUM_HEAT_SOURCE_TYPES; i++)
    {
        if (x <= limit_heat_source)
        {
            heat_source_type = (HeatSourceType)i;
            break;
        }
        limit_heat_source += config->household.rnd_heat_source[i+1];
    }
    if (heat_source_type == HEAT_PUMP)
    {
        add_appliance (&heatpump);
        num_heatpumps++;
        max_heat_power = heatpump->max_heat_power;
        // If the primary heat source is a heat pump we may add an electrical heating
        // as a backup for those really cold days
        percent = config->household.prevalence.heating[residents-1];
        if (percent > 0 && get_random_number (0., 100.) <= percent)
        {
            add_appliance (&heating);
            num_heatings++;
            max_heat_power += heating->max_heat_power;
        }
    }
    else if (heat_source_type == SOLAR_COLLECTOR)
    {
        add_solar_collector ();
        add_heat_storage ();
        add_appliance (&heatpump);
        num_heatpumps++;
        max_heat_power = heatpump->max_heat_power;
    }
    else
    {
        add_heat_source ();  // oil, gas or district heating
        max_heat_power = heat_source->max_heat_power;
    }
    percent = config->household.reduce_heat;
    reduce_heat = (percent > 0 && get_random_number (0., 100.) <= percent);
    if (aircon) max_cool_power = aircon->max_cool_power;

    // BOILER
    int num_hh_with_boiler = global_count
                             * config->household.size_distribution[residents-1]/100.0 
                             * config->household.prevalence.boiler[residents-1]/100.0;
    if (number <= limit[residents-1] + num_hh_with_boiler)
    {
        add_appliance (&boiler);
        num_boilers++;
    }

    // FRIDGE
    fridge = NULL;
    num_fridges = 0;
    percent = config->household.prevalence.fridge[residents-1];
    if (percent > 0 && get_random_number (0., 100.) <= percent)
    {
        add_appliance (&fridge);
        num_fridges++;
        percent = config->household.second_fridge[residents-1];
        if (percent > 0 && get_random_number (0., 100.) <= percent)
        {
            add_appliance (&fridge);
            num_fridges++;
        }
    }

    // FREEZER
    freezer = NULL;
    num_freezers = 0;
    int num_hh_with_freezer = global_count      // number of households with a freezer (per household size)
                              * config->household.size_distribution[residents-1]/100.0 
                              * config->household.prevalence.freezer[residents-1]/100.0;
    if (number <= limit[residents-1] + num_hh_with_freezer)
    {
        add_appliance (&freezer);
        num_freezers++;
    }

    // STOVE
    e_stove = NULL;
    num_e_stoves = 0;
    gas_stove = NULL;
    num_gas_stoves = 0;
    percent = config->household.prevalence.stove[residents-1];
    if (percent > 0 && get_random_number (0., 100.) <= percent)
    {
        add_appliance (&e_stove);
        num_e_stoves++;
    }
    else
    {
        add_appliance (&gas_stove);
        num_gas_stoves++;
    }

    // TV
    tv = NULL;
    num_tvs = 0;
    percent = config->household.prevalence.tv[residents-1];
    if (percent > 0 && get_random_number (0., 100.) <= percent)
    {
        num_tvs = 1;   // At least one TV in this household

        // Add second and third tv according to the hh configuration
        percent = config->household.second_tv[residents-1];
        if (percent > 0 && get_random_number (0., 100.) <= percent)
        {
            num_tvs++;
            percent = config->household.third_tv[residents-1];
            if (percent > 0 && get_random_number (0., 100.) <= percent) num_tvs++;
        }
        for (int t=1; t<=num_tvs; t++) add_tv (t);
    }

    // COMPUTER
    computer = NULL;
    num_computers = 0;
    int num_hh_with_computer = global_count      // number of households with a computer (per household size)
                               * config->household.size_distribution[residents-1]/100.0 
                               * config->household.prevalence.computer[residents-1]/100.0;
    if (number <= limit[residents-1] + num_hh_with_computer)
    {
        add_appliance (&computer);
        num_computers++;
    }
    int num_hh_with_2nd_computer = global_count
                                   * config->household.size_distribution[residents-1]/100.0 
                                   * config->household.second_computer[residents-1]/100.0;
    if (number <= limit[residents-1] + num_hh_with_2nd_computer)
    {
        add_appliance (&computer);
        num_computers++;
    }

    // WASHING MACHINE
    wmachine = NULL;
    num_wmachines = 0;
    percent = config->household.prevalence.wmachine[residents-1];
    if (percent > 0 && get_random_number (0., 100.) <= percent)
    {
        delta_laundry = get_random_number (config->household.min_delta_laundry[residents-1],
                                           config->household.max_delta_laundry[residents-1]);
        add_appliance (&wmachine);
        num_wmachines++;
    }
    laundry = get_random_number (config->household.min_init_laundry, config->household.max_init_laundry);

    // TUMBLE DRYER
    tumble_dryer = NULL;
    num_dryers = 0;
    int num_hh_with_dryer = global_count
                            * config->household.size_distribution[residents-1]/100.0 
                            * config->household.prevalence.dryer[residents-1]/100.0;
    if (number <= limit[residents-1] + num_hh_with_dryer)
    {
        add_appliance (&tumble_dryer);
        num_dryers++;
    }

    // VACUUM CLEANER
    vacuum = NULL;
    num_vacuums = 0;
    percent = config->household.prevalence.vacuum[residents-1];
    if (percent > 0 && get_random_number (0., 100.) <= percent)
    {
        vacuum_interval = get_random_number (config->household.min_vacuum_interval, config->household.max_vacuum_interval);
        add_appliance (&vacuum);
        num_vacuums++;
    }

    // DISHWASHER
    dishwasher = NULL;
    num_dishwashers = 0;  // init number of dishwashers of this household
    int num_hh_with_dishwasher = global_count
                                 * config->household.size_distribution[residents-1]/100.0 
                                 * config->household.prevalence.dishwasher[residents-1]/100.0;
    if (number <= limit[residents-1] + num_hh_with_dishwasher)
    {
        add_appliance (&dishwasher);
        num_dishwashers++;
    }

    // LIGHT
    light = NULL;
    num_lamps = 0;
    percent = config->household.prevalence.light[residents-1];
    if (percent > 0 && get_random_number (0., 100.) <= percent)
    {
        num_lamps = area/config->household.light_factor[residents-1];
        for (int i=0; i<num_lamps; i++)
        {
            add_appliance (&light);
        }
    }

    // CIRCULATION PUMP
    circpump = NULL;
    num_circpumps = 0;
    percent = config->household.prevalence.circpump[residents-1];
    if (percent > 0 && get_random_number (0., 100.) <= percent)
    {
        add_appliance (&circpump);
        num_circpumps++;
    }

    // E_VEHICLE
    e_vehicle = NULL;
    num_evehicles = 0;
    /*
    int num_cars = 0;
    double p_car;
    switch (location->type)
    {
        case URBAN: p_car = config->household.urban_car_percentage; break;
        case RURAL: p_car = config->household.rural_car_percentage; break;
    }
    if (get_random_number (0., 100.) <= p_car)  // the household has at least one car
    {
        // determine the number of cars
        x = get_random_number (0., 100.);
        if (x <= 50.) num_cars = 1;
        else if (residents > 1 && x <= 90.) num_cars = 2;
        else if (residents > 2) num_cars = 3;
        // check for each car whether it is an electric vehicle
        for (int c=0; c<num_cars; c++)
        {
            if (get_random_number (0., 100.) <= config->household.prevalence.e_vehicle[residents-1])
            {
                add_appliance (&e_vehicle);
                num_evehicles++;
            }
        }
    }*/
    if (get_random_number (0., 100.) <= config->household.prevalence.e_vehicle[residents-1])
    {
        add_appliance (&e_vehicle);
        num_evehicles++;
    }

    distance = NULL;
    if (e_vehicle)
    {
        try
        {
            distance = new double [NUM_DESTINATIONS * NUM_DESTINATIONS];
        }
        catch (...)
        {
            fprintf (stderr, "Unable to allocate memory for avg_distance.\n");
            exit (1);
        }
        for (int i=0; i<NUM_DESTINATIONS*NUM_DESTINATIONS; i++) distance[i] = 0.;
        if (location->type == URBAN)
        {
            x = get_random_number (5., 15.);
            distance [HOME*NUM_DESTINATIONS+WORK] = x;
            distance [WORK*NUM_DESTINATIONS+HOME] = x;
            x = get_random_number (1., 5.);
            distance [HOME*NUM_DESTINATIONS+SHOP] = x;
            distance [SHOP*NUM_DESTINATIONS+HOME] = x;
        }
        else
        {
            x = get_random_number (10., 50.);
            distance [HOME*NUM_DESTINATIONS+WORK] = x;
            distance [WORK*NUM_DESTINATIONS+HOME] = x;
            x = get_random_number (5., 10.);
            distance [HOME*NUM_DESTINATIONS+SHOP] = x;
            distance [SHOP*NUM_DESTINATIONS+HOME] = x;
        }
    }
}

Household* Household::get_household_ptr (int id)
{
    // This works in the scalar version only
    return hh+id-1;
}

void Household::allocate_memory (int num_households)
{
    global_count = num_households;
    local_count = num_households/num_processes + (rank < num_households%num_processes);
    count[0] = local_count;
#ifdef PARALLEL
    MPI_Status status;
    int next_first_number;
    if (rank > 0) MPI_Recv (&first_number, 1, MPI_INT, rank-1, 1, MPI_COMM_WORLD, &status);
    next_first_number = first_number + local_count;
    if (rank < num_processes-1) MPI_Send (&next_first_number, 1, MPI_INT, rank+1, 1, MPI_COMM_WORLD);
#endif
    alloc_memory (&hh, local_count, "Household::allocate_memory");
#ifdef PARALLEL
    MPI_Allreduce (MPI_IN_PLACE, count, k_max_residents+1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    MPI_Allreduce (MPI_IN_PLACE, &SolarModule::count, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    MPI_Allreduce (MPI_IN_PLACE, &Battery::count, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
#endif
}


Household::~Household()
{
    AirConditioner *next_ac, *ac = aircon;
    Boiler *next_bo, *bo = boiler;
    CirculationPump *next_cp, *cp = circpump;
    Computer *next_co, *co = computer;
    ElectricStove *next_es, *es = e_stove;
    GasStove *next_gs, *gs = gas_stove;
    Dishwasher *next_dw, *dw = dishwasher;
    E_Vehicle *next_ev, *ev = e_vehicle;
    Freezer *next_fz, *fz = freezer;
    Fridge *next_fr, *fr = fridge;
    Heating *next_ht, *ht = heating;
    Light *next_lt, *lt = light;
    TumbleDryer *next_td, *td = tumble_dryer;
    TV *next_tel, *tel = tv;
    Vacuum *next_vc, *vc = vacuum;
    WashingMachine *next_wm, *wm = wmachine;
    HeatPump *next_hp, *hp = heatpump;

    if (solar_module) delete solar_module;
    if (battery) delete battery;
    for (int i=0; i<num_aircons; i++) {next_ac = ac->next_app; delete ac; ac = next_ac;}
    for (int i=0; i<num_boilers; i++) {next_bo = bo->next_app; delete bo; bo = next_bo;}
    for (int i=0; i<num_circpumps; i++) {next_cp = cp->next_app; delete cp; cp = next_cp;}
    for (int i=0; i<num_computers; i++) {next_co = co->next_app; delete co; co = next_co;}
    for (int i=0; i<num_e_stoves; i++) {next_es = es->next_app; delete es; es = next_es;}
    for (int i=0; i<num_gas_stoves; i++) {next_gs = gs->next_app; delete gs; gs = next_gs;}
    for (int i=0; i<num_dishwashers; i++) {next_dw = dw->next_app; delete dw; dw = next_dw;}
    for (int i=0; i<num_evehicles; i++) {next_ev = ev->next_app; delete ev; ev = next_ev;}
    for (int i=0; i<num_freezers; i++) {next_fz = fz->next_app; delete fz; fz = next_fz;}
    for (int i=0; i<num_fridges; i++) {next_fr = fr->next_app; delete fr; fr = next_fr;}
    for (int i=0; i<num_heatings; i++) {next_ht = ht->next_app; delete ht; ht = next_ht;}
    for (int i=0; i<num_heatpumps; i++) {next_hp = hp->next_app; delete hp; hp = next_hp;}
    for (int i=0; i<num_lamps; i++) {next_lt = lt->next_app; delete lt; lt = next_lt;}
    for (int i=0; i<num_dryers; i++) {next_td = td->next_app; delete td; td = next_td;}
    for (int i=0; i<num_tvs; i++) {next_tel = tel->next_app; delete tel; tel = next_tel;}
    for (int i=0; i<num_vacuums; i++) {next_vc = vc->next_app; delete vc; vc = next_vc;}
    for (int i=0; i<num_wmachines; i++) {next_wm = wm->next_app; delete wm; wm = next_wm;}
    if (config->simulate_heating)
    {
        delete [] a_matrix;
        delete [] b_vector;
        delete [] offsets;
    }
    if (num_evehicles) delete [] distance;
}


void Household::deallocate_memory()
{
    delete [] hh;
}


template <class AP>
void Household::add_appliance (AP **first)
{
    AP *appliance;
    try
    {
        appliance = new AP (this);
    }
    catch (...)
    {
        fprintf (stderr, "Unable to allocate memory for appliance.\n");
        exit (1);
    }
    *first = appliance;
}


void Household::add_tv (int rnk)
{
    class TV *new_tv;
    try
    {
        new_tv = new TV (this, rnk);
    }
    catch (...)
    {
        fprintf (stderr, "Unable to allocate memory for a new TV.\n");
        exit (1);
    }
    tv = new_tv;
}


void Household::add_solar_module()
{
    try
    {
        solar_module = new SolarModule (this);
    }
    catch (...)
    {
        fprintf (stderr, "Unable to allocate memory for solar_module.\n");
        exit (1);
    }
}

void Household::add_battery()
{
    try
    {
        battery = new Battery (this, solar_module);
    }
    catch (...)
    {
        fprintf (stderr, "Unable to allocate memory for battery.\n");
        exit (1);
    }
}

void Household::add_solar_collector()
{
    try
    {
        solar_collector = new SolarCollector (this);
    }
    catch (...)
    {
        fprintf (stderr, "Unable to allocate memory for solar_collector.\n");
        exit (1);
    }
}

void Household::add_heat_storage()
{
    try
    {
        heat_storage = new HeatStorage (this);
    }
    catch (...)
    {
        fprintf (stderr, "Unable to allocate memory for heat_storage.\n");
        exit (1);
    }
}

void Household::add_heat_source()
{
    try
    {
        heat_source = new HeatSource (this);
    }
    catch (...)
    {
        fprintf (stderr, "Unable to allocate memory for heat_storage.\n");
        exit (1);
    }
}

double Household::residents_at_home_duration (double start_time, int num)
{
    int i = 0, first_i;

    while (start_time > at_home[i][0]) i++;
    first_i = i;
    while (at_home[i][0] < k_seconds_per_day && at_home[i][1] >= num) i++;

    if (at_home[i][1] >= num) return at_home[i][0] - start_time;
    else if (i > first_i) return at_home[i-1][0] - start_time;
    else return 0.;
}

int Household::residents_at_home (double daytime)
{
    int i = 0;
    while (daytime > at_home[i][0]) i++;
    return at_home[i][1];
}

double Household::return_time (int tv_rank)
{
    int i = 0;
    while (at_home[i][0] < k_seconds_per_day) i++;
    while (at_home[i][1] >= tv_rank && i>=0) i--;
    if (i==-1) return DBL_MAX;
    else return at_home[i][0];
}


void Household::get_interval (double *begin, double *end, double limit_1, int rnk)
{
    int i = 0;
    *begin = limit_1;
    while (limit_1 > at_home[i][0])
    {
        *begin = at_home[i][0];
        i++;
    }
    while (at_home[i][0] < k_seconds_per_day && at_home[i][1] < rnk)
    {
        *begin = at_home[i][0];
        i++;
    }
    if (at_home[i][1] >= rnk)
    {
        *end = at_home[i][0];
        while (at_home[i][0] < k_seconds_per_day && at_home[i][1] >= rnk)
        {
            i++;
            if (at_home[i][1] >= rnk) *end = at_home[i][0];
        }
    }
    else *begin = -1;
}

double Household::get_random_start_time (double begin, double end)
{
    double rst = get_random_number (begin, end);
    while (residents_at_home (rst) == 0) rst = get_random_number (begin, end);
    return rst;
}


void Household::simulate_forerun()
{
    double time = sim_clock->cur_time;
    for (int i=0; i<local_count; i++) hh[i].simulate_1st_pass (time);
    if (config->control == PEAK_SHAVING) producer->update_maximum_peak();
    real_power_total[0] = 0.;
    reactive_power_total[0] = 0.;
    apparent_power_total[0] = 0.;
    power_hot_water[0] = 0.;
    for (int i=0; i<local_count; i++) hh[i].simulate_2nd_pass (time, false);
    for (int i=0; i<local_count; i++) hh[i].simulate_3rd_pass (time, false);
}

void Household::simulate()
{
    double time = sim_clock->cur_time;
    for (int i=0; i<local_count; i++) hh[i].simulate_1st_pass (time);
    producer->simulate (time);
    for (int i=0; i<local_count; i++) hh[i].simulate_2nd_pass (time, true);
    for (int i=0; i<local_count; i++) hh[i].simulate_3rd_pass (time, true);
}

void Household::simulate_1st_pass (double time)
{
    int limit;
    AirConditioner *ac = aircon;
    Boiler *bo = boiler;
    CirculationPump *cp = circpump;
    Computer *co = computer;
    ElectricStove *es = e_stove;
    GasStove *gs = gas_stove;
    Dishwasher *dw = dishwasher;
    E_Vehicle *ev = e_vehicle;
    Freezer *fz = freezer;
    Fridge *fr = fridge;
    Heating *ht = heating;
    Light *lt = light;
    TumbleDryer *td = tumble_dryer;
    TV *tel = tv;
    Vacuum *vc = vacuum;
    WashingMachine *wm = wmachine;
    HeatPump *hp = heatpump;

    if (sim_clock->midnight)
    {
        if (bedtime > k_seconds_per_day) bedtime_old = bedtime - k_seconds_per_day;
        else bedtime_old = 0.;

        if (occupation != RETIRED)
        {
            if (sim_clock->weekday == SATURDAY || sim_clock->weekday == SUNDAY || sim_clock->holiday)
                wakeup = normal_distributed_random_with_limits (config->household.rnd_wakeup_weekend[0],
                                                                config->household.rnd_wakeup_weekend[1],
                                                                config->household.rnd_wakeup_weekend[2],
                                                                config->household.rnd_wakeup_weekend[3]);
            else
                wakeup = normal_distributed_random_with_limits (config->household.rnd_wakeup[0],
                                                                config->household.rnd_wakeup[1],
                                                                config->household.rnd_wakeup[2],
                                                                config->household.rnd_wakeup[3]);

            if (sim_clock->weekday == FRIDAY || sim_clock->weekday == SATURDAY)
                bedtime = normal_distributed_random_with_limits (config->household.rnd_bedtime_weekend[0],
                                                                 config->household.rnd_bedtime_weekend[1],
                                                                 wakeup,
                                                                 DBL_MAX);
            else
                bedtime = normal_distributed_random_with_limits (config->household.rnd_bedtime[0],
                                                                 config->household.rnd_bedtime[1],
                                                                 wakeup,
                                                                 DBL_MAX);
        }
        else // retired
        {
            wakeup = normal_distributed_random_with_limits (config->household.rnd_wakeup_retired[0],
                                                            config->household.rnd_wakeup_retired[1],
                                                            config->household.rnd_wakeup_retired[2],
                                                            config->household.rnd_wakeup_retired[3]);
            bedtime = normal_distributed_random_with_limits (config->household.rnd_bedtime_retired[0],
                                                             config->household.rnd_bedtime_retired[1],
                                                             wakeup,
                                                             DBL_MAX);
        }
        if (vacation <= 0) laundry += delta_laundry;

        // Make a guess when people are at home and store that in a profile

        if (occupation == RETIRED)
        {
            at_home[0][0] = k_seconds_per_day;
            at_home[0][1] = residents;
        }
        else
        {
            if (sim_clock->weekday == SATURDAY || sim_clock->weekday == SUNDAY || sim_clock->holiday)
            {
                at_home[0][0] = k_seconds_per_day;
                at_home[0][1] = residents;
            }
            else
            {
                if (residents < 3)
                {
                    at_home[0][0] = wakeup + config->household.at_home_param[0];
                    at_home[0][1] = residents;
                    limit = wakeup + get_random_number (config->household.at_home_param[1],
                                                        config->household.at_home_param[2]);
                    limit = (limit > k_seconds_per_day) ? k_seconds_per_day : limit;
                    at_home[1][0] = limit;
                    if (get_random_number (1, 100) <= config->household.at_home_param[3]) at_home[1][1] = 0;
                    else at_home[1][1] = 1;
                    if (limit < k_seconds_per_day)
                    {
                        at_home[2][0] = k_seconds_per_day;
                        at_home[2][1] = residents;
                    }
                }
                else
                {
                    at_home[0][0] = wakeup + config->household.at_home_param[4];
                    at_home[0][1] = residents;
                    limit = wakeup + get_random_number (config->household.at_home_param[5], config->household.at_home_param[6]);
                    limit = (limit > k_seconds_per_day) ? k_seconds_per_day : limit;
                    at_home[1][0] = limit;
                    at_home[1][1] = get_random_number (1, residents-2);
                    if (limit < k_seconds_per_day)
                    {
                        at_home[2][0] = k_seconds_per_day;
                        at_home[2][1] = residents;
                    }
                }
            }
        }
        shopping_done = false;
    }
    // Calculate the amount of energy needed for space heating and cooling according to ISO 52016-1

    if (config->simulate_heating && (int)sim_clock->daytime % 3600 == 0)  // every hour
    {
        space_heating_and_cooling_demand();
    }

    // Calculate the amount of heat for domestic hot water (dhw)

    if (vacation <= 0)
    {
        if (sim_clock->midnight)  // Schedule hygiene activities at the beginning of each day
        {
            // Init the probability array depending on sleep times and erase the schedule
            probability_sum = 0.;
            double *table;
            switch (sim_clock->weekday)
            {
                case SATURDAY: table = table_DHW_saturday; break;
                case SUNDAY:   table = table_DHW_sunday; break;
                default:       table = table_DHW_weekday; break;
            }
            if (sim_clock->holiday) table = table_DHW_sunday;
            for (int i=0; i<1440; i++)
            {
                if (i*60. < bedtime_old || (i*60. >= wakeup && i*60. < bedtime))
                {
                    probability[i] = probability_sum + table[i];
                    probability_sum += table[i];
                }
                else probability[i] = -1.;
                dhw_schedule[i] = DO_NOTHING;
            }
            dhw_schedule_pos = 0;

            // Wash your hands
            int num_handw = 0;
            for (int r=0; r<residents; r++) num_handw += get_random_number (2, 4);
            for (int h=0; h<num_handw; h++) schedule (HANDWASH, -1);

            // Shower and bath
            int num_shower = 0;
            int num_bath = 0;
            for (int r=0; r<residents; r++)
            {
                if (get_random_number (1, 100) < 50) num_shower++;
                else if (get_random_number (1, 100) < 20) num_bath++;
            }
            for (int h=0; h<num_shower; h++) schedule (SHOWER, -1);
            for (int h=0; h<num_bath; h++) schedule (BATH, -1);
        }

        while (dhw_schedule_pos*60 <= sim_clock->daytime)
        {
            double mass_flow;
            double volume;
            double duration;
            double temp;
            double heat_demand;
            double min_temp = config->household.min_temperature_DHW;
            double max_temp = config->household.max_temperature_DHW;
            switch (dhw_schedule[dhw_schedule_pos])
            {
                case HANDWASH:
                    mass_flow = get_random_number (3.,8.)/60.;                              // liter per second
                    volume = get_random_number (config->household.min_volume_handwash,
                                                config->household.max_volume_handwash);     // liter total
                    duration = volume/mass_flow;                                            // in seconds
                    temp = get_random_number (min_temp, max_temp);    // hot water temperature at tapping point [°C]
                    heat_demand = k_heat_capacity_H2O * mass_flow * (temp - location->temp_H2O_cold_0);
                    add_timer (duration, heat_demand);
                    break;
                case SHOWER:
                    mass_flow = get_random_number (9.,11.)/60.;
                    volume = get_random_number (config->household.min_volume_shower,
                                                config->household.max_volume_shower);
                    duration = volume/mass_flow;
                    temp = get_random_number (min_temp, max_temp);
                    heat_demand = k_heat_capacity_H2O * mass_flow * (temp - location->temp_H2O_cold_0);
                    add_timer (duration, heat_demand);
                    break;
                case BATH:
                    mass_flow = get_random_number (9.,11.)/60.;
                    volume = get_random_number (config->household.min_volume_bath,
                                                config->household.max_volume_bath);
                    duration = volume/mass_flow;
                    temp = get_random_number (min_temp, max_temp);
                    heat_demand = k_heat_capacity_H2O * mass_flow * (temp - location->temp_H2O_cold_0);
                    add_timer (duration, heat_demand);
                    break;
                case COOK:
                    break;
                default:
                    break;
            }
            dhw_schedule_pos++;
        }
        Timer *t = first_timer;
        Timer *prev = NULL;
        double sum_heat = 0.;
        while (t)
        {
            if (t->duration < config->timestep_size)
            {
                sum_heat += t->heat_demand * t->duration/config->timestep_size;
            }
            else sum_heat += t->heat_demand;

            t->duration -= config->timestep_size;
            if (t->duration <= 0.) // time's up. delete the timer
            {
                if (t==first_timer) first_timer = t->next;
                else prev->next = t->next;
                Timer *del = t;
                t = t->next;
                delete del;
            }
            else
            {
                prev = t;
                t = t->next;
            }
        }
        heat_demand_DHW = location->seasonal_factor * (heat_loss_DHW + sum_heat);
    }
    else heat_demand_DHW = 0.;  // household is on vacation

    // transfer to output variables
    power_hot_water[0] += heat_demand_DHW;
    power_hot_water[residents] += heat_demand_DHW;

    // Simulate the electric appliances
    power.real = 0.;
    power.reactive = 0.;
    heat_loss_app = 0.;
    if (vacation <= 0)  // if not on vacation
    {
        for (int i=0; i<num_aircons; i++) {ac->simulate(); ac=ac->next_app;}
        for (int i=0; i<num_circpumps; i++) {cp->simulate(); cp=cp->next_app;}
        for (int i=0; i<num_computers; i++) {co->simulate(); co=co->next_app;}
        for (int i=0; i<num_e_stoves; i++) {es->simulate(); es=es->next_app;}
        for (int i=0; i<num_gas_stoves; i++) {gs->simulate(); gs=gs->next_app;}
        for (int i=0; i<num_dishwashers; i++) {dw->simulate (time); dw=dw->next_app;}
        for (int i=0; i<num_evehicles; i++) { ev->simulate(); ev = ev->next_app; }
        for (int i=0; i<num_fridges; i++) {fr->simulate (time); fr=fr->next_app;}
        for (int i=0; i<num_lamps; i++) {lt->simulate(); lt=lt->next_app;}
        for (int i=0; i<num_dryers; i++) {td->simulate (time); td=td->next_app;}
        for (int i=0; i<num_tvs; i++) {tel->simulate(); tel=tel->next_app;}
        for (int i=0; i<num_vacuums; i++) {vc->simulate(); vc=vc->next_app;}
        for (int i=0; i<num_wmachines; i++) {wm->simulate (time); wm=wm->next_app;}
        for (int i=0; i<num_boilers; i++) {bo->simulate(); bo=bo->next_app;}
    }
    for (int i=0; i<num_freezers; i++) {fz->simulate (time); fz=fz->next_app;}

    // Simulate heat sources
    switch (heat_source_type)
    {
        case OIL:
        case GAS:
        case DISTRICT:
            heat_source->simulate();
            break;
        case HEAT_PUMP:
            for (int i=0; i<num_heatpumps; i++) {hp->simulate(); hp=hp->next_app;}
            // auxiliary electrical heating:
            for (int i=0; i<num_heatings; i++) {ht->simulate (); ht=ht->next_app;}
            break;
        case SOLAR_COLLECTOR:
            solar_collector->simulate();
            for (int i=0; i<num_heatpumps; i++) {hp->simulate(); hp=hp->next_app;}
            heat_storage->simulate();
            break;
        default:;
    }
}


void Household::simulate_2nd_pass (double time, bool main_simulation)
{
    static const double factor = config->timestep_size/3600.;
    double daytime = sim_clock->daytime;
    double delta, power_solar, power_discharging;

    if (almost_equal (daytime, sim_clock->sunrise) && main_simulation && solar_module && battery)
    {
        double production_forecast = 0.;
        double consumption_forecast = consumption_prev_day;
        switch (config->battery_charging.production_forecast_method)
        {
            case 1:
                production_forecast = solar_module->production_forecast();  // perfect solar forecast based on PVGIS
                feed_to_grid = production_forecast - consumption_forecast - (battery->capacity - battery->charge);
                break;
            case 2:
                production_forecast = solar_module->production_prev_day;    // naive solar forecast
                feed_to_grid = production_forecast - consumption_forecast - (battery->capacity - battery->charge);
                break;
            case 3:
                production_forecast = solar_module->production_forecast();  // GEFS reforecast
                feed_to_grid = production_forecast - consumption_forecast - (battery->capacity - battery->charge);
                break;
            default:
                break;
        }
        consumption_prev_day = 0.;
    }
    if (solar_module)
    {
        solar_module->simulate();
        power_solar = solar_module->power.real;
    }
    else power_solar = 0.;

    if (battery && batteries_active)
    {
        battery->simulate (time, power.real, power_solar, feed_to_grid);
        power_discharging = battery->power_discharging;
        consumption_battery += power_discharging * factor;
    }
    else power_discharging = 0.;

    // If the household consumption (the current power) is higher than the solar
    // production plus the power drawn from the battery, then we need to draw
    // power from the grid:

    delta = power.real - (power_solar + power_discharging);

    if (delta > 0.)
    {
        power_from_grid = delta;
        power_from_grid_total += delta;
    }
    else power_from_grid = 0.;
}


void Household::simulate_3rd_pass (double time, bool main_simulation)
{
    static const double factor = config->timestep_size/3600.;
    double daytime = sim_clock->daytime;
    double delta, above, costs, inc = 0.;
    double power_solar, power_charging;

    if (solar_module) power_solar = solar_module->power.real; else power_solar = 0.;
    if (battery && batteries_active) power_charging = battery->power_charging; else power_charging = 0.;

    // If the solar production is higher than the houshold consumption plus the
    // power used for charging the battery, then we can feed power into the grid:

    delta = power_solar - (power.real + power_charging);

    if (delta > 0.)
    {
        // 'above' is the part of 'delta' which is above the feed in limit
        // When using shared batteries, we try to store 'above' in another household's battery
        above = delta - config->battery_charging.feed_in_limit * solar_module->nominal_power;
        if (above > 0.000000001)
        {
            if (config->battery_charging.shared && main_simulation)
            {
                delta -= above;
                Household::shared_battery_charging (&above);
                delta += above;
            }
            power_above_limit_total += above;
            power_above_limit_total_integral += above;
        }
        power_to_grid = delta;
        power_to_grid_total += delta;
        power_to_grid_total_integral += delta;
        feed_to_grid -= power_to_grid * factor;
    }
    else power_to_grid = 0.;

    // production_used_total is the part of solar production that is used to
    // power the appliances and to load the battery, integrated over all households.
    // In other words, it is the solar production minus the power fed into the grid.

    production_used_total += power_solar - power_to_grid;

    // consumption_solar is the part of solar production that is used to
    // power the appliances and to load the battery, integrated over time

    consumption_solar += (power_solar - power_to_grid) * factor;

    if (solar_module)
    {
        costs = producer->price (GRID, time) * power_from_grid * factor;
        with_solar_costs[0] += costs;
        with_solar_costs[residents] += costs;
        inc = producer->price (SOLAR, time) * power_to_grid * factor;
        income_total[0] -= inc;
        income_total[residents] -= inc;
        if (almost_equal (daytime, sim_clock->sunrise))
        {
            sr_ss_consumption = 0.;
        }
        if (daytime >= sim_clock->sunrise && daytime <= sim_clock->sunset)
        {
            sr_ss_consumption += power.real;
        }
    }
    else  // no solarmodule installed
    {
        costs = producer->price (GRID, time) * power.real * factor;
        without_solar_costs[0] += costs;
        without_solar_costs[residents] += costs;
    }
    consumption += power.real * factor;
    if (sim_clock->daytime > sim_clock->sunrise && sim_clock->daytime < sim_clock->sunset)
    {
        consumption_prev_day += power.real * factor;
    }
    costs_year += costs;
    income_year -= inc;

    if (time - last_update_mp > 15*60)
    {
        if (power.real > max_power[0])
        {
            max_power[2] = max_power[1];
            max_power[1] = max_power[0];
            max_power[0] = power.real;
            if (solar_module)
            {
                sol_power_at_mp[2] = sol_power_at_mp[1];
                sol_power_at_mp[1] = sol_power_at_mp[0];
                sol_power_at_mp[0] = solar_module->power.real;
            }
            timestamp_at_mp[2] = timestamp_at_mp[1];
            timestamp_at_mp[1] = timestamp_at_mp[0];
            timestamp_at_mp[0] = time;
            last_update_mp = time;
        }
        else if (power.real > max_power[1])
        {
            max_power[2] = max_power[1];
            max_power[1] = power.real;
            if (solar_module)
            {
                sol_power_at_mp[2] = sol_power_at_mp[1];
                sol_power_at_mp[1] = solar_module->power.real;
            }
            timestamp_at_mp[2] = timestamp_at_mp[1];
            timestamp_at_mp[1] = time;
            last_update_mp = time;
        }
        else if (power.real > max_power[2])
        {
            max_power[2] = power.real;
            if (solar_module)
            {
                sol_power_at_mp[2] = solar_module->power.real;
            }
            timestamp_at_mp[2] = time;
            timestamp_at_mp[2] = time;
            last_update_mp = time;
        }
    }
    if (solar_module && (time - last_update_mpfg > 15*60))
    {
        if (power_from_grid > max_power_from_grid[0])
        {
            max_power_from_grid[2] = max_power_from_grid[1];
            max_power_from_grid[1] = max_power_from_grid[0];
            max_power_from_grid[0] = power_from_grid;
            sol_power_at_mpfg[2] = sol_power_at_mpfg[1];
            sol_power_at_mpfg[1] = sol_power_at_mpfg[0];
            sol_power_at_mpfg[0] = solar_module->power.real;
            power_at_mpfg[2] = power_at_mpfg[1];
            power_at_mpfg[1] = power_at_mpfg[0];
            power_at_mpfg[0] = power.real;
            timestamp_at_mpfg[2] = timestamp_at_mpfg[1];
            timestamp_at_mpfg[1] = timestamp_at_mpfg[0];
            timestamp_at_mpfg[0] = time;
            last_update_mpfg = time;
        }
        else if (power_from_grid > max_power_from_grid[1])
        {
            max_power_from_grid[2] = max_power_from_grid[1];
            max_power_from_grid[1] = power_from_grid;
            sol_power_at_mpfg[2] = sol_power_at_mpfg[1];
            sol_power_at_mpfg[1] = solar_module->power.real;
            power_at_mpfg[2] = power_at_mpfg[1];
            power_at_mpfg[1] = power.real;
            timestamp_at_mpfg[2] = timestamp_at_mpfg[1];
            timestamp_at_mpfg[1] = time;
            last_update_mpfg = time;
        }
        else if (power_from_grid > max_power_from_grid[2])
        {
            max_power_from_grid[2] = power_from_grid;
            sol_power_at_mpfg[2] = solar_module->power.real;
            power_at_mpfg[2] = power.real;
            timestamp_at_mpfg[2] = time;
            last_update_mpfg = time;
        }
    }
}


void Household::shared_battery_charging (double *above)
{
    double dist, min_dist = DBL_MAX;
    double cp, max_cp = 0.;
    int min_i = -1, max_i = -1;

    for (int i=0; i<local_count; i++)
    {
        if (hh[i].battery)
        {
            cp = hh[i].battery->charging_power_limit();
            dist = cp - *above;
            if (dist >= 0)
            {
                if (dist < min_dist)
                {
                    min_dist = dist;
                    min_i = i;
                }
            }
            else
            {
                if (cp > max_cp)
                {
                    max_cp = cp;
                    max_i = i;
                }
            }
        }
    }
    if (min_i != -1)
    {
        hh[min_i].battery->charge_from_neighbour (above);
    }
    else if (max_i != -1)
    {
        hh[max_i].battery->charge_from_neighbour (above);
    }
}


void Household::reset_integrals()
{
    power_to_grid_total_integral = 0.;
    power_above_limit_total_integral = 0.;
    for (int i=0; i<NUM_HEAT_SOURCE_TYPES; i++)
    {
        consumption_SH_total_integral[i] = 0.;
        consumption_DHW_total_integral[i] = 0.;
    }
    for (int i=0; i<local_count; i++)
    {
        hh[i].consumption = 0.;
        hh[i].consumption_SH = 0.;
        hh[i].consumption_DHW = 0.;
        hh[i].consumption_solar = 0.;
        hh[i].consumption_battery = 0.;
        hh[i].consumption_cooking = 0.;
        hh[i].costs_year = 0.;
        hh[i].income_year = 0.;
        if (hh[i].solar_module) hh[i].solar_module->reset_production();
        if (hh[i].solar_collector) hh[i].solar_collector->heat_to_storage_integral = 0.;
        if (hh[i].heat_storage)
        {
            hh[i].heat_storage->power_integral_SH = 0.;
            hh[i].heat_storage->power_integral_DHW = 0.;
        }
    }
    for (int i=0; i<=k_max_residents; i++)
    {
        with_solar_costs[i] = 0.;
        without_solar_costs[i] = 0.;
        income_total[i] = 0.;
    }
}


void Household::calc_consumption()
{
    double consumption;
    int res;

    consumption_cooking_total = 0.;
    for (res=0; res<=k_max_residents; res++)
    {
        consumption_min[res] = DBL_MAX;
        consumption_max[res] = 0.;
        consumption_sum[res] = 0.;
        consumption_square[res] = 0.;
    }
    for (int i=0; i<local_count; i++)
    {
        consumption_cooking_total += hh[i].consumption_cooking;
        res = hh[i].residents;
        consumption = hh[i].consumption;
        consumption_sum[0] += consumption;
        consumption_square[0] += consumption*consumption;
        if (consumption < consumption_min[0]) consumption_min[0] = consumption;
        if (consumption > consumption_max[0]) consumption_max[0] = consumption;
        consumption_sum[res] += consumption;
        consumption_square[res] += consumption*consumption;
        if (consumption < consumption_min[res]) consumption_min[res] = consumption;
        if (consumption > consumption_max[res]) consumption_max[res] = consumption;
    }
#ifdef PARALLEL
    MPI_Allreduce (MPI_IN_PLACE, consumption_min, k_max_residents+1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
    MPI_Allreduce (MPI_IN_PLACE, consumption_max, k_max_residents+1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
    MPI_Allreduce (MPI_IN_PLACE, consumption_sum, k_max_residents+1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    MPI_Allreduce (MPI_IN_PLACE, consumption_square, k_max_residents+1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    MPI_Allreduce (MPI_IN_PLACE, &consumption_cooking_total, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
#endif
}


double Household::std_deviation (int res)
{
    double mean;
    if (count[res])
    {
        mean = consumption_sum[res]/count[res];
        return sqrt (consumption_square[res]/count[res] - mean*mean);
    }
    else return 0.;
}


double Household::median (int res)
{
    double *values = NULL;
    double median = 0;
    int j;
#ifdef PARALLEL
    int num;
    MPI_Status status;
#endif

    if (count[res])
    {
        alloc_memory (&values, count[res], "Household::median");
        j = 0;
        for (int i=0; i<local_count; i++)
        {
            if (hh[i].residents == res || res == 0)
            {
                values[j] = hh[i].consumption;
                j++;
            }
        }
#ifdef PARALLEL
        if (rank == 0)
        {
            for (int i=1; i<num_processes; i++)
            {
                MPI_Recv (&num, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &status);
                MPI_Recv (values+j, num, MPI_DOUBLE, i, 0, MPI_COMM_WORLD, &status);
                j += num;
            }
        }
        else
        {
            MPI_Send (&j, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
            MPI_Send (values, j, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD);
        }
        MPI_Barrier (MPI_COMM_WORLD);
#endif
        if (rank == 0)
        {
            qsort (values, (size_t)count[res], sizeof(double), (int (*)(const void *, const void *))&compare_double);
            if (count[res]%2 == 1) median = values[count[res]/2];
            else median = (values[count[res]/2] + values[count[res]/2-1]) / 2.;
        }
        delete [] values;
        return median;
    }
    else return 0.;
}


void Household::print_distribution (FILE *fp, int res)
{
    int *dist, num_categories, index;
    double delta;

    num_categories = 5;
    if (count[res] > 500)  num_categories = 10;
    if (count[res] > 1000) num_categories = 20;
    if (count[res] > 5000) num_categories = 25;
    if (count[res] > 10000) num_categories = 30;
    if (count[res] > 50000) num_categories = 40;
    if (count[res] > 100000) num_categories = 100;

    delta = (consumption_max[res] - consumption_min[res]) / num_categories;
    if (fabs (delta) < k_float_compare_eps) num_categories = 1;

    dist = new int [num_categories];
    for (int j=0; j<num_categories; j++) dist[j] = 0;

    for (int i=0; i<local_count; i++)
    {
        if (hh[i].residents == res)
        {
            if (delta > 0)
            {
                index = (hh[i].consumption - consumption_min[res]) / delta;
                if (index == num_categories) index--;
            }
            else index = 0;
            dist[index]++;
        }
    }
#ifdef PARALLEL
    if (rank == 0)
        MPI_Reduce (MPI_IN_PLACE, dist, num_categories, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    else
        MPI_Reduce (dist, dist, num_categories, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
#endif
    if (rank == 0)
    {
        for (int j=0; j<num_categories; j++)
        {
            fprintf (fp, "%lf %d\n", consumption_min[res] + j*delta + delta/2., dist[j]);
        }
    }
    delete [] dist;
}


void Household::print_heat_consumption (int year)
{
    FILE *fp = NULL;
    char file_name[k_max_path];
    char labels[9][16] = {"A+ (0-30)", "A (30-50)", "B (50-75)", "C (75-100)", "D (100-130)", "E (130-160)", "F (160-200)", "G (200-250)", "H (> 250)"};
    double limit[9] = {30., 50., 75., 100., 130., 160., 200., 250., DBL_MAX};
    double avg_consumption_eec[9];
    double min_consumption_eec[9];
    double max_consumption_eec[9];
    double cons_per_m2;
    int count_eec[9], eff_energy_class;

    if (rank == 0)
    {
        snprintf (file_name, sizeof(file_name), "heat_consumption.%d", year);
        open_file (&fp, file_name, "w");
    }
    for (int i=0; i<9; i++)
    {
        min_consumption_eec[i] = DBL_MAX;
        avg_consumption_eec[i] = 0.;
        max_consumption_eec[i] = 0.;
        count_eec[i] = 0;
    }
    for (int i=0; i<local_count; i++)
    {
        cons_per_m2 = hh[i].consumption_SH/hh[i].area;
        eff_energy_class = 0; // effective energy class based on actual consumption
        while (cons_per_m2 > limit[eff_energy_class]) eff_energy_class++;
        avg_consumption_eec[eff_energy_class] += cons_per_m2;
        count_eec[eff_energy_class]++;
        if (cons_per_m2 < min_consumption_eec[eff_energy_class])
        {
            min_consumption_eec[eff_energy_class] = cons_per_m2;
        }
        if (cons_per_m2 > max_consumption_eec[eff_energy_class])
        {
            max_consumption_eec[eff_energy_class] = cons_per_m2;
        }
    }
#ifdef PARALLEL
    if (rank == 0)
    {
        MPI_Reduce (MPI_IN_PLACE, avg_consumption_eec, 9, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce (MPI_IN_PLACE, min_consumption_eec, 9, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
        MPI_Reduce (MPI_IN_PLACE, max_consumption_eec, 9, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        MPI_Reduce (MPI_IN_PLACE, count_eec, 9, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    }
    else
    {
        MPI_Reduce (avg_consumption_eec, avg_consumption_eec, 9, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce (min_consumption_eec, min_consumption_eec, 9, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
        MPI_Reduce (max_consumption_eec, max_consumption_eec, 9, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        MPI_Reduce (count_eec, count_eec, 9, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    }
#endif
    if (rank == 0)
    {
        int sum = 0;
        for (int i=0; i<9; i++) sum += count_eec[i];
        fprintf (fp, "SPACE HEATING\n\n");
        fprintf (fp, "1) Consumption/year/m2 for all households (%d)\n\n", sum);
        fprintf (fp, "Energy Class |   Number   |    Min.    |    Avg.    |    Max.\n");
        fprintf (fp, "-----------------------------------------------------------------\n");
        for (int i=0; i<9; i++)
        {
            fprintf (fp, "%-12s | %10d |", labels[i], count_eec[i]);
            if (count_eec[i])
            {
                fprintf (fp, " %10.1lf | %10.1lf | %10.1lf\n",
                         min_consumption_eec[i], avg_consumption_eec[i]/count_eec[i], max_consumption_eec[i]);
            }
            else fprintf (fp, "            |            |\n");
        }
        fprintf (fp, "\n\n");
    }
    for (int r=1; r<=k_max_residents; r++)
    {
        for (int i=0; i<9; i++)
        {
            min_consumption_eec[i] = DBL_MAX;
            avg_consumption_eec[i] = 0.;
            max_consumption_eec[i] = 0.;
            count_eec[i] = 0;
        }
        for (int i=0; i<local_count; i++)
        {
            if (hh[i].residents == r)
            {
                cons_per_m2 = hh[i].consumption_SH/hh[i].area;
                eff_energy_class = 0; // effective energy class based on actual consumption
                while (cons_per_m2 > limit[eff_energy_class]) eff_energy_class++;
                avg_consumption_eec[eff_energy_class] += cons_per_m2;
                count_eec[eff_energy_class]++;
                if (cons_per_m2 < min_consumption_eec[eff_energy_class])
                {
                    min_consumption_eec[eff_energy_class] = cons_per_m2;
                }
                if (cons_per_m2 > max_consumption_eec[eff_energy_class])
                {
                    max_consumption_eec[eff_energy_class] = cons_per_m2;
                }
            }
        }
#ifdef PARALLEL
        if (rank == 0)
        {
            MPI_Reduce (MPI_IN_PLACE, avg_consumption_eec, 9, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
            MPI_Reduce (MPI_IN_PLACE, min_consumption_eec, 9, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
            MPI_Reduce (MPI_IN_PLACE, max_consumption_eec, 9, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
            MPI_Reduce (MPI_IN_PLACE, count_eec, 9, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
        }
        else
        {
            MPI_Reduce (avg_consumption_eec, avg_consumption_eec, 9, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
            MPI_Reduce (min_consumption_eec, min_consumption_eec, 9, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
            MPI_Reduce (max_consumption_eec, max_consumption_eec, 9, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
            MPI_Reduce (count_eec, count_eec, 9, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
        }
#endif
        if (rank == 0)
        {
            int sum = 0;
            for (int i=0; i<9; i++) sum += count_eec[i];
            fprintf (fp, "%d) Consumption/year/m2 for %d-person households (%d)\n\n", r+1, r, sum);
            fprintf (fp, "Energy Class |   Number   |    Min.    |    Avg.    |    Max.\n");
            fprintf (fp, "-----------------------------------------------------------------\n");
            for (int i=0; i<9; i++)
            {
                fprintf (fp, "%-12s | %10d |", labels[i], count_eec[i]);
                if (count_eec[i])
                {
                    fprintf (fp, " %10.1lf | %10.1lf | %10.1lf\n",
                             min_consumption_eec[i], avg_consumption_eec[i]/count_eec[i], max_consumption_eec[i]);
                }
                else fprintf (fp, "            |            |\n");
            }
            fprintf (fp, "\n\n");
        }
    }
    if (rank == 0) fclose (fp);
}

/*
void Household::print_debug_heat_consumption (int year)
{
    FILE *fp = NULL;
    char file_name[k_max_path];
#ifdef PARALLEL
    MPI_Status status;
    char finished;
#endif
    snprintf (file_name, sizeof(file_name), "debug_heat_consumption.%d", year);
    if (rank == 0) open_file (&fp, file_name, "w");
#ifdef PARALLEL
    else
    {
        MPI_Recv (&finished, 1, MPI_CHAR, rank-1, 1, MPI_COMM_WORLD, &status);
        open_file (&fp, file_name, "a");
    }
#endif
    for (int i=0; i<local_count; i++)
    {
        fprintf (fp, "%d %lf\n", hh[i].energy_class, hh[i].consumption_SH/hh[i].area);
    }
    fclose (fp);
#ifdef PARALLEL
    if (rank < num_processes-1) MPI_Send (&finished, 1, MPI_CHAR, rank+1, 1, MPI_COMM_WORLD);
#endif
}
*/

/*
void Household::print_debug_info (int num)
{
    static FILE *fp = NULL;
    if (!fp) fp = fopen ("debuginfo", "w");
    fprintf (fp, "%lf\n", hh[num-1].battery->charge);
}
*/

void Household::print_costs (int year)
{
    FILE *fp = NULL;
    char file_name[k_max_path];
    double total_consumption[k_max_residents+1];
    int count_with[k_max_residents+1];
    int count_without[k_max_residents+1];
    int res;

    if (rank == 0)
    {
        snprintf (file_name, sizeof(file_name), "costs.%d", year);
        open_file (&fp, file_name, "w");
    }

    // Households WITHOUT a photovoltaic installation:

    for (int i=0; i<=k_max_residents; i++)
    {
        total_consumption[i] = 0.;
        count_without[i] = 0;
    }
    for (int i=0; i<local_count; i++)
    {
        if (hh[i].solar_module == NULL)
        {
            res = hh[i].residents;
            total_consumption[0] += hh[i].consumption;
            total_consumption[res] += hh[i].consumption;
            count_without[0]++;
            count_without[res]++;
        }
    }
#ifdef PARALLEL
    if (rank == 0)
    {
        MPI_Reduce (MPI_IN_PLACE, total_consumption, k_max_residents+1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce (MPI_IN_PLACE, count_without, k_max_residents+1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce (MPI_IN_PLACE, without_solar_costs, k_max_residents+1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    }
    else
    {
        MPI_Reduce (total_consumption, total_consumption, k_max_residents+1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce (count_without, count_without, k_max_residents+1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce (without_solar_costs, without_solar_costs, k_max_residents+1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    }
#endif
    if (rank == 0)
    {
        fprintf (fp, "\nHouseholds without a photovoltaic installation:\n\n");
        fprintf (fp, "Cat.   Number     Mean Consumption      Mean Costs\n");
        fprintf (fp, "--------------------------------------------------\n");
        for (int cat=1; cat<=k_max_residents; cat++)
        {
            if (count_without[cat] > 0)
            {
                fprintf (fp, "%4d %8d %16.3lf kWh %13.3lf €\n", cat, count_without[cat],
                         total_consumption[cat]/count_without[cat],
                         without_solar_costs[cat]/count_without[cat]);
            }
            else fprintf (fp, "%4d %8d\n", cat, 0);
            fprintf (fp, "--------------------------------------------------\n");
        }
        if (count_without[0] > 0)
        {
            fprintf (fp, " All %8d %16.3lf kWh %13.3lf €\n\n\n",
                     count_without[0], total_consumption[0]/count_without[0],
                     without_solar_costs[0]/count_without[0]);
        }
        else fprintf (fp, " All %8d\n", 0);
    }

    // Households WITH a photovoltaic installation

    for (int i=0; i<=k_max_residents; i++)
    {
        total_consumption[i] = 0.;
        count_with[i] = 0;
    }
    for (int i=0; i<local_count; i++)
    {
        if (hh[i].solar_module)
        {
            res = hh[i].residents;
            total_consumption[0] += hh[i].consumption;
            total_consumption[res] += hh[i].consumption;
            count_with[0]++;
            count_with[res]++;
        }
    }
#ifdef PARALLEL
    if (rank == 0)
    {
        MPI_Reduce (MPI_IN_PLACE, total_consumption, k_max_residents+1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce (MPI_IN_PLACE, count_with, k_max_residents+1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce (MPI_IN_PLACE, with_solar_costs, k_max_residents+1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce (MPI_IN_PLACE, income_total, k_max_residents+1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    }
    else
    {
        MPI_Reduce (total_consumption, total_consumption, k_max_residents+1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce (count_with, count_with, k_max_residents+1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce (with_solar_costs, with_solar_costs, k_max_residents+1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce (income_total, income_total, k_max_residents+1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    }
#endif
    if (rank == 0)
    {
        fprintf (fp, "Households with a photovoltaic installation:\n\n");
        fprintf (fp, "Cat.   Number     Mean Consumption      Mean Costs     Mean Income         Balance\n");
        fprintf (fp, "----------------------------------------------------------------------------------\n");
        for (int cat=1; cat<=k_max_residents; cat++)
        {
            if (count_with[cat] > 0)
            {
                fprintf (fp, "%4d %8d %16.3lf kWh %13.3lf € %13.3lf € %13.3lf €\n", cat, count_with[cat],
                         total_consumption[cat]/count_with[cat],
                         with_solar_costs[cat]/count_with[cat], income_total[cat]/count_with[cat],
                         with_solar_costs[cat]/count_with[cat] + income_total[cat]/count_with[cat]);
            }
            else fprintf (fp, "%4d %8d\n", cat, 0);
            fprintf (fp, "----------------------------------------------------------------------------------\n");
        }
        if (count_with[0] > 0)
        {
            fprintf (fp, " All %8d %16.3lf kWh %13.3lf € %13.3lf € %13.3lf €\n\n\n",
                     count_with[0], total_consumption[0]/count_with[0],
                     with_solar_costs[0]/count_with[0], income_total[0]/count_with[0],
                     with_solar_costs[0]/count_with[0] + income_total[0]/count_with[0]);
        }
        else fprintf (fp, " All %8d\n", 0);
        fclose (fp);
    }
}


void Household::print (FILE *fp1[], FILE *fp2[])
{
    int res, hs_SH, hs_DHW, hs_cooking;
    for (int i=0; i<local_count; i++)
    {
        res = hh[i].residents;
        hs_SH = hh[i].heat_source_type;
        if (hh[i].boiler) hs_DHW = NUM_HEAT_SOURCE_TYPES; else hs_DHW = hh[i].heat_source_type;
        if (hh[i].e_stove) hs_cooking = 1;
        else if (hh[i].gas_stove) hs_cooking = 2;
        else hs_cooking = 0;
        fprintf (fp1[res], "%d %d %.3lf %.3lf %.3lf %d %.3lf %.3lf %.3lf %d %d %d %.3lf %.3lf %.3lf %d",
                 hh[i].number,
                 res,
                 hh[i].consumption,
                 hh[i].consumption_solar,
                 hh[i].consumption_battery,
                 hh[i].num_tvs,
                 hh[i].costs_year,
                 hh[i].income_year,
                 hh[i].area,
                 hs_SH,
                 hs_DHW,
                 hs_cooking,
                 hh[i].consumption_SH,
                 hh[i].consumption_DHW,
                 hh[i].consumption_cooking,
                 hh[i].energy_class);
        if (hh[i].solar_module) hh[i].solar_module->print (fp1[res]); else fprintf (fp1[res], " 0.0 0.0");
        if (hh[i].battery) hh[i].battery->print (fp1[res]); else fprintf (fp1[res], " 0.0 0.0 0.0");
        fprintf (fp1[res], "\n");

        AirConditioner::print_EEI (fp2[res], hh[i].aircon, hh[i].num_aircons);
        Boiler::print_EEI (fp2[res], hh[i].boiler, hh[i].num_boilers);
        CirculationPump::print_EEI (fp2[res], hh[i].circpump, hh[i].num_circpumps);
        Computer::print_EEI (fp2[res], hh[i].computer, hh[i].num_computers);
        ElectricStove::print_EEI (fp2[res], hh[i].e_stove, hh[i].num_e_stoves);
        Dishwasher::print_EEI (fp2[res], hh[i].dishwasher, hh[i].num_dishwashers);
        E_Vehicle::print_EEI (fp2[res], hh[i].e_vehicle, hh[i].num_evehicles);
        Freezer::print_EEI (fp2[res], hh[i].freezer, hh[i].num_freezers);
        Fridge::print_EEI (fp2[res], hh[i].fridge, hh[i].num_fridges);
        Heating::print_EEI (fp2[res], hh[i].heating, hh[i].num_heatings);
        Light::print_EEI (fp2[res], hh[i].light, hh[i].num_lamps);
        TumbleDryer::print_EEI (fp2[res], hh[i].tumble_dryer, hh[i].num_dryers);
        TV::print_EEI (fp2[res], hh[i].tv, hh[i].num_tvs);
        Vacuum::print_EEI (fp2[res], hh[i].vacuum, hh[i].num_vacuums);
        WashingMachine::print_EEI (fp2[res], hh[i].wmachine, hh[i].num_wmachines);
        HeatPump::print_EEI (fp2[res], hh[i].heatpump, hh[i].num_heatpumps);
        fprintf (fp2[res], "\n");
    }
}

void Household::print_max (FILE *fp1[], FILE *fp2[])
{
    FILE *fp;
    int res;
    for (int i=0; i<local_count; i++)
    {
        res = hh[i].residents;
        if (hh[i].solar_module) fp = fp2[res]; else fp = fp1[res];
        fprintf (fp, "%d ", hh[i].number);
        if (hh[i].solar_module)
        {
            for (int j=0; j<3; j++)
                fprintf (fp, "%.3lf %.3lf %.3lf ", hh[i].timestamp_at_mp[j]/3600.,
                                             hh[i].max_power[j],
                                             hh[i].sol_power_at_mp[j]);
            for (int j=0; j<3; j++)
                fprintf (fp, "%.3lf %.3lf %.3lf %.3lf ", hh[i].timestamp_at_mpfg[j]/3600.,
                                                 hh[i].max_power_from_grid[j],
                                                 hh[i].power_at_mpfg[j],
                                                 hh[i].sol_power_at_mpfg[j]);
        }
        else
        {
            for (int j=0; j<3; j++)
                fprintf (fp, "%.3lf %.3lf ", hh[i].timestamp_at_mp[j]/3600., hh[i].max_power[j]);
        }
        fprintf (fp, "\n");
    }
}


void Household::adapt_pv_module_size()
{
    for (int i=0; i<local_count; i++)
        if (hh[i].solar_module) hh[i].solar_module->adapt_size (hh[i].consumption);
}

void Household::adapt_battery_capacity()
{
    for (int i=0; i<local_count; i++)
        if (hh[i].battery) hh[i].battery->adapt_capacity (hh[i].consumption);
}

bool Household::has_enough_solar_power (double pwr)
{
    if (power_to_grid - pwr > 0)
    {
        power_to_grid -= pwr;
        return true;
    }
    else return false;
}

bool Household::solar_prediction (int days_in_the_future)
{
    double predicted_solar_output = 0.;
    for (int i=sim_clock->sunrise; i<=sim_clock->sunset; i++)
    {
        predicted_solar_output += solar_module->calc_future_power_output (i, days_in_the_future);
    }
    return predicted_solar_output > 1.05 * sr_ss_consumption;
}


void Household::smartification()
{
    // Making an appliance smart was originally done within the appliance's
    // constructor, but that made it impossible to compare two consecutive
    // runs of the simulation (one with smart appliances and one without).
    // Now the smartification is done after all solarmodules have been
    // initialized.

    class Dishwasher *dw;
    class WashingMachine *wm;
    class E_Vehicle *ev;
    class Fridge *fr;
    class Freezer *fz;

    for (int i=0; i<local_count; i++)
    {
        if (hh[i].solar_module)
        {
            dw = hh[i].dishwasher;
            for (int j=0; j<hh[i].num_dishwashers; j++)
            {
                dw->make_smart();
                dw = dw->next_app;
            }
            wm = hh[i].wmachine;
            for (int j=0; j<hh[i].num_wmachines; j++)
            {
                wm->make_smart();
                wm = wm->next_app;
            }
            ev = hh[i].e_vehicle;
            for (int j=0; j<hh[i].num_evehicles; j++)
            {
                ev->make_smart();
                ev = ev->next_app;
            }
            fr = hh[i].fridge;
            for (int j=0; j<hh[i].num_fridges; j++)
            {
                fr->make_smart();
                fr = fr->next_app;
            }
            fz = hh[i].freezer;
            for (int j=0; j<hh[i].num_freezers; j++)
            {
                fz->make_smart();
                fz = fz->next_app;
            }
        }
    }
}

void Household::schedule (DHW_activity activity, int start_time)
{
    if (start_time > 1439) return; // This happens only if someone wants to cook after midnight.
                                   // Extending the schedule into the following day would make
                                   // it possible to handle this case properly. For the time
                                   // being we just assume that no hot water is used for cooking
                                   // after midnight :-)
    if (start_time < 0)  // select a random time of start by using a probability distribution
    {
        double rnd = get_random_number (0., probability_sum);
        start_time = 0;
        while (rnd > probability[start_time]) start_time++;
    }
    while (dhw_schedule[start_time] != DO_NOTHING && start_time < 1439) start_time++; // avoid 2 activities starting at the same time
    dhw_schedule[start_time] = activity;
}

void Household::add_timer (double duration, double heat_demand)
{
    Timer *t = new Timer;
    t->duration = duration;
    t->heat_demand = heat_demand;
    t->next = first_timer;
    first_timer = t;
}

void Household::increase_power (double real, double reactive)
{
    power.real += real;
    real_power_total[0] += real;
    real_power_total[residents] += real;

    power.reactive += reactive;
    reactive_power_total[0] += reactive;
    reactive_power_total[residents] += reactive;

    apparent_power_total[0] = sqrt (real_power_total[0]*real_power_total[0] + reactive_power_total[0]*reactive_power_total[0]);
    apparent_power_total[residents] = sqrt (real_power_total[residents]*real_power_total[residents] + reactive_power_total[residents]*reactive_power_total[residents]);
}

void Household::decrease_power (double real, double reactive)
{
    power.real -= real;
    real_power_total[0] -= real;
    real_power_total[residents] -= real;

    power.reactive -= reactive;
    reactive_power_total[0] -= reactive;
    reactive_power_total[residents] -= reactive;

    apparent_power_total[0] = real_power_total[0]*real_power_total[0] + reactive_power_total[0]*reactive_power_total[0];
    apparent_power_total[residents] = real_power_total[residents]*real_power_total[residents] + reactive_power_total[residents]*reactive_power_total[residents];
}

void Household::construct_building (void)
{
    // In an attempt to simplify the heat demand model we assume
    // that the building has a simple rectangular (square) floor plan
    // and no inner walls. The building is stored as a list of building
    // elements (walls, floor, windows, doors)

    area = get_random_number (config->household.min_area[residents-1], config->household.max_area[residents-1]);
    double length = sqrt (area);
    double height = 2.6;
    elements = new Element* [11];

    // Floor and ceiling

    elements[0] = new Element (FLOOR, length, length, temp_int_air, energy_class, NULL);
    elements[1] = new Element (CEILING, length, length, temp_int_air, energy_class, NULL);

    // The 4 walls with windows and doors

    int index = 2;
    for (int i=0; i<4; i++)
    {
        elements[index] = new Element (WALL, length, height, temp_int_air, energy_class, NULL);
        elements[index+1] = new Element (WINDOW, 1.5, 1.5, temp_int_air, energy_class, elements[index]);
        if (i==0)
        {
            elements[index+2] = new Element (DOOR, 1.1, 2.2, temp_int_air, energy_class, elements[index]);
            index += 3;
        }
        else index += 2;
    }
    num_elements = index;
    num_nodes = 0;
    for (int i=0; i<num_elements; i++) num_nodes += elements[i]->num_nodes;
}


double Household::operative_temperature (double phi_HC)
{
    const char function_name[] = "operative_temperature";
    const double kappa_int = 10000.;          // ISO 52016-1 table B.12
    const double f_int_c = 0.4;               // ISO 52016-1 table B.11
    const double f_sol_c = 0.4;               // ISO 52016-1 table B.11
    const double f_HC_c = 0.4;                // ISO 52016-1 table B.11
    const double delta_t = 3600.;             // timestep size in seconds
    const double C_int = area * kappa_int;
    double sum_area_hci, value;
    double phi_int, phi_int_residents, phi_int_app, phi_sol;
    int index;
    int n = num_nodes+1;    // number of linear equations

    // Initialize matrix A
    // Since A does not change, this has to be done only once
    if (a_matrix == NULL)
    {
        // Memory allocation
        alloc_memory (&a_matrix, n*n, function_name);
        for (int i=0; i<n*n; i++) a_matrix[i] = 0.;
        alloc_memory (&b_vector, n, function_name);
        alloc_memory (&offsets, num_elements, function_name);

        // ISO 52016-1 equation (17)
        sum_area_hci = 0.;
        index = 1;
        for (int i=0; i<num_elements; i++)
        {
            offsets[i] = index;
            sum_area_hci += elements[i]->area * elements[i]->h_ci;
            area_tot += elements[i]->area;
            a_matrix[index] = -elements[i]->area * elements[i]->h_ci;
            index += elements[i]->num_nodes;
        }
        a_matrix[0] = C_int/delta_t + sum_area_hci;
        if (config->ventilation_model) a_matrix[0] += heat_transfer_ventilation();

        index = n;
        for (int i=0; i<num_elements; i++)
        {
            // ISO 52016-1 equation (19)   inner surface node
            a_matrix[index+offsets[i]+1] = -elements[i]->h[0];
            a_matrix[index+offsets[i]] = elements[i]->h_ci + elements[i]->h_ri + elements[i]->h[0];
            a_matrix[index] = -elements[i]->h_ci;
            for (int k=0; k<num_elements; k++)
            {
                a_matrix[index+offsets[k]] -= elements[k]->area*elements[i]->h_ri/area_tot;
            }
            index += n;
            // ISO 52016-1 equation (20)   inner nodes
            if (elements[i]->num_nodes == 5)
            {
                a_matrix[index+offsets[i]+2] = -elements[i]->h[1];
                a_matrix[index+offsets[i]+1] = elements[i]->kappa[0]/delta_t + elements[i]->h[0] + elements[i]->h[1];
                a_matrix[index+offsets[i]]   = -elements[i]->h[0];
                index += n;
                a_matrix[index+offsets[i]+3] = -elements[i]->h[2];
                a_matrix[index+offsets[i]+2] = elements[i]->kappa[1]/delta_t + elements[i]->h[1] + elements[i]->h[2];
                a_matrix[index+offsets[i]+1] = -elements[i]->h[1];
                index += n;
                a_matrix[index+offsets[i]+4] = -elements[i]->h[3];
                a_matrix[index+offsets[i]+3] = elements[i]->kappa[2]/delta_t + elements[i]->h[2] + elements[i]->h[3];
                a_matrix[index+offsets[i]+2] = -elements[i]->h[2];
                index += n;
            }
            // ISO 52016-1 equation (21)   outer surface node
            if (elements[i]->num_nodes == 5)
            {
                a_matrix[index+offsets[i]+4] = elements[i]->h_ce + elements[i]->h_re + elements[i]->h[3];
                a_matrix[index+offsets[i]+3] = -elements[i]->h[3];
            }
            else
            {
                a_matrix[index+offsets[i]+1] = elements[i]->h_ce + elements[i]->h_re + elements[i]->h[0];
                a_matrix[index+offsets[i]]   = -elements[i]->h[0];
            }
            index += n;
        }
        // LU factorization
        for (int i=0; i<n-1; i++)
        {
            for (int k=i+1; k<n; k++)
            {
                a_matrix[k*n+i] /= a_matrix[i*n+i];
                for (int j=i+1; j<n; j++)
                {
                    a_matrix[k*n+j] -= a_matrix[k*n+i] * a_matrix[i*n+j];
                }
            }
        }
    }

    // Setup vector b
    phi_int_residents = residents_at_home (sim_clock->daytime) * 5. * 2. * (32.-temp_int_air_prev);
    phi_int_app = heat_loss_app * 1000.;
    phi_int = phi_int_residents + phi_int_app;
    phi_sol = 0.;
    value = ((1-f_int_c)*phi_int + (1-f_sol_c)*phi_sol + (1-f_HC_c)*phi_HC) / area_tot;
    b_vector[0] = (C_int/delta_t)*temp_int_air_prev + f_int_c*phi_int + f_sol_c*phi_sol + f_HC_c*phi_HC;
    index = 1;
    for (int i=0; i<num_elements; i++)
    {
        b_vector[index++] = value;
        if (elements[i]->num_nodes == 5)
        {
            for (int j=1; j<=3; j++)
            {
                b_vector[index++] = elements[i]->node_temp_prev[j] * elements[i]->kappa[j-1]/delta_t;
            }
        }
        b_vector[index++] = (elements[i]->h_ce + elements[i]->h_re) * location->temperature - elements[i]->phi_sky;
    }

    // Solve the linear system. The result is stored in vector b.
    double *row_start;
    for (int i=1; i<n; i++)
    {
        row_start = a_matrix+i*n;
        for (int j=0; j<i; j++)
        {
            //b_vector[i] -= a_matrix[i*n+j] * b_vector[j];
            b_vector[i] -= *(row_start+j) * b_vector[j];
        }
    }
    for (int i=n-1; i>=0; i--)
    {
        row_start = a_matrix+i*n;
        for (int j=i+1; j<n; j++)
        {
            b_vector[i] -= *(row_start+j) * b_vector[j];
        }
        b_vector[i] /= *(row_start+i);
    }
    temp_int_air = b_vector[0];
    double sum = 0.;
    for (int i=0; i<num_elements; i++)
    {
        sum += elements[i]->area * b_vector[offsets[i]];
        for (int j=0; j<elements[i]->num_nodes; j++) elements[i]->node_temp[j] = b_vector[offsets[i]+j];
    }
    double temp_op = 0.5 * (temp_int_air + sum/area_tot);
    return temp_op;
}


void Household::space_heating_and_cooling_demand (void)
{
    double temp_int_op_0;
    double temp_int_op_upper;

    if (reduce_heat)
    {
 //       if (sim_clock->daytime >= bedtime || sim_clock->daytime < wakeup-7200)
        if (sim_clock->daytime >= get_random_number (72000, 86400) || sim_clock->daytime <= get_random_number (0, 18000))
        {
            temp_int_set_H = config->household.set_temperature_H_night;
        }
        else
        {
            temp_int_set_H = config->household.set_temperature_H_day;
        }
    }
    heat_demand_SH = cool_demand = 0.;
    temp_int_op_0 = operative_temperature (0.);  // operative temperature without any heating or cooling
    if (temp_int_op_0 < temp_int_set_H && sim_clock->heating_period) // activation of the heating is required
    {
        temp_int_op_upper = operative_temperature (max_heat_power * 1000.); // operative temperature with heating enabled at max.power
        if (temp_int_op_upper < temp_int_set_H)
        {
            heat_demand_SH = max_heat_power;
        }
        else
        {
            heat_demand_SH = max_heat_power * (temp_int_set_H - temp_int_op_0) / (temp_int_op_upper - temp_int_op_0);
            operative_temperature (heat_demand_SH * 1000.);  // calculate temperatures for the actual amount of heat power
        }
    }
    else if (aircon && temp_int_op_0 > temp_int_set_C)  // aircon available && cooling needed ?
    {
        temp_int_op_upper = operative_temperature (-max_cool_power * 1000.); // operative temperature with cooling enabled at max.power
        if (temp_int_op_upper > temp_int_set_C)
        {
            cool_demand = max_cool_power;
        }
        else
        {
            cool_demand = max_cool_power * (temp_int_op_0 - temp_int_set_C) / (temp_int_op_0 - temp_int_op_upper);
            operative_temperature (-cool_demand * 1000.);  // calculate temperatures for the actual amount of cooling power
        }
    }
    // save temperatures for the next timestep
    for (int i=0; i<num_elements; i++)
    {
        for (int j=0; j<elements[i]->num_nodes; j++) elements[i]->node_temp_prev[j] = elements[i]->node_temp[j];
    }
    temp_int_air_prev = temp_int_air;
}


double Household::solar_collector_SH (void)
{
    double fraction_SH;
    double sum_SH = 0.;

    for (int i=0; i<local_count; i++)
    {
        if (hh[i].solar_collector)
        {
            if (hh[i].heat_storage->power_integral_SH > 0 && hh[i].heat_storage->power_integral_DHW > 0)
            {
                fraction_SH = hh[i].heat_storage->power_integral_SH /
                (hh[i].heat_storage->power_integral_SH + hh[i].heat_storage->power_integral_DHW);
            }
            else fraction_SH = 0.;
            sum_SH += hh[i].solar_collector->heat_to_storage_integral * fraction_SH;
        }
    }
    return sum_SH;
}

double Household::solar_collector_DHW (void)
{
    double fraction_DHW;
    double sum_DHW = 0.;

    for (int i=0; i<local_count; i++)
    {
        if (hh[i].solar_collector)
        {
            if (hh[i].heat_storage->power_integral_SH > 0 && hh[i].heat_storage->power_integral_DHW > 0)
            {
                fraction_DHW = hh[i].heat_storage->power_integral_DHW /
                               (hh[i].heat_storage->power_integral_SH + hh[i].heat_storage->power_integral_DHW);
            }
            else fraction_DHW = 0.;
            sum_DHW += hh[i].solar_collector->heat_to_storage_integral * fraction_DHW;
        }
    }
    return sum_DHW;
}


double Household::heat_transfer_ventilation (void)
{
    double H_vent = 0., q_vent, area_ow, V, alpha;
    double rnd;
    double v_wind = get_random_number (0., 10.);  // wind speed [m/s]
    double param_1 = 2.;
    double param_2 = 10.;

    for (int i=0; i<num_elements; i++)
    {
        if (elements[i]->category == WINDOW)
        {
            rnd = get_random_number (0., 100.);
            if (rnd < param_1) // it's totally open
            {
                area_ow = elements[i]->area;
            }
            else if (rnd < param_2) // it's partially open
            {
                alpha = get_random_number (5., 30.);
                area_ow = elements[i]->area * (2.6e-7*alpha*alpha*alpha - 1.19e-4*alpha*alpha + 1.86e-2*alpha);
            }
            else {}; // it's closed
            if (rnd < param_2)
            {
                V = 0.01 + 0.001 * v_wind * v_wind
                    + 0.0035 * sqrt(elements[i]->area) * fabs (temp_int_air - location->temperature);
                q_vent = 3.6 * 500. * area_ow * sqrt(V);  // [m3/h]
                H_vent += 1.200 * q_vent/3600.;
            }
        }
    }
    return H_vent;
}


void Household::update_vacation()
{
    // Determine which households are on vacation. This function is called once
    // at the start of each day.

    int num_hh_on_vacation = 0;  // number of households currently on vacation
    int delta;
    class Household **list = NULL;      // a list of household pointers used for sorting

    alloc_memory (&list, local_count, "Household::update_vacation");
    for (int i=0; i<local_count; i++) 
    {
        list[i] = hh+i;
        list[i]->vacation--;
    }
    qsort (list, (size_t)local_count, sizeof(class Household*), &compare_households_vacation);
    for (int h=local_count-1; h>=0; h--) if (list[h]->vacation > 0) num_hh_on_vacation++;
    delta = (int)(local_count * config->household.vacation_percentage[sim_clock->month-1][sim_clock->day-1] / 100. + 0.5)
            - num_hh_on_vacation;
    if (delta > 0)      // send more households on vacation
    {
        for (int h=0; h<delta; h++) list[h]->vacation = get_random_number (3, 4);
    }
    else if (delta < 0) // end vacation for some households
    {
        for (int h=0; h>delta; h--) list[local_count-1+h]->vacation = 0;
    }   
    delete [] list;
}

static int compare_households_vacation (const void *h1, const void *h2)
{
    Household *ptr1 = *(Household**)h1;
    Household *ptr2 = *(Household**)h2;
    int delta = ptr1->vacation - ptr2->vacation;
    if (delta > 0) return 1;
    else if (delta < 0) return -1;
    return 0;
}
