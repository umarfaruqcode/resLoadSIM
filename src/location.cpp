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
#ifdef _WIN32
#   include <io.h>
#   define F_OK 0
#else
#   include <unistd.h>
#endif
#ifdef HAVE_CURL
#include <curl/curl.h>
#endif

#include "globals.H"
#include "proto.H"
#include "types.H"
#include "location.H"

/*
 The irradiation and temperature data is provided by PVGIS:
 https://re.jrc.ec.europa.eu/pvg_tools/en/#HR
 The PVGIS database can also be accessed in a non-interactive way.
 A description of how to do this can be found here:
 https://ec.europa.eu/jrc/en/PVGIS/docs/noninteractive

 The following assumptions are made:
 - all PV installations are fixed
 - the inclination and azimuth of the panels is optimal for a given location
 */

Location::Location (char location_name[], int year, char pv_data_file_name[], char pv_forecast_file_name[],
                    int charging_strategy, int forecast_method)
{
    const char function_name[] = "Location::Location";
    FILE *fp = NULL;
    char file_name[k_name_length], type_name[k_name_length], keyword[k_name_length];
    char *line = NULL;
    int num_years;
    int initial_date, initial_time;
#ifdef HAVE_CURL
    char url[k_name_length];
#endif

    snprintf (name, sizeof (name), "%s", location_name);

    // Read the location data file

    snprintf (file_name, sizeof (file_name), "locations/%s/location.json", name);
    open_file (&fp, file_name, "r");
    fscanf (fp, "{\n");
    fscanf (fp, "%*s \"%[A-Za-z]%*s", country);
    fscanf (fp, "    \"%[A-Za-z]%*s", keyword);
    if (strcmp (keyword, "type"))
    {
        fprintf (stderr, "Error in location.json: expected to read the keyword 'type', found '%s' instead.\n", keyword);
        exit (1);
    }
    fscanf (fp, " \"%[A-Za-z]%*s", type_name);
    if (!strncmp (type_name, "urban", 5)) type = URBAN;
    else if (!strncmp (type_name, "rural", 5)) type = RURAL;
    else
    {
        fprintf (stderr, "Unknown location type '%s'. The location type can be either 'urban' or 'rural'.\n", type_name);
        exit (1);
    }
    fscanf (fp, "%*s %lf%*s", &latitude);
    fscanf (fp, "%*s %lf%*s", &longitude);
    fscanf (fp, "%*s %d",  &utc_offset_base);
    fclose (fp);

    // Open the file that contains the temperature and solar radiation data for this location.

    if (!strlen (pv_data_file_name))    // no filename for the pv data was provided
    {
        // First try to find a suitable PVGIS file in the locations directory
        snprintf (pv_data_file_name, k_name_length, "Timeseries_%.3lf_%.3lf_SA.csv", latitude, longitude);
        snprintf (file_name, sizeof (file_name), "locations/%s/%s", name, pv_data_file_name);
        if (access (file_name, F_OK) == -1)
        {
            // Didn't find a file at the default location, so let's try to download the data from the PVGIS site
#ifdef HAVE_CURL
            if (!silent_mode && rank == 0)
            {
                printf ("Trying to download solar data from the PVGIS site..."); fflush (stdout);
            }
            CURL *curl;
            CURLcode result;
            curl_global_init (CURL_GLOBAL_DEFAULT);
            curl = curl_easy_init();
            if (!curl)
            {
                fprintf (stderr, "Error: couldn't init cURL.\n");
                exit(1);
            }
            snprintf (url, sizeof (url),
                      "https://re.jrc.ec.europa.eu/api/seriescalc?lat=%.3lf&lon=%.3lf&outputformat=csv&browser=0&optimalangles=1",
                      latitude, longitude);
            curl_easy_setopt (curl, CURLOPT_URL, url);
            curl_easy_setopt (curl, CURLOPT_SSL_VERIFYPEER, 0L);
            open_file (&fp, file_name, "w");
            curl_easy_setopt (curl, CURLOPT_WRITEDATA, fp);
            result = curl_easy_perform (curl);
            if (result != CURLE_OK)
            {
                fprintf (stderr, "Error: curl_easy_perform() failed: %s\n", curl_easy_strerror (result));
                exit(1);
            }
            curl_easy_cleanup (curl);
            curl_global_cleanup();
            fclose (fp);
            if (!silent_mode && rank == 0)
            {
                printf ("success!\n"); fflush (stdout);
            }
#else
            fprintf (stderr, "Error: No PVGIS file has been specified in resLoadSIM.json.\n");
            exit(1);
#endif
        }
    }
    else
    {
        snprintf (file_name, sizeof (file_name), "locations/%s/%s", name, pv_data_file_name);
    }
    open_file (&fp, file_name, "r");

    // Skip the file header depending on whether it is a PVGIS file or a custom format

    char first_word[32];
    read_line (fp, &line);
    sscanf (line, "%s", first_word);
    if (!strncmp (first_word, "Latitude", 8))  // it's a PVGIS file
    {
        is_PVGIS = true;
        for (int i=0; i<8; i++) read_line (fp, &line); // skip the remaining header lines
    }
    else is_PVGIS = false;

    // Get the first and the last year of the timeseries

    long file_pos = ftell (fp);
    read_line (fp, &line);
    if (is_PVGIS)
    {
        sscanf (line, "%d:%d", &initial_date, &initial_time);
        first_year = initial_date/10000;
    }
    else
    {
        sscanf (line, "%*d.%*d.%d", &first_year);
    }
    while (!feof(fp) && strlen(line)>0)
    {
        if (is_PVGIS) sscanf (line, "%d", &last_year); else sscanf (line, "%*d.%*d.%d", &last_year);
        read_line (fp, &line);
    }
    if (is_PVGIS) last_year /= 10000;
    num_years = last_year - first_year + 1;
    fseek (fp, file_pos, SEEK_SET);

    update_year_ts (year);

    // Allocate memory for storing irradiation/solar_output and temperature data

    alloc_memory (&offset_year, num_years+1, function_name);
    num_entries = 0;
    offset_year[0] = 0;
    int index = 1;
    for (int y=first_year; y<=last_year; y++)
    {
        if (y%4==0 && (y%100>0 || y%400==0)) num_entries += 366;   // leap year
        else num_entries += 365;
        offset_year[index++] = num_entries;
    }
    if (is_PVGIS) num_entries *= 24;
    else num_entries *= 24 * 12;
    alloc_memory (&irradiance_timeline, num_entries, function_name);
    alloc_memory (&temperature_timeline, num_entries, function_name);

    // Read the PV data file

    for (int i=0; i<num_entries; i++)
    {
        read_line (fp, &line);
        if (is_PVGIS) sscanf (line, "%*d:%*d,%lf,%*f,%lf,%*f,%*d", irradiance_timeline+i, temperature_timeline+i);
        else          sscanf (line, "%*s %*s %lf %lf", irradiance_timeline+i, temperature_timeline+i);
    }
    fclose (fp);

    if (charging_strategy > 0 && forecast_method == 3)
    {
        // Open the file that contains the solar radiation forecast for this location.
        if (!strlen (pv_forecast_file_name))
        {
            fprintf (stderr, "The name of a solar forecast file must be specified in resLoadSIM.json when setting production_forecast_method = 3\n");
            exit (1);
        }
        snprintf (file_name, sizeof (file_name), "locations/%s/%s", name, pv_forecast_file_name);
        open_file (&fp, file_name, "r");

        // Remove the file header
        for (int i=0; i<9; i++) read_line (fp, &line); // skip the remaining header lines

        // Allocate memory for storing the forecast data
        alloc_memory (&forecast_timeline, num_entries, function_name);

        // Read the first line with solar data and check whether the initial date and time matches the PVGIS file's initial date and time
        int initial_date_fc, initial_time_fc;
        read_line (fp, &line);
        sscanf (line, "%d:%d,%lf", &initial_date_fc, &initial_time_fc, forecast_timeline);
        if (initial_date_fc != initial_date || initial_time_fc != initial_time)
        {
            fprintf (stderr, "The initial date/time of the forecast file must match the initial date/time of the PVGIS file\n");
            exit (1);
        }
        // Read the forecast data file
        for (int i=1; i<num_entries; i++)
        {
            read_line (fp, &line);
            sscanf (line, "%*d:%*d,%lf", forecast_timeline+i);
        }
        fclose (fp);
    }
    else forecast_timeline = NULL;
    free (line);

    // Calculate the mean temperatures for the years FIRST to LAST
    // and find the coldest day of each year

    alloc_memory (&temp_ambient_mean, num_years, function_name);
    alloc_memory (&coldest_day, num_years, function_name);
    int num_entries_per_day;
    if (is_PVGIS) num_entries_per_day = 24; else num_entries_per_day = 24 * 12;
    for (int y=0; y<num_years; y++)
    {
        temp_ambient_mean[y] = 0.;
        double lowest_temp = 12345.;
        int lowest_temp_index = 0;
        for (int i=offset_year[y]*num_entries_per_day; i<offset_year[y+1]*num_entries_per_day; i++)
        {
            temp_ambient_mean[y] += temperature_timeline[i];
            if (temperature_timeline[i] < lowest_temp)
            {
                lowest_temp = temperature_timeline[i];
                lowest_temp_index = i;
            }
        }
        temp_ambient_mean[y] /= (offset_year[y+1]-offset_year[y])*num_entries_per_day;
        coldest_day[y] = lowest_temp_index/num_entries_per_day - offset_year[y] + 1;
    }
    irradiance_integral = 0.;
    temp_H2O_cold_0 = 10.;
}


