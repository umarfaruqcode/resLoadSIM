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

#include "proto.H"
#include "globals.H"
#include "clock.H"


Clock::Clock()
{
    set_date_time (config->start.day, config->start.month, config->start.year, config->start.time * 3600);
    forerun = false;
}


void Clock::set_date_time (int d, int m, int y, double t)
{
    day = d;
    month = m;
    year = y;
    daytime = t;

    leap_year = year%4==0 && (year%100>0 || year%400==0);
    location->update_year_ts (year);

    check_date (day, month, year, "simulation start date", k_rls_json_file_name);
    yearday = convert_to_day_of_year (day, month);

    check_date (config->household.heating_period_start_day,
                config->household.heating_period_start_month, year,
                "first day of the heating period", k_hh_json_file_name);
    heat_start_day = convert_to_day_of_year (config->household.heating_period_start_day,
                                             config->household.heating_period_start_month)
                     + get_random_number (0, 10);

    check_date (config->household.heating_period_end_day,
                config->household.heating_period_end_month, year,
                "last day of the heating period", k_hh_json_file_name);
    heat_end_day = convert_to_day_of_year (config->household.heating_period_end_day,
                                           config->household.heating_period_end_month)
                   + get_random_number (0, 10);

    if (heat_start_day > heat_end_day) heating_period = (yearday >= heat_start_day) || (yearday < heat_end_day);
    else heating_period = (yearday >= heat_start_day) && (yearday < heat_end_day);

    weekday = calc_weekday (day, month, year, leap_year);
    yeartime = (yearday-1)*24*3600 + daytime;
    if (almost_equal (daytime, k_seconds_per_day))  // user has set the start time to 24 hours
    {
        daytime -= config->timestep_size;
        yeartime -= config->timestep_size;
        forward();
    }
    midnight = almost_equal (daytime, 0.);
    switch (config->daylight_saving_time)
    {
        case 0:  // wintertime throughout the year
            location->utc_offset = location->utc_offset_base;
            break;

        case 1:  // standard DST behaviour, clock changes twice a year
            init_daylight_saving_time();
            if (   (month == MARCH && day >= dst_day_1)
                || (month > MARCH && month < OCTOBER)
                || (month == OCTOBER && day < dst_day_2))
            {
                location->utc_offset = location->utc_offset_base+1;
            }
            else
            {
                location->utc_offset = location->utc_offset_base;
            }
            break;

        case 2:  // summertime throughout the year
            location->utc_offset = location->utc_offset_base+1;
            break;
    }
    calc_sunrise_sunset();
    init_holidays();
}


void Clock::forward()
{
    daytime  += config->timestep_size;
    yeartime += config->timestep_size;
    cur_time += config->timestep_size;
    if (daytime >= k_seconds_per_day)
    {
        daytime = 0.;
        midnight = true;
        day++;
        yearday++;
        weekday = (Weekday)((weekday+1) % 7);
        switch (month)
        {
            case JANUARY:
            case MARCH:
            case MAY:
            case JULY:
            case AUGUST:
            case OCTOBER:
            case DECEMBER:
                if (day == 32)
                {
                    day = 1;
                    month = month % 12 + 1;
                }
                break;
            case APRIL:
            case JUNE:
            case SEPTEMBER:
            case NOVEMBER:
                if (day == 31)
                {
                    day = 1;
                    month = month % 12 + 1;
                }
                break;
            case FEBRUARY:
                if ((day == 29 && !leap_year) || day == 30)
                {
                    day = 1;
                    month = MARCH;
                }
                break;
        }
        if (day == 1 && month == JANUARY)
        {
            year++;
            leap_year = year%4==0 && (year%100>0 || year%400==0);
            if (cur_time < end_time) location->update_year_ts (year);
            yeartime = 0.;
            yearday = 1;
            init_holidays();
        }
        if (config->daylight_saving_time == 1)
        {
            if (day == dst_day_1 && month == MARCH) location->utc_offset++;
            if (day == dst_day_2 && month == OCTOBER) location->utc_offset--;
        }
        if (config->simulate_heating)
        {
            if (yearday == heat_start_day) heating_period = true;
            if (yearday == heat_end_day) heating_period = false;
        }
        calc_sunrise_sunset();
    }
    else midnight = false;
    if (daytime == 0.) holiday = holiday_matrix[month-1][day-1];
}


