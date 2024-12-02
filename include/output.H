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

#ifndef OUTPUT_H
#define OUTPUT_H

#include <stdio.h>

#include "constants.H"


enum
{
    SINGLE=0,
    MULTIPLE,
    ALL,
    NUM_OUTPUT_MODES
};


class Output
{
private:
    FILE *file_ptr[k_max_files];
    FILE *battery_file;
    FILE *gridbalance_file;
    double *power_from_grid_total;
    double *power_to_grid_total;
    double *power_above_limit_total;
    double *power[k_max_files];
    double *value_ptr_2[k_max_files];
    double *charge_total;
    double *power_charging_total;
    double *power_discharging_total;
    double *loss_charging_total;
    double *loss_discharging_total;
    double *battery_from_grid_total;
    char names[k_max_files][k_name_length];
    int num_files;

public:
    Output();
    void add (const char *classname, double *ptr_1, double *ptr_2);
    void add_battery (double *ptr_1, double *ptr_2, double *ptr_3,
                      double *ptr_4, double *ptr_5);
    void add_gridbalance (double *ptr_1, double *ptr_2, double *ptr_3, double *ptr_4);
    void remove_old_files();
    void open_files();
    void close_files();
    void print_power();
    void print_battery_stats();
    void print_gridbalance();
    void print_consumption (int year);
    void print_distribution (int year);
    void print_households (int year);
    void print_summary (int year);
    void print_max (int year);
    void reset();
};

#endif
