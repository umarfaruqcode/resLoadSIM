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

#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>

#include "types.H"
#include "constants.H"

struct KeyValuePair
{
    char key[128];
    char value_str[512];
};

struct Profile
{
    double begin[k_profile_length];
    double end[k_profile_length];
    double price[k_profile_length];
    int length;
};

struct PriceTable
{
    Profile *profiles;
    int num_profiles;
    int sequence[k_max_sequence_length];
    int seq_length;
};


class Configuration
{
    friend class Clock;
private:
    KeyValuePair *dictionary;
    int num_entries;
    char location_name[32];
    void create_dictionary (const char file_name_1[], const char file_name_2[]);
    void lookup_integer (const char *file_name, const char *key, int *setting, int min, int max);
    void lookup_decimal (const char *file_name, const char *key, double *setting, double min, double max);
    void lookup_boolean (const char *file_name, const char *key, bool *setting);
    bool lookup_string  (const char *key, char setting[], size_t setting_length);
    void lookup_vector  (const char *file_name, const char *key, double setting[], int length, bool is_eec_vector);
    void lookup_vector  (const char *file_name, const char *key, int setting[], int length);
    void lookup_variable_length_vector (const char *file_name, const char *key, int setting[], int *vec_length, int max_length);
    void lookup_price_table (const char *file_name, const char *group, PriceTable *table);
    void log (FILE *fp, const char *key, int value, int tab);
    void log (FILE *fp, const char *key, int vector[], int length, int tab);
    void log (FILE *fp, const char *key, bool value, int tab);
    void log (FILE *fp, const char *key, const char *string, int tab);
    void log (FILE *fp, const char *key, double value, int precision, int tab);
    void log (FILE *fp, const char *key, double vector[], int length, int precision, int tab);

public:
    char pv_data_file_name[k_name_length];          // name of the pv data file (e.g. PVGIS data)
    char pv_forecast_file_name [k_name_length];     // name of the file that contains a forecast of the pv production
    int control;                                    // smartgrid control mode
    struct
    {
        int strategy;                               // charging strategy of solar batteries
        int production_forecast_method;             // 0 = no forecast, 1 = perfect forecast, 2 = naive forecast, 3 = GEFS reforecast
        double feed_in_limit;                       // limit for solar power fed into the grid
        double precharge_threshold;                 // only used for charging strategy 3 (precharging of batteries).
        bool shared;                                // allow sharing of batteries across households yes/no
    } battery_charging;
    struct
    {
        char case_file_name[k_name_length]; // name of the case file
        bool ov_control;            // overvoltage control on/off
        bool uv_control;            // undervoltage control on/off
        int step_size;              // pf is called every 'step_size' timesteps
        int output_level;           // how much output do we want
        double ov_lower_limit;      // lower limit for overvoltage control
        double ov_upper_limit;      // upper limit for overvoltage control
        double uv_lower_limit;      // lower limit for undervoltage control
        double uv_upper_limit;      // upper limit for undervoltage control
    } powerflow;
    struct
    {
        bool relative;                 // absolute or relative threshold
        double threshold;              // peak shaving threshold
    } peak_shaving;
    PriceTable price[2];               // 2 price tables (grid and solar)
    int seed;                          // seed for the random number generator
    int output;                        // output mode
    struct
    {
        int day, month, year;          // the start date
        double time;                   // start time in hours
    } start;
    int daylight_saving_time;          // 0 = no DST (wintertime only)
                                       // 1 = standard DST (clock changes twice a year)
                                       // 2 = permanent DST (summertime only)
    double transient_time;             // pre-simulation time in days
    double timestep_size;              // size of simulation timestep in seconds
    bool simulate_heating;             // turn on/off the simulation of space heating
    bool ventilation_model;            // turn on/off the ventilation model
    bool variable_load;                // turn on/off variable load
    bool comments_in_logfiles;         // turn on/off comments in logfiles
    bool energy_classes_2021;          // use the energy efficiency class definitions of 2021
    int solar_production_reference_year[k_max_ref_years];    // years used for calculating the average solar production in the forerun simulation
    int num_ref_years;                 // number of solar production reference years

