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

#include "appliance.H"
#include "globals.H"

int compare_double (double *val_1, double *val_2);


template <class AP>
void Appliance_CRTP<AP>::print_EEI (FILE *fp, AP *head, int num_apps)
{
    double value[16];
    AP *app = head;

    if (count[0]==0) return;

    for (int i=0; i<num_energy_classes; i++) value[i] = 0.;
    for (int i=0; i<num_apps; i++)
    {
        value[app->energy_class] += app->consumption;
        app = app->next_app;
    }
    for (int i=0; i<num_energy_classes; i++) fprintf (fp, " %lf", value[i]);
}
template void Appliance_CRTP<Computer>::print_EEI (FILE *fp, Computer *head, int num);
template void Appliance_CRTP<TV>::print_EEI (FILE *fp, TV *head, int num);
template void Appliance_CRTP<Boiler>::print_EEI (FILE *fp, Boiler *head, int num);
template void Appliance_CRTP<Fridge>::print_EEI (FILE *fp, Fridge *head, int num);
template void Appliance_CRTP<Light>::print_EEI (FILE *fp, Light *head, int num);
template void Appliance_CRTP<ElectricStove>::print_EEI (FILE *fp, ElectricStove *head, int num);
template void Appliance_CRTP<TumbleDryer>::print_EEI (FILE *fp, TumbleDryer *head, int num);
template void Appliance_CRTP<CirculationPump>::print_EEI (FILE *fp, CirculationPump *head, int num);
template void Appliance_CRTP<Dishwasher>::print_EEI (FILE *fp, Dishwasher *head, int num);
template void Appliance_CRTP<WashingMachine>::print_EEI (FILE *fp, WashingMachine *head, int num);
template void Appliance_CRTP<Freezer>::print_EEI (FILE *fp, Freezer *head, int num);
template void Appliance_CRTP<E_Vehicle>::print_EEI (FILE *fp, E_Vehicle *head, int num);
template void Appliance_CRTP<AirConditioner>::print_EEI (FILE *fp, AirConditioner *head, int num);
template void Appliance_CRTP<Vacuum>::print_EEI (FILE *fp, Vacuum *head, int num);
template void Appliance_CRTP<Heating>::print_EEI (FILE *fp, Heating *head, int num);
template void Appliance_CRTP<HeatPump>::print_EEI (FILE *fp, HeatPump *head, int num);


