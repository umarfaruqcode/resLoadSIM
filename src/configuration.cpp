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
#include <string.h>
#include <sys/stat.h>
#include <float.h>
#include <ctype.h>
#include <limits.h>

#include "configuration.H"
#include "proto.H"
#include "version.H"
#include "globals.H"
#include "jsmn.h"
#include "appliance.H"


Configuration::Configuration()
{
    char file_name[k_max_path];
    dictionary = NULL;

    // Set default values for the settings in resLoadSIM.json
    strncpy (location_name, "Hannover", sizeof (location_name));
    strncpy (pv_data_file_name, "", sizeof (pv_data_file_name));
    strncpy (pv_forecast_file_name, "", sizeof (pv_forecast_file_name));

    battery_charging.strategy = 0;
    battery_charging.production_forecast_method = 0;
    battery_charging.feed_in_limit = 0.5;
    battery_charging.precharge_threshold = 0.1;
    battery_charging.shared = false;

    peak_shaving.relative = true;
    peak_shaving.threshold = 85.0;

    powerflow.case_file_name[0] = '\0';
    powerflow.step_size = 0;
    powerflow.ov_control = false;
    powerflow.uv_control = false;
    powerflow.output_level = 1;
    powerflow.ov_lower_limit = 1.075;
    powerflow.ov_upper_limit = 1.090;
    powerflow.uv_lower_limit = 0.910;
    powerflow.uv_upper_limit = 0.925;

    price[GRID].num_profiles = 1;
    price[GRID].profiles = new Profile;
    price[GRID].profiles[0].begin[0] = 0.;
    price[GRID].profiles[0].end[0] = 24.;
    price[GRID].profiles[0].price[0] = 0.2;
    price[GRID].profiles[0].length = 1;
    price[GRID].seq_length = 1;
    price[GRID].sequence[0] = 1;

    price[SOLAR].num_profiles = 1;
    price[SOLAR].profiles = new Profile;
    price[SOLAR].profiles[0].begin[0] = 0.;
    price[SOLAR].profiles[0].end[0] = 24.;
    price[SOLAR].profiles[0].price[0] = 0.10;
    price[SOLAR].profiles[0].length = 1;
    price[SOLAR].seq_length = 1;
    price[SOLAR].sequence[0] = 1;

    control = 0;
    seed = 0;
    output = 1;
    start.day = 1;
    start.month = 1;
    start.year = 2015;
    start.time = 0.0;
    transient_time = 1.0;
    daylight_saving_time = 1;
    timestep_size = 60.0;
    simulate_heating = false;
    ventilation_model = false;
    variable_load = false;
    comments_in_logfiles = true;
    energy_classes_2021 = true;
    num_ref_years = 0;

    // Parse the resLoadSIM configuration file
    create_dictionary (k_rls_json_file_name, NULL);
    if (dictionary)
    {
        lookup_string ("location", location_name, sizeof (location_name));
        lookup_string ("pv_data_file_name", pv_data_file_name, sizeof (pv_data_file_name));
        lookup_string ("pv_forecast_file_name", pv_forecast_file_name, sizeof (pv_forecast_file_name));
        lookup_integer (k_rls_json_file_name, "battery_charging.strategy", &battery_charging.strategy, 0, 4);
        lookup_integer (k_rls_json_file_name, "battery_charging.production_forecast_method", &battery_charging.production_forecast_method, 0, 3);
        lookup_decimal (k_rls_json_file_name, "battery_charging.feed_in_limit", &battery_charging.feed_in_limit, 0., 1.);
        lookup_decimal (k_rls_json_file_name, "battery_charging.precharge_threshold", &battery_charging.precharge_threshold, 0., 1.);
        lookup_boolean (k_rls_json_file_name, "battery_charging.shared", &battery_charging.shared);
        if (battery_charging.strategy && !battery_charging.production_forecast_method)
        {
            fprintf (stderr, "If battery_charging.strategy > 0, then production_forecast_method must be > 0 as well\n");
            exit(1);
        }
        lookup_variable_length_vector (k_rls_json_file_name, "solar_production_reference_year",
                                       solar_production_reference_year, &num_ref_years, k_max_ref_years);
        lookup_string ("powerflow.case_file_name", powerflow.case_file_name, sizeof (powerflow.case_file_name));
        lookup_integer (k_rls_json_file_name, "powerflow.step_size", &powerflow.step_size, 0, INT_MAX);
#ifndef HAVE_PF
#ifndef HAVE_POWER
        if (powerflow.step_size)
        {
            fprintf (stderr, "No powerflow solver installed (or not found by cmake)!\n");
            fprintf (stderr, "You can either install 'pf' or 'power', which are part of the PETSc library, and then cmake/make resLoadSIM again,\n");
            fprintf (stderr, "or you disable grid voltage control by setting powerflow.step_size = 0 in the configuration 'resLoadSIM.json'.\n");
            exit(1);
        }
#endif
#endif
        lookup_boolean (k_rls_json_file_name, "powerflow.ov_control", &powerflow.ov_control);
        lookup_boolean (k_rls_json_file_name, "powerflow.uv_control", &powerflow.uv_control);
        lookup_integer (k_rls_json_file_name, "powerflow.output_level", &powerflow.output_level, 0, 3);
        lookup_decimal (k_rls_json_file_name, "powerflow.ov_lower_limit", &powerflow.ov_lower_limit, 1.0, DBL_MAX);
        lookup_decimal (k_rls_json_file_name, "powerflow.ov_upper_limit", &powerflow.ov_upper_limit, 1.0, DBL_MAX);
        lookup_decimal (k_rls_json_file_name, "powerflow.uv_lower_limit", &powerflow.uv_lower_limit, 0.0, 1.0);
        lookup_decimal (k_rls_json_file_name, "powerflow.uv_upper_limit", &powerflow.uv_upper_limit, 0.0, 1.0);
        lookup_integer (k_rls_json_file_name, "control", &control, 0, 4);
        lookup_boolean (k_rls_json_file_name, "peak_shaving.relative", &peak_shaving.relative);
        if (peak_shaving.relative)
            lookup_decimal (k_rls_json_file_name, "peak_shaving.threshold", &peak_shaving.threshold, 0., 100.);
        else
            lookup_decimal (k_rls_json_file_name, "peak_shaving.threshold", &peak_shaving.threshold, 0., DBL_MAX);
        lookup_integer (k_rls_json_file_name, "seed", &seed, 0, INT_MAX);
        lookup_integer (k_rls_json_file_name, "output", &output, 0, 2);
        lookup_integer (k_rls_json_file_name, "start.day", &start.day, 1, 31);
        lookup_integer (k_rls_json_file_name, "start.month", &start.month, 1, 12);
        lookup_integer (k_rls_json_file_name, "start.year", &start.year, 1, 4800);
        lookup_decimal (k_rls_json_file_name, "start.time", &start.time, 0., 24.);
        lookup_decimal (k_rls_json_file_name, "transient_time", &transient_time, 1.0, 10.0);
        lookup_integer (k_rls_json_file_name, "daylight_saving_time", &daylight_saving_time, 0, 2);
        lookup_decimal (k_rls_json_file_name, "timestep_size", &timestep_size, 1.0e-6, 3600.0);
        lookup_price_table (k_rls_json_file_name, "price_grid", &price[GRID]);
        lookup_price_table (k_rls_json_file_name, "price_solar", &price[SOLAR]);
        lookup_boolean (k_rls_json_file_name, "simulate_heating", &simulate_heating);
        lookup_boolean (k_rls_json_file_name, "ventilation_model", &ventilation_model);
        lookup_boolean (k_rls_json_file_name, "variable_load", &variable_load);
        lookup_boolean (k_rls_json_file_name, "comments_in_logfiles", &comments_in_logfiles);
        lookup_boolean (k_rls_json_file_name, "energy_classes_2021", &energy_classes_2021);
        delete [] dictionary;
    }

    // Initialize the location
    try
    {
        location = new class Location (location_name,
                                       start.year,
                                       pv_data_file_name,
                                       pv_forecast_file_name,
                                       battery_charging.strategy,
                                       battery_charging.production_forecast_method);
    }
    catch (...)
    {
        fprintf (stderr, "Cannot allocate memory for 'location'.\n");
        exit (1);
    }

    // If the user has not selected one or more solar production reference years,
    // take the first year of the timeseries data as a reference year
    if (num_ref_years == 0)
    {
        solar_production_reference_year[0] = location->first_year;
        num_ref_years = 1;
    }
    else // check whether the reference year/s is/are valid
    {
        for (int i=0; i<num_ref_years; i++)
        {
            if (   solar_production_reference_year[i] < location->first_year
                || solar_production_reference_year[i] > location->last_year)
            {
                fprintf (stderr,
                         "%s: 'solar_production_reference_year' contains a year (%d), which is not part of the PVGIS timeseries data (%d - %d)\n",
                         k_rls_json_file_name, solar_production_reference_year[i], location->first_year, location->last_year);
                exit (1);
            }
        }
    }

    // Set default values for the settings in households.json and tech.json
    household.size_distribution[0] = 54.5;
    household.size_distribution[1] = 25.8;
    household.size_distribution[2] = 9.9;
    household.size_distribution[3] = 6.8;
    household.size_distribution[4] = 2.0;
    household.size_distribution[5] = 1.0;
    household.retired_1 = 60.0;
    household.retired_2 = 25.0;
    household.min_area[0] = 30.;
    household.min_area[1] = 40.;
    household.min_area[2] = 50.;
    household.min_area[3] = 70.;
    household.min_area[4] = 80.;
    household.min_area[5] = 80.;
    household.max_area[0] = 55.;
    household.max_area[1] = 60.;
    household.max_area[2] = 100.;
    household.max_area[3] = 120.;
    household.max_area[4] = 150.;
    household.max_area[5] = 150.;
    if (energy_classes_2021)
    {
        household.second_fridge[0] = 0.;
        household.second_fridge[1] = 0.;
        household.second_fridge[2] = 0.;
        household.second_fridge[3] = 0.;
        household.second_fridge[4] = 5.;
        household.second_fridge[5] = 15.;
        household.second_tv[0] = 0.;
        household.second_tv[1] = 50.;
        household.second_tv[2] = 75.;
        household.second_tv[3] = 85.;
        household.second_tv[4] = 80.;
        household.second_tv[5] = 100.;
        household.third_tv[0] = 0.;
        household.third_tv[1] = 0.;
        household.third_tv[2] = 0.;
        household.third_tv[3] = 0.;
        household.third_tv[4] = 0.;
        household.third_tv[5] = 5.;
    }
    else
    {
        household.second_fridge[0] = 0.;
        household.second_fridge[1] = 0.;
        household.second_fridge[2] = 0.;
        household.second_fridge[3] = 0.;
        household.second_fridge[4] = 5.;
        household.second_fridge[5] = 13.;
        household.second_tv[0] = 0.;
        household.second_tv[1] = 72.;
        household.second_tv[2] = 90.;
        household.second_tv[3] = 100.;
        household.second_tv[4] = 100.;
        household.second_tv[5] = 100.;
        household.third_tv[0] = 0.;
        household.third_tv[1] = 0.;
        household.third_tv[2] = 25.;
        household.third_tv[3] = 30.;
        household.third_tv[4] = 25.;
        household.third_tv[5] = 70.;
    }
    household.second_computer[0] = 0.;
    household.second_computer[1] = 0.;
    household.second_computer[2] = 0.;
    household.second_computer[3] = 16.;
    household.second_computer[4] = 35.;
    household.second_computer[5] = 56.;
    household.set_temperature_H_day = 20.0;
    household.set_temperature_H_night = 10.0;
    household.set_temperature_C = 20;
    household.reduce_heat = 100.0;
    household.heating_period_start_day = 1;
    household.heating_period_start_month = 9;
    household.heating_period_end_day = 1;
    household.heating_period_end_month = 6;
    household.min_init_laundry = 0;
    household.max_init_laundry = 10;
    if (energy_classes_2021)
    {
        household.min_delta_laundry[0] = 1.60;
        household.max_delta_laundry[0] = 1.70;
        household.min_delta_laundry[1] = 2.90;
        household.max_delta_laundry[1] = 3.00;
        household.min_delta_laundry[2] = 4.80;
        household.max_delta_laundry[2] = 4.90;
        household.min_delta_laundry[3] = 6.70;
        household.max_delta_laundry[3] = 6.80;
        household.min_delta_laundry[4] = 9.00;
        household.max_delta_laundry[4] = 9.10;
        household.min_delta_laundry[5] = 10.70;
        household.max_delta_laundry[5] = 10.80;
    }
    else
    {
        household.min_delta_laundry[0] = 0.95;
        household.max_delta_laundry[0] = 1.05;
        household.min_delta_laundry[1] = 1.55;
        household.max_delta_laundry[1] = 1.65;
        household.min_delta_laundry[2] = 2.35;
        household.max_delta_laundry[2] = 2.45;
        household.min_delta_laundry[3] = 2.95;
        household.max_delta_laundry[3] = 3.05;
        household.min_delta_laundry[4] = 3.75;
        household.max_delta_laundry[4] = 3.85;
        household.min_delta_laundry[5] = 4.30;
        household.max_delta_laundry[5] = 4.40;
    }
    household.min_vacuum_interval = 3;
    household.max_vacuum_interval = 10;
    if (energy_classes_2021)
    {
        household.light_factor[0] = 3.30;
        household.light_factor[1] = 3.30;
        household.light_factor[2] = 4.00;
        household.light_factor[3] = 4.00;
        household.light_factor[4] = 4.00;
        household.light_factor[5] = 3.50;
    }
    else
    {
        household.light_factor[0] = 9.00;
        household.light_factor[1] = 8.40;
        household.light_factor[2] = 10.0;
        household.light_factor[3] = 10.5;
        household.light_factor[4] = 9.50;
        household.light_factor[5] = 9.35;
    }
    household.prevalence.boiler[0] = 44.86;
    household.prevalence.boiler[1] = 40.11;
    household.prevalence.boiler[2] = 36.15;
    household.prevalence.boiler[3] = 30.97;
    household.prevalence.boiler[4] = 30.18;
    household.prevalence.boiler[5] = 28.82;
    household.prevalence.computer[0] = 66.0;
    household.prevalence.computer[1] = 80.0;
    household.prevalence.computer[2] = 100.0;
    household.prevalence.computer[3] = 100.0;
    household.prevalence.computer[4] = 100.0;
    household.prevalence.computer[5] = 100.0;
    if (energy_classes_2021)
    {
        household.prevalence.dryer[0] = 25.0;
        household.prevalence.dryer[1] = 41.0;
        household.prevalence.dryer[2] = 47.0;
        household.prevalence.dryer[3] = 46.0;
        household.prevalence.dryer[4] = 48.0;
        household.prevalence.dryer[5] = 40.0;
        household.prevalence.dishwasher[0] = 55.0;
        household.prevalence.dishwasher[1] = 80.0;
        household.prevalence.dishwasher[2] = 90.0;
        household.prevalence.dishwasher[3] = 88.0;
        household.prevalence.dishwasher[4] = 100.0;
        household.prevalence.dishwasher[5] = 100.0;
        household.prevalence.freezer[0] = 17.0;
        household.prevalence.freezer[1] = 35.0;
        household.prevalence.freezer[2] = 40.0;
        household.prevalence.freezer[3] = 48.0;
        household.prevalence.freezer[4] = 55.0;
        household.prevalence.freezer[5] = 60.0;
    }
    else
    {
        household.prevalence.dryer[0] = 26.0;
        household.prevalence.dryer[1] = 52.0;
        household.prevalence.dryer[2] = 71.0;
        household.prevalence.dryer[3] = 88.0;
        household.prevalence.dryer[4] = 95.0;
        household.prevalence.dryer[5] = 98.0;
        household.prevalence.dishwasher[0] = 55.0;
        household.prevalence.dishwasher[1] = 87.0;
        household.prevalence.dishwasher[2] = 95.0;
        household.prevalence.dishwasher[3] = 90.0;
        household.prevalence.dishwasher[4] = 90.0;
        household.prevalence.dishwasher[5] = 98.0;
        household.prevalence.freezer[0] = 17.0;
        household.prevalence.freezer[1] = 40.0;
        household.prevalence.freezer[2] = 50.0;
        household.prevalence.freezer[3] = 55.0;
        household.prevalence.freezer[4] = 60.0;
        household.prevalence.freezer[5] = 65.0;
    }
    for (int i=0; i<k_max_residents; i++)
    {
        household.prevalence.aircon[i] = 0.0;
        household.prevalence.stove[i] = 100.0;
        household.prevalence.gas_stove[i] = 0.0;
        household.prevalence.circpump[i] = 100.0;
        household.prevalence.fridge[i] = 100.0;
        household.prevalence.heating[i] = 0.0;
        household.prevalence.light[i] = 100.0;
        household.prevalence.tv[i] = 100.0;
        household.prevalence.vacuum[i] = 100.0;
        household.prevalence.wmachine[i] = 100.0;
        household.prevalence.solar_module[i] = 0.0;
        household.prevalence.e_vehicle[i] = 0.;
    }
    household.rnd_wakeup[0] = 25200.;
    household.rnd_wakeup[1] = 3600.;
    household.rnd_wakeup[2] = 10800.;
    household.rnd_wakeup[3] = 36000.;
    household.rnd_wakeup_weekend[0] = 32400.;
    household.rnd_wakeup_weekend[1] = 3600.;
    household.rnd_wakeup_weekend[2] = 25200.;
    household.rnd_wakeup_weekend[3] = 84000.;
    household.rnd_wakeup_retired[0] = 25200.;
    household.rnd_wakeup_retired[1] = 3600.;
    household.rnd_wakeup_retired[2] = 10800.;
    household.rnd_wakeup_retired[3] = 36000.;
    household.rnd_bedtime[0] = 79200.;
    household.rnd_bedtime[1] = 7200.;
    household.rnd_bedtime_weekend[0] = 82800.;
    household.rnd_bedtime_weekend[1] = 7200.;
    household.rnd_bedtime_retired[0] = 79200.;
    household.rnd_bedtime_retired[1] = 7200.;
    household.at_home_param[0] = 4800;
    household.at_home_param[1] = 28800;
    household.at_home_param[2] = 43200;
    household.at_home_param[3] = 50;
    household.at_home_param[4] = 4800;
    household.at_home_param[5] = 28800;
    household.at_home_param[6] = 43200;
    household.energy_class[0] =  0.2;
    household.energy_class[1] =  0.5;
    household.energy_class[2] =  0.3;
    household.energy_class[3] =  1.;
    household.energy_class[4] =  7.;
    household.energy_class[5] = 12.;
    household.energy_class[6] = 16.;
    household.energy_class[7] = 22.;
    household.energy_class[8] = 41.;
    household.rnd_heat_source[0] = 19.7;     // oil
    household.rnd_heat_source[1] = 67.4;     // gas
    household.rnd_heat_source[2] = 12.9;     // district heating
    household.rnd_heat_source[3] =  0.;      // heat pumps with electrical heating and boilers
    household.rnd_heat_source[4] =  0.;      // solar collectors with heat pumps
    household.min_temperature_DHW = 38.;
    household.max_temperature_DHW = 42.;
    household.min_volume_handwash = 0.25;
    household.max_volume_handwash = 1.50;
    household.min_volume_shower = 12.0;
    household.max_volume_shower = 60.0;
    household.min_volume_bath = 100.0;
    household.max_volume_bath = 130.0;
    household.urban_car_percentage = 58.0;
    household.rural_car_percentage = 90.0;
    for (int m=0; m<12; m++) 
        for (int d=0; d<31; d++) household.vacation_percentage[m][d] = -1;
    
    fridge.min_temperature = 2.0;
    fridge.max_temperature = 8.0;
    fridge.smartgrid_enabled = 0.0;
    fridge.smart = 0.0;
    fridge.delta_t_rise_factor = 0.0004;
    fridge.delta_t_rise_mean = 100.;
    fridge.delta_t_rise_sigma = 10.;
    fridge.delta_t_drop_factor = 0.002;
    fridge.delta_t_drop_mean = 100.;
    fridge.delta_t_drop_sigma = 10.;
    fridge.Vc_sigma[0] = 10.;
    fridge.Vc_sigma[1] = 10.;
    fridge.Vc_sigma[2] = 10.;
    fridge.Vc_sigma[3] = 10.;
    fridge.Vc_sigma[4] = 10.;
    fridge.Vc_sigma[5] = 10.;
    fridge.Vc_high[0] = 9999.;
    fridge.Vc_high[1] = 9999.;
    fridge.Vc_high[2] = 9999.;
    fridge.Vc_high[3] = 9999.;
    fridge.Vc_high[4] = 9999.;
    fridge.Vc_high[5] = 9999.;
    fridge.Tc = 5.0;
    fridge.power_factor = 0.9;
    if (energy_classes_2021)
    {
        fridge.num_energy_classes = 7;
        fridge.energy_classes[0] = 0.0;
        fridge.energy_classes[1] = 1.0;
        fridge.energy_classes[2] = 3.0;
        fridge.energy_classes[3] = 6.0;
        fridge.energy_classes[4] = 20.0;
        fridge.energy_classes[5] = 35.0;
        fridge.energy_classes[6] = 35.0;
        fridge.factor_1 = 12.;
        fridge.factor_2 = 14.;
        fridge.Vc_low[0] = 50.;
        fridge.Vc_low[1] = 50.;
        fridge.Vc_low[2] = 120.;
        fridge.Vc_low[3] = 220.;
        fridge.Vc_low[4] = 220.;
        fridge.Vc_low[5] = 220.;
        fridge.Vc_mean[0] = 80.;
        fridge.Vc_mean[1] = 80.;
        fridge.Vc_mean[2] = 200.;
        fridge.Vc_mean[3] = 260.;
        fridge.Vc_mean[4] = 260.;
        fridge.Vc_mean[5] = 260.;
    }
    else
    {
        fridge.num_energy_classes = 10;
        fridge.energy_classes[0] = 1.0;
        fridge.energy_classes[1] = 3.0;
        fridge.energy_classes[2] = 6.0;
        fridge.energy_classes[3] = 15.0;
        fridge.energy_classes[4] = 25.0;
        fridge.energy_classes[5] = 25.0;
        fridge.energy_classes[6] = 15.0;
        fridge.energy_classes[7] = 6.0;
        fridge.energy_classes[8] = 3.0;
        fridge.energy_classes[9] = 1.0;
        fridge.factor_1 = 9.;
        fridge.factor_2 = 10.;
        fridge.Vc_low[0] = 15.;
        fridge.Vc_low[1] = 30.;
        fridge.Vc_low[2] = 120.;
        fridge.Vc_low[3] = 220.;
        fridge.Vc_low[4] = 220.;
        fridge.Vc_low[5] = 220.;
        fridge.Vc_mean[0] = 30.;
        fridge.Vc_mean[1] = 60.;
        fridge.Vc_mean[2] = 200.;
        fridge.Vc_mean[3] = 260.;
        fridge.Vc_mean[4] = 260.;
        fridge.Vc_mean[5] = 260.;
    }

    freezer.min_temperature = -20.0;
    freezer.max_temperature = -16.0;
    freezer.smartgrid_enabled = 0.0;
    freezer.smart = 0.0;
    freezer.delta_t_rise_factor = 0.0004;
    freezer.delta_t_rise_mean = 100.;
    freezer.delta_t_rise_sigma = 10.;
    freezer.delta_t_drop_factor = 0.002;
    freezer.delta_t_drop_mean = 100.;
    freezer.delta_t_drop_sigma = 10.;
    freezer.Vc_per_resident = 50.;
    freezer.Tc = -18.0;
    freezer.mn_percentage = 50;
    freezer.power_factor = 0.8;
    if (energy_classes_2021)
    {
        freezer.num_energy_classes = 7;
        freezer.energy_classes[0] = 0.0;
        freezer.energy_classes[1] = 1.0;
        freezer.energy_classes[2] = 3.0;
        freezer.energy_classes[3] = 6.0;
        freezer.energy_classes[4] = 20.0;
        freezer.energy_classes[5] = 35.0;
        freezer.energy_classes[6] = 35.0;
        freezer.factor_1 = 12.;
    }
    else
    {
        freezer.num_energy_classes = 10;
        for (int i=0; i<freezer.num_energy_classes; i++) freezer.energy_classes[i] = 10.0;
        freezer.factor_1 = 6.;
    }

    e_vehicle.smartgrid_enabled = 0.0;
    e_vehicle.smart = 0.0;
    e_vehicle.departure_delay = 3600.;
    strcpy (e_vehicle.models[0].name, "Tesla Model 3, LR");
    e_vehicle.models[0].consumption_per_100km = 16.0;
    e_vehicle.models[0].battery_capacity = 75.0;
    e_vehicle.models[0].max_charge_power_AC = 11.0;
    e_vehicle.models[0].max_charge_power_DC = 200.0;
    e_vehicle.models[0].charging_curve[0] = 0.880;
    e_vehicle.models[0].charging_curve[1] = 0.892;
    e_vehicle.models[0].charging_curve[2] = 0.904;
    e_vehicle.models[0].charging_curve[3] = 0.916;
    e_vehicle.models[0].charging_curve[4] = 0.928;
    e_vehicle.models[0].charging_curve[5] = 0.940;
    e_vehicle.models[0].charging_curve[6] = 0.952;
    e_vehicle.models[0].charging_curve[7] = 0.964;
    e_vehicle.models[0].charging_curve[8] = 0.976;
    e_vehicle.models[0].charging_curve[9] = 0.988;
    e_vehicle.models[0].charging_curve[10] = 1.0;
    e_vehicle.models[0].charging_curve[11] = 0.917;
    e_vehicle.models[0].charging_curve[12] = 0.733;
    e_vehicle.models[0].charging_curve[13] = 0.667;
    e_vehicle.models[0].charging_curve[14] = 0.52;
    e_vehicle.models[0].charging_curve[15] = 0.5;
    e_vehicle.models[0].charging_curve[16] = 0.38;
    e_vehicle.models[0].charging_curve[17] = 0.293;
    e_vehicle.models[0].charging_curve[18] = 0.23;
    e_vehicle.models[0].charging_curve[19] = 0.133;
    e_vehicle.models[0].charging_curve[20] = 0.00;
    E_Vehicle::num_models = 1;

    dishwasher.smartgrid_enabled = 0.0;
    dishwasher.smart = 0.0;
    dishwasher.hours_per_cycle = 1.0;
    dishwasher.SAEc_small[0] = 126.;
    dishwasher.SAEc_small[1] = 25.2;
    dishwasher.SAEc_big[0] = 378.;
    dishwasher.SAEc_big[1] = 7.;
    dishwasher.factor = 280.;
    dishwasher.ignore_price = 10;
    dishwasher.fraction = 85;
    dishwasher.timer_1_mean = 43200.;
    dishwasher.timer_1_sigma = 7200.;
    dishwasher.timer_2_mean = 64800.;
    dishwasher.timer_2_sigma = 7200.;
    dishwasher.timer_3_mean = 3600.;
    dishwasher.timer_3_sigma = 1800.;
    dishwasher.preview_length = 1800;
    dishwasher.peak_delay = 1800;
    dishwasher.power_factor = 0.95;
    if (energy_classes_2021)
    {
        dishwasher.num_energy_classes = 7;
        dishwasher.energy_classes[0] = 0.;
        dishwasher.energy_classes[1] = 1.,
        dishwasher.energy_classes[2] = 2.;
        dishwasher.energy_classes[3] = 7.;
        dishwasher.energy_classes[4] = 20.;
        dishwasher.energy_classes[5] = 35.;
        dishwasher.energy_classes[6] = 35.;
        dishwasher.place_settings[0] = 5.;
        dishwasher.place_settings[1] = 7.;
        dishwasher.place_settings[2] = 9.;
        dishwasher.place_settings[3] = 11.;
        dishwasher.place_settings[4] = 14.;
        dishwasher.place_settings[5] = 22.;
        dishwasher.probability[0] = 45;
        dishwasher.probability[1] = 15;
    }
    else
    {
        dishwasher.num_energy_classes = 7;
        dishwasher.energy_classes[0] = 14.;
        dishwasher.energy_classes[1] = 14.,
        dishwasher.energy_classes[2] = 14.;
        dishwasher.energy_classes[3] = 14.;
        dishwasher.energy_classes[4] = 14.;
        dishwasher.energy_classes[5] = 14.;
        dishwasher.energy_classes[6] = 16.;
        dishwasher.place_settings[0] = 6.5;
        dishwasher.place_settings[1] = 8.0;
        dishwasher.place_settings[2] = 9.5;
        dishwasher.place_settings[3] = 11.0;
        dishwasher.place_settings[4] = 12.5;
        dishwasher.place_settings[5] = 14.0;
        dishwasher.probability[0] = 25;
        dishwasher.probability[1] = 15;
    }

    wmachine.smartgrid_enabled = 0.0;
    wmachine.smart = 0.0;
    wmachine.hours_per_cycle = 2.0;
    wmachine.random_limit = 50;
    wmachine.ignore_price = 10;
    wmachine.best_price_lookahead = 1440;
    wmachine.timer_mean = 3600;
    wmachine.timer_sigma = 1800;
    wmachine.peak_delay = 1800;
    wmachine.power_factor = 0.6;
    if (energy_classes_2021)
    {
        wmachine.num_energy_classes = 7;
        wmachine.energy_classes[0] = 0.0;
        wmachine.energy_classes[1] = 2.0;
        wmachine.energy_classes[2] = 3.0;
        wmachine.energy_classes[3] = 15.0;
        wmachine.energy_classes[4] = 60.0;
        wmachine.energy_classes[5] = 15.0;
        wmachine.energy_classes[6] = 5.0;
        wmachine.capacity[0] = 4.0;
        wmachine.capacity[1] = 5.0;
        wmachine.capacity[2] = 6.0;
        wmachine.capacity[3] = 7.0;
        wmachine.capacity[4] = 8.0;
        wmachine.capacity[5] = 8.0;
    }
    else
    {
        wmachine.num_energy_classes = 10;
        wmachine.energy_classes[0] = 1.0;
        wmachine.energy_classes[1] = 3.0;
        wmachine.energy_classes[2] = 6.0;
        wmachine.energy_classes[3] = 15.0;
        wmachine.energy_classes[4] = 25.0;
        wmachine.energy_classes[5] = 25.0;
        wmachine.energy_classes[6] = 15.0;
        wmachine.energy_classes[7] = 6.0;
        wmachine.energy_classes[8] = 3.0;
        wmachine.energy_classes[9] = 1.0;
        wmachine.capacity[0] = 4.0;
        wmachine.capacity[1] = 5.0;
        wmachine.capacity[2] = 6.0;
        wmachine.capacity[3] = 7.0;
        wmachine.capacity[4] = 8.0;
        wmachine.capacity[5] = 9.0;
    }

    dryer.smartgrid_enabled = 0.0;
    dryer.hours_per_cycle = 1.0;
    dryer.ignore_price = 10;
    dryer.peak_delay = 1800;
    dryer.power_factor = 0.95;
    if (energy_classes_2021)
    {
        dryer.num_energy_classes = 7;
        dryer.energy_classes[0] = 0.0;
        dryer.energy_classes[1] = 10.0;
        dryer.energy_classes[2] = 15.0;
        dryer.energy_classes[3] = 40.0;
        dryer.energy_classes[4] = 20.0;
        dryer.energy_classes[5] = 10.0;
        dryer.energy_classes[6] = 5.0;
        dryer.capacity[0] = 4.0;
        dryer.capacity[1] = 5.0;
        dryer.capacity[2] = 6.0;
        dryer.capacity[3] = 8.0;
        dryer.capacity[4] = 8.0;
        dryer.capacity[5] = 10.0;
    }
    else
    {
        dryer.num_energy_classes = 7;
        dryer.energy_classes[0] = 1.0;
        dryer.energy_classes[1] = 2.0;
        dryer.energy_classes[2] = 3.0;
        dryer.energy_classes[3] = 50.0;
        dryer.energy_classes[4] = 25.0;
        dryer.energy_classes[5] = 15.0;
        dryer.energy_classes[6] = 4.0;
        dryer.capacity[0] = 6.0;
        dryer.capacity[1] = 6.8;
        dryer.capacity[2] = 7.6;
        dryer.capacity[3] = 8.4;
        dryer.capacity[4] = 9.2;
        dryer.capacity[5] = 10.0;
    }

    boiler.power_factor = 1.0;

    heating.smartgrid_enabled = 0.;
    heating.kW_per_m2 = 0.1;
    heating.power_factor = 1.0;

    heatpump.min_eff = 0.45;
    heatpump.max_eff = 0.55;
    heatpump.min_temperature = 35.;
    heatpump.max_temperature = 50.;
    heatpump.kW_per_m2 = 0.1;
    heatpump.power_factor = 0.80;

    aircon.min_eff = 0.45;
    aircon.max_eff = 0.55;
    aircon.kW_per_m2 = 0.1;
    aircon.power_factor = 0.85;

    vacuum.timer_min = 28800;
    vacuum.timer_max = 72000;
    vacuum.timer_factor = 60;
    vacuum.power_factor = 0.70;
    if (energy_classes_2021)
    {
        vacuum.num_energy_classes = 7;
        vacuum.energy_classes[0] = 0.;
        vacuum.energy_classes[1] = 1.,
        vacuum.energy_classes[2] = 2.;
        vacuum.energy_classes[3] = 7.;
        vacuum.energy_classes[4] = 20.;
        vacuum.energy_classes[5] = 35.;
        vacuum.energy_classes[6] = 35.;
    }
    else
    {
        vacuum.num_energy_classes = 10;
        for (int i=0; i<vacuum.num_energy_classes; i++) vacuum.energy_classes[i] = 10.0;
    }

    tv.diagonal_1 = 50.0;
    tv.diagonal_2 = 40.0;
    tv.diagonal_3 = 30.0;
    tv.avg_duration[0] = 3.9 * 3600.;
    tv.avg_duration[1] = 3.45 * 3600.;
    tv.avg_duration[2] = 3.9 * 3600.;
    tv.avg_duration[3] = 3.85 * 3600.;
    tv.avg_duration[4] = 4.4 * 3600.;
    tv.avg_duration[5] = 4.4 * 3600.;
    tv.factor_mean = 1.0;
    tv.factor_sigma = 0.25;
    tv.factor_mean_we = 1.2;
    tv.factor_sigma_we = 0.3;
    tv.duration_factor = 0.6;
    tv.duration_factor_sat = 0.5;
    tv.duration_factor_sun = 0.5;
    tv.random[0] = 30;
    tv.random[1] = 55;
    tv.random[2] = 60;
    tv.random_sat[0] = 10;
    tv.random_sat[1] = 25;
    tv.random_sat[2] = 55;
    tv.random_sun[0] = 30;
    tv.random_sun[1] = 55;
    tv.random_sun[2] = 80;
    tv.delay[0] = 6000;
    tv.delay[1] = 12000;
    tv.delay[2] = 24000;
    tv.delay_sat[0] = 6000;
    tv.delay_sat[1] = 12000;
    tv.delay_sat[2] = 24000;
    tv.delay_sun[0] = 6000;
    tv.delay_sun[1] = 12000;
    tv.delay_sun[2] = 24000;
    tv.time_2_mean = 19.*3600.;
    tv.time_2_sigma = 7200.;
    tv.power_factor = 0.95;
    if (energy_classes_2021)
    {
        tv.num_energy_classes = 7;
        tv.energy_classes[0] = 0.0;
        tv.energy_classes[1] = 5.0;
        tv.energy_classes[2] = 10.0;
        tv.energy_classes[3] = 15.0;
        tv.energy_classes[4] = 30.0;
        tv.energy_classes[5] = 25.0;
        tv.energy_classes[6] = 15.0;
    }
    else
    {
        tv.num_energy_classes = 10;
        tv.energy_classes[0] = 1.0;
        tv.energy_classes[1] = 2.0;
        tv.energy_classes[2] = 4.0;
        tv.energy_classes[3] = 7.0;
        tv.energy_classes[4] = 10.0;
        tv.energy_classes[5] = 23.0;
        tv.energy_classes[6] = 23.0;
        tv.energy_classes[7] = 15.0;
        tv.energy_classes[8] = 10.0;
        tv.energy_classes[9] = 5.0;
    }

    light.luminous_flux_mean = 800;
    light.luminous_flux_sigma = 200;
    light.luminous_flux_min = 400;
    light.luminous_flux_max = 1500;
    light.sigma_morning = 1800.;
    light.sigma_evening = 1800.;
    light.power_factor = 0.9;
    if (energy_classes_2021)
    {
        light.num_energy_classes = 7;
        light.energy_classes[0] = 0.0;
        light.energy_classes[1] = 0.0;
        light.energy_classes[2] = 5.0;
        light.energy_classes[3] = 10.0;
        light.energy_classes[4] = 15.0;
        light.energy_classes[5] = 30.0;
        light.energy_classes[6] = 40.0;
    }
    else
    {
        light.num_energy_classes = 7;
        light.energy_classes[0] = 1.0;
        light.energy_classes[1] = 5.0;
        light.energy_classes[2] = 11.0;
        light.energy_classes[3] = 21.0;
        light.energy_classes[4] = 26.0;
        light.energy_classes[5] = 25.0;
        light.energy_classes[6] = 11.0;
    }

    computer.power = 0.4;
    computer.duration_mean = 13200.;
    computer.duration_sigma = 4500.;
    computer.duration_fraction = 0.6;
    computer.duration_fraction_saturday = 0.6;
    computer.duration_fraction_sunday = 0.6;
    computer.time_offset[0] = 6000.;
    computer.time_offset[1] = 12000.;
    computer.time_offset[2] = 24000.;
    computer.time_offset_saturday[0] = 6000.;
    computer.time_offset_saturday[1] = 12000.;
    computer.time_offset_saturday[2] = 24000.;
    computer.time_offset_sunday[0] = 6000.;
    computer.time_offset_sunday[1] = 12000.;
    computer.time_offset_sunday[2] = 24000.;
    computer.rnd[0] = 30;
    computer.rnd[1] = 55;
    computer.rnd[2] = 90;
    computer.rnd_saturday[0] = 10;
    computer.rnd_saturday[1] = 25;
    computer.rnd_saturday[2] = 65;
    computer.rnd_sunday[0] = 30;
    computer.rnd_sunday[1] = 55;
    computer.rnd_sunday[2] = 80;
    computer.time_2_mean = 68400.;
    computer.time_2_sigma = 7200.;
    computer.power_factor = 0.95;

    circpump.controlled = 90.0;
    circpump.power_per_size = 0.0018;
    circpump.rnd_first_day[0] = 28;
    circpump.rnd_first_day[1] = 5;
    circpump.rnd_first_day[2] = 15;
    circpump.rnd_first_day[3] = 42;
    circpump.rnd_last_day[0] = 28;
    circpump.rnd_last_day[1] = 5;
    circpump.rnd_last_day[2] = 15;
    circpump.rnd_last_day[3] = 42;
    circpump.first_month = 9;
    circpump.last_month = 4;
    circpump.time_1 = 79200;
    circpump.time_2 = 21600;
    circpump.rnd_time_on[0] = 300.;
    circpump.rnd_time_on[1] = 600.;
    circpump.rnd_time_off[0] = 300.;
    circpump.rnd_time_off[1] = 600.;
    circpump.power_factor = 0.85;

    stove.power[0] = 0.850;
    stove.power[1] = 1.710;
    stove.power[2] = 1.710;
    stove.power[3] = 2.040;
    stove.power[4] = 2.220;
    stove.power[5] = 2.400;
    stove.duration_1_percent = 90;
    stove.duration_2_percent = 70;
    stove.duration_2_percent_saturday = 35;
    stove.duration_2_percent_sunday = 75;
    stove.time_offset = 1800.;
    stove.rnd_duration_1[0] = 300.;
    stove.rnd_duration_1[1] = 180.;
    stove.rnd_duration_1[2] = 120.;
    stove.rnd_duration_1[3] = 600.;
    stove.rnd_duration_2[0] = 2280;
    stove.rnd_duration_2[1] = 900.;
    stove.rnd_duration_2[2] = 1200.;
    stove.rnd_duration_2[3] = k_seconds_per_day;
    stove.rnd_duration_3[0] = 2280;
    stove.rnd_duration_3[1] = 600.;
    stove.rnd_duration_3[2] = 1200.;
    stove.rnd_duration_3[3] = k_seconds_per_day;
    stove.time_2_mean = 43200.;
    stove.time_2_sigma = 4800.;
    stove.time_3_mean = 64800.;
    stove.time_3_sigma = 7200.;
    stove.power_factor = 0.98;

    solar_module.system_loss = 14.0;
    solar_module.production_ratio = 0.0;
    solar_module.min_area = 1.;
    solar_module.max_area = 10.;
    solar_module.min_eff = 0.1;
    solar_module.max_eff = 0.2;
    solar_module.power_factor = 0.95;

    solar_collector.area_factor_1 = 2.75;
    solar_collector.area_factor_2 = 0.05;
    solar_collector.eff_0 = 0.8;
    solar_collector.min_flow_rate = 40.0;
    solar_collector.max_flow_rate = 60.0;

    battery.frequency_solar = 60.0;
    battery.frequency_non_solar = 0.0;
    battery.capacity_in_days = 0.;
    battery.allow_grid_charge_solar = false;
    battery.smartgrid_enabled = 0.0;
    battery.installation_costs = 500.0;
    battery.avg_lifetime = 8.0;
    battery.min_price = 315.0;
    battery.max_price = 1650.0;
    battery.min_capacity_per_resident = 0.375;
    battery.max_capacity_per_resident = 1.5;
    battery.min_eff_charging = 0.925;
    battery.max_eff_charging = 0.975;
    battery.min_eff_discharging = 0.925;
    battery.max_eff_discharging = 0.975;
    battery.max_power_charging = 0.25;
    battery.max_power_discharging = 0.5;

    heat_storage.liter_per_m2 = 50.;
    heat_storage.max_temperature = 60.;
    heat_storage.max_heat_power = 20.;

    // Read the household related parameters from household.json.
    // First look in the working directory, and if it's not there,
    // go to the country specific directory

    snprintf (file_name, sizeof(file_name), "countries/%s/%s", location->country, k_hh_json_file_name);
    create_dictionary (k_hh_json_file_name, file_name);
    if (dictionary)
    {
        lookup_vector (k_hh_json_file_name, "size_distribution", household.size_distribution, k_max_residents, true);
        lookup_vector (k_hh_json_file_name, "prevalence.air_conditioner", household.prevalence.aircon, k_max_residents, false);
        lookup_vector (k_hh_json_file_name, "prevalence.boiler", household.prevalence.boiler, k_max_residents, false);
        lookup_vector (k_hh_json_file_name, "prevalence.circulation_pump", household.prevalence.circpump, k_max_residents, false);
        lookup_vector (k_hh_json_file_name, "prevalence.computer", household.prevalence.computer, k_max_residents, false);
        lookup_vector (k_hh_json_file_name, "prevalence.stove", household.prevalence.stove, k_max_residents, false);
        lookup_vector (k_hh_json_file_name, "prevalence.gas_stove", household.prevalence.gas_stove, k_max_residents, false);
        lookup_vector (k_hh_json_file_name, "prevalence.dishwasher", household.prevalence.dishwasher, k_max_residents, false);
        lookup_vector (k_hh_json_file_name, "prevalence.freezer", household.prevalence.freezer, k_max_residents, false);
        lookup_vector (k_hh_json_file_name, "prevalence.fridge", household.prevalence.fridge, k_max_residents, false);
        lookup_vector (k_hh_json_file_name, "prevalence.heating", household.prevalence.heating, k_max_residents, false);
        lookup_vector (k_hh_json_file_name, "prevalence.light", household.prevalence.light, k_max_residents, false);
        lookup_vector (k_hh_json_file_name, "prevalence.solar_module", household.prevalence.solar_module, k_max_residents, false);
        lookup_vector (k_hh_json_file_name, "prevalence.tumble_dryer", household.prevalence.dryer, k_max_residents, false);
        lookup_vector (k_hh_json_file_name, "prevalence.tv", household.prevalence.tv, k_max_residents, false);
        lookup_vector (k_hh_json_file_name, "prevalence.vacuum", household.prevalence.vacuum, k_max_residents, false);
        lookup_vector (k_hh_json_file_name, "prevalence.washing_machine", household.prevalence.wmachine, k_max_residents, false);
        lookup_vector (k_hh_json_file_name, "prevalence.e_vehicle", household.prevalence.e_vehicle, k_max_residents, false);
        lookup_decimal (k_hh_json_file_name, "retired_1", &household.retired_1, 0., 100.);
        lookup_decimal (k_hh_json_file_name, "retired_2", &household.retired_2, 0., 100.);
        lookup_vector (k_hh_json_file_name, "min_area", household.min_area, k_max_residents, false);
        lookup_vector (k_hh_json_file_name, "max_area", household.max_area, k_max_residents, false);
        lookup_decimal (k_hh_json_file_name, "set_temperature_heating_day", &household.set_temperature_H_day, 0., DBL_MAX);
        lookup_decimal (k_hh_json_file_name, "set_temperature_heating_night", &household.set_temperature_H_night, 0., DBL_MAX);
        lookup_decimal (k_hh_json_file_name, "set_temperature_cooling", &household.set_temperature_C, 0., DBL_MAX);
        lookup_decimal (k_hh_json_file_name, "reduce_heat", &household.reduce_heat, 0., 100.);
        lookup_integer (k_hh_json_file_name, "heating_period_start_day", &household.heating_period_start_day, 1, 31);
        lookup_integer (k_hh_json_file_name, "heating_period_start_month", &household.heating_period_start_month, 1, 12);
        lookup_integer (k_hh_json_file_name, "heating_period_end_day", &household.heating_period_end_day, 1, 31);
        lookup_integer (k_hh_json_file_name, "heating_period_end_month", &household.heating_period_end_month, 1, 12);
        lookup_integer (k_hh_json_file_name, "min_init_laundry", &household.min_init_laundry, 0, INT_MAX);
        lookup_integer (k_hh_json_file_name, "max_init_laundry", &household.max_init_laundry, household.min_init_laundry, INT_MAX);
        lookup_vector (k_hh_json_file_name, "second_fridge", household.second_fridge, k_max_residents, false);
        lookup_vector (k_hh_json_file_name, "second_tv", household.second_tv, k_max_residents, false);
        lookup_vector (k_hh_json_file_name, "third_tv", household.third_tv, k_max_residents, false);
        lookup_vector (k_hh_json_file_name, "second_computer", household.second_computer, k_max_residents, false);
        lookup_vector (k_hh_json_file_name, "min_delta_laundry", household.min_delta_laundry, k_max_residents, false);
        lookup_vector (k_hh_json_file_name, "max_delta_laundry", household.max_delta_laundry, k_max_residents, false);
        lookup_integer (k_hh_json_file_name, "min_vacuum_interval", &household.min_vacuum_interval, 0, INT_MAX);
        lookup_integer (k_hh_json_file_name, "max_vacuum_interval", &household.max_vacuum_interval, household.min_vacuum_interval, INT_MAX);
        lookup_vector (k_hh_json_file_name, "light_factor", household.light_factor, k_max_residents, false);
        lookup_vector (k_hh_json_file_name, "rnd_wakeup", household.rnd_wakeup, 4, false);
        lookup_vector (k_hh_json_file_name, "rnd_wakeup_weekend", household.rnd_wakeup_weekend, 4, false);
        lookup_vector (k_hh_json_file_name, "rnd_wakeup_retired", household.rnd_wakeup_retired, 4, false);
        lookup_vector (k_hh_json_file_name, "rnd_bedtime", household.rnd_bedtime, 2, false);
        lookup_vector (k_hh_json_file_name, "rnd_bedtime_weekend", household.rnd_bedtime_weekend, 2, false);
        lookup_vector (k_hh_json_file_name, "rnd_bedtime_retired", household.rnd_bedtime_retired, 2, false);
        lookup_vector (k_hh_json_file_name, "at_home_param", household.at_home_param, 7);
        lookup_vector (k_hh_json_file_name, "energy_class", household.energy_class, k_num_ec_household, true);
        lookup_vector (k_hh_json_file_name, "rnd_heat_source", household.rnd_heat_source, NUM_HEAT_SOURCE_TYPES, true);
        lookup_decimal (k_hh_json_file_name, "min_temperature_DHW", &household.min_temperature_DHW, 0., DBL_MAX);
        lookup_decimal (k_hh_json_file_name, "max_temperature_DHW", &household.max_temperature_DHW, 0., DBL_MAX);
        lookup_decimal (k_hh_json_file_name, "min_volume_handwash", &household.min_volume_handwash, 0., DBL_MAX);
        lookup_decimal (k_hh_json_file_name, "max_volume_handwash", &household.max_volume_handwash, 0., DBL_MAX);
        lookup_decimal (k_hh_json_file_name, "min_volume_shower", &household.min_volume_shower, 0., DBL_MAX);
        lookup_decimal (k_hh_json_file_name, "max_volume_shower", &household.max_volume_shower, 0., DBL_MAX);
        lookup_decimal (k_hh_json_file_name, "min_volume_bath", &household.min_volume_bath, 0., DBL_MAX);
        lookup_decimal (k_hh_json_file_name, "max_volume_bath", &household.max_volume_bath, 0., DBL_MAX);
        lookup_decimal (k_hh_json_file_name, "urban_car_percentage", &household.urban_car_percentage, 0., 100.);
        lookup_decimal (k_hh_json_file_name, "rural_car_percentage", &household.rural_car_percentage, 0., 100.);
        delete [] dictionary;
    }

    // The vacation data is also household related, but we read it from a separate file.
    snprintf (file_name, sizeof(file_name), "countries/%s/%s", location->country, k_vacation_json_file_name);
    create_dictionary (k_vacation_json_file_name, file_name);
    if (dictionary)
    {
        double last_value = 0.0;
        for (int m=0; m<12; m++)
        {
            for (int d=0; d<31; d++)
            {
                char keystr[32];
                sprintf (keystr, "vacation_percentage.%d.%d", d+1, m+1);
                lookup_decimal (k_vacation_json_file_name, keystr, &household.vacation_percentage[m][d], 0., 100.);
                if (household.vacation_percentage[m][d] == -1)    // key not found
                {
                    household.vacation_percentage[m][d] = last_value;
                }
                else
                {
                    last_value = household.vacation_percentage[m][d];
                }
            }
        }
        delete [] dictionary;
    }

    // Read the appliance related parameters from tech.json.
    // First look in the working directory, and if it's not there,
    // go to the country specific directory

    sprintf (file_name, "countries/%s/%s", location->country, k_tech_json_file_name);
    create_dictionary (k_tech_json_file_name, file_name);
    if (dictionary)
    {
        lookup_decimal (k_tech_json_file_name, "battery.frequency_solar", &battery.frequency_solar, 0., 100.);
        lookup_decimal (k_tech_json_file_name, "battery.frequency_non_solar", &battery.frequency_non_solar, 0., 100.);
        lookup_decimal (k_tech_json_file_name, "battery.capacity_in_days", &battery.capacity_in_days, 0., DBL_MAX);
        lookup_boolean (k_tech_json_file_name, "battery.allow_grid_charge_solar", &battery.allow_grid_charge_solar);
        lookup_decimal (k_tech_json_file_name, "battery.smartgrid_enabled", &battery.smartgrid_enabled, 0., 100.);
        lookup_decimal (k_tech_json_file_name, "battery.installation_costs", &battery.installation_costs, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "battery.avg_lifetime", &battery.avg_lifetime, 1.0, DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "battery.min_price", &battery.min_price, 0.0, DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "battery.max_price", &battery.max_price, 0.0, DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "battery.min_capacity_per_resident", &battery.min_capacity_per_resident, 0.0, DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "battery.max_capacity_per_resident", &battery.max_capacity_per_resident, 0.0, DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "battery.min_eff_charging", &battery.min_eff_charging, 0.0, 1.0);
        lookup_decimal (k_tech_json_file_name, "battery.max_eff_charging", &battery.max_eff_charging, 0.0, 1.0);
        lookup_decimal (k_tech_json_file_name, "battery.min_eff_discharging", &battery.min_eff_discharging, 0.0, 1.0);
        lookup_decimal (k_tech_json_file_name, "battery.max_eff_discharging", &battery.max_eff_discharging, 0.0, 1.0);
        lookup_decimal (k_tech_json_file_name, "battery.max_power_charging", &battery.max_power_charging, 0.0, 1.0);
        lookup_decimal (k_tech_json_file_name, "battery.max_power_discharging", &battery.max_power_discharging, 0.0, 1.0);

        lookup_decimal (k_tech_json_file_name, "boiler.power_factor", &boiler.power_factor, -1.0, 1.0);

        lookup_decimal (k_tech_json_file_name, "circulation_pump.controlled", &circpump.controlled, -DBL_MAX, 100.);
        lookup_decimal (k_tech_json_file_name, "circulation_pump.power_per_size", &circpump.power_per_size, 0.0, DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "circulation_pump.power_factor", &circpump.power_factor, -1.0, 1.0);
        lookup_vector  (k_tech_json_file_name, "circulation_pump.rnd_first_day", circpump.rnd_first_day, 4);
        lookup_vector  (k_tech_json_file_name, "circulation_pump.rnd_last_day", circpump.rnd_last_day, 4);
        lookup_integer (k_tech_json_file_name, "circulation_pump.first_month", &circpump.first_month, 1, 12);
        lookup_integer (k_tech_json_file_name, "circulation_pump.last_month", &circpump.last_month, 1, 12);
        lookup_integer (k_tech_json_file_name, "circulation_pump.time_1", &circpump.time_1, 0, k_seconds_per_day);
        lookup_integer (k_tech_json_file_name, "circulation_pump.time_2", &circpump.time_2, 0, k_seconds_per_day);
        lookup_vector  (k_tech_json_file_name, "circulation_pump.rnd_time_on", circpump.rnd_time_on, 2, false);
        lookup_vector  (k_tech_json_file_name, "circulation_pump.rnd_time_off", circpump.rnd_time_off, 2, false);

        lookup_decimal (k_tech_json_file_name, "computer.power", &computer.power, 0.0, DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "computer.power_factor", &computer.power_factor, -1.0, 1.0);
        lookup_decimal (k_tech_json_file_name, "computer.duration_mean", &computer.duration_mean, 0.0, DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "computer.duration_sigma", &computer.duration_sigma, 0.0, DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "computer.duration_fraction", &computer.duration_fraction, 0.0, 1.0);
        lookup_decimal (k_tech_json_file_name, "computer.duration_fraction_saturday", &computer.duration_fraction_saturday, 0.0, 1.0);
        lookup_decimal (k_tech_json_file_name, "computer.duration_fraction_sunday", &computer.duration_fraction_sunday, 0.0, 1.0);
        lookup_vector  (k_tech_json_file_name, "computer.time_offset", computer.time_offset, 3, false);
        lookup_vector  (k_tech_json_file_name, "computer.time_offset_saturday", computer.time_offset_saturday, 3, false);
        lookup_vector  (k_tech_json_file_name, "computer.time_offset_sunday", computer.time_offset_sunday, 3, false);
        lookup_vector  (k_tech_json_file_name, "computer.rnd", computer.rnd, 3);
        lookup_vector  (k_tech_json_file_name, "computer.rnd_saturday", computer.rnd_saturday, 3);
        lookup_vector  (k_tech_json_file_name, "computer.rnd_sunday", computer.rnd_sunday, 3);
        lookup_decimal (k_tech_json_file_name, "computer.time_2_mean", &computer.time_2_mean, 0.0, DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "computer.time_2_sigma", &computer.time_2_sigma, 0.0, DBL_MAX);

        lookup_vector  (k_tech_json_file_name, "stove.power", stove.power, k_max_residents, false);
        lookup_decimal (k_tech_json_file_name, "stove.power_factor", &stove.power_factor, -1.0, 1.0);
        lookup_integer (k_tech_json_file_name, "stove.duration_1_percent", &stove.duration_1_percent, 0, 100);
        lookup_integer (k_tech_json_file_name, "stove.duration_2_percent", &stove.duration_2_percent, 0, 100);
        lookup_integer (k_tech_json_file_name, "stove.duration_2_percent_saturday", &stove.duration_2_percent_saturday, 0, 100);
        lookup_integer (k_tech_json_file_name, "stove.duration_2_percent_sunday", &stove.duration_2_percent_sunday, 0, 100);
        lookup_decimal (k_tech_json_file_name, "stove.time_offset", &stove.time_offset, 0., k_seconds_per_day);
        lookup_vector  (k_tech_json_file_name, "stove.rnd_duration_1", stove.rnd_duration_1, 4, false);
        lookup_vector  (k_tech_json_file_name, "stove.rnd_duration_2", stove.rnd_duration_2, 4, false);
        lookup_vector  (k_tech_json_file_name, "stove.rnd_duration_3", stove.rnd_duration_3, 4, false);
        lookup_decimal (k_tech_json_file_name, "stove.time_2_mean", &stove.time_2_mean, 0.0, DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "stove.time_2_sigma", &stove.time_2_sigma, 0.0, DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "stove.time_3_mean", &stove.time_3_mean, 0.0, DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "stove.time_3_sigma", &stove.time_3_sigma, 0.0, DBL_MAX);

        lookup_decimal (k_tech_json_file_name, "dishwasher.smartgrid_enabled", &dishwasher.smartgrid_enabled, 0., 100.);
        lookup_decimal (k_tech_json_file_name, "dishwasher.smart", &dishwasher.smart, 0., 100.);
        lookup_vector  (k_tech_json_file_name, "dishwasher.energy_classes", dishwasher.energy_classes, dishwasher.num_energy_classes, true);
        lookup_decimal (k_tech_json_file_name, "dishwasher.hours_per_cycle", &dishwasher.hours_per_cycle, 0., 3.0);
        lookup_vector  (k_tech_json_file_name, "dishwasher.place_settings", dishwasher.place_settings, k_max_residents, false);
        lookup_vector  (k_tech_json_file_name, "dishwasher.SAEc_small", dishwasher.SAEc_small, 2, false);
        lookup_vector  (k_tech_json_file_name, "dishwasher.SAEc_big", dishwasher.SAEc_big, 2, false);
        lookup_decimal (k_tech_json_file_name, "dishwasher.factor", &dishwasher.factor, 0., DBL_MAX);
        lookup_vector  (k_tech_json_file_name, "dishwasher.probability", dishwasher.probability, 2);
        lookup_integer (k_tech_json_file_name, "dishwasher.ignore_price", &dishwasher.ignore_price, 0, 100);
        lookup_integer (k_tech_json_file_name, "dishwasher.fraction", &dishwasher.fraction, 0, 100);
        lookup_decimal (k_tech_json_file_name, "dishwasher.timer_1_mean", &dishwasher.timer_1_mean, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "dishwasher.timer_1_sigma", &dishwasher.timer_1_sigma, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "dishwasher.timer_2_mean", &dishwasher.timer_2_mean, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "dishwasher.timer_2_sigma", &dishwasher.timer_2_sigma, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "dishwasher.timer_3_mean", &dishwasher.timer_3_mean, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "dishwasher.timer_3_sigma", &dishwasher.timer_3_sigma, 0., DBL_MAX);
        lookup_integer (k_tech_json_file_name, "dishwasher.preview_length", &dishwasher.preview_length, 0, INT_MAX);
        lookup_integer (k_tech_json_file_name, "dishwasher.peak_delay", &dishwasher.peak_delay, 0., INT_MAX);
        lookup_decimal (k_tech_json_file_name, "dishwasher.power_factor", &dishwasher.power_factor, -1.0, 1.0);

        lookup_decimal (k_tech_json_file_name, "e_vehicle.smartgrid_enabled", &e_vehicle.smartgrid_enabled, 0., 100.);
        lookup_decimal (k_tech_json_file_name, "e_vehicle.smart", &e_vehicle.smart, 0., 100.);
        lookup_decimal (k_tech_json_file_name, "e_vehicle.departure_delay", &e_vehicle.departure_delay, 0., k_seconds_per_day);
        for (int i=0; i<k_num_ev_models; i++)
        {
            char group_name[16];
            char setting_name[64];
            sprintf (group_name, "model %d", i+1);
            sprintf (setting_name, "%s.name", group_name);
            if (lookup_string (setting_name, e_vehicle.models[i].name, sizeof (e_vehicle.models[i].name)))
            {
                sprintf (setting_name, "%s.consumption_per_100km", group_name);
                lookup_decimal (k_tech_json_file_name, setting_name, &(e_vehicle.models[i].consumption_per_100km), 0., DBL_MAX);
                sprintf (setting_name, "%s.battery_capacity", group_name);
                lookup_decimal (k_tech_json_file_name, setting_name, &(e_vehicle.models[i].battery_capacity), 0., DBL_MAX);
                sprintf (setting_name, "%s.max_charge_power_AC", group_name);
                lookup_decimal (k_tech_json_file_name, setting_name, &(e_vehicle.models[i].max_charge_power_AC), 0., DBL_MAX);
                sprintf (setting_name, "%s.max_charge_power_DC", group_name);
                lookup_decimal (k_tech_json_file_name, setting_name, &(e_vehicle.models[i].max_charge_power_DC), 0., DBL_MAX);
                sprintf (setting_name, "%s.charging_curve", group_name);
                lookup_vector  (k_tech_json_file_name, setting_name, e_vehicle.models[i].charging_curve, k_num_curve_points, false);
                if (i>0) E_Vehicle::num_models++;
            }
            else break;
        }

        lookup_decimal (k_tech_json_file_name, "freezer.smartgrid_enabled", &freezer.smartgrid_enabled, 0., 100.);
        lookup_decimal (k_tech_json_file_name, "freezer.smart", &freezer.smart, 0., 100.);
        lookup_decimal (k_tech_json_file_name, "freezer.temperature_min", &freezer.min_temperature, -DBL_MAX, DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "freezer.temperature_max", &freezer.max_temperature, -DBL_MAX, DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "freezer.delta_t_rise_factor", &freezer.delta_t_rise_factor, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "freezer.delta_t_rise_mean", &freezer.delta_t_rise_mean, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "freezer.delta_t_rise_sigma", &freezer.delta_t_rise_sigma, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "freezer.delta_t_drop_factor", &freezer.delta_t_drop_factor, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "freezer.delta_t_drop_mean", &freezer.delta_t_drop_mean, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "freezer.delta_t_drop_sigma", &freezer.delta_t_drop_sigma, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "freezer.Vc_per_resident", &freezer.Vc_per_resident, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "freezer.Tc", &freezer.Tc, -DBL_MAX, DBL_MAX);
        lookup_integer (k_tech_json_file_name, "freezer.mn_percentage", &freezer.mn_percentage, 0, 100);
        lookup_decimal (k_tech_json_file_name, "freezer.factor_1", &freezer.factor_1, 0., DBL_MAX);
        lookup_vector  (k_tech_json_file_name, "freezer.energy_classes", freezer.energy_classes, freezer.num_energy_classes, true);
        lookup_decimal (k_tech_json_file_name, "freezer.power_factor", &freezer.power_factor, -1.0, 1.0);

        lookup_decimal (k_tech_json_file_name, "fridge.smartgrid_enabled", &fridge.smartgrid_enabled, 0., 100.);
        lookup_decimal (k_tech_json_file_name, "fridge.smart", &fridge.smart, 0., 100.);
        lookup_decimal (k_tech_json_file_name, "fridge.temperature_min", &fridge.min_temperature, -DBL_MAX, DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "fridge.temperature_max", &fridge.max_temperature, -DBL_MAX, DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "fridge.delta_t_rise_factor", &fridge.delta_t_rise_factor, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "fridge.delta_t_rise_mean", &fridge.delta_t_rise_mean, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "fridge.delta_t_rise_sigma", &fridge.delta_t_rise_sigma, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "fridge.delta_t_drop_factor", &fridge.delta_t_drop_factor, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "fridge.delta_t_drop_mean", &fridge.delta_t_drop_mean, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "fridge.delta_t_drop_sigma", &fridge.delta_t_drop_sigma, 0., DBL_MAX);
        lookup_vector  (k_tech_json_file_name, "fridge.Vc_mean", fridge.Vc_mean, k_max_residents, false);
        lookup_vector  (k_tech_json_file_name, "fridge.Vc_sigma", fridge.Vc_sigma, k_max_residents, false);
        lookup_vector  (k_tech_json_file_name, "fridge.Vc_low", fridge.Vc_low, k_max_residents, false);
        lookup_vector  (k_tech_json_file_name, "fridge.Vc_high", fridge.Vc_high, k_max_residents, false);
        lookup_decimal (k_tech_json_file_name, "fridge.Tc", &fridge.Tc, -DBL_MAX, DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "fridge.factor_1", &fridge.factor_1, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "fridge.factor_2", &fridge.factor_2, 0., DBL_MAX);
        lookup_vector  (k_tech_json_file_name, "fridge.energy_classes", fridge.energy_classes, fridge.num_energy_classes, true);
        lookup_decimal (k_tech_json_file_name, "fridge.power_factor", &fridge.power_factor, -1.0, 1.0);

        lookup_decimal (k_tech_json_file_name, "heating.smartgrid_enabled", &heating.smartgrid_enabled, 0., 100.);
        lookup_decimal (k_tech_json_file_name, "heating.kW_per_m2", &heating.kW_per_m2, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "heating.power_factor", &heating.power_factor, -1.0, 1.0);

        lookup_decimal (k_tech_json_file_name, "heat_pump.min_eff", &heatpump.min_eff, 0.0, 1.0);
        lookup_decimal (k_tech_json_file_name, "heat_pump.max_eff", &heatpump.max_eff, 0.0, 1.0);
        lookup_decimal (k_tech_json_file_name, "heat_pump.min_temperature", &heatpump.min_temperature, -DBL_MAX, DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "heat_pump.max_temperature", &heatpump.max_temperature, -DBL_MAX, DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "heat_pump.kW_per_m2", &heatpump.kW_per_m2, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "heat_pump.power_factor", &heatpump.power_factor, -1.0, 1.0);

        lookup_decimal (k_tech_json_file_name, "heat_storage.liter_per_m2", &heat_storage.liter_per_m2, 0.0, DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "heat_storage.max_temperature", &heat_storage.max_temperature, 0.0, DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "heat_storage.max_heat_power", &heat_storage.max_heat_power, 0.0, DBL_MAX);

        lookup_decimal (k_tech_json_file_name, "air_conditioner.min_eff", &aircon.min_eff, 0.0, 1.0);
        lookup_decimal (k_tech_json_file_name, "air_conditioner.max_eff", &aircon.max_eff, 0.0, 1.0);
        lookup_decimal (k_tech_json_file_name, "air_conditioner.kW_per_m2", &aircon.kW_per_m2, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "air_conditioner.power_factor", &aircon.power_factor, -1.0, 1.0);

        lookup_vector  (k_tech_json_file_name, "light.energy_classes", light.energy_classes, light.num_energy_classes, true);
        lookup_integer (k_tech_json_file_name, "light.luminous_flux_mean", &light.luminous_flux_mean, 0, INT_MAX);
        lookup_integer (k_tech_json_file_name, "light.luminous_flux_sigma", &light.luminous_flux_sigma, 0, INT_MAX);
        lookup_integer (k_tech_json_file_name, "light.luminous_flux_min", &light.luminous_flux_min, 0, INT_MAX);
        lookup_integer (k_tech_json_file_name, "light.luminous_flux_max", &light.luminous_flux_max, 0, INT_MAX);
        lookup_decimal (k_tech_json_file_name, "light.sigma_morning", &light.sigma_morning, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "light.sigma_evening", &light.sigma_evening, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "light.power_factor", &light.power_factor, -1.0, 1.0);

        lookup_decimal (k_tech_json_file_name, "solar_module.system_loss", &solar_module.system_loss, 0., 100.);
        lookup_decimal (k_tech_json_file_name, "solar_module.production_ratio", &solar_module.production_ratio, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "solar_module.min_area", &solar_module.min_area, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "solar_module.max_area", &solar_module.max_area, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "solar_module.min_eff", &solar_module.min_eff, 0., 1.);
        lookup_decimal (k_tech_json_file_name, "solar_module.max_eff", &solar_module.max_eff, 0., 1.);
        lookup_decimal (k_tech_json_file_name, "solar_module.power_factor", &solar_module.power_factor, -1.0, 1.0);

        lookup_decimal (k_tech_json_file_name, "solar_collector.area_factor_1", &solar_collector.area_factor_1, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "solar_collector.area_factor_2", &solar_collector.area_factor_2, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "solar_collector.eff_0", &solar_collector.eff_0, 0., 1.);
        lookup_decimal (k_tech_json_file_name, "solar_collector.min_flow_rate", &solar_collector.min_flow_rate, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "solar_collector.max_flow_rate", &solar_collector.max_flow_rate, 0., DBL_MAX);

        lookup_decimal (k_tech_json_file_name, "tumble_dryer.smartgrid_enabled", &dryer.smartgrid_enabled, 0., 100.);
        lookup_vector  (k_tech_json_file_name, "tumble_dryer.energy_classes", dryer.energy_classes, dryer.num_energy_classes, true);
        lookup_decimal (k_tech_json_file_name, "tumble_dryer.hours_per_cycle", &dryer.hours_per_cycle, 0., 3.0);
        lookup_vector  (k_tech_json_file_name, "tumble_dryer.capacity", dryer.capacity, k_max_residents, false);
        lookup_integer (k_tech_json_file_name, "tumble_dryer.ignore_price", &dryer.ignore_price, 0, 100);
        lookup_integer (k_tech_json_file_name, "tumble_dryer.peak_delay", &dryer.peak_delay, 0, INT_MAX);
        lookup_decimal (k_tech_json_file_name, "tumble_dryer.power_factor", &dryer.power_factor, -1.0, 1.0);

        lookup_vector  (k_tech_json_file_name, "tv.energy_classes", tv.energy_classes, tv.num_energy_classes, true);
        lookup_decimal (k_tech_json_file_name, "tv.diagonal_1", &tv.diagonal_1, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "tv.diagonal_2", &tv.diagonal_2, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "tv.diagonal_3", &tv.diagonal_3, 0., DBL_MAX);
        lookup_vector  (k_tech_json_file_name, "tv.avg_duration", tv.avg_duration, k_max_residents, false);
        lookup_decimal (k_tech_json_file_name, "tv.factor_mean", &tv.factor_mean, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "tv.factor_sigma", &tv.factor_sigma, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "tv.factor_mean_we", &tv.factor_mean_we, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "tv.factor_sigma_we", &tv.factor_sigma_we, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "tv.duration_factor", &tv.duration_factor, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "tv.duration_factor_sat", &tv.duration_factor_sat, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "tv.duration_factor_sun", &tv.duration_factor_sun, 0., DBL_MAX);
        lookup_vector  (k_tech_json_file_name, "tv.random", tv.random, 3);
        lookup_vector  (k_tech_json_file_name, "tv.random_sat", tv.random_sat, 3);
        lookup_vector  (k_tech_json_file_name, "tv.random_sun", tv.random_sun, 3);
        lookup_vector  (k_tech_json_file_name, "tv.delay", tv.delay, 3);
        lookup_vector  (k_tech_json_file_name, "tv.delay_sat", tv.delay_sat, 3);
        lookup_vector  (k_tech_json_file_name, "tv.delay_sun", tv.delay_sun, 3);
        lookup_decimal (k_tech_json_file_name, "tv.time_2_mean", &tv.time_2_mean, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "tv.time_2_sigma", &tv.time_2_sigma, 0., DBL_MAX);
        lookup_decimal (k_tech_json_file_name, "tv.power_factor", &tv.power_factor, -1.0, 1.0);

        lookup_vector  (k_tech_json_file_name, "vacuum.energy_classes", vacuum.energy_classes, vacuum.num_energy_classes, true);
        lookup_integer (k_tech_json_file_name, "vacuum.timer_min", &vacuum.timer_min, 0, INT_MAX);
        lookup_integer (k_tech_json_file_name, "vacuum.timer_max", &vacuum.timer_max, 0, INT_MAX);
        lookup_integer (k_tech_json_file_name, "vacuum.timer_factor", &vacuum.timer_factor, 0, INT_MAX);
        lookup_decimal (k_tech_json_file_name, "vacuum.power_factor", &vacuum.power_factor, -1.0, 1.0);

        lookup_decimal (k_tech_json_file_name, "washing_machine.smartgrid_enabled", &wmachine.smartgrid_enabled, 0., 100.);
        lookup_decimal (k_tech_json_file_name, "washing_machine.smart", &wmachine.smart, 0., 100.);
        lookup_vector  (k_tech_json_file_name, "washing_machine.energy_classes", wmachine.energy_classes, wmachine.num_energy_classes, true);
        lookup_decimal (k_tech_json_file_name, "washing_machine.hours_per_cycle", &wmachine.hours_per_cycle, 0., 3.0);
        lookup_vector  (k_tech_json_file_name, "washing_machine.capacity", wmachine.capacity, k_max_residents, false);
        lookup_integer (k_tech_json_file_name, "washing_machine.random_limit", &wmachine.random_limit, 0, 100);
        lookup_integer (k_tech_json_file_name, "washing_machine.ignore_price", &wmachine.ignore_price, 0, 100);
        lookup_integer (k_tech_json_file_name, "washing_machine.best_price_lookahead", &wmachine.best_price_lookahead, 60, 10080);
        lookup_integer (k_tech_json_file_name, "washing_machine.timer_mean", &wmachine.timer_mean, 0, INT_MAX);
        lookup_integer (k_tech_json_file_name, "washing_machine.timer_sigma", &wmachine.timer_sigma, 0, INT_MAX);
        lookup_integer (k_tech_json_file_name, "washing_machine.peak_delay", &wmachine.peak_delay, 0, INT_MAX);
        lookup_decimal (k_tech_json_file_name, "washing_machine.power_factor", &wmachine.power_factor, -1.0, 1.0);

        delete [] dictionary;
    }
}