Location::~Location()
{
    delete [] offset_year;
    delete [] coldest_day;
    delete [] temp_ambient_mean;
    delete [] irradiance_timeline;
    delete [] temperature_timeline;
    if (forecast_timeline) delete [] forecast_timeline;
}


void Location::update_values()
{
    int this_day, pos_of_day, index, year;
    int offset_month[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int offset_month_leap[12] = {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335};
    int forecast_method = config->battery_charging.production_forecast_method;
    const double factor = 2*M_PI/365.;
    const double delta_temp_0 = 42.0; // nominal temperature difference between hot and cold water
    double temp_H2O_cold;             // cold water temperature

    // update cold water temperature and seasonal factor

    if (sim_clock->leap_year)
    {
        this_day = sim_clock->day + offset_month_leap[sim_clock->month-1];  // position of the current day in the current simulation year
    }
    else
    {
        this_day = sim_clock->day + offset_month[sim_clock->month-1];
    }
    temp_H2O_cold = temp_ambient_mean[year_ts-first_year] - 3. * cos (factor * (this_day - coldest_day[year_ts-first_year]));
    seasonal_factor = 1. + (temp_H2O_cold_0 - temp_H2O_cold) / delta_temp_0;

    // update irradiance and temperature

    if (is_PVGIS)
    {
        irradiance = 0.;
        temperature = 0.;
        if (sim_clock->forerun)
        {
            for (int i=0; i<config->num_ref_years; i++)
            {
                year = config->solar_production_reference_year[i];
                if (year%4==0 && (year%100>0 || year%400==0))  // is leap year
                {
                    this_day = sim_clock->day + offset_month_leap[sim_clock->month-1];  // position of the current day in the current reference year
                }
                else
                {
                    this_day = sim_clock->day + offset_month[sim_clock->month-1];
                }
                pos_of_day = this_day + offset_year[year-first_year] -1;        // position of the day in the timeseries data
                index = pos_of_day*24 + sim_clock->daytime/3600. - utc_offset;  // PVGIS date:time is in UTC
                update_irradiance_and_temperature_PVGIS (&irradiance, &temperature, index);
            }
            irradiance /= config->num_ref_years;
            temperature /= config->num_ref_years;
        }
        else
        {
            pos_of_day = this_day + offset_year[year_ts-first_year] -1;     // position of the day in the timeseries data
            index = pos_of_day*24 + sim_clock->daytime/3600. - utc_offset;  // PVGIS date:time is in UTC
            update_irradiance_and_temperature_PVGIS (&irradiance, &temperature, index);
        }
    }
    else  // custom format used in the Hannover zero:e park scenario
    {
        index = (this_day + offset_year[year_ts-first_year] -1) * 24 * 12 + sim_clock->daytime/300;
        update_irradiance_and_temperature_custom (index);
    }

    // Calculate the solar irradiance integral of the day ahead, which is used to forecast the
    // output of the solar modules

    if (   !sim_clock->forerun
        && sim_clock->midnight
        && config->battery_charging.strategy > 0
        && (forecast_method == 1 || forecast_method == 3))
    {
        irradiance_integral = 0.;
        double daytime = sim_clock->sunrise;
        double irr_1, irr_2;
        int x;
        while (daytime < sim_clock->sunset)
        {
            index = pos_of_day*24 + daytime/3600. - utc_offset;  // PVGIS date:time is in UTC
            x = (int)daytime % 3600;
            // correction needed, because data in timeseries has 10 minutes (600s) offset to each hour
            if (x < 600) index -= 1;
            if (index < 0)
            {
                if (forecast_method == 1) irradiance_integral += irradiance_timeline[0];
                else if (forecast_method == 3) irradiance_integral += forecast_timeline[0];
            }
            else if (index == num_entries-1)
            {
                if (forecast_method == 1) irradiance_integral += irradiance_timeline[index];
                else if (forecast_method == 3) irradiance_integral += forecast_timeline[index];
            }
            else // the ordinary case
            {
                if (forecast_method == 1)
                {
                    irr_1 = irradiance_timeline[index];
                    irr_2 = irradiance_timeline[index+1];
                    if (x < 600) irradiance_integral += irr_1 + (x+3000)*(irr_2-irr_1)/3600.;
                    else irradiance_integral += irr_1 + (x-600)*(irr_2-irr_1)/3600.;
                }
                else if (forecast_method == 3)
                {
                    irr_1 = forecast_timeline[index];
                    irr_2 = forecast_timeline[index+1];
                    if (x < 600) irradiance_integral += irr_1 + (x+3000)*(irr_2-irr_1)/3600.;
                    else irradiance_integral += irr_1 + (x-600)*(irr_2-irr_1)/3600.;
                }
            }
            daytime += config->timestep_size;
        }
    }
}


void Location::update_year_ts (int year)
{
    if (year < first_year || year > last_year)
    {
        if (is_PVGIS)
        {
            // The PVGIS file contains data of more than one year (either 2005-2016 or 2007-2016).
            // Try to find a suitable one:
            year_ts = 2008 + year%4;  // 2008 is the first leap year in the timeseries
            //fprintf (stderr, "\nNo PVGIS timeseries data available for the year %d.\n", year);
            //fprintf (stderr, "I will continue and use the data from the year %d.\n", year_ts);
        }
        else
        {
            year_ts = first_year;
            //fprintf (stderr, "\nThe pv_data file you specified does not contain data of the year %d.\n", year);
            //fprintf (stderr, "I will continue and use the data from the year %d.\n", year_ts);
        }
    }
    else year_ts = year;
}


void Location::update_irradiance_and_temperature_PVGIS (double *irr, double *temp, int index)
{
    double irr_1, irr_2, temp_1, temp_2;

    int x = (int)sim_clock->daytime % 3600;
    // correction needed, because data in PVGIS timeseries has 10 minutes (600s) offset to each hour
    if (x < 600) index -= 1;
    if (index < 0)
    {
        *irr  += irradiance_timeline[0];    // we should actually do a wrap-around here
        *temp += temperature_timeline[0];
    }
    else if (index == num_entries-1) // the last index
    {
        *irr  += irradiance_timeline[index];
        *temp += temperature_timeline[index];
    }
    else // the ordinary case
    {
        irr_1 = irradiance_timeline[index];
        irr_2 = irradiance_timeline[index+1];
        temp_1 = temperature_timeline[index];
        temp_2 = temperature_timeline[index+1];
        if (x < 600)
        {
            *irr  += irr_1 + (x+3000)*(irr_2-irr_1)/3600.;
            *temp += temp_1 + (x+3000)*(temp_2-temp_1)/3600.;
        }
        else
        {
            *irr  += irr_1 + (x-600)*(irr_2-irr_1)/3600.;
            *temp += temp_1 + (x-600)*(temp_2-temp_1)/3600.;
        }
    }
}


void Location::update_irradiance_and_temperature_custom (int index)
{
    double irr_1, irr_2, temp_1, temp_2;

    if (index == num_entries-1) // the last index
    {
        irradiance  = irradiance_timeline[index];
        temperature = temperature_timeline[index];
    }
    else
    {
        int x = (int)sim_clock->daytime % 300;
        irr_1 = irradiance_timeline[index];
        irr_2 = irradiance_timeline[index+1];
        temp_1 = temperature_timeline[index];
        temp_2 = temperature_timeline[index+1];
        irradiance  = irr_1 + x*(irr_2-irr_1)/300.;
        temperature = temp_1 + x*(temp_2-temp_1)/300.;
    }
}
