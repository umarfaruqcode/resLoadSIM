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

#define MAIN_MODULE

#include <stdio.h>
#include <stdlib.h>
#ifdef PARALLEL
#include <mpi.h>
#endif

#include "appliance.H"
#include "household.H"
#include "producer.H"
#include "battery.H"
#include "powerflow.H"
#include "heatsource.H"
#include "heatstorage.H"
#include "proto.H"
#include "output.H"
#include "solarmodule.H"
#include "solarcollector.H"
#include "globals.H"

// Global variable definition
int rank, num_processes;
class Configuration *config = NULL;
class Location *location = NULL;
class Clock *sim_clock = NULL;
class Powerflow *powerflow = NULL;
double table_DHW_saturday[1440];
double table_DHW_sunday[1440];
double table_DHW_weekday[1440];
bool silent_mode;
#ifdef DEBUG
FILE *debug_file;
#endif

// Local function prototypes
void reset_integral_values();
void print_results (class Output *output, int year);


int main (int argc, char **argv)
{
    class Output output;
    class Producer *producer = NULL;
    int num_households = 0;
    double num_days = 0;
    int completed, completed_old = 0;
    double transient_time;  // in seconds
    double forerun_time;    // in seconds

#ifdef PARALLEL
    MPI_Init (&argc, &argv);
    MPI_Comm_rank (MPI_COMM_WORLD, &rank);
    MPI_Comm_size (MPI_COMM_WORLD, &num_processes);
#else
    rank = 0;
    num_processes = 1;
#endif
    parse_arguments (argc, argv, &num_households, &num_days, &silent_mode);
    alloc_memory (&config, 1, "main");
    alloc_memory (&sim_clock, 1, "main");
    sim_clock->end_time = num_days * 24 * 3600;
    sim_clock->cur_time = 0;
    location->update_values();
#ifdef PARALLEL
MPI_Barrier (MPI_COMM_WORLD);
#endif
    init_random();

    if (rank == 0)
    {
        output.remove_old_files();
        config->print_log (num_households, num_days);
    }
    Household::allocate_memory (num_households);
    alloc_memory (&producer, 1, "main");
    Household::producer = producer;
    if (config->powerflow.step_size) powerflow = new class Powerflow (num_households);

    // Start a pre-simulation run so that the transient oscillations have time to settle
    // before the actual simulation starts. Other reasons for a pre-simulation run are:
    // • When using peak shaving we need to have a reference value for the
    //   maximum power peak.
    // • In case there are households with a PV installation with batteries we
    //   need to initialize the batteries properly.

    if (!silent_mode && rank == 0)
    {
        printf ("\nPre-run phase 1 (transient time):     "); fflush (stdout);
    }
    transient_time = config->transient_time * 24 * 3600;  // convert from days to seconds
    while (sim_clock->cur_time < transient_time)
    {
        location->update_values();
        Household::simulate_forerun();
        sim_clock->forward();
        completed = (int)((sim_clock->cur_time/transient_time)*100.0);
        if (!silent_mode && rank == 0 && completed > completed_old)
        {
            printf ("\b\b\b\b%3d%%", completed);
            fflush (stdout);
            completed_old = completed;
        }
    }
    reset_integral_values();

    // If the annual production of the PV modules is given as a fraction of the
    // annual household consumption, we need to calculate the annual consumption in
    // advance. So let's start another pre-simulation run...

    if (   (config->solar_module.production_ratio > 0 && SolarModule::count > 0)
        || (config->battery.capacity_in_days > 0 && Battery::count > 0))
    {
        if (!silent_mode && rank == 0)
        {
            printf ("\nPre-run phase 2 (one year):   0%%"); fflush (stdout);
            completed_old = 0;
        }
        int y=0;
        while (   y<config->num_ref_years
               && (   config->solar_production_reference_year[y]%4==0
                   && (config->solar_production_reference_year[y]%100>0 || config->solar_production_reference_year[y]%400==0))) y++;
        if (y == config->num_ref_years) // haven't found a non-leap year among the reference years
        {
            sim_clock->set_date_time (1, 1, config->solar_production_reference_year[0], 0.);    // take the first reference year
        }
        else
        {
            sim_clock->set_date_time (1, 1, config->solar_production_reference_year[y], 0.);    // take the reference year at position y
        }
        sim_clock->forerun = true;
        Household::deactivate_batteries();
        forerun_time = k_seconds_per_year;
        sim_clock->cur_time = 0.;
        while (sim_clock->cur_time < forerun_time)
        {
            location->update_values();
            if (sim_clock->midnight) Household::update_vacation();
            Household::simulate_forerun();
            sim_clock->forward();
            completed = (int)((sim_clock->cur_time/forerun_time)*100.0);
            if (!silent_mode && rank == 0 && completed > completed_old)
            {
                printf ("\b\b\b\b%3d%%", completed);
                fflush (stdout);
                completed_old = completed;
            }
        }
        if (config->solar_module.production_ratio > 0 && SolarModule::count > 0)
        {
            Household::adapt_pv_module_size();
        }
        if (config->battery.capacity_in_days > 0 && Battery::count > 0)
        {
            Household::adapt_battery_capacity();
        }
        Household::activate_batteries();
        reset_integral_values();
    }

    // Finally start the proper simulation

    if (!silent_mode && rank == 0)
    {
        printf ("\nSimulation progress:   0%%"); fflush (stdout);
        completed_old = 0;
    }
    sim_clock->set_date_time (config->start.day, config->start.month, config->start.year, config->start.time * 3600);
    sim_clock->forerun = false;
    Household::smartification();
    output.open_files();
    sim_clock->cur_time = 0.;
    int step = 1;
    while (sim_clock->cur_time < sim_clock->end_time)
    {
        output.reset();
        location->update_values();
        if (sim_clock->midnight) Household::update_vacation();
        Household::simulate();
        output.print_power();
        output.print_battery_stats();
        output.print_gridbalance();
        if (   (   sim_clock->cur_time > 0
                && sim_clock->daytime + config->timestep_size >= k_seconds_per_day
                && sim_clock->day == 31
                && sim_clock->month == DECEMBER)
            || sim_clock->cur_time + config->timestep_size >= sim_clock->end_time)
        {
            print_results (&output, sim_clock->year);
            reset_integral_values();
        }
        if (config->powerflow.step_size && step%config->powerflow.step_size == 0)
        {
            powerflow->simulate();
        }
        step++;
        sim_clock->forward();
        completed = (int)((sim_clock->cur_time/sim_clock->end_time)*100.0);
        if (!silent_mode && rank == 0 && completed > completed_old)
        {
            printf ("\b\b\b\b%3d%%", completed);
            fflush (stdout);
            completed_old = completed;
        }
    }
    output.close_files();
    if (!silent_mode && rank == 0) printf ("\n\n");
    delete [] sim_clock;
    delete [] producer;
    delete powerflow;
    delete [] config;
    Household::deallocate_memory();
#ifdef PARALLEL
    MPI_Finalize();
#endif
    return 0;
}