Configuration::~Configuration()
{
    delete [] price[GRID].profiles;
    delete [] price[SOLAR].profiles;
    delete location;
}


void Configuration::print_log (int households, double days)
{
    FILE *fp = NULL;

    // Print log for resLoadSIM.json

    open_file (&fp, k_rls_json_log_file_name, "w");
    if (comments_in_logfiles)
    {
        fprintf (fp, "// This log was created by ResLoadSIM version %s\n", VERSION);
        fprintf (fp, "// Arguments: %d %.2lf (households days)\n", households, days);
    }
    fprintf (fp, "{\n");
    if (comments_in_logfiles)
    {
        fprintf (fp, "// The location is used to determine sunrise, sunset and the climate.\n");
        fprintf (fp, "// Available locations are defined in the directory 'resLoadSIM/locations'.\n\n");
    }
    log (fp, "location", location_name, 2);
    if (comments_in_logfiles)
    {
        fprintf (fp, "\n// The name of the file that contains the solar radiation and temperature data (e.g. PVGIS file).\n\n");
    }
    log (fp, "pv_data_file_name", pv_data_file_name, 2);
    if (comments_in_logfiles)
    {
        fprintf (fp, "\n// The name of the file that contains the solar radiation forecast.\n\n");
    }
    log (fp, "pv_forecast_file_name", pv_forecast_file_name, 2);
    if (comments_in_logfiles)
    {
        fprintf (fp, "\n// A vector containing the years used for calculating the average solar production in the simulation forerun.\n");
        fprintf (fp, "// If the user provided vector is empty, resLoadSIM will choose a reference year from the PVGIS timeseries data.\n\n");
    }
    log (fp, "solar_production_reference_year", solar_production_reference_year, num_ref_years, 2);
    if (comments_in_logfiles)
    {
        fprintf (fp, "\n// The following battery charging strategies are available:\n");
        fprintf (fp, "// 0 = charge whenever solar production exceeds consumption\n");
        fprintf (fp, "// 2 = first feed to grid, then charge batteries\n");
        fprintf (fp, "// 3 = strategy 0, as long as battery charge is below a given threshold, otherwise strategy 2\n");
        fprintf (fp, "// 4 = method without the need of a forecast\n//\n");
        fprintf (fp, "// The following production forecast methods are available:\n");
        fprintf (fp, "// 0 = no forecast (makes only sense with battery_charging.strategy = 0)\n");
        fprintf (fp, "// 1 = perfect forecast of solar production (use PVGIS data)\n");
        fprintf (fp, "// 2 = use the production of the previous day as a forecast\n");
        fprintf (fp, "// 3 = read solar forecast data from a file\n");
        fprintf (fp, "// 4 = read solar overproduction from a file\n");
        fprintf (fp, "// 5 = read feed_to_grid from a file\n\n");
    }
    fprintf (fp, "  \"battery_charging\":\n  {\n");
    log (fp, "strategy", battery_charging.strategy, 4);
    log (fp, "production_forecast_method", battery_charging.production_forecast_method, 4);
    log (fp, "feed_in_limit", battery_charging.feed_in_limit, 2, 4);
    log (fp, "precharge_threshold", battery_charging.precharge_threshold, 2, 4);
    log (fp, "shared", battery_charging.shared, 4);
    fseek (fp, -2, SEEK_CUR);
    fprintf (fp, "\n  },\n");
    if (comments_in_logfiles)
    {
        fprintf (fp, "\n// Use the PETSc power flow solver?\n");
        fprintf (fp, "// The power flow solver is called every step_size timesteps.\n");
        fprintf (fp, "// If step_size = 0, it is not used at all.\n");
        fprintf (fp, "// ov_control and uv_control are used to turn on/off overvoltage and undervoltage control.\n");
        fprintf (fp, "// uv_lower_limit: grid voltage magnitude that triggers energy conservation mode in affected housholds\n");
        fprintf (fp, "// uv_upper_limit: if voltage levels recover above this limit, then energy conservation mode is turned off again stepwise\n");
        fprintf (fp, "// ov_lower_limit: if voltage falls below this limit, additional consumption is turned off again\n");
        fprintf (fp, "// ov_upper_limit: if grid voltage level exceeds this limit, household consumption is raised\n");
        fprintf (fp, "// output_level = 0: no output related to the PETSc power flow solver\n");
        fprintf (fp, "//                1: transformer files only\n");
        fprintf (fp, "//                2: transformer files, partial input/output of the power flow solver\n");
        fprintf (fp, "//                3: transformer files, full input/output of the power flow solver\n\n");
    }
    fprintf (fp, "  \"powerflow\":\n  {\n");
    log (fp, "case_file_name", powerflow.case_file_name, 4);
    log (fp, "step_size", powerflow.step_size, 4);
    log (fp, "uv_control", powerflow.uv_control, 4);
    log (fp, "uv_lower_limit", powerflow.uv_lower_limit, 3, 4);
    log (fp, "uv_upper_limit", powerflow.uv_upper_limit, 3, 4);
    log (fp, "ov_control", powerflow.ov_control, 4);
    log (fp, "ov_lower_limit", powerflow.ov_lower_limit, 3, 4);
    log (fp, "ov_upper_limit", powerflow.ov_upper_limit, 3, 4);
    log (fp, "output_level", powerflow.output_level, 4);
    fseek (fp, -2, SEEK_CUR);
    fprintf (fp, "\n  },\n");
    if (comments_in_logfiles)
    {
        fprintf (fp, "\n// The following producer control options are available:\n");
        fprintf (fp, "// 0 = no control\n");
        fprintf (fp, "// 1 = peak shaving (keep load below a limit, e.g. 85%% of peak)\n");
        fprintf (fp, "// 2 = follow a given load profile\n");
        fprintf (fp, "// 3 = try to compensate a gap between projected and actual production\n");
        fprintf (fp, "// 4 = decentralized control via electricity tariff\n\n");
    }
    log (fp, "control", control, 2);
    if (comments_in_logfiles)
    {
        fprintf (fp, "\n// Threshold for peak shaving (used only when control = 1).\n");
        fprintf (fp, "// A relative threshold is given in percent, an absolute one in kWh.\n\n");
    }
    fprintf (fp, "  \"peak_shaving\":\n  {\n");
    log (fp, "relative", peak_shaving.relative, 4);
    log (fp, "threshold", peak_shaving.threshold, 2, 4);
    fseek (fp, -2, SEEK_CUR);
    fprintf (fp, "\n  },\n");
    if (comments_in_logfiles)
    {
        fprintf (fp, "\n// Seed for the random number generator.\n");
        fprintf (fp, "// Values >0 are used as seed. If seed = 0, then the current time will be used as seed.\n\n");
    }
    log (fp, "seed", seed, 2);
    if (comments_in_logfiles)
    {
        fprintf (fp, "\n// The following output options are available:\n");
        fprintf (fp, "// 0 = all power data is written to a single file\n");
        fprintf (fp, "// 1 = several power output files (one per appliance type)\n");
        fprintf (fp, "// 2 = one file per appliance type + a single file with all data)\n\n");
    }
    log (fp, "output", output, 2);
    if (comments_in_logfiles) fprintf (fp, "\n// The date and time at which we want to start the simulation:\n\n");
    fprintf (fp, "  \"start\":\n  {\n");
    log (fp, "day", start.day, 4);
    log (fp, "month", start.month, 4);
    log (fp, "year", start.year, 4);
    log (fp, "time", start.time, 2, 4);
    fseek (fp, -2, SEEK_CUR);
    fprintf (fp, "\n  },\n");
    if (comments_in_logfiles) fprintf (fp, "\n// The number of days for the transient phase (given as a decimal value)\n\n");
    log (fp, "transient_time", transient_time, 2, 2);
    if (comments_in_logfiles)
    {
        fprintf (fp, "\n// Does the simulation take daylight saving time into account?\n");
        fprintf (fp, "// 0 = no DST (wintertime only)\n");
        fprintf (fp, "// 1 = standard DST (clock changes twice a year)\n");
        fprintf (fp, "// 2 = permanent DST (summertime only)\n\n");
    }
    log (fp, "daylight_saving_time", daylight_saving_time, 2);
    log (fp, "timestep_size", timestep_size, 2, 2);
    if (comments_in_logfiles)
    {
        fprintf (fp, "\n// Activate the simulation of space heating? This includes the calculation of the\n");
        fprintf (fp, "// heat demand (ISO 52016-1), which increases memory usage and runtime significantly\n\n");
    }
    log (fp, "simulate_heating", simulate_heating, 2);
    if (comments_in_logfiles) fprintf (fp, "\n// Activate the ventilation model\n\n");
    log (fp, "ventilation_model", ventilation_model, 2);
    if (comments_in_logfiles) fprintf (fp, "\n// Choose whether some appliances like washing machines can have a variable load\n\n");
    log (fp, "variable_load", variable_load, 2);
    if (comments_in_logfiles) fprintf (fp, "\n// Turn on/off comments in logfiles? Useful in case the log is going to be used as a JSON input\n\n");
    log (fp, "comments_in_logfiles", comments_in_logfiles, 2);
    if (comments_in_logfiles) fprintf (fp, "\n// Use the energy efficiency class definition of the year 2021?\n\n");
    log (fp, "energy_classes_2021", energy_classes_2021, 2);
    if (comments_in_logfiles)
    {
        fprintf (fp, "\n// This group of settings defines the price for the electricity\n");
        fprintf (fp, "// delivered by the grid. The first setting is a list of profiles.\n");
        fprintf (fp, "// Each profile consists of a set of arrays, which define intervals\n");
        fprintf (fp, "// in time, together with a price. The second setting is a\n");
        fprintf (fp, "// sequence of profiles.\n\n");
    }
    fprintf (fp, "  \"price_grid\":\n  {\n");
    fprintf (fp, "    \"profiles\":\n    [\n");   // start array of profiles
    for (int i=0; i<price[GRID].num_profiles; i++)
    {
        fprintf (fp, "      [");
        for (int j=0; j<price[GRID].profiles[i].length; j++)
        {
            fprintf (fp, "[%.2lf, %.2lf, %.2lf]",
                     price[GRID].profiles[i].begin[j],
                     price[GRID].profiles[i].end[j],
                     price[GRID].profiles[i].price[j]);
            if (j < price[GRID].profiles[i].length-1) fprintf (fp, ", ");
        }
        if (i < price[GRID].num_profiles-1) fprintf (fp, "],\n");
        else fprintf (fp, "]\n");
    }
    fprintf (fp, "    ],\n");  // end array of profiles
    fprintf (fp, "    \"sequence\": [%d", price[GRID].sequence[0]);
    for (int i=1; i<price[GRID].seq_length; i++) fprintf (fp, ", %d", price[GRID].sequence[i]);
    fprintf (fp, "]\n  },\n");
    if (comments_in_logfiles)
    {
        fprintf (fp, "\n// Same as above, but this time it's the tariff a household gets for feeding\n");
        fprintf (fp, "// PV electricity into the grid.\n\n");
    }
    fprintf (fp, "  \"price_solar\":\n  {\n");
    fprintf (fp, "    \"profiles\":\n    [\n");   // start array of profiles
    for (int i=0; i<price[SOLAR].num_profiles; i++)
    {
        fprintf (fp, "      [");
        for (int j=0; j<price[SOLAR].profiles[i].length; j++)
        {
            fprintf (fp, "[%.2lf, %.2lf, %.2lf]",
                     price[SOLAR].profiles[i].begin[j],
                     price[SOLAR].profiles[i].end[j],
                     price[SOLAR].profiles[i].price[j]);
            if (j < price[SOLAR].profiles[i].length-1) fprintf (fp, ", ");
        }
        if (i < price[SOLAR].num_profiles-1) fprintf (fp, "],\n");
        else fprintf (fp, "]\n");
    }
    fprintf (fp, "    ],\n");  // end array of profiles
    fprintf (fp, "    \"sequence\": [%d", price[SOLAR].sequence[0]);
    for (int i=1; i<price[SOLAR].seq_length; i++) fprintf (fp, ", %d", price[SOLAR].sequence[i]);
    fprintf (fp, "]\n  }\n}\n");
    fclose (fp);

    // Print log for the household configuration

    open_file (&fp, k_hh_json_log_file_name, "w");

    if (comments_in_logfiles)
    {
        fprintf (fp, "// This log was created by ResLoadSIM version %s\n", VERSION);
        fprintf (fp, "// Arguments: %d %.2lf (households days)\n", households, days);
    }
    fprintf (fp, "{\n");
    log (fp, "size_distribution", household.size_distribution, k_max_residents, 1, 2);
    log (fp, "retired_1", household.retired_1, 2, 2);
    log (fp, "retired_2", household.retired_2, 2, 2);
    log (fp, "min_area", household.min_area, k_max_residents, 2, 2);
    log (fp, "max_area", household.max_area, k_max_residents, 2, 2);
    log (fp, "set_temperature_heating_day", household.set_temperature_H_day, 1, 2);
    log (fp, "set_temperature_heating_night", household.set_temperature_H_night, 1, 2);
    log (fp, "set_temperature_cooling", household.set_temperature_C, 1, 2);
    log (fp, "reduce_heat", household.reduce_heat, 1, 2);
    log (fp, "heating_period_start_day", household.heating_period_start_day, 2);
    log (fp, "heating_period_start_month", household.heating_period_start_month, 2);
    log (fp, "heating_period_end_day", household.heating_period_end_day, 2);
    log (fp, "heating_period_end_month", household.heating_period_end_month, 2);
    log (fp, "min_init_laundry", household.min_init_laundry, 2);
    log (fp, "max_init_laundry", household.max_init_laundry, 2);
    log (fp, "min_delta_laundry", household.min_delta_laundry, k_max_residents, 2, 2);
    log (fp, "max_delta_laundry", household.max_delta_laundry, k_max_residents, 2, 2);
    log (fp, "second_fridge", household.second_fridge, k_max_residents, 1, 2);
    log (fp, "second_tv", household.second_tv, k_max_residents, 1, 2);
    log (fp, "third_tv", household.third_tv, k_max_residents, 1, 2);
    log (fp, "second_computer", household.second_computer, k_max_residents, 1, 2);
    log (fp, "min_vacuum_interval", household.min_vacuum_interval, 2);
    log (fp, "max_vacuum_interval", household.max_vacuum_interval, 2);
    log (fp, "light_factor", household.light_factor, k_max_residents, 2, 2);
    log (fp, "rnd_wakeup", household.rnd_wakeup, 4, 1, 2);
    log (fp, "rnd_wakeup_weekend", household.rnd_wakeup_weekend, 4, 1, 2);
    log (fp, "rnd_wakeup_retired", household.rnd_wakeup_retired, 4, 1, 2);
    log (fp, "rnd_bedtime", household.rnd_bedtime, 2, 1, 2);
    log (fp, "rnd_bedtime_weekend", household.rnd_bedtime_weekend, 2, 1, 2);
    log (fp, "rnd_bedtime_retired", household.rnd_bedtime_retired, 2, 1, 2);
    log (fp, "at_home_param", household.at_home_param, 7, 2);
    log (fp, "energy_class", household.energy_class, k_num_ec_household, 2, 2);
    log (fp, "rnd_heat_source", household.rnd_heat_source, NUM_HEAT_SOURCE_TYPES, 1, 2);
    log (fp, "min_temperature_DHW", household.min_temperature_DHW, 1, 2);
    log (fp, "max_temperature_DHW", household.max_temperature_DHW, 1, 2);
    log (fp, "min_volume_handwash", household.min_volume_handwash, 2, 2);
    log (fp, "max_volume_handwash", household.max_volume_handwash, 2, 2);
    log (fp, "min_volume_shower", household.min_volume_shower, 2, 2);
    log (fp, "max_volume_shower", household.max_volume_shower, 2, 2);
    log (fp, "min_volume_bath", household.min_volume_bath, 2, 2);
    log (fp, "max_volume_bath", household.max_volume_bath, 2, 2);
    log (fp, "urban_car_percentage", household.urban_car_percentage, 2, 2);
    log (fp, "rural_car_percentage", household.rural_car_percentage, 2, 2);
    fprintf (fp, "  \"prevalence\":\n  {\n");
    log (fp, "air_conditioner", household.prevalence.aircon, k_max_residents, 2, 4);
    log (fp, "boiler", household.prevalence.boiler, k_max_residents, 2, 4);
    log (fp, "circulation_pump", household.prevalence.circpump, k_max_residents, 2, 4);
    log (fp, "computer", household.prevalence.computer, k_max_residents, 2, 4);
    log (fp, "stove", household.prevalence.stove, k_max_residents, 2, 4);
    log (fp, "gas_stove", household.prevalence.gas_stove, k_max_residents, 2, 4);
    log (fp, "dishwasher", household.prevalence.dishwasher, k_max_residents, 2, 4);
    log (fp, "freezer", household.prevalence.freezer, k_max_residents, 2, 4);
    log (fp, "fridge", household.prevalence.fridge, k_max_residents, 2, 4);
    log (fp, "heating", household.prevalence.heating, k_max_residents, 2, 4);
    log (fp, "light", household.prevalence.light, k_max_residents, 2, 4);
    log (fp, "solar_module", household.prevalence.solar_module, k_max_residents, 2, 4);
    log (fp, "tumble_dryer", household.prevalence.dryer, k_max_residents, 2, 4);
    log (fp, "tv", household.prevalence.tv, k_max_residents, 2, 4);
    log (fp, "vacuum", household.prevalence.vacuum, k_max_residents, 2, 4);
    log (fp, "washing_machine", household.prevalence.wmachine, k_max_residents, 2, 4);
    log (fp, "e_vehicle", household.prevalence.e_vehicle, k_max_residents, 2, 4);
    fseek (fp, -2, SEEK_CUR); 
    fprintf (fp, "\n  }\n}\n");
    fclose (fp);


    /* Print log for the vacation configuration

    open_file (&fp, k_vacation_json_log_file_name, "w");

    if (comments_in_logfiles)
    {
        fprintf (fp, "// This log was created by ResLoadSIM version %s\n", VERSION);
        fprintf (fp, "// Arguments: %d %.2lf (households days)\n", households, days);
    }
    fprintf (fp, "{\n");

    fprintf (fp, "  \"vacation_percentage\":\n  {\n");
    double last_value = -1;
    for (int m=0; m<12; m++)
    {
        for (int d=0; d<31; d++)
        {
            if (household.vacation_percentage[m][d] != last_value)
            {
                char keystr[8];
                sprintf (keystr, "%d.%d", d+1, m+1);
                log (fp, keystr, household.vacation_percentage[m][d], 2, 4);
                last_value = household.vacation_percentage[m][d];
            }
        }
    }
    fseek (fp, -2, SEEK_CUR); 
    fprintf (fp, "\n  }\n}\n");
    fclose (fp);
    */

    // Print log for the appliance configuration

    open_file (&fp, k_tech_json_log_file_name, "w");

    if (comments_in_logfiles)
    {
        fprintf (fp, "// This log was created by ResLoadSIM version %s\n", VERSION);
        fprintf (fp, "// Arguments: %d %.2lf (households days)\n", households, days);
    }
    fprintf (fp, "{\n");
    fprintf (fp, "  \"battery\":\n  {\n");
    log (fp, "frequency_solar", battery.frequency_solar, 2, 4);
    log (fp, "frequency_non_solar", battery.frequency_non_solar, 2, 4);
    log (fp, "capacity_in_days", battery.capacity_in_days, 2, 4);
    log (fp, "smartgrid_enabled", battery.smartgrid_enabled, 2, 4);
    log (fp, "allow_grid_charge_solar", battery.allow_grid_charge_solar, 4);
    log (fp, "installation_costs", battery.installation_costs, 2, 4);
    log (fp, "avg_lifetime", battery.avg_lifetime, 2, 4);
    log (fp, "min_price", battery.min_price, 2, 4);
    log (fp, "max_price", battery.max_price, 2, 4);
    log (fp, "min_capacity_per_resident", battery.min_capacity_per_resident, 3, 4);
    log (fp, "max_capacity_per_resident", battery.max_capacity_per_resident, 3, 4);
    log (fp, "min_eff_charging", battery.min_eff_charging, 3, 4);
    log (fp, "max_eff_charging", battery.max_eff_charging, 3, 4);
    log (fp, "min_eff_discharging", battery.min_eff_discharging, 3, 4);
    log (fp, "max_eff_discharging", battery.max_eff_discharging, 3, 4);
    log (fp, "max_power_charging", battery.max_power_charging, 3, 4);
    log (fp, "max_power_discharging", battery.max_power_discharging, 3, 4);
    fseek (fp, -2, SEEK_CUR);
    fprintf (fp, "\n  },\n");

    fprintf (fp, "  \"boiler\":\n  {\n");
    log (fp, "power_factor", boiler.power_factor, 2, 4);
    fseek (fp, -2, SEEK_CUR);
    fprintf (fp, "\n  },\n");

    fprintf (fp, "  \"circulation_pump\":\n  {\n");
    log (fp, "power_per_size", circpump.power_per_size, 5, 4);
    log (fp, "power_factor", circpump.power_factor, 2, 4);
    log (fp, "controlled", circpump.controlled, 2, 4);
    log (fp, "rnd_first_day", circpump.rnd_first_day, 4, 4);
    log (fp, "rnd_last_day", circpump.rnd_last_day, 4, 4);
    log (fp, "first_month", circpump.first_month, 4);
    log (fp, "last_month", circpump.last_month, 4);
    log (fp, "time_1", circpump.time_1, 4);
    log (fp, "time_2", circpump.time_2, 4);
    log (fp, "rnd_time_on", circpump.rnd_time_on, 2, 2, 4);
    log (fp, "rnd_time_off", circpump.rnd_time_off, 2, 2, 4);
    fseek (fp, -2, SEEK_CUR);
    fprintf (fp, "\n  },\n");

    fprintf (fp, "  \"air_conditioner\":\n  {\n");
    log (fp, "min_eff", aircon.min_eff, 2, 4);
    log (fp, "max_eff", aircon.max_eff, 2, 4);
    log (fp, "kW_per_m2", aircon.kW_per_m2, 3, 4);
    log (fp, "power_factor", aircon.power_factor, 2, 4);
    fseek (fp, -2, SEEK_CUR);
    fprintf (fp, "\n  },\n");

    fprintf (fp, "  \"computer\":\n  {\n");
    log (fp, "power", computer.power, 3, 4);
    log (fp, "power_factor", computer.power_factor, 2, 4);
    log (fp, "duration_mean", computer.duration_mean, 1, 4);
    log (fp, "duration_sigma", computer.duration_sigma, 1, 4);
    log (fp, "duration_fraction", computer.duration_fraction, 2, 4);
    log (fp, "duration_fraction_saturday", computer.duration_fraction_saturday, 2, 4);
    log (fp, "duration_fraction_sunday", computer.duration_fraction_sunday, 2, 4);
    log (fp, "time_offset", computer.time_offset, 3, 1, 4);
    log (fp, "time_offset_saturday", computer.time_offset_saturday, 3, 1, 4);
    log (fp, "time_offset_sunday", computer.time_offset_sunday, 3, 1, 4);
    log (fp, "rnd", computer.rnd, 3, 4);
    log (fp, "rnd_saturday", computer.rnd_saturday, 3, 4);
    log (fp, "rnd_sunday", computer.rnd_sunday, 3, 4);
    log (fp, "time_2_mean", computer.time_2_mean, 1, 4);
    log (fp, "time_2_sigma", computer.time_2_sigma, 1, 4);
    fseek (fp, -2, SEEK_CUR);
    fprintf (fp, "\n  },\n");

    fprintf (fp, "  \"stove\":\n  {\n");
    log (fp, "power", stove.power, k_max_residents, 3, 4);
    log (fp, "power_factor", stove.power_factor, 2, 4);
    log (fp, "duration_1_percent", stove.duration_1_percent, 4);
    log (fp, "duration_2_percent", stove.duration_2_percent, 4);
    log (fp, "duration_2_percent_saturday", stove.duration_2_percent_saturday, 4);
    log (fp, "duration_2_percent_sunday", stove.duration_2_percent_sunday, 4);
    log (fp, "time_offset", stove.time_offset, 1, 4);
    log (fp, "rnd_duration_1", stove.rnd_duration_1, 4, 1, 4);
    log (fp, "rnd_duration_2", stove.rnd_duration_2, 4, 1, 4);
    log (fp, "rnd_duration_3", stove.rnd_duration_3, 4, 1, 4);
    log (fp, "time_2_mean", stove.time_2_mean, 1, 4);
    log (fp, "time_2_sigma", stove.time_2_sigma, 1, 4);
    log (fp, "time_3_mean", stove.time_3_mean, 1, 4);
    log (fp, "time_3_sigma", stove.time_3_sigma, 1, 4);
    fseek (fp, -2, SEEK_CUR);
    fprintf (fp, "\n  },\n");

    fprintf (fp, "  \"dishwasher\":\n  {\n");
    log (fp, "smartgrid_enabled", dishwasher.smartgrid_enabled, 2, 4);
    log (fp, "smart", dishwasher.smart, 2, 4);
    log (fp, "energy_classes", dishwasher.energy_classes, dishwasher.num_energy_classes, 2, 4);
    log (fp, "hours_per_cycle", dishwasher.hours_per_cycle, 2, 4);
    log (fp, "place_settings", dishwasher.place_settings, k_max_residents, 2, 4);
    log (fp, "SAEc_small", dishwasher.SAEc_small, 2, 1, 4);
    log (fp, "SAEc_big", dishwasher.SAEc_big, 2, 1, 4);
    log (fp, "factor", dishwasher.factor, 1, 4);
    log (fp, "probability", dishwasher.probability, 2, 4);
    log (fp, "ignore_price", dishwasher.ignore_price, 4);
    log (fp, "fraction", dishwasher.fraction, 4);
    log (fp, "timer_1_mean", dishwasher.timer_1_mean, 1, 4);
    log (fp, "timer_1_sigma", dishwasher.timer_1_sigma, 1, 4);
    log (fp, "timer_2_mean", dishwasher.timer_2_mean, 1, 4);
    log (fp, "timer_2_sigma", dishwasher.timer_2_sigma, 1, 4);
    log (fp, "timer_3_mean", dishwasher.timer_3_mean, 1, 4);
    log (fp, "timer_3_sigma", dishwasher.timer_3_sigma, 1, 4);
    log (fp, "preview_length", dishwasher.preview_length, 4);
    log (fp, "peak_delay", dishwasher.peak_delay, 4);
    log (fp, "power_factor", dishwasher.power_factor, 2, 4);
    fseek (fp, -2, SEEK_CUR);
    fprintf (fp, "\n  },\n");

    fprintf (fp, "  \"e_vehicle\":\n  {\n");
    log (fp, "smartgrid_enabled", e_vehicle.smartgrid_enabled, 2, 4);
    log (fp, "smart", e_vehicle.smart, 2, 4);
    log (fp, "departure_delay", e_vehicle.departure_delay, 2, 4);
    for (int i=0; i<E_Vehicle::num_models; i++)
    {
        fprintf (fp, "    \"model %d\":\n    {\n", i+1);
        log (fp, "name", e_vehicle.models[i].name, 6);
        log (fp, "consumption_per_100km", e_vehicle.models[i].consumption_per_100km, 2, 6);
        log (fp, "battery_capacity", e_vehicle.models[i].battery_capacity, 2, 6);
        log (fp, "max_charge_power_AC", e_vehicle.models[i].max_charge_power_AC, 2, 6);
        log (fp, "max_charge_power_DC", e_vehicle.models[i].max_charge_power_DC, 2, 6);
        log (fp, "charging_curve", e_vehicle.models[i].charging_curve, 21, 3, 6);
        fseek (fp, -2, SEEK_CUR);
        fprintf (fp, "\n    }");
        if (i<E_Vehicle::num_models-1) fprintf (fp, ",\n"); else fprintf (fp, "\n");
    }
    fprintf (fp, "  },\n");

    fprintf (fp, "  \"freezer\":\n  {\n");
    log (fp, "smartgrid_enabled", freezer.smartgrid_enabled, 2, 4);
    log (fp, "smart", freezer.smart, 2, 4);
    log (fp, "min_temperature", freezer.min_temperature, 2, 4);
    log (fp, "max_temperature", freezer.max_temperature, 2, 4);
    log (fp, "delta_t_rise_factor", freezer.delta_t_rise_factor, 6, 4);
    log (fp, "delta_t_rise_mean", freezer.delta_t_rise_mean, 2, 4);
    log (fp, "delta_t_rise_sigma", freezer.delta_t_rise_sigma, 2, 4);
    log (fp, "delta_t_drop_factor", freezer.delta_t_drop_factor, 6, 4);
    log (fp, "delta_t_drop_mean", freezer.delta_t_drop_mean, 2, 4);
    log (fp, "delta_t_drop_sigma", freezer.delta_t_drop_sigma, 2, 4);
    log (fp, "Vc_per_resident", freezer.Vc_per_resident, 2, 4);
    log (fp, "Tc", freezer.Tc, 2, 4);
    log (fp, "mn_percentage", freezer.mn_percentage, 4);
    log (fp, "factor_1", freezer.factor_1, 2, 4);
    log (fp, "energy_classes", freezer.energy_classes, freezer.num_energy_classes, 2, 4);
    log (fp, "power_factor", freezer.power_factor, 2, 4);
    fseek (fp, -2, SEEK_CUR);
    fprintf (fp, "\n  },\n");

    fprintf (fp, "  \"fridge\":\n  {\n");
    log (fp, "smartgrid_enabled", fridge.smartgrid_enabled, 2, 4);
    log (fp, "smart", fridge.smart, 2, 4);
    log (fp, "min_temperature", fridge.min_temperature, 2, 4);
    log (fp, "max_temperature", fridge.max_temperature, 2, 4);
    log (fp, "delta_t_rise_factor", fridge.delta_t_rise_factor, 6, 4);
    log (fp, "delta_t_rise_mean", fridge.delta_t_rise_mean, 2, 4);
    log (fp, "delta_t_rise_sigma", fridge.delta_t_rise_sigma, 2, 4);
    log (fp, "delta_t_drop_factor", fridge.delta_t_drop_factor, 6, 4);
    log (fp, "delta_t_drop_mean", fridge.delta_t_drop_mean, 2, 4);
    log (fp, "delta_t_drop_sigma", fridge.delta_t_drop_sigma, 2, 4);
    log (fp, "Vc_mean", fridge.Vc_mean, k_max_residents, 2, 4);
    log (fp, "Vc_sigma", fridge.Vc_sigma, k_max_residents, 2, 4);
    log (fp, "Vc_low", fridge.Vc_low, k_max_residents, 2, 4);
    log (fp, "Vc_high", fridge.Vc_high, k_max_residents, 2, 4);
    log (fp, "Tc", fridge.Tc, 2, 4);
    log (fp, "factor_1", fridge.factor_1, 2, 4);
    log (fp, "factor_2", fridge.factor_2, 2, 4);
    log (fp, "energy_classes", fridge.energy_classes, fridge.num_energy_classes, 2, 4);
    log (fp, "power_factor", fridge.power_factor, 2, 4);
    fseek (fp, -2, SEEK_CUR);
    fprintf (fp, "\n  },\n");

    fprintf (fp, "  \"heating\":\n  {\n");
    log (fp, "smartgrid_enabled", heating.smartgrid_enabled, 2, 4);
    log (fp, "kW_per_m2", heating.kW_per_m2, 3, 4);
    log (fp, "power_factor", heating.power_factor, 2, 4);
    fseek (fp, -2, SEEK_CUR);
    fprintf (fp, "\n  },\n");

    fprintf (fp, "  \"heat_pump\":\n  {\n");
    log (fp, "min_eff", heatpump.min_eff, 2, 4);
    log (fp, "max_eff", heatpump.max_eff, 2, 4);
    log (fp, "min_temperature", heatpump.min_temperature, 2, 4);
    log (fp, "max_temperature", heatpump.max_temperature, 2, 4);
    log (fp, "kW_per_m2", heatpump.kW_per_m2, 3, 4);
    log (fp, "power_factor", heatpump.power_factor, 2, 4);
    fseek (fp, -2, SEEK_CUR);
    fprintf (fp, "\n  },\n");

    fprintf (fp, "  \"light\":\n  {\n");
    log (fp, "energy_classes", light.energy_classes, light.num_energy_classes, 2, 4);
    log (fp, "luminous_flux_mean", light.luminous_flux_mean, 4);
    log (fp, "luminous_flux_sigma", light.luminous_flux_sigma, 4);
    log (fp, "luminous_flux_min", light.luminous_flux_min, 4);
    log (fp, "luminous_flux_max", light.luminous_flux_max, 4);
    log (fp, "sigma_morning", light.sigma_morning, 1, 4);
    log (fp, "sigma_evening", light.sigma_evening, 1, 4);
    log (fp, "power_factor", light.power_factor, 2, 4);
    fseek (fp, -2, SEEK_CUR);
    fprintf (fp, "\n  },\n");

    fprintf (fp, "  \"tumble_dryer\":\n  {\n");
    log (fp, "smartgrid_enabled", dryer.smartgrid_enabled, 2, 4);
    log (fp, "energy_classes", dryer.energy_classes, dryer.num_energy_classes, 2, 4);
    log (fp, "hours_per_cycle", dryer.hours_per_cycle, 2, 4);
    log (fp, "capacity", dryer.capacity, k_max_residents, 2, 4);
    log (fp, "ignore_price", dryer.ignore_price, 4);
    log (fp, "peak_delay", dryer.peak_delay, 4);
    log (fp, "power_factor", dryer.power_factor, 2, 4);
    fseek (fp, -2, SEEK_CUR);
    fprintf (fp, "\n  },\n");

    fprintf (fp, "  \"tv\":\n  {\n");
    log (fp, "energy_classes", tv.energy_classes, tv.num_energy_classes, 2, 4);
    log (fp, "diagonal_1", tv.diagonal_1, 2, 4);
    log (fp, "diagonal_2", tv.diagonal_2, 2, 4);
    log (fp, "diagonal_3", tv.diagonal_3, 2, 4);
    log (fp, "avg_duration", tv.avg_duration, k_max_residents, 2, 4);
    log (fp, "factor_mean", tv.factor_mean, 3, 4);
    log (fp, "factor_sigma", tv.factor_sigma, 3, 4);
    log (fp, "factor_mean_we", tv.factor_mean_we, 3, 4);
    log (fp, "factor_sigma_we", tv.factor_sigma_we, 3, 4);
    log (fp, "duration_factor", tv.duration_factor, 3, 4);
    log (fp, "duration_factor_sat", tv.duration_factor_sat, 3, 4);
    log (fp, "duration_factor_sun", tv.duration_factor_sun, 3, 4);
    log (fp, "random", tv.random, 3, 4);
    log (fp, "random_sat", tv.random_sat, 3, 4);
    log (fp, "random_sun", tv.random_sun, 3, 4);
    log (fp, "delay", tv.delay, 3, 4);
    log (fp, "delay_sat", tv.delay_sat, 3, 4);
    log (fp, "delay_sun", tv.delay_sun, 3, 4);
    log (fp, "time_2_mean", tv.time_2_mean, 1, 4);
    log (fp, "time_2_sigma", tv.time_2_sigma, 1, 4);
    log (fp, "power_factor", tv.power_factor, 2, 4);
    fseek (fp, -2, SEEK_CUR);
    fprintf (fp, "\n  },\n");

    fprintf (fp, "  \"vacuum\":\n  {\n");
    log (fp, "energy_classes", vacuum.energy_classes, vacuum.num_energy_classes, 2, 4);
    log (fp, "timer_min", vacuum.timer_min, 4);
    log (fp, "timer_max", vacuum.timer_max, 4);
    log (fp, "timer_factor", vacuum.timer_factor, 4);
    log (fp, "power_factor", vacuum.power_factor, 2, 4);
    fseek (fp, -2, SEEK_CUR);
    fprintf (fp, "\n  },\n");

    fprintf (fp, "  \"washing_machine\":\n  {\n");
    log (fp, "smartgrid_enabled", wmachine.smartgrid_enabled, 2, 4);
    log (fp, "smart", wmachine.smart, 2, 4);
    log (fp, "energy_classes", wmachine.energy_classes, wmachine.num_energy_classes, 2, 4);
    log (fp, "hours_per_cycle", wmachine.hours_per_cycle, 2, 4);
    log (fp, "capacity", wmachine.capacity, k_max_residents, 2, 4);
    log (fp, "random_limit", wmachine.random_limit, 4);
    log (fp, "ignore_price", wmachine.ignore_price, 4);
    log (fp, "best_price_lookahead", wmachine.best_price_lookahead, 4);
    log (fp, "timer_mean", wmachine.timer_mean, 4);
    log (fp, "timer_sigma", wmachine.timer_sigma, 4);
    log (fp, "peak_delay", wmachine.peak_delay, 4);
    log (fp, "power_factor", wmachine.power_factor, 2, 4);
    fseek (fp, -2, SEEK_CUR);
    fprintf (fp, "\n  },\n");

    fprintf (fp, "  \"solar_module\":\n  {\n");
    log (fp, "system_loss", solar_module.system_loss, 2, 4);
    log (fp, "production_ratio", solar_module.production_ratio, 2, 4);
    log (fp, "min_area", solar_module.min_area, 2, 4);
    log (fp, "max_area", solar_module.max_area, 2, 4);
    log (fp, "min_eff", solar_module.min_eff, 2, 4);
    log (fp, "max_eff", solar_module.max_eff, 2, 4);
    log (fp, "power_factor", solar_module.power_factor, 2, 4);
    fseek (fp, -2, SEEK_CUR);
    fprintf (fp, "\n  },\n");

    fprintf (fp, "  \"solar_collector\":\n  {\n");
    log (fp, "area_factor_1", solar_collector.area_factor_1, 2, 4);
    log (fp, "area_factor_2", solar_collector.area_factor_2, 2, 4);
    log (fp, "eff_0", solar_collector.eff_0, 2, 4);
    log (fp, "min_flow_rate", solar_collector.min_flow_rate, 2, 4);
    log (fp, "max_flow_rate", solar_collector.max_flow_rate, 2, 4);
    fseek (fp, -2, SEEK_CUR);
    fprintf (fp, "\n  },\n");

    fprintf (fp, "  \"heat_storage\":\n  {\n");
    log (fp, "liter_per_m2", heat_storage.liter_per_m2, 2, 4);
    log (fp, "max_temperature", heat_storage.max_temperature, 2, 4);
    log (fp, "max_heat_power", heat_storage.max_heat_power, 2, 4);
    fseek (fp, -2, SEEK_CUR);
    fprintf (fp, "\n  }\n}\n");

    fclose (fp);
}