// Calculation of sunrise and sunset times according to:
// http://lexikon.astronomie.info/zeitgleichung/

void Clock::calc_sunrise_sunset()
{
    int day_count[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int day_of_year;
    double declination, latitude, time_diff, time_eq;

    day_of_year = day_count[month-1]+day;
    declination = 0.4095*sin(0.016906*(day_of_year-80.086));
    latitude = location->latitude * M_PI / 180.;

    time_diff = 12 * acos((sin(-0.0145)-sin(latitude)*sin(declination))/(cos(latitude)*cos(declination)))/M_PI;
    time_eq = 0.171*sin(0.0337 * day_of_year + 0.465) + 0.1299*sin(0.01787 * day_of_year - 0.168);

    sunrise = 3600 * (12 - time_diff + time_eq - location->longitude/15. + location->utc_offset);
    sunset  = 3600 * (12 + time_diff + time_eq - location->longitude/15. + location->utc_offset);
}

Weekday Clock::calc_weekday (int dday, int mmonth, int yyear, bool lleap_year)
{
    int list[12] = {0, 3, 3, 6, 1, 4, 6, 2, 5, 0, 3, 5};
    return (Weekday)((dday % 7
               + list[mmonth-1]
               + (yyear%100 + (yyear%100)/4) % 7
               + (3 - (yyear/100)%4) * 2
               + lleap_year*6) % 7);
}

void Clock::init_daylight_saving_time()
{
    int d=31;
    while (calc_weekday (d, MARCH, year, leap_year) != SUNDAY) d--;
    dst_day_1 = d;
    d=31;
    while (calc_weekday (d, OCTOBER, year, leap_year) != SUNDAY) d--;
    dst_day_2 = d;
}

void Clock::check_date (int d, int m, int y, const char *descriptor, const char *file_name)
{
    if (   (m==FEBRUARY && leap_year && d>29)
        || (m==FEBRUARY && !leap_year && d>28)
        || ((m==APRIL || m==JUNE || m==SEPTEMBER || m==NOVEMBER) && d>30))
    {
        fprintf (stderr, "The date %d.%d.%d (%s) is not valid. Check file '%s'\n", d, m, y, descriptor, file_name);
        exit(1);
    }
}

int Clock::convert_to_day_of_year (int d, int m)
{
    int offset_month[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int offset_month_leap[12] = {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335};

    if (leap_year) return d + offset_month_leap[m-1];
    else return d + offset_month[m-1];
}


void Clock::init_holidays()
{
    FILE *fp = NULL;
    char *line = NULL, key[32], value[256], *token;
    char file_name[k_max_path];
    int read_year, read_month, read_day;
    bool is_default = false;
    
    for (int m=0; m<12; m++) for (int d=0; d<31; d++) holiday_matrix[m][d] = false;
    snprintf (file_name, sizeof (file_name), "countries/%s/%s", location->country, k_holidays_json_file_name);
    open_file (&fp, file_name, "r");
    read_line (fp, &line);
    do 
    {
        read_line (fp, &line);
        sscanf (line, "%s \[%[\"0-9., ]", key, value);
        if (strcmp (key, "\"default\":")) read_year = atoi (key+1);
        else is_default = true;
        if (is_default || read_year == year)
        {
            token = strtok (value, ",");
            while (token)
            {
                sscanf (token, "%*[ \"]%d.%d", &read_day, &read_month);
                holiday_matrix[read_month-1][read_day-1] = true;
                token = strtok (NULL, ",");
            }    
        }
    } while (!is_default);
    fclose (fp);
    free (line);
    holiday = holiday_matrix[month-1][day-1];
}