template <class AP>
void Appliance_CRTP<AP>::print_consumption (FILE *fp, const char name[])
{
    int res;
    char str[64];
    double medi[k_max_residents+1];

    if (count[0])
    {
        calc_consumption();
        for (res=0; res<=k_max_residents; res++) medi[res] = median(res);
        if (rank == 0)
        {
            fprintf (fp, "%-20s", name);
            for (res=1; res<=k_max_residents; res++)
            {
                sprintf (str, "%d/%d", hh_count[res], count[res]);
                fprintf (fp, "%16s", str);
            }
            sprintf (str, "%d/%d", hh_count[0], count[0]);
            fprintf (fp, "%16s", str);

            fprintf (fp, "\n  Cons. min.        ");
            for (res=1; res<=k_max_residents; res++)
                if (count[res]) fprintf (fp, "%16.3lf", consumption_min[res]);
                else fprintf (fp, "%16.3lf", 0.);
            fprintf (fp, "%16.3lf", consumption_min[0]);

            fprintf (fp, "\n  Cons. avg. (w/a)  ");
            for (res=1; res<=k_max_residents; res++)
                if (hh_count[res]) fprintf (fp, "%16.3lf", consumption_sum[res]/hh_count[res]);
                else fprintf (fp, "%16.3lf", 0.);
            fprintf (fp, "%16.3lf", consumption_sum[0]/hh_count[0]);

            fprintf (fp, "\n  Cons. avg. (all)  ");
            for (res=1; res<=k_max_residents; res++)
                if (Household::count[res]) fprintf (fp, "%16.3lf", consumption_sum[res]/Household::count[res]);
                else fprintf (fp, "%16.3lf", 0.);
            fprintf (fp, "%16.3lf", consumption_sum[0]/Household::count[0]);

            fprintf (fp, "\n  Cons. max.        ");
            for (res=1; res<=k_max_residents; res++) fprintf (fp, "%16.3lf", consumption_max[res]);
            fprintf (fp, "%16.3lf", consumption_max[0]);

            fprintf (fp, "\n  Std. dev.         ");
            for (res=1; res<=k_max_residents; res++) fprintf (fp, "%16.3lf", std_deviation(res));
            fprintf (fp, "%16.3lf", std_deviation(0));

            fprintf (fp, "\n  Median            ");
            for (res=1; res<=k_max_residents; res++) fprintf (fp, "%16.3lf", medi[res]);
            fprintf (fp, "%16.3lf", medi[0]);
            fprintf (fp, "\n\n");
        }
    }
}
template void Appliance_CRTP<Computer>::print_consumption (FILE *fp, const char name[]);
template void Appliance_CRTP<TV>::print_consumption (FILE *fp, const char name[]);
template void Appliance_CRTP<Boiler>::print_consumption (FILE *fp, const char name[]);
template void Appliance_CRTP<Fridge>::print_consumption (FILE *fp, const char name[]);
template void Appliance_CRTP<Light>::print_consumption (FILE *fp, const char name[]);
template void Appliance_CRTP<ElectricStove>::print_consumption (FILE *fp, const char name[]);
template void Appliance_CRTP<TumbleDryer>::print_consumption (FILE *fp, const char name[]);
template void Appliance_CRTP<CirculationPump>::print_consumption (FILE *fp, const char name[]);
template void Appliance_CRTP<Dishwasher>::print_consumption (FILE *fp, const char name[]);
template void Appliance_CRTP<WashingMachine>::print_consumption (FILE *fp, const char name[]);
template void Appliance_CRTP<Freezer>::print_consumption (FILE *fp, const char name[]);
template void Appliance_CRTP<E_Vehicle>::print_consumption (FILE *fp, const char name[]);
template void Appliance_CRTP<AirConditioner>::print_consumption (FILE *fp, const char name[]);
template void Appliance_CRTP<Vacuum>::print_consumption (FILE *fp, const char name[]);
template void Appliance_CRTP<Heating>::print_consumption (FILE *fp, const char name[]);
template void Appliance_CRTP<HeatPump>::print_consumption (FILE *fp, const char name[]);


template <class AP>
double Appliance_CRTP<AP>::print_summary (FILE *fp, const char name[])
{
    if (count[0]) fprintf (fp, "%20s %17.3lf kWh\n", name, consumption_sum[0]);
    return consumption_sum[0];
}
template double Appliance_CRTP<Computer>::print_summary (FILE *fp, const char name[]);
template double Appliance_CRTP<TV>::print_summary (FILE *fp, const char name[]);
template double Appliance_CRTP<Boiler>::print_summary (FILE *fp, const char name[]);
template double Appliance_CRTP<Fridge>::print_summary (FILE *fp, const char name[]);
template double Appliance_CRTP<Light>::print_summary (FILE *fp, const char name[]);
template double Appliance_CRTP<ElectricStove>::print_summary (FILE *fp, const char name[]);
template double Appliance_CRTP<TumbleDryer>::print_summary (FILE *fp, const char name[]);
template double Appliance_CRTP<CirculationPump>::print_summary (FILE *fp, const char name[]);
template double Appliance_CRTP<Dishwasher>::print_summary (FILE *fp, const char name[]);
template double Appliance_CRTP<WashingMachine>::print_summary (FILE *fp, const char name[]);
template double Appliance_CRTP<Freezer>::print_summary (FILE *fp, const char name[]);
template double Appliance_CRTP<E_Vehicle>::print_summary (FILE *fp, const char name[]);
template double Appliance_CRTP<AirConditioner>::print_summary (FILE *fp, const char name[]);
template double Appliance_CRTP<Vacuum>::print_summary (FILE *fp, const char name[]);
template double Appliance_CRTP<Heating>::print_summary (FILE *fp, const char name[]);
template double Appliance_CRTP<HeatPump>::print_summary (FILE *fp, const char name[]);