void Configuration::create_dictionary (const char file_name_1[], const char file_name_2[])
{
    FILE *fp = NULL;
    char *buffer = NULL;
    char group_name[64], setting_name[64];
    struct stat st;
    int index = 0, num_tokens, group_size, length;
    off_t size;
    jsmn_parser parser;
    jsmntok_t *tokens = NULL;

    fp = fopen (file_name_1, "r");
    if (fp) stat (file_name_1, &st);
    else  // try alternative location
    {
        fp = fopen (file_name_2, "r");
        if (fp) stat (file_name_2, &st);
    }
    if (fp)  // Read JSON input and parse it
    {
        size = st.st_size;
        buffer = (char *) malloc (size * sizeof (char) + 1);
        for (int i=0; i<size; i++) buffer[index++] = fgetc (fp);
        buffer[index] = '\0';
        fclose (fp);

        // Parsing with 'tokens' set to NULL returns the number of tokens.
        // num_tokens < 0 indicates a problem with the JSON file

        jsmn_init (&parser);
        num_tokens = jsmn_parse (&parser, buffer, strlen(buffer), tokens, 0);
        if (num_tokens < 0)  // num_tokens is interpreted as error code
        {
            switch (num_tokens)
            {
                case JSMN_ERROR_INVAL:
                    fprintf (stderr, "Bad JSON file '%s'. Please check the file's format.\n", file_name_1);
                    break;
                case JSMN_ERROR_NOMEM:
                    fprintf (stderr, "Not enough tokens for parsing JSON file '%s'.\n", file_name_1);
                    break;
                case JSMN_ERROR_PART:
                    fprintf (stderr, "JSON file '%s' is too short.\n", file_name_1);
                    break;
            }
            exit(1);
        }

        // Everything ok so far. Let's parse...

        tokens = (jsmntok_t *) malloc (num_tokens * sizeof (jsmntok_t));
        jsmn_init (&parser);
        jsmn_parse (&parser, buffer, strlen(buffer), tokens, num_tokens);

        num_entries = 0;
        for (int t=0; t<num_tokens; t++)
        {
            if (tokens[t].type == JSMN_STRING && t+1 < num_tokens && tokens[t+1].type > 1)
            {
                num_entries++;
                if (t+1 < num_tokens && tokens[t+1].type == JSMN_STRING) t++;
            }
        }
        dictionary = new KeyValuePair [num_entries];

        index = 0;
        group_size = 0;
        for (int t=0; t<num_tokens; t++)
        {
            if (tokens[t].type == JSMN_STRING)
            {
                if (t+1 < num_tokens && tokens[t+1].type == JSMN_OBJECT)  // token[t] is the name of an object (a group of settings)
                {
                    group_size = tokens[t+1].size; // the number of settings in the group
                    length = tokens[t].end - tokens[t].start;
                    strncpy (group_name, buffer+tokens[t].start, length);
                    group_name[length] = '\0';
                }
                else // token[t] is the name of a setting (key)
                {
                    length = tokens[t].end - tokens[t].start;
                    strncpy (setting_name, buffer+tokens[t].start, length);
                    setting_name[length] = '\0';
                    if (group_size)
                    {
                        snprintf (dictionary[index].key, sizeof(dictionary[index].key), "%s.%s", group_name, setting_name);
                        group_size--;
                    }
                    else
                    {
                        sprintf (dictionary[index].key, "%s", setting_name);
                    }
                    t++;
                    length = tokens[t].end - tokens[t].start;
                    strncpy (dictionary[index].value_str, buffer+tokens[t].start, length);
                    dictionary[index].value_str[length] = '\0';
                    index++;
                }
            }
        }
        free (buffer);
        free (tokens);
        qsort (dictionary, (size_t)num_entries, sizeof (KeyValuePair), (int(*)(const void *, const void *))strcmp);
    }
    else
    {
        dictionary = NULL;
        fprintf (stderr, "Could not open file '%s'. Using default configuration.\n", file_name_1);
    }
}


