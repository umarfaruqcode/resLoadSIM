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
#ifdef PARALLEL
#   include <mpi.h>
#endif

#include "appliance.H"
#include "household.H"
#include "globals.H"
#include "proto.H"
#include "producer.H"

static int compare_fridges (const void *f1, const void *f2);
static int compare_freezers (const void *f1, const void *f2);

Producer::Producer()
{
    const char function_name[] = "Producer::Producer";
    maximum_peak = 0.;
    profile_fp = NULL;
    profile_data = NULL;
    delta_fp = NULL;
    delta_data = NULL;
    power_fp = NULL;
    power = 0.;
    power_gradient = 0.;
    fridge = NULL;
    freezer = NULL;
    vehicle = NULL;

    init_price_table (GRID);
    init_price_table (SOLAR);
    init_price_intervals();

    if (config->control == PROFILE)
    {
        open_file (&profile_fp, "profile", "r");
        alloc_memory (&profile_data, 1440, function_name);
        for (int i=0; i<1440; i++)
        {
            fscanf (profile_fp, "%*f%lf", profile_data+i);
        }
        fclose (profile_fp);
    }
    else if (config->control == COMPENSATE)
    {
        open_file (&delta_fp, "delta", "r");
        alloc_memory (&delta_data, 96, function_name);
        for (int i=0; i<96; i++)
        {
            fscanf (delta_fp, "%lf", delta_data+i);
        }
        fclose (delta_fp);
        if (rank == 0) open_file (&power_fp, "power_Producer", "w");
    }
    if (config->fridge.smartgrid_enabled > 0.) num_fridges = Fridge::create_smart_list (&fridge);
    if (config->freezer.smartgrid_enabled > 0.) num_freezers = Freezer::create_smart_list (&freezer);
    if (config->e_vehicle.smartgrid_enabled > 0.) num_vehicles = E_Vehicle::create_smart_list (&vehicle);
}


Producer::~Producer()
{
    delete [] price_intervals;
    for (int i=0; i<NUM_PRICE_TABLES; i++) delete [] price_table[i];
    if (config->control == PROFILE) delete [] profile_data;
    if (config->control == COMPENSATE) delete [] delta_data;
}