    struct
    {
        double size_distribution[k_max_residents];
        double retired_1;
        double retired_2;
        double min_area[k_max_residents];
        double max_area[k_max_residents];
        double set_temperature_H_day;
        double set_temperature_H_night;
        double set_temperature_C;
        double reduce_heat;
        int heating_period_start_day;
        int heating_period_start_month;
        int heating_period_end_day;
        int heating_period_end_month;
        int min_init_laundry;
        int max_init_laundry;
        double second_fridge[k_max_residents];
        double second_tv[k_max_residents];
        double third_tv[k_max_residents];
        double second_computer[k_max_residents];
        double min_delta_laundry[k_max_residents];
        double max_delta_laundry[k_max_residents];
        int min_vacuum_interval;
        int max_vacuum_interval;
        double light_factor[k_max_residents];
        double rnd_wakeup[4];
        double rnd_wakeup_weekend[4];
        double rnd_wakeup_retired[4];
        double rnd_bedtime[2];
        double rnd_bedtime_weekend[2];
        double rnd_bedtime_retired[2];
        int at_home_param[7];
        double energy_class[k_num_ec_household];
        double rnd_heat_source[NUM_HEAT_SOURCE_TYPES];
        double min_temperature_DHW;     // minimum hot water temperature at tapping point [°C]
        double max_temperature_DHW;     // maximum hot water temperature at tapping point [°C]
        double min_volume_handwash;
        double max_volume_handwash;
        double min_volume_shower;
        double max_volume_shower;
        double min_volume_bath;
        double max_volume_bath;
        double urban_car_percentage;    // fraction of urban households with at least 1 car
        double rural_car_percentage;    // fraction of rural households with at least 1 car
        struct
        {
            double aircon[k_max_residents];
            double boiler[k_max_residents];
            double circpump[k_max_residents];
            double computer[k_max_residents];
            double stove[k_max_residents];
            double gas_stove[k_max_residents];
            double dishwasher[k_max_residents];
            double freezer[k_max_residents];
            double fridge[k_max_residents];
            double heating[k_max_residents];
            double light[k_max_residents];
            double solar_module[k_max_residents];
            double dryer[k_max_residents];
            double tv[k_max_residents];
            double vacuum[k_max_residents];
            double wmachine[k_max_residents];
            double e_vehicle[k_max_residents];
        } prevalence;
        double vacation_percentage[12][31];
    } household;

    struct
    {
        double min_temperature;
        double max_temperature;
        double smartgrid_enabled;
        double energy_classes[k_max_energy_classes];
        int num_energy_classes;
        double smart;
        double delta_t_rise_factor;
        double delta_t_rise_mean;
        double delta_t_rise_sigma;
        double delta_t_drop_factor;
        double delta_t_drop_mean;
        double delta_t_drop_sigma;
        double Vc_mean[k_max_residents];
        double Vc_sigma[k_max_residents];
        double Vc_low[k_max_residents];
        double Vc_high[k_max_residents];
        double Tc;
        double factor_1;
        double factor_2;
        double power_factor;
    } fridge;

    struct
    {
        double min_temperature;
        double max_temperature;
        double smartgrid_enabled;
        double energy_classes[k_max_energy_classes];
        int num_energy_classes;
        double smart;
        double delta_t_rise_factor;
        double delta_t_rise_mean;
        double delta_t_rise_sigma;
        double delta_t_drop_factor;
        double delta_t_drop_mean;
        double delta_t_drop_sigma;
        double Vc_per_resident;
        double Tc;
        double factor_1;
        double power_factor;
        int mn_percentage;
    } freezer;

    struct
    {
        double smartgrid_enabled;
        double smart;
        double departure_delay;
        struct
        {
            char name[k_name_length];
            double consumption_per_100km;
            double battery_capacity;
            double max_charge_power_AC;
            double max_charge_power_DC;
            double charging_curve[k_num_curve_points];
        } models[k_num_ev_models];
    } e_vehicle;

    struct
    {
        double smartgrid_enabled;
        double energy_classes[k_max_energy_classes];
        int num_energy_classes;
        double smart;
        double hours_per_cycle;
        double capacity[k_max_residents];
        double power_factor;
        int random_limit;
        int ignore_price;
        int best_price_lookahead;
        int timer_mean;
        int timer_sigma;
        int peak_delay;
    } wmachine;

    struct
    {
        double smartgrid_enabled;
        double energy_classes[k_max_energy_classes];
        int num_energy_classes;
        double smart;
        double hours_per_cycle;
        double place_settings[k_max_residents];
        double SAEc_small[2];
        double SAEc_big[2];
        double factor;
        double power_factor;
        int probability[2];
        int ignore_price;
        int fraction;
        double timer_1_mean;
        double timer_1_sigma;
        double timer_2_mean;
        double timer_2_sigma;
        double timer_3_mean;
        double timer_3_sigma;
        int preview_length;
        int peak_delay;
    } dishwasher;

    struct
    {
        double smartgrid_enabled;
        double energy_classes[k_max_energy_classes];
        int num_energy_classes;
        double hours_per_cycle;
        double capacity[k_max_residents];
        double power_factor;
        int ignore_price;
        int peak_delay;
    } dryer;