void Configuration::lookup_integer (const char *file_name, const char *key, int *setting, int min, int max)
{
    KeyValuePair kvp, *result;
    int value;
    strncpy (kvp.key, key, sizeof(kvp.key));
    result = (KeyValuePair *) bsearch (&kvp, dictionary, (size_t)num_entries, sizeof (KeyValuePair),
                                       (int(*)(const void *, const void *))strcmp);
    if (result)
    {
        value = atoi (result->value_str);
        if (value >= min && value <= max) *setting = value;
        else
        {
            fprintf (stderr, "%s: The setting '%s' must be a value between %d and %d\n",
                     file_name, key, min, max);
            exit (1);
        }
    }
}


void Configuration::lookup_decimal (const char *file_name, const char *key, double *setting, double min, double max)
{
    KeyValuePair kvp, *result;
    double value;

    strncpy (kvp.key, key, sizeof(kvp.key));
    result = (KeyValuePair *) bsearch (&kvp, dictionary, (size_t)num_entries, sizeof (KeyValuePair),
                                       (int(*)(const void *, const void *))strcmp);
    if (result)
    {
        sscanf (result->value_str, "%lf", &value);
        if (value >= min && value <= max) *setting = value;
        else
        {
            if (min == -DBL_MAX && max != DBL_MAX)
                fprintf (stderr, "%s: The setting '%s' must be <= %lf\n",
                         file_name, key, max);
            else if (min != -DBL_MAX && max == DBL_MAX)
                fprintf (stderr, "%s: The setting '%s' must be >= %lf\n",
                         file_name, key, min);
            else
                fprintf (stderr, "%s: The setting '%s' must be a value between %lf and %lf\n",
                         file_name, key, min, max);
            exit (1);
        }
    }
}


