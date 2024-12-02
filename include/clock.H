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

#ifndef CLOCK_H
#define CLOCK_H

enum Weekday
{
    SUNDAY=0,
    MONDAY,
    TUESDAY,
    WEDNESDAY,
    THURSDAY,
    FRIDAY,
    SATURDAY
};

enum Month
{
    JANUARY=1,
    FEBRUARY,
    MARCH,
    APRIL,
    MAY,
    JUNE,
    JULY,
    AUGUST,
    SEPTEMBER,
    OCTOBER,
    NOVEMBER,
    DECEMBER
};


class Clock
{
private:
    int yearday;
    int dst_day_1, dst_day_2;
    int heat_start_day, heat_end_day;
    Weekday calc_weekday (int day, int month, int year, bool leap_year);
    void calc_sunrise_sunset();
    void init_daylight_saving_time();
    void check_date (int d, int m, int y, const char *descriptor, const char *file_name);
    int convert_to_day_of_year (int d, int m);
    bool holiday_matrix[12][31];
    void init_holidays();

public:
    double sunrise;     // time of sunrise in seconds
    double sunset;      // time of sunset in seconds
    double daytime;     // time of the day in seconds; a value between 0 and 86400
    double yeartime;    // time of the year in seconds; starts with 0 on 1st of January every year
    double cur_time;    // current spent simulation time in seconds
    double end_time;    // duration of the simulation (main phase) in seconds
    Weekday weekday;    // sunday = 0, ... , saturday = 6
    int day;            // a value between 1 and 31
    int month;          // a value between 1 and 12
    int year;
    bool leap_year;
    bool midnight;
    bool heating_period;
    bool forerun;
    bool holiday;

    Clock();
    void forward();
    void set_date_time (int d, int m, int y, double t);
};


#endif
