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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <typeinfo>
#ifdef _WIN32
#   include <io.h>
#   define F_OK 0
#else
#   include <unistd.h>
#endif
#ifdef PARALLEL
#   include <mpi.h>
#endif

#include "appliance.H"
#include "household.H"
#include "solarmodule.H"
#include "solarcollector.H"
#include "battery.H"
#include "producer.H"
#include "heatsource.H"
#include "heatstorage.H"
#include "proto.H"
#include "output.H"
#include "globals.H"
#include "types.H"

int compare_double (double *val_1, double *val_2);


Output::Output()
{
    num_files = 0;

    for (int i=0; i<k_max_files; i++)
    {
        file_ptr[i] = NULL;
        power[i] = NULL;
        value_ptr_2[i] = NULL;
    }
    battery_file = NULL;
    gridbalance_file = NULL;
}


void Output::open_files()
{
    if (Computer::global_count())        add ("Computer", Computer::power_total, NULL);
    if (TV::global_count())              add ("TV", TV::power_total, NULL);
    if (Boiler::global_count())          add ("Boiler", Boiler::power_total, NULL);
    if (Fridge::global_count())          add ("Fridge", Fridge::power_total, NULL);
    if (Light::global_count())           add ("Light", Light::power_total, NULL);
    if (ElectricStove::global_count())   add ("Electric-Stove", ElectricStove::power_total, NULL);
    if (GasStove::global_count())        add ("Gas-Stove", GasStove::power_total, NULL);
    if (TumbleDryer::global_count())     add ("Tumble-Dryer", TumbleDryer::power_total, NULL);
    if (CirculationPump::global_count()) add ("Circulation-Pump", CirculationPump::power_total, NULL);
    if (Dishwasher::global_count())      add ("Dishwasher", Dishwasher::power_total, NULL);
    if (WashingMachine::global_count())  add ("Washing-Machine", WashingMachine::power_total, NULL);
    if (Freezer::global_count())         add ("Freezer", Freezer::power_total, NULL);
    if (E_Vehicle::global_count())       add ("E-Vehicle", E_Vehicle::power_total, &E_Vehicle::arr_counter);
    if (AirConditioner::global_count())  add ("Air-Conditioner", AirConditioner::power_total, NULL);
    if (Vacuum::global_count())          add ("Vacuum", Vacuum::power_total, NULL);
    if (Heating::global_count())         add ("E-Heating", Heating::power_total, NULL);
    if (HeatPump::global_count())        add ("Heat-Pump", HeatPump::power_total, NULL);
    if (SolarCollector::count)           add ("Solar-Collector", SolarCollector::power_total, NULL);
    if (SolarModule::count)
    {
        add ("Solar-Module.real",     SolarModule::real_power_total, &Household::production_used_total);
        add ("Solar-Module.reactive", SolarModule::reactive_power_total, NULL);
        add ("Solar-Module.apparent", SolarModule::apparent_power_total, NULL);
    }
    if (Battery::count)
    {
        add_battery (&Battery::charge_total,
                     &Battery::power_charging_total,
                     &Battery::power_discharging_total,
                     &Battery::loss_charging_total,
                     &Battery::loss_discharging_total);
    }
    if (HeatStorage::count) add ("Heat-Storage", HeatStorage::power_total, &HeatStorage::stored_heat_total);
    add_gridbalance (&Household::power_from_grid_total,
                     &Household::power_to_grid_total,
                     &Household::power_above_limit_total,
                     &Battery::power_from_grid_total);
    add ("Household.real", Household::real_power_total, NULL);
    add ("Household.reactive", Household::reactive_power_total, NULL);
    add ("Household.apparent", Household::apparent_power_total, NULL);
    add ("Hot-Water-Demand", Household::power_hot_water, NULL);
    if (HeatSource::global_count(OIL))
    {
        add ("Oil-Heating", HeatSource::heat_power_SH_total[OIL], NULL);
        add ("Oil-Hot-Water", HeatSource::heat_power_DHW_total[OIL], NULL);
    }
    if (HeatSource::global_count(GAS))
    {
        add ("Gas-Heating", HeatSource::heat_power_SH_total[GAS], NULL);
        add ("Gas-Hot-Water", HeatSource::heat_power_DHW_total[GAS], NULL);
    }
    if (HeatSource::global_count(DISTRICT))
    {
        add ("District-Heating", HeatSource::heat_power_SH_total[DISTRICT], NULL);
        add ("District-Hot-Water", HeatSource::heat_power_DHW_total[DISTRICT], NULL);
    }
}


