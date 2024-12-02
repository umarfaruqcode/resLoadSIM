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

#ifndef LOCATION_H
#define LOCATION_H

#define _USE_MATH_DEFINES
#include <math.h>

#include "constants.H"

class Location
{
private:
    double *irradiance_timeline;     // global irradiance in W/m2 (hourly data read from PVGIS or custom format file)
    double *temperature_timeline;    // ambient temperature in °C (hourly data read from PVGIS or custom format file)
    double *forecast_timeline;       // forecast of the global irradiance in W/m2 (hourly data read from forecast file)
    int year_ts;                     // this is the timeseries year, which is not necessarily
                                     // the actual year of the simulation. timeseries data is
                                     // only available for the years FIRST_YEAR to LAST_YEAR
    int num_entries;
    int *offset_year;
    double *temp_ambient_mean;        // mean yearly ambient temperature in K
    int *coldest_day;                 // coldest day of the year. 1.Jan = 1, 2.Jan = 2, ...
    bool is_PVGIS;                    // if true => PV data is read from a PVGIS file

    void update_irradiance_and_temperature_PVGIS (double *irr, double *temp, int index);
    void update_irradiance_and_temperature_custom (int index);

public:
    int first_year;
    int last_year;
    char name[32];
    int type;                   // 0 = "urban", 1 = "rural"
    char country[8];            // ISO country code
    double latitude;            // the location's coordinate in degrees
    double longitude;           // -"-
    int utc_offset_base;        // difference between local time and UTC when no daylight saving time (DST) is in effect
    int utc_offset;             // actual difference between local time and UTC in hours (depending on DST)
    double seasonal_factor;     // seasonal effect of the cold water temperature
    double temp_H2O_cold_0;     // nominal cold water temperature in °C
    double irradiance;          // global irradiance in W/m2 (updated every timestep)
    double temperature;         // ambient temperature in °C (updated every timestep)
    double irradiance_integral; // total irradiance between sunrise and sunset

    Location (char location_name[], int year, char pv_data_file_name[], char pv_forecast_file_name[],
              int charging_strategy, int forecast_method);
    ~Location();
    void update_values();
    void update_year_ts (int year);
};

#endif