void reset_integral_values()
{
    Household::reset_integrals();
    SolarModule::power_total_integral = 0.;
    SolarCollector::power_total_integral = 0.;
    Battery::power_from_grid_total_integral = 0.;
    AirConditioner::reset_consumption();
    Boiler::reset_consumption();
    CirculationPump::reset_consumption();
    Computer::reset_consumption();
    ElectricStove::reset_consumption();
    GasStove::reset_consumption();
    Dishwasher::reset_consumption();
    E_Vehicle::reset_consumption();
    Freezer::reset_consumption();
    Fridge::reset_consumption();
    Heating::reset_consumption();
    Light::reset_consumption();
    TumbleDryer::reset_consumption();
    TV::reset_consumption();
    Vacuum::reset_consumption();
    WashingMachine::reset_consumption();
    HeatPump::reset_consumption();
}

void print_results (class Output *output, int year)
{
    output->print_households (year);
    output->print_consumption (year);
    Household::print_costs (year);
    Household::print_heat_consumption (year);
    //Household::print_debug_heat_consumption (year);
    output->print_distribution (year);
    output->print_summary (year);
    output->print_max (year);
}

int read_line (FILE *fp, char **line)
{
#ifdef _WIN32
    *line = new char[1048];
    fgets (*line, 1048, fp);
    return 0;
#else
    size_t line_length = 0;
    int ret = getline (line, &line_length, fp);
    /*if (ret == -1)
    {
        perror ("ERROR in 'read_line'");
        exit (1);
    }*/
    return ret;
#endif
}

#undef MAIN_MODULE