void Output::add (const char *classname, double *ptr_1, double *ptr_2)
{
    char file_name[k_max_path];

    if (num_files == k_max_files)
    {
        fprintf (stderr, "Cannot open more than %d files. Increase k_max_files.\n", num_files);
        exit (1);
    }
    strncpy (names[num_files], classname, k_name_length);
    snprintf (file_name, sizeof(file_name), "power.%d.%s", sim_clock->year, classname);
    if (rank == 0) open_file (file_ptr+num_files, file_name, "w");
    power[num_files] = ptr_1;
    value_ptr_2[num_files] = ptr_2;
    num_files++;
}


void Output::add_battery (double *ptr_1, double *ptr_2, double *ptr_3,
                          double *ptr_4, double *ptr_5)
{
    char file_name[k_max_path];

    if (rank == 0)
    {
        snprintf (file_name, sizeof(file_name), "battery.%d", sim_clock->year);
        open_file (&battery_file, file_name, "w");
    }
    charge_total = ptr_1;
    power_charging_total = ptr_2;
    power_discharging_total = ptr_3;
    loss_charging_total = ptr_4;
    loss_discharging_total = ptr_5;
}


void Output::add_gridbalance (double *ptr_1, double *ptr_2, double *ptr_3, double *ptr_4)
{
    char file_name[k_max_path];

    if (rank == 0)
    {
        snprintf (file_name, sizeof(file_name), "gridbalance.%d", sim_clock->year);
        open_file (&gridbalance_file, file_name, "w");
    }
    power_from_grid_total = ptr_1;
    power_to_grid_total = ptr_2;
    power_above_limit_total = ptr_3;
    battery_from_grid_total = ptr_4;
}


void Output::reset()
{
    for (int i=0; i<num_files; i++)
    {
        for (int j=0; j<=k_max_residents; j++) power[i][j] = 0.;
        if (value_ptr_2[i]) *value_ptr_2[i] = 0.;
    }
    *power_from_grid_total = 0.;
    *power_to_grid_total = 0.;
    *power_above_limit_total = 0.;
    if (Battery::count)
    {
        *charge_total = 0.;
        *power_charging_total = 0.;
        *power_discharging_total = 0.;
        *loss_charging_total = 0.;
        *loss_discharging_total = 0.;
        *battery_from_grid_total = 0.;
    }
    if (   sim_clock->midnight
        && sim_clock->day == 1
        && sim_clock->month == JANUARY
        && rank == 0)
    {
        char file_name[k_max_path];
        for (int i=0; i<num_files; i++)
        {
            fclose (file_ptr[i]);
            snprintf (file_name, sizeof(file_name), "power.%d.%s", sim_clock->year, names[i]);
            open_file (file_ptr+i, file_name, "w");
        }
        if (battery_file)
        {
            fclose (battery_file);
            snprintf (file_name, sizeof(file_name), "battery.%d", sim_clock->year);
            open_file (&battery_file, file_name, "w");
        }
        if (gridbalance_file)
        {
            fclose (gridbalance_file);
            snprintf (file_name, sizeof(file_name), "gridbalance.%d", sim_clock->year);
            open_file (&gridbalance_file, file_name, "w");
        }
    }
}


void Output::close_files()
{
    if (rank == 0)
    {
        for (int i=0; i<num_files; i++) fclose (file_ptr[i]);
        if (battery_file) fclose (battery_file);
        if (gridbalance_file) fclose (gridbalance_file);
    }
}


