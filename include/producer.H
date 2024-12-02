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

#ifndef PRODUCER_H
#define PRODUCER_H

#include <stdio.h>
#include "types.H"

class Producer
{
private:
    double maximum_peak;  // [kW]
    FILE *profile_fp;
    double *profile_data;
    FILE *delta_fp;
    double *delta_data;
    FILE *power_fp;
    double power;
    double power_gradient;
    class Fridge **fridge;
    class Freezer **freezer;
    class E_Vehicle **vehicle;
    int num_fridges, num_freezers, num_vehicles;
    double *price_table[NUM_PRICE_TABLES];
    int price_table_length[NUM_PRICE_TABLES];
    void init_price_table (int table_id);
    void init_best_price();
    void init_price_intervals();
    PriceInterval *price_intervals;
    int num_intervals;

public:
    Producer();
    ~Producer();
    void update_maximum_peak();
    void simulate (double cur_time);
    void best_price (double cur_time, int preview_length, int *num, int intervals[]);
    double price (int table_id, double time) { return price_table[table_id][((int)(time/60.))%price_table_length[table_id]]; }
    double min_price_in_time_interval (double start_time, double end_time);
    void next_best_price_interval (double start_time, double end_time, int *best_start, int *best_end);
};

#endif