void Configuration::lookup_boolean (const char *file_name, const char *key, bool *setting)
{
    KeyValuePair kvp, *result;

    strncpy (kvp.key, key, sizeof(kvp.key));
    result = (KeyValuePair *) bsearch (&kvp, dictionary, (size_t)num_entries, sizeof (KeyValuePair),
                                       (int(*)(const void *, const void *))strcmp);
    if (result)
    {
        for (unsigned int i=0; i<strlen(result->value_str); i++)
            result->value_str[i] = tolower (result->value_str[i]);
        if      (!strcmp (result->value_str, "true")) *setting = true;
        else if (!strcmp (result->value_str, "false")) *setting = false;
        else
        {
            fprintf (stderr, "%s: The setting '%s' must be either 'true' or 'false'\n",
                     file_name, key);
            exit (1);
        }
    }
}


bool Configuration::lookup_string (const char *key, char setting[], size_t setting_length)
{
    KeyValuePair kvp, *result;

    if (strlen (key) + 1 > sizeof (kvp.key))
    {
        fprintf (stderr, "ERROR in lookup_string: key is too long.\n");
        exit(1);
    }
    strcpy (kvp.key, key);
    result = (KeyValuePair *) bsearch (&kvp, dictionary, (size_t)num_entries, sizeof (KeyValuePair),
                                       (int(*)(const void *, const void *))strcmp);
    if (result)
    {
        if (strlen (result->value_str) + 1 > setting_length)
        {
            fprintf (stderr, "ERROR in lookup_string: result is too long.\n");
            exit(1);
        }
        strcpy (setting, result->value_str);
    }
    return result;
}