template <class AP>
void Appliance_CRTP<AP>::calc_consumption()
{
    AP *app = first_app;
    double consumption;
    int res;

    for (res=0; res<=k_max_residents; res++)
    {
        consumption_min[res] = DBL_MAX;
        consumption_max[res] = 0.;
        consumption_sum[res] = 0.;
        consumption_square[res] = 0.;
        hh_count[res] = 0;
    }
    while (app)
    {
        res = app->household->residents;
        hh_count[0]++;
        hh_count[res]++;
        consumption = app->consumption;
        while (app->next_app && app->next_app->household == app->household)
        {
            app = app->next_app;
            consumption += app->consumption;
        }
        consumption_sum[0] += consumption;
        consumption_square[0] += consumption*consumption;
        if (consumption < consumption_min[0]) consumption_min[0] = consumption;
        if (consumption > consumption_max[0]) consumption_max[0] = consumption;
        consumption_sum[res] += consumption;
        consumption_square[res] += consumption*consumption;
        if (consumption < consumption_min[res]) consumption_min[res] = consumption;
        if (consumption > consumption_max[res]) consumption_max[res] = consumption;
        app = app->next_app;
    }
#ifdef PARALLEL
    MPI_Allreduce (MPI_IN_PLACE, consumption_min, k_max_residents+1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
    MPI_Allreduce (MPI_IN_PLACE, consumption_max, k_max_residents+1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
    MPI_Allreduce (MPI_IN_PLACE, consumption_sum, k_max_residents+1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    MPI_Allreduce (MPI_IN_PLACE, consumption_square, k_max_residents+1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    MPI_Allreduce (MPI_IN_PLACE, hh_count, k_max_residents+1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
#endif
}

template <class AP>
double Appliance_CRTP<AP>::std_deviation (int res)
{
    double mean;

    if (hh_count[res])
    {
        mean = consumption_sum[res]/hh_count[res];
        return sqrt (consumption_square[res]/hh_count[res] - mean*mean);
    }
    else return 0.;
}

template <class AP>
double Appliance_CRTP<AP>::median (int res)
{
    double *values;
    double median = 0, consumption;
    int j;
    AP *app;
#ifdef PARALLEL
    int num;
    MPI_Status status;
#endif

    if (count[res])
    {
        alloc_memory (&values, hh_count[res], "Appliance_CRTP::median");
        j=0;
        app = first_app;
        while (app)
        {
            if (app->household->residents == res || res == 0)
            {
                consumption = app->consumption;
                while (app->next_app && app->next_app->household == app->household)
                {
                    app = app->next_app;
                    consumption += app->consumption;
                }
                values[j] = consumption;
                j++;
            }
            app = app->next_app;
        }
#ifdef PARALLEL
        if (rank == 0)
        {
            for (int i=1; i<num_processes; i++)
            {
                MPI_Recv (&num, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &status);
                MPI_Recv (values+j, num, MPI_DOUBLE, i, 0, MPI_COMM_WORLD, &status);
                j += num;
            }
        }
        else
        {
            MPI_Send (&j, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
            MPI_Send (values, j, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD);
        }
        MPI_Barrier (MPI_COMM_WORLD);
#endif
        if (rank == 0)
        {
            qsort (values, (size_t)hh_count[res], sizeof(double), (int (*)(const void *, const void *))&compare_double);
            if (hh_count[res]%2 == 1) median = values[hh_count[res]/2];
            else median = (values[hh_count[res]/2] + values[hh_count[res]/2-1]) / 2.;
        }
        delete [] values;
        return median;
    }
    else return 0.;
}


int compare_double (double *val_1, double *val_2)
{
    if (*val_1 < *val_2) return -1;
    else if (*val_1 > *val_2) return 1;
    else return 0;
}