void Producer::update_maximum_peak()
{
#ifdef PARALLEL
    if (rank == 0)
        MPI_Reduce (MPI_IN_PLACE, Household::real_power_total, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    else
        MPI_Reduce (Household::real_power_total, Household::real_power_total, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
#endif
    if (Household::real_power_total[0] > maximum_peak) maximum_peak = Household::real_power_total[0];
}


void Producer::simulate (double cur_time)
{
    double upper_limit, lower_limit;
    static int pos = -1;
    static bool flag = false;
    static double limit_0;
    static int time_0;
    int i;
    int time = cur_time/60.;  // the simulation time in minutes
#ifdef PARALLEL
    double power_global;
    int finished;
#endif

    switch (config->control)
    {
        case NONE:
            return;

        case PEAK_SHAVING:
#ifdef PARALLEL
            if (time == 0.) MPI_Bcast (&maximum_peak, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif
            if (config->peak_shaving.relative)
            {
                upper_limit = maximum_peak*config->peak_shaving.threshold/100.;
                lower_limit = maximum_peak*(config->peak_shaving.threshold-5)/100.;
            }
            else
            {
                upper_limit = config->peak_shaving.threshold;
                lower_limit = config->peak_shaving.threshold*0.9;
            }
            break;

        case PROFILE:
            upper_limit = profile_data[time % 1440];
            lower_limit = upper_limit;
            break;

        case COMPENSATE:
            if (time % 15 == 0 && time < 2880)  // delta_data available every 15 minutes
            {
                pos++;  // adjust read position
            }
            if (!flag)
            {
                flag = true;
                time_0 = time;
                limit_0 = (1.+ delta_data[pos]) * Household::real_power_total[0];
                power_gradient = (Household::real_power_total[0] - limit_0) / 60.;
            }

            if (time-time_0 < 60)
            {
                upper_limit = limit_0;
                lower_limit = limit_0;
                if (power_gradient > 0.) power += power_gradient;
            }
    /*        else if (time-time_0 < 120)
            {
                upper_limit = limit_0 + power;
                lower_limit = 0;
            }*/
            else  // do not interfere
            {
                upper_limit = INFINITY;
                lower_limit = 0.;
            }
            break;

        case PRICE:
            return;

        default:
            printf ("Producer: Unknown control mode\n");
            exit (1);
    }
#ifdef PARALLEL
    MPI_Allreduce (Household::real_power_total, &power_global, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    if (power_global > upper_limit)
#else
    if (Household::real_power_total[0] > upper_limit)
#endif
    {
        if (config->fridge.smartgrid_enabled)
        {
            qsort (fridge, (size_t)num_fridges, sizeof(class Fridge*), &compare_fridges);
            // turn off as many appliances as necessary to
            // keep the total load below the upper limit
            i = 0;
#ifdef PARALLEL
            finished = 0;
            while (power_global > upper_limit && finished < num_processes)
            {
                if (i<num_fridges)
                {
                    fridge[i]->turn_off();
                    i++;
                    finished = 0;
                }
                else finished = 1;
                MPI_Allreduce (MPI_IN_PLACE, &finished, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
                MPI_Allreduce (Household::real_power_total, &power_global, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
            }
#else
            while (Household::real_power_total[0] > upper_limit && i<num_fridges)
            {
                fridge[i]->turn_off();
                i++;
            }
#endif
        }
        if (config->freezer.smartgrid_enabled)
        {
            qsort (freezer, (size_t)num_freezers, sizeof(class Freezer*), &compare_freezers);
            // turn off as many appliances as necessary to
            // keep the total load below the upper limit
            i = 0;
#ifdef PARALLEL
            finished = 0;
            while (power_global > upper_limit && finished < num_processes)
            {
                if (i<num_freezers)
                {
                    freezer[i]->turn_off();
                    i++;
                    finished = 0;
                }
                else finished = 1;
                MPI_Allreduce (MPI_IN_PLACE, &finished, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
                MPI_Allreduce (Household::real_power_total, &power_global, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
            }
#else
            while (Household::real_power_total[0] > upper_limit && i<num_freezers)
            {
                freezer[i]->turn_off();
                i++;
            }
#endif
        }
        if (config->e_vehicle.smartgrid_enabled)
        {
            // turn off as many appliances as necessary to
            // keep the total load below the upper limit
            i = 0;
#ifdef PARALLEL
            finished = 0;
            while (power_global > upper_limit && finished < num_processes)
            {
                if (i<num_vehicles)
                {
                    vehicle[i]->turn_off();
                    i++;
                    finished = 0;
                }
                else finished = 1;
                MPI_Allreduce (MPI_IN_PLACE, &finished, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
                MPI_Allreduce (Household::real_power_total, &power_global, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
            }
#else
            while (Household::real_power_total[0] > upper_limit && i<num_vehicles)
            {
                vehicle[i]->turn_off();
                i++;
            }
#endif
        }
        if (config->dishwasher.smartgrid_enabled) Dishwasher::stop = true;
        if (config->wmachine.smartgrid_enabled) WashingMachine::stop = true;
        if (config->dryer.smartgrid_enabled) TumbleDryer::stop = true;
    }
#ifdef PARALLEL
    else if (power_global < lower_limit && config->control != PEAK_SHAVING)
#else
    else if (Household::real_power_total[0] < lower_limit && config->control != PEAK_SHAVING)
#endif
    {
        if (config->fridge.smartgrid_enabled)
        {
            qsort (fridge, (size_t)num_fridges, sizeof(class Fridge*), &compare_fridges);
            i = num_fridges-1;
#ifdef PARALLEL
            finished = 0;
            while (power_global < lower_limit && finished < num_processes)
            {
                if (i>=0)
                {
                    fridge[i]->turn_on();
                    i--;
                    finished = 0;
                }
                else finished = 1;
                MPI_Allreduce (MPI_IN_PLACE, &finished, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
                MPI_Allreduce (Household::real_power_total, &power_global, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
            }
#else
            while (Household::real_power_total[0] < lower_limit && i>=0)
            {
                fridge[i]->turn_on();
                i--;
            }
#endif
        }
        if (config->freezer.smartgrid_enabled)
        {
            qsort (freezer, (size_t)num_freezers, sizeof(class Freezer*), &compare_freezers);
            i = num_freezers-1;
#ifdef PARALLEL
            finished = 0;
            while (power_global < lower_limit && finished < num_processes)
            {
                if (i>=0)
                {
                    freezer[i]->turn_on();
                    i--;
                    finished = 0;
                }
                else finished = 1;
                MPI_Allreduce (MPI_IN_PLACE, &finished, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
                MPI_Allreduce (Household::real_power_total, &power_global, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
            }
#else
            while (Household::real_power_total[0] < lower_limit && i>=0)
            {
                freezer[i]->turn_on();
                i--;
            }
#endif
        }
        if (config->e_vehicle.smartgrid_enabled)
        {
            i = num_vehicles-1;
#ifdef PARALLEL
            finished = 0;
            while (power_global < lower_limit && finished < num_processes)
            {
                if (i>=0)
                {
                    vehicle[i]->turn_on();
                    i--;
                    finished = 0;
                }
                else finished = 1;
                MPI_Allreduce (MPI_IN_PLACE, &finished, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
                MPI_Allreduce (Household::real_power_total, &power_global, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
            }
#else
            while (Household::real_power_total[0] < lower_limit && i>=0)
            {
                vehicle[i]->turn_on();
                i--;
            }
#endif
        }
        if (config->dishwasher.smartgrid_enabled) Dishwasher::stop = false;
        if (config->wmachine.smartgrid_enabled) WashingMachine::stop = false;
        if (config->dryer.smartgrid_enabled) TumbleDryer::stop = false;
    }
#ifdef PARALLEL
    else if (power_global > lower_limit && config->control == PEAK_SHAVING)
#else
    else if (Household::real_power_total[0] > lower_limit && config->control == PEAK_SHAVING)
#endif
    {

        if (config->fridge.smartgrid_enabled)
        {
            qsort (fridge, (size_t)num_fridges, sizeof(class Fridge*), &compare_fridges);
            i = num_fridges-1;
#ifdef PARALLEL
            finished = 0;
            while (power_global < upper_limit && finished < num_processes)
            {
                if (i>=0)
                {
                    fridge[i]->turn_on();
                    i--;
                    finished = 0;
                }
                else finished = 1;
                MPI_Allreduce (MPI_IN_PLACE, &finished, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
                MPI_Allreduce (Household::real_power_total, &power_global, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
            }
#else
            while (Household::real_power_total[0] < upper_limit && i>=0)
            {
                fridge[i]->turn_on();
                i--;
            }
#endif
        }
        if (config->freezer.smartgrid_enabled)
        {
            qsort (freezer, (size_t)num_freezers, sizeof(class Freezer*), &compare_freezers);
            i = num_freezers-1;
#ifdef PARALLEL
            finished = 0;
            while (power_global < upper_limit && finished < num_processes)
            {
                if (i>=0)
                {
                    freezer[i]->turn_on();
                    i--;
                    finished = 0;
                }
                else finished = 1;
                MPI_Allreduce (MPI_IN_PLACE, &finished, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
                MPI_Allreduce (Household::real_power_total, &power_global, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
            }
#else
            while (Household::real_power_total[0] < upper_limit && i>=0)
            {
                freezer[i]->turn_on();
                i--;
            }
#endif
        }
        if (config->e_vehicle.smartgrid_enabled)
        {
            i = num_vehicles-1;
#ifdef PARALLEL
            finished = 0;
            while (power_global < upper_limit && finished < num_processes)
            {
                if (i>=0)
                {
                    vehicle[i]->turn_on();
                    i--;
                    finished = 0;
                }
                else finished = 1;
                MPI_Allreduce (MPI_IN_PLACE, &finished, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
                MPI_Allreduce (Household::real_power_total, &power_global, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
            }
#else
            while (Household::real_power_total[0] < upper_limit && i>=0)
            {
                vehicle[i]->turn_on();
                i--;
            }
#endif
        }
        if (config->dishwasher.smartgrid_enabled) Dishwasher::stop = false;
        if (config->wmachine.smartgrid_enabled) WashingMachine::stop = false;
        if (config->dryer.smartgrid_enabled) TumbleDryer::stop = false;
    }

    if (config->control == COMPENSATE)
    {
#ifdef PARALLEL
        if (rank == 0)
            MPI_Reduce (MPI_IN_PLACE, &power, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        else
            MPI_Reduce (&power, &power, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
#endif
        if (rank == 0) fprintf (power_fp, "%lf %lf\n", (double)time/60., power);
    }
}


void Producer::init_price_table (int table_id)
{
    const char function_name[] = "Producer::init_price_table";
#ifdef DEBUG
    FILE *out=NULL;
#endif
    int index_1, begin, end, end_old;
    double p, p_old, k;
    int seq_elem, num_triples=0;
    double *time=NULL, *price=NULL;
    Profile *profile;

    price_table_length[table_id] = config->price[table_id].seq_length*1440;
    alloc_memory (&price_table[table_id], price_table_length[table_id], function_name);
    for (int i=0; i<config->price[table_id].seq_length; i++)
    {
        seq_elem = config->price[table_id].sequence[i];
        num_triples += config->price[table_id].profiles[seq_elem-1].length;
    }
    alloc_memory (&time, num_triples*2, function_name);
    alloc_memory (&price, num_triples, function_name);
    index_1 = 0;   // triple index
    for (int i=0; i<config->price[table_id].seq_length; i++)
    {
        seq_elem = config->price[table_id].sequence[i];
        profile = config->price[table_id].profiles+(seq_elem-1);
        for (int j=0; j<profile->length; j++)
        {
            time[2*index_1]   = profile->begin[j];
            time[2*index_1+1] = profile->end[j];
            price[index_1]    = profile->price[j];
            index_1++;
        }
    }
    int idx=0;  // price table index
    p_old = 0.;
    end_old = 0;
    for (int t=0; t<num_triples; t++)
    {
        begin = time[2*t] * 60;
        end = time[2*t+1] * 60;
        p = price[t];
        if (idx % 1440 < begin)  // interpolate
        {
            k = (p-p_old)/(begin-end_old);
            for (int i=0; i<(begin-end_old); i++) price_table[table_id][idx++] = p_old + i*k;
        }
        else if (idx % 1440 > begin)  // interpolate
        {
            k = (p-p_old)/(begin-end_old+1440);
            for (int i=0; i<(begin-end_old+1440); i++) price_table[table_id][idx++] = p_old + i*k;
        }
        for (int i=0; i<(end-begin); i++)
        {
            price_table[table_id][idx++] = p;
        }
        p_old = p;
        end_old = end;
    }
    delete [] time;
    delete [] price;

#ifdef DEBUG
    if (rank == 0)
    {
        if (table_id == GRID) open_file (&out, "price_table_grid", "w");
        else open_file (&out, "price_table_solar", "w");
        for (int i=0; i<price_table_length[table_id]; i++)
        {
            fprintf (out, "%lf %lf\n", i/60., price_table[table_id][i]);
        }
        fclose (out);
    }
#endif
}

void Producer::init_price_intervals()
{
    const char function_name[] = "Producer::init_price_intervals";
    int seq_elem;
    int index, start = 0;
    Profile *profile, *first_profile = NULL, *last_profile = NULL;

    num_intervals=0;
    for (int i=0; i<config->price[GRID].seq_length; i++)
    {
        seq_elem = config->price[GRID].sequence[i];
        profile = config->price[GRID].profiles+(seq_elem-1);
        if (i==0) first_profile = profile;
        if (i>0 && fabs (profile->price[0] - last_profile->price[last_profile->length-1]) < k_float_compare_eps)
            num_intervals += profile->length-1;
        else
            num_intervals += profile->length;
        last_profile = profile;
    }
    alloc_memory (&price_intervals, num_intervals, function_name);
    index = 0;   // interval index
    for (int i=0; i<config->price[GRID].seq_length; i++)
    {
        seq_elem = config->price[GRID].sequence[i];
        profile = config->price[GRID].profiles+(seq_elem-1);
        if (i>0 && fabs (profile->price[0] - last_profile->price[last_profile->length-1]) < k_float_compare_eps)
        {
            price_intervals[index-1].length = 60*(profile->end[0] + 24 - price_intervals[index-1].begin);
            start = 1;
        }
        for (int j=start; j<profile->length; j++)
        {
            price_intervals[index].begin = 60*(profile->begin[j] + 24*i);
            price_intervals[index].length = 60*(profile->end[j] - profile->begin[j]);
            price_intervals[index].price = profile->price[j];
            index++;
        }
        start = 0;
        last_profile = profile;
    }
    if (fabs (first_profile->price[0] - last_profile->price[last_profile->length-1]) < k_float_compare_eps)
    {
        price_intervals[0].begin = price_intervals[index-1].begin;
        price_intervals[0].length = first_profile->end[0]*60 + 1440 - price_intervals[index-1].begin;
        num_intervals--;
    }
}

void Producer::best_price (double time, int preview_length, int *num, int intervals[])
{
    // time is in seconds, preview_length and intervals in minutes
    int pos = int(time/60.) % price_table_length[GRID]; // minutes
    double best = DBL_MAX;

    *num = 0;
    for (int i=0; i<num_intervals; i++)
    {
        if (((pos > price_intervals[i].begin && pos < price_intervals[i].begin + price_intervals[i].length)
             || price_intervals[i].begin - pos < preview_length)
            && price_intervals[i].price <= best)
        {
            if (price_intervals[i].price < best)
            {
                best = price_intervals[i].price;
                *num = 0;
            }
            if (pos > price_intervals[i].begin)
                intervals[*num*2] = pos;
            else
                intervals[*num*2] = price_intervals[i].begin;
            intervals[*num*2+1] = price_intervals[i].length;
            (*num)++;
        }
    }
}

void Producer::next_best_price_interval (double start_time, double end_time, int *best_start, int *best_end)
{
    int i;
    double min = min_price_in_time_interval (start_time, end_time);
    int pos = int(start_time/60.) % price_table_length[GRID];  // pos marks an index in the price table
    int length = (end_time - start_time)/60.;                  // length of the time interval in minutes
    *best_start = -1;
    *best_end = -1;
    for (i=0; i<length; i++)
    {
        if (fabs (price_table[GRID][pos] - min) < k_float_compare_eps && *best_start == -1) *best_start = i*60;
        if (fabs (price_table[GRID][pos] - min) >= k_float_compare_eps && *best_start != -1 && *best_end == -1)
        {
            *best_end = i*60;
            break;
        }
        pos++;
        if (pos == price_table_length[GRID]) pos = 0;     // wrap around
    }
    if (*best_end == -1) *best_end = i*60;
}


double Producer::min_price_in_time_interval (double start_time, double end_time)
{
    double min_price = DBL_MAX;
    int pos = int(start_time/60.) % price_table_length[GRID];  // pos marks an index in the price table
    int length = (end_time - start_time)/60.;                  // length of the time interval in minutes
    for (int i=0; i<length; i++)
    {
        if (min_price > price_table[GRID][pos]) min_price = price_table[GRID][pos];
        pos++;
        if (pos == price_table_length[GRID]) pos = 0;     // wrap around
    }
    return min_price;
}


static int compare_fridges (const void *f1, const void *f2)
{
    double delta = *(Fridge*)f1 - *(Fridge*)f2;
    if (delta > 0) return 1;
    else if (delta < 0) return -1;
    return 0;
}

static int compare_freezers (const void *f1, const void *f2)
{
    double delta = *(Freezer*)f1 - *(Freezer*)f2;
    if (delta > 0) return 1;
    else if (delta < 0) return -1;
    return 0;
}