void Configuration::lookup_vector (const char *file_name, const char *key, double setting[], int length, bool sum_must_be_100)
{
    KeyValuePair kvp, *result;
    int index = 0;
    char *token, *str;
    size_t last_pos;

    strncpy (kvp.key, key, sizeof(kvp.key));
    result = (KeyValuePair *) bsearch (&kvp, dictionary, (size_t)num_entries, sizeof (KeyValuePair),
                                       (int(*)(const void *, const void *))strcmp);
    if (result)
    {
        str = result->value_str;
        last_pos = strlen (str) - 1;
        if (str[0] == '[' && str[last_pos] == ']')  // ok, looks like a vector
        {
            str[0] = ' ';
            str[last_pos] = '\0';
            token = strtok (str, ",");
            while (token)
            {
                if (index < length) sscanf (token, "%lf", setting+index);
                else
                {
                    fprintf (stderr,
                             "%s: The setting '%s' requires a vector of length %d\n",
                             file_name, key, length);
                    exit (1);
                }
                index++;
                token = strtok (NULL, ",");
            }
            if (index < length)
            {
                fprintf (stderr,
                         "%s: The setting '%s' requires a vector of length %d\n",
                         file_name, key, length);
                exit (1);
            }
        }
        else
        {
            fprintf (stderr,
                     "%s: The setting '%s' must be a vector\n",
                     file_name, key);
            exit (1);
        }
        if (sum_must_be_100)  // the sum of all vector components must be 100
        {
            double sum = 0.;
            for (int i=0; i<length; i++) sum += setting[i];
            if (sum < 99.999 || sum > 100.001)
            {
                fprintf (stderr,
                         "%s: the components of this vector must add up to 100: '%s'\n",
                         file_name, key);
                exit (1);
            }
        }
    }
}