    struct
    {
        double energy_classes[k_max_energy_classes];
        int num_energy_classes;
        double diagonal_1;
        double diagonal_2;
        double diagonal_3;
        double avg_duration[k_max_residents];
        double factor_mean;
        double factor_sigma;
        double factor_mean_we;
        double factor_sigma_we;
        double duration_factor;
        double duration_factor_sat;
        double duration_factor_sun;
        double time_2_mean;
        double time_2_sigma;
        double power_factor;
        int random[3];
        int random_sat[3];
        int random_sun[3];
        int delay[3];
        int delay_sat[3];
        int delay_sun[3];
    } tv;

    struct
    {
        double energy_classes[k_max_energy_classes];
        int num_energy_classes;
        double power_factor;
        int timer_factor;
        int timer_min;
        int timer_max;
    } vacuum;

    struct
    {
        double power_factor;
    } boiler;

    struct
    {
        double energy_classes[k_max_energy_classes];
        int num_energy_classes;
        int luminous_flux_mean;
        int luminous_flux_sigma;
        int luminous_flux_min;
        int luminous_flux_max;
        double sigma_morning;
        double sigma_evening;
        double power_factor;
    } light;

    struct
    {
        double smartgrid_enabled;
        double kW_per_m2;
        double power_factor;
    } heating;

    struct
    {
        double min_eff;
        double max_eff;
        double min_temperature;
        double max_temperature;
        double kW_per_m2;
        double power_factor;
    } heatpump;

    struct
    {
        double min_eff;
        double max_eff;
        double kW_per_m2;
        double power_factor;
    } aircon;

    struct
    {
        double power;
        double power_factor;
        double duration_mean;
        double duration_sigma;
        double duration_fraction;
        double duration_fraction_saturday;
        double duration_fraction_sunday;
        double time_offset[3];
        double time_offset_saturday[3];
        double time_offset_sunday[3];
        double time_2_mean;
        double time_2_sigma;
        int rnd[3];
        int rnd_saturday[3];
        int rnd_sunday[3];
    } computer;

    struct
    {
        double power[k_max_residents];
        double power_factor;
        double time_offset;
        double rnd_duration_1[4];
        double rnd_duration_2[4];
        double rnd_duration_3[4];
        double time_2_mean;
        double time_2_sigma;
        double time_3_mean;
        double time_3_sigma;
        int duration_1_percent;
        int duration_2_percent;
        int duration_2_percent_saturday;
        int duration_2_percent_sunday;
        bool uses_gas;
    } stove;

    struct
    {
        double controlled;
        double power_per_size;
        double power_factor;
        double rnd_time_on[2];
        double rnd_time_off[2];
        int rnd_first_day[4];
        int rnd_last_day[4];
        int first_month;
        int last_month;
        int time_1, time_2;
    } circpump;

    struct
    {
        double system_loss;
        double production_ratio;
        double min_area;
        double max_area;
        double min_eff;
        double max_eff;
        double power_factor;
    } solar_module;

    struct
    {
        double area_factor_1;
        double area_factor_2;
        double eff_0;
        double min_flow_rate;
        double max_flow_rate;
    } solar_collector;

    struct
    {
        double frequency_solar;             // frequency of solar batteries
        double frequency_non_solar;         // frequency of non solar batteries
        double capacity_in_days;
        double smartgrid_enabled;           // percentage of smartgrid enabled batteries
        double installation_costs;          // fixed installation cost for solar battery [Euro]
        double avg_lifetime;                // avg. lifetime [years]
        double min_price;                   // min. battery retail price per kWh capacity [Euro]
        double max_price;                   // max. battery retail price per kWh capacity [Euro]
        double min_capacity_per_resident;   // [kWh]
        double max_capacity_per_resident;   // [kWh]
        double min_eff_charging;            // min. efficiency while charging
        double max_eff_charging;            // max. efficiency while charging
        double min_eff_discharging;         // min. efficiency while discharging
        double max_eff_discharging;         // max. efficiency while discharging
        double max_power_charging;          // measured as a fraction of the max. capacity
        double max_power_discharging;       // measured as a fraction of the max. capacity
        bool   allow_grid_charge_solar;     // solar battery may be charged by grid yes/no
    } battery;

    struct
    {
        double liter_per_m2;                // capacity in liter per m2 solar collector area
        double max_temperature;             // hot water temperature [°C]
        double max_heat_power;              // maximum heat power [kW]
    } heat_storage;

    Configuration();
    ~Configuration();
    void print_log (int households, double days);
};

#endif