void Output::print_power()
{
    for (int i=0; i<num_files; i++)
    {
#ifdef PARALLEL
        if (rank == 0)
            MPI_Reduce (MPI_IN_PLACE, power[i], k_max_residents+1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        else
            MPI_Reduce (power[i], power[i], k_max_residents+1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
#endif
        if (rank == 0)
        {
            fprintf (file_ptr[i], "%lf", sim_clock->yeartime/3600.);
            for (int j=0; j<=k_max_residents; j++)
            {
                fprintf (file_ptr[i], " %lf", power[i][j]);
            }
        }
        if (value_ptr_2[i])
        {
#ifdef PARALLEL
            if (rank == 0)
                MPI_Reduce (MPI_IN_PLACE, value_ptr_2[i], 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
            else
                MPI_Reduce (value_ptr_2[i], value_ptr_2[i], 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
#endif
            if (rank == 0) fprintf (file_ptr[i], " %lf", *value_ptr_2[i]);
        }
        if (rank == 0) fprintf (file_ptr[i], "\n");
    }
}


void Output::print_battery_stats()
{
    if (Battery::count)
    {
#ifdef PARALLEL
        if (rank == 0)
        {
            MPI_Reduce (MPI_IN_PLACE, charge_total, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
            MPI_Reduce (MPI_IN_PLACE, power_charging_total, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
            MPI_Reduce (MPI_IN_PLACE, power_discharging_total, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
            MPI_Reduce (MPI_IN_PLACE, loss_charging_total, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
            MPI_Reduce (MPI_IN_PLACE, loss_discharging_total, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        }
        else
        {
            MPI_Reduce (charge_total, charge_total, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
            MPI_Reduce (power_charging_total, power_charging_total, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
            MPI_Reduce (power_discharging_total, power_discharging_total, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
            MPI_Reduce (loss_charging_total, loss_charging_total, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
            MPI_Reduce (loss_discharging_total, loss_discharging_total, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        }
#endif
        if (rank == 0) fprintf (battery_file, "%lf %lf %lf %lf %lf %lf\n",
                               sim_clock->yeartime/3600.,
                               *charge_total/Battery::count,
                               *power_charging_total,
                               *power_discharging_total,
                               *loss_charging_total,
                               *loss_discharging_total);
    }
}


void Output::print_gridbalance()
{
#ifdef PARALLEL
    if (rank == 0)
    {
        MPI_Reduce (MPI_IN_PLACE, power_to_grid_total, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce (MPI_IN_PLACE, power_from_grid_total, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce (MPI_IN_PLACE, power_above_limit_total, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce (MPI_IN_PLACE, battery_from_grid_total, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    }
    else
    {
        MPI_Reduce (power_to_grid_total, power_to_grid_total, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce (power_from_grid_total, power_from_grid_total, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce (power_above_limit_total, power_above_limit_total, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce (battery_from_grid_total, battery_from_grid_total, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    }
#endif
    if (rank == 0) fprintf (gridbalance_file, "%lf %lf %lf %lf %lf %lf\n",
                            sim_clock->yeartime/3600.,
                            (*power_to_grid_total - *power_from_grid_total),
                            *power_from_grid_total,
                            *power_to_grid_total,
                            *power_above_limit_total,
                            *battery_from_grid_total);

}


void Output::print_consumption (int year)
{
    FILE *fp = NULL;
    char file_name[k_max_path];
    int res;
    double median[k_max_residents+1];

    if (rank == 0)
    {
        snprintf (file_name, sizeof(file_name), "consumption.%d", year);
        open_file (&fp, file_name, "w");
        fprintf (fp, "\n                                   1               2               3               4               5               6             All\n");
        fprintf (fp, "------------------------------------------------------------------------------------------------------------------------------------\n");
    }
    Computer::print_consumption (fp, "Computer");
    TV::print_consumption (fp, "TV");
    Boiler::print_consumption (fp, "Boiler");
    Fridge::print_consumption (fp, "Fridge");
    Light::print_consumption (fp, "Light");
    ElectricStove::print_consumption (fp, "ElectricStove");
    TumbleDryer::print_consumption (fp, "Tumble-Dryer");
    CirculationPump::print_consumption (fp, "Circulation-Pump");
    Dishwasher::print_consumption (fp, "Dishwasher");
    WashingMachine::print_consumption (fp, "Washing-Machine");
    Freezer::print_consumption (fp, "Freezer");
    E_Vehicle::print_consumption (fp, "E-Vehicle");
    AirConditioner::print_consumption (fp, "Air-Conditioner");
    Vacuum::print_consumption (fp, "Vacuum");
    Heating::print_consumption (fp, "E-Heating");
    HeatPump::print_consumption (fp, "Heat-Pump");
    if (rank == 0)
    {
        fprintf (fp, "------------------------------------------------------------------------------------------------------------------------------------\n");
        fprintf (fp, "Households          ");
    }
    Household::calc_consumption();
    for (res=0; res<=k_max_residents; res++) median[res] = Household::median(res);
    if (rank == 0)
    {
        for (res=1; res<=k_max_residents; res++) fprintf (fp, "%16d", Household::count[res]);
        fprintf (fp, "%16d", Household::count[0]);
        fprintf (fp, "\nConsumption min.    ");
        for (res=1; res<=k_max_residents; res++)
            if (Household::count[res]) fprintf (fp, "%16.3lf", Household::consumption_min[res]);
            else fprintf (fp, "%16.3lf", 0.);
        fprintf (fp, "%16.3lf", Household::consumption_min[0]);
        fprintf (fp, "\nConsumption avg.    ");
        for (res=1; res<=k_max_residents; res++)
            if (Household::count[res]) fprintf (fp, "%16.3lf", Household::consumption_sum[res]/Household::count[res]);
            else fprintf (fp, "%16.3lf", 0.);
        fprintf (fp, "%16.3lf", Household::consumption_sum[0]/Household::count[0]);
        fprintf (fp, "\nConsumption max.    ");
        for (res=1; res<=k_max_residents; res++) fprintf (fp, "%16.3lf", Household::consumption_max[res]);
        fprintf (fp, "%16.3lf", Household::consumption_max[0]);
        fprintf (fp, "\nStd. deviation      ");
        for (res=1; res<=k_max_residents; res++) fprintf (fp, "%16.3lf", Household::std_deviation (res));
        fprintf (fp, "%16.3lf", Household::std_deviation (0));
        fprintf (fp, "\nMedian              ");
        for (res=1; res<=k_max_residents; res++) fprintf (fp, "%16.3lf", median[res]);
        fprintf (fp, "%16.3lf", median[0]);
        fprintf (fp, "\n\n");
        fclose (fp);
    }
}


void Output::print_summary (int year)
{
    static const double factor = config->timestep_size/3600.;
    double sc_SH  = Household::solar_collector_SH ();
    double sc_DHW = Household::solar_collector_DHW ();

    HeatPump::correction_term();

#ifdef PARALLEL
    if (rank == 0)
    {
        MPI_Reduce (MPI_IN_PLACE, &SolarModule::power_total_integral, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce (MPI_IN_PLACE, &SolarCollector::power_total_integral, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce (MPI_IN_PLACE, &Household::power_to_grid_total_integral, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce (MPI_IN_PLACE, &Household::power_above_limit_total_integral, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce (MPI_IN_PLACE, &Battery::power_from_grid_total_integral, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce (MPI_IN_PLACE, Household::consumption_SH_total_integral, NUM_HEAT_SOURCE_TYPES, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce (MPI_IN_PLACE, Household::consumption_DHW_total_integral, NUM_HEAT_SOURCE_TYPES, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    }
    else
    {
        MPI_Reduce (&SolarModule::power_total_integral, &SolarModule::power_total_integral, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce (&SolarCollector::power_total_integral, &SolarCollector::power_total_integral, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce (&Household::power_to_grid_total_integral, &Household::power_to_grid_total_integral, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce (&Household::power_above_limit_total_integral, &Household::power_above_limit_total_integral, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce (&Battery::power_from_grid_total_integral, &Battery::power_from_grid_total_integral, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce (Household::consumption_SH_total_integral, Household::consumption_SH_total_integral, NUM_HEAT_SOURCE_TYPES, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce (Household::consumption_DHW_total_integral, Household::consumption_DHW_total_integral, NUM_HEAT_SOURCE_TYPES, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    }
#endif
    if (rank == 0)
    {
        FILE *fp = NULL;
        char file_name[k_max_path];
        double bo, he, hp, sto, overall_consumption;
        double *con_SH = Household::consumption_SH_total_integral;
        double *con_DHW = Household::consumption_DHW_total_integral;
        snprintf (file_name, sizeof(file_name), "summary.%d", year);
        open_file (&fp, file_name, "w");
        fprintf (fp, "\n%20s %21s\n", "Appliance", "Consumption");
        fprintf (fp, "------------------------------------------\n");
        Computer::print_summary (fp, "Computer");
        TV::print_summary (fp, "TV");
        Boiler::print_summary (fp, "Boiler");
        Fridge::print_summary (fp, "Fridge");
        Light::print_summary (fp, "Light");
        ElectricStove::print_summary (fp, "Electric Stove");
        TumbleDryer::print_summary (fp, "Tumble-Dryer");
        CirculationPump::print_summary (fp, "Circulation-Pump");
        Dishwasher::print_summary (fp, "Dishwasher");
        WashingMachine::print_summary (fp, "Washing-Machine");
        Freezer::print_summary (fp, "Freezer");
        E_Vehicle::print_summary (fp, "E-Vehicle");
        AirConditioner::print_summary (fp, "Air-Conditioner");
        Vacuum::print_summary (fp, "Vacuum");
        hp = HeatPump::print_summary (fp, "Heat-Pump");
        Heating::print_summary (fp, "Electric Heating");
        Battery::print_summary (fp, "Battery");
        fprintf (fp, "------------------------------------------\n");
        fprintf (fp, "%20s %17.3lf kWh\n", "Appliances Total", Household::consumption_sum[0]);

        fprintf (fp, "\n\n%20s\n", "Photovoltaic Energy");
        fprintf (fp, "------------------------------------------\n");
        fprintf (fp, "%20s %17.3lf kWh\n", "Used", SolarModule::power_total_integral*factor-
                                                   Household::power_to_grid_total_integral*factor);
        fprintf (fp, "%20s %17.3lf kWh", "Fed into the grid", Household::power_to_grid_total_integral*factor);
        if (Household::power_to_grid_total_integral > 0)
            fprintf (fp, "  (%.3lf kWh -> %.3lf%%)\n",
                     Household::power_above_limit_total_integral*factor,
                     100.*Household::power_above_limit_total_integral/SolarModule::power_total_integral*factor);
        else fprintf (fp, "\n");
        fprintf (fp, "--------------------------------------\n");
        fprintf (fp, "%20s %17.3lf kWh\n", "PV Energy Total", SolarModule::power_total_integral*factor);

        fprintf (fp, "\n\n%20s\n", "Solar Thermal Energy");
        fprintf (fp, "------------------------------------------\n");
        fprintf (fp, "%20s %17.3lf kWh\n", "Used for SH",  sc_SH);
        fprintf (fp, "%20s %17.3lf kWh\n", "Used for DHW", sc_DHW);
        fprintf (fp, "%20s %17.3lf kWh\n", "Unused", SolarCollector::power_total_integral*factor - sc_SH - sc_DHW);
        fprintf (fp, "------------------------------------------\n");
        fprintf (fp, "%20s %17.3lf kWh\n", "ST Energy Total", SolarCollector::power_total_integral*factor);

        fprintf (fp, "\n\n%20s\n", "Space Heating");
        fprintf (fp, "------------------------------------------\n");
        fprintf (fp, "%20s %17.3lf kWh\n", "Oil", con_SH[OIL]);
        fprintf (fp, "%20s %17.3lf kWh\n", "Gas", con_SH[GAS]);
        fprintf (fp, "%20s %17.3lf kWh\n", "District H.", con_SH[DISTRICT]);
        fprintf (fp, "%20s %17.3lf kWh\n", "Heat-Pump", con_SH[HEAT_PUMP]);
        he  = Heating::print_summary (fp, "Electric Heating");
        fprintf (fp, "------------------------------------------\n");
        fprintf (fp, "%20s %17.3lf kWh\n", "Space Heating Total", con_SH[OIL] + con_SH[GAS] + con_SH[DISTRICT] + con_SH[HEAT_PUMP] + he);

        fprintf (fp, "\n\n%20s\n", "Domestic Hot Water");
        fprintf (fp, "------------------------------------------\n");
        fprintf (fp, "%20s %17.3lf kWh\n", "Oil", con_DHW[OIL]);
        fprintf (fp, "%20s %17.3lf kWh\n", "Gas", con_DHW[GAS]);
        fprintf (fp, "%20s %17.3lf kWh\n", "District H.", con_DHW[DISTRICT]);
        fprintf (fp, "%20s %17.3lf kWh\n", "Heat-Pump", con_DHW[HEAT_PUMP]);
        bo  = Boiler::print_summary (fp, "Boiler");
        fprintf (fp, "------------------------------------------\n");
        fprintf (fp, "%20s %17.3lf kWh\n", "DHW Total", con_DHW[OIL] + con_DHW[GAS] + con_DHW[DISTRICT] + con_DHW[HEAT_PUMP] + bo);

        fprintf (fp, "\n\n%20s\n", "Cooking");
        fprintf (fp, "------------------------------------------\n");
        sto = ElectricStove::print_summary (fp, "Electric Stove");
        fprintf (fp, "%20s %17.3lf kWh\n", "Gas Stove", Household::consumption_cooking_total - sto);
        fprintf (fp, "------------------------------------------\n");
        fprintf (fp, "%20s %17.3lf kWh\n", "Cooking Total", Household::consumption_cooking_total);

        overall_consumption = Household::consumption_sum[0]
                              + con_SH[OIL] + con_SH[GAS] + con_SH[DISTRICT]
                              + con_DHW[OIL] + con_DHW[GAS] + con_DHW[DISTRICT]
                              + Household::consumption_cooking_total - sto;
        fprintf (fp, "\n\nOverall Consumption\n");
        fprintf (fp, "------------------------------------------\n");
        fprintf (fp, "Total Energy                    %8.1lf KWh\n", overall_consumption);
        fprintf (fp, "Appliances (w/o heat sources)   %8.1lf %%\n", 100.*(Household::consumption_sum[0] - bo - hp - he - sto) / overall_consumption);
        fprintf (fp, "Space Heating                   %8.1lf %%\n", 100.*(con_SH[OIL] + con_SH[GAS] + con_SH[DISTRICT] + con_SH[HEAT_PUMP] + he) / overall_consumption);
        fprintf (fp, "Domestic Hot Water              %8.1lf %%\n", 100.*(con_DHW[OIL] + con_DHW[GAS] + con_DHW[DISTRICT] + con_DHW[HEAT_PUMP] + bo) / overall_consumption);
        fprintf (fp, "Cooking                         %8.1lf %%\n", 100.*Household::consumption_cooking_total/overall_consumption);
        fprintf (fp, "------------------------------------------\n\n");
        fclose (fp);
    }
}


void Output::print_distribution (int year)
{
    FILE *fp = NULL;
    char filename[k_max_path];

    for (int i=1; i<=k_max_residents; i++)
    {
        if (rank == 0)
        {
            snprintf (filename, sizeof(filename), "dist.%d.%d", year, i);
            open_file (&fp, filename, "w");
        }
        Household::print_distribution (fp, i);
        if (rank == 0) fclose (fp);
    }
}


void Output::print_households (int year)
{
    FILE *fp1[k_max_residents+1];
    FILE *fp2[k_max_residents+1];
    char file_name[k_max_path], mode[2]="w";
    int res;
#ifdef PARALLEL
    MPI_Status status;
    char finished;

    if (rank > 0)
    {
        MPI_Recv (&finished, 1, MPI_CHAR, rank-1, 1, MPI_COMM_WORLD, &status);
        mode[0] = 'a';
    }
#endif
    for (res=1; res<=k_max_residents; res++)
    {
        snprintf (file_name, sizeof(file_name), "households.%d.%d", year, res);
        open_file (fp1+res, file_name, mode);
        snprintf (file_name, sizeof(file_name), "appliances.%d.%d", year, res);
        open_file (fp2+res, file_name, mode);
    }
    Household::print (fp1, fp2);
    for (res=1; res<=k_max_residents; res++)
    {
        fclose (fp1[res]);
        fclose (fp2[res]);
    }
#ifdef PARALLEL
    if (rank < num_processes-1) MPI_Send (&finished, 1, MPI_CHAR, rank+1, 1, MPI_COMM_WORLD);
#endif
}


void Output::print_max (int year)
{
    FILE *fp1[k_max_residents+1];
    FILE *fp2[k_max_residents+1];
    char file_name[k_max_path], mode[2]="w";
    int res;
#ifdef PARALLEL
    MPI_Status status;
    char finished;

    if (rank > 0)
    {
        MPI_Recv (&finished, 1, MPI_CHAR, rank-1, 1, MPI_COMM_WORLD, &status);
        mode[0] = 'a';
    }
#endif
    for (res=1; res<=k_max_residents; res++)
    {
        snprintf (file_name, sizeof(file_name), "max.%d.%d", year, res);
        open_file (fp1+res, file_name, mode);
        snprintf (file_name, sizeof(file_name), "max_sol.%d.%d", year, res);
        open_file (fp2+res, file_name, mode);
    }
    Household::print_max (fp1, fp2);
    for (res=1; res<=k_max_residents; res++)
    {
        fclose (fp1[res]);
        fclose (fp2[res]);
    }
#ifdef PARALLEL
    if (rank < num_processes-1) MPI_Send (&finished, 1, MPI_CHAR, rank+1, 1, MPI_COMM_WORLD);
#endif
}


void Output::remove_old_files()
{
#ifdef _WIN32
    if (access ("households.json", F_OK) == 0)
    {
        shell_command ("move households.json hh.json");
        shell_command ("del households*");
        shell_command ("move hh.json households.json");
    }
    else shell_command ("if exist households* del households*");
    shell_command ("if exist appliances* del appliances*");
    shell_command ("if exist battery* del battery*");
    shell_command ("if exist bus* del bus*");
    shell_command ("if exist consumption* del consumption*");
    shell_command ("if exist costs* del costs*");
    shell_command ("if exist debug* del debug*");
    shell_command ("if exist dist* del dist*");
    shell_command ("if exist gridbalance* del gridbalance*");
    shell_command ("if exist heat* del heat*");
    shell_command ("if exist max* del max*");
    shell_command ("if exist pf* del pf*");
    shell_command ("if exist power* del power*");
    shell_command ("if exist summary* del summary*");
    shell_command ("if exist trafo* del trafo*");
#else
    if (access ("households.json", F_OK) == 0)
    {
        shell_command ("mv households.json hh.json; rm -f households*; mv hh.json households.json");
    }
    else shell_command ("rm -f households*");
    shell_command ("rm -rf appliances* battery* bus* consumption* costs* debug* dist* gridbalance* heat* max* pf* power* summary* trafo*");
#endif
}


void shell_command (const char command[])
{
    int ret;
    ret = system (command);
    if (ret)
    {
        printf ("Unable to execute shell command: '%s'\n", command);
    }
}