void Configuration::lookup_vector (const char *file_name, const char *key, int setting[], int length)
{
    KeyValuePair kvp, *result;
    int index = 0;
    char *token, *str;
    size_t last_pos;

    strncpy (kvp.key, key, sizeof(kvp.key));
    result = (KeyValuePair *) bsearch (&kvp, dictionary, (size_t)num_entries, sizeof (KeyValuePair),
                                       (int(*)(const void *, const void *))strcmp);
    if (result)
    {
        str = result->value_str;
        last_pos = strlen (str) - 1;
        if (str[0] == '[' && str[last_pos] == ']')  // ok, looks like a vector
        {
            str[0] = ' ';
            str[last_pos] = '\0';
            token = strtok (str, ",");
            while (token)
            {
                if (index < length) sscanf (token, "%d", setting+index);
                else
                {
                    fprintf (stderr,
                             "%s: The setting '%s' requires a vector of length %d\n",
                             file_name, key, length);
                    exit (1);
                }
                index++;
                token = strtok (NULL, ",");
            }
            if (index < length)
            {
                fprintf (stderr,
                         "%s: The setting '%s' requires a vector of length %d\n",
                         file_name, key, length);
                exit (1);
            }
        }
        else
        {
            fprintf (stderr,
                     "%s: The setting '%s' must be a vector\n",
                     file_name, key);
            exit (1);
        }
    }
}


void Configuration::lookup_variable_length_vector (const char *file_name, const char *key, int setting[], int *vec_length, int max_length)
{
    KeyValuePair kvp, *result;
    char *token, *str;
    size_t last_pos;

    strncpy (kvp.key, key, sizeof(kvp.key));
    result = (KeyValuePair *) bsearch (&kvp, dictionary, (size_t)num_entries, sizeof (KeyValuePair),
                                       (int(*)(const void *, const void *))strcmp);
    *vec_length = 0;
    if (result)
    {
        str = result->value_str;
        last_pos = strlen (str) - 1;
        if (str[0] == '[' && str[last_pos] == ']')  // ok, looks like a vector
        {
            str[last_pos] = '\0';
            str++;
            token = strtok (str, ",");
            while (token)
            {
                if (*vec_length == max_length)
                {
                    fprintf (stderr,
                             "%s: The maximum length of vector '%s' is %d\n",
                             file_name, key, max_length);
                    exit (1);
                }
                if (sscanf (token, "%d", setting + *vec_length) == 1) (*vec_length)++;
                token = strtok (NULL, ",");
            }
        }
        else
        {
            fprintf (stderr,
                     "%s: The setting '%s' must be a vector\n",
                     file_name, key);
            exit (1);
        }
    }
}


void Configuration::lookup_price_table (const char *file_name, const char *group, PriceTable *table)
{
    char setting[1024], profiles_str[1024], sequence_str[1024], *number_str, *str;
    int index, num_open;
    int leng;

    snprintf (setting, sizeof(setting), "%s.profiles", group);
    if (lookup_string (setting, profiles_str, sizeof (profiles_str)))
    {
        index = 0;
        for (size_t i=0; i<strlen(profiles_str); i++)
        {
            if (profiles_str[i] != ' ' && profiles_str[i] != '\n') profiles_str[index++] = profiles_str[i];
        }
        leng = index;
        profiles_str[leng] = '\0';
        if (profiles_str[0] != '[' || profiles_str[leng-1] != ']')
        {
            fprintf (stderr, "File '%s', setting '%s': there is a problem with the format.\n", file_name, setting);
            exit (1);
        }
        num_open = table->num_profiles = 1;
        for (int i=0; i<leng; i++)
        {
            if (profiles_str[i] == '[') num_open++;
            if (profiles_str[i] == ']') num_open--;
            if (profiles_str[i] == ',' && (num_open == 1)) table->num_profiles++;
        }
        try
        {
            delete table->profiles; // delete the default profile first
            table->profiles = new Profile [table->num_profiles];
        }
        catch (...)
        {
            fprintf (stderr, "File '%s': could not allocate memory for '%s'\n", file_name, setting);
            exit (1);
        }
        num_open = 0;
        int p = 0;
        table->profiles[p].length = 1;
        for (int i=0; i<leng; i++)
        {
            if (profiles_str[i] == '[') num_open++;
            if (profiles_str[i] == ']') num_open--;
            if (profiles_str[i] == ',')
            {
                if (num_open == 1)
                {
                    p++;
                    table->profiles[p].length = 1;
                }
                else if (num_open == 2) table->profiles[p].length++;
            }
        }
        index = 0;
        num_open = 0;
        for (int i=0; i<leng; i++)
        {
            if (profiles_str[i] == '[') num_open++;
            if (profiles_str[i] == ']') num_open--;
            if (!(   profiles_str[i] == '['
                  || profiles_str[i] == ']'
                  || (profiles_str[i] == ',' && (num_open != 3)))) profiles_str[index++] = profiles_str[i];
        }
        leng = index;
        profiles_str[leng] = '\0';

        str = profiles_str;
        for (int p=0; p<table->num_profiles; p++)
        {
            for (int j=0; j<table->profiles[p].length; j++)
            {
                number_str = strtok (str, ",");
                sscanf (number_str, "%lf", table->profiles[p].begin+j);
                number_str = strtok (NULL, ",");
                sscanf (number_str, "%lf", table->profiles[p].end+j);
                number_str = strtok (NULL, ",");
                sscanf (number_str, "%lf", table->profiles[p].price+j);
            }
        }
    }

    snprintf (setting, sizeof(setting), "%s.sequence", group);
    lookup_string (setting, sequence_str, sizeof (sequence_str));
    leng = strlen (sequence_str);
    if (leng > 0)
    {
        int j, num;
        sequence_str[leng-1] = '\0';
        str = sequence_str+1;
        j = 0;
        number_str = strtok (str, ",");
        while (number_str)
        {
            if (j<k_max_sequence_length)
            {
                sscanf (number_str, "%d", &num);
                if (num <= table->num_profiles) table->sequence[j] = num;
                else
                {
                    fprintf (stderr,
                             "%s: price table sequence element too big (= %d). there are only %d profiles!\n",
                             file_name, num, table->num_profiles);
                    exit (1);
                }
                j++;
            }
            else
            {
                fprintf (stderr,
                         "%s: price table sequence is too long (max. length = %d)\n",
                         file_name, k_max_sequence_length);
                exit (1);
            }
            number_str = strtok (NULL, ",");
        }
        table->seq_length = j;
    }
}


void Configuration::log (FILE *fp, const char *key, int value, int tab)
{
    for (int i=0; i<tab; i++) fputc (' ', fp);
    fprintf (fp, "\"%s\": %d,\n", key, value);
}

void Configuration::log (FILE *fp, const char *key, bool value, int tab)
{
    for (int i=0; i<tab; i++) fputc (' ', fp);
    if (value) fprintf (fp, "\"%s\": TRUE,\n", key);
    else       fprintf (fp, "\"%s\": FALSE,\n", key);
}

void Configuration::log (FILE *fp, const char *key, const char *value, int tab)
{
    for (int i=0; i<tab; i++) fputc (' ', fp);
    fprintf (fp, "\"%s\": \"%s\",\n", key, value);
}

void Configuration::log (FILE *fp, const char *key, double value, int precision, int tab)
{
    char format_str[32];
    char value_str[32];

    for (int i=0; i<tab; i++) fputc (' ', fp);
    snprintf (format_str, sizeof(format_str), "%%.%dlf", precision);
    snprintf (value_str, sizeof(value_str), format_str, value);
    fprintf (fp, "\"%s\": %s,\n", key, value_str);
}

void Configuration::log (FILE *fp, const char *key, double vector[], int length, int precision, int tab)
{
    char format_str[32];
    char component_str[32];
    char vector_str[256] = "";

    for (int i=0; i<tab; i++) fputc (' ', fp);
    for (int i=0; i<length; i++)
    {
        if (i<length-1) snprintf (format_str, sizeof(format_str), "%%.%dlf, ", precision);
        else            snprintf (format_str, sizeof(format_str), "%%.%dlf", precision);
        snprintf (component_str, sizeof(component_str), format_str, vector[i]);
        strncat (vector_str, component_str, sizeof(vector_str)-strlen(vector_str)-1);
    }
    fprintf (fp, "\"%s\": [%s],\n", key, vector_str);
}

void Configuration::log (FILE *fp, const char *key, int vector[], int length, int tab)
{
    char component_str[32];
    char vector_str[256] = "";

    for (int i=0; i<tab; i++) fputc (' ', fp);
    for (int i=0; i<length; i++)
    {
        if (i<length-1) snprintf (component_str, sizeof(component_str), "%d, ", vector[i]);
        else            snprintf (component_str, sizeof(component_str), "%d", vector[i]);
        strncat (vector_str, component_str, sizeof(vector_str)-strlen(vector_str)-1);
    }
    fprintf (fp, "\"%s\": [%s],\n", key, vector_str);
}
