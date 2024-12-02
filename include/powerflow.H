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

#ifndef POWERFLOW_H
#define POWERFLOW_H

class Powerflow
{
private:
    int index;
    int num_buses;
    int num_generators;
    int num_branches;
    int num_transformers;
    int num_files;
    int *signal_points;
    int *hh_to_bus;
    double baseMVA;

    // The structures 'Bus', 'Generator' and 'Branch' store the info which is
    // read from the MATPOWER case file.
    struct Bus
    {
        int nr;
        int type;
        double Pd;
        double Qd;
        double Gs;
        double Bs;
        int area;
        double Vm;
        double Va;
        double baseKV;
        int zone;
        double Vmax;
        double Vmin;
    } *bus;

    struct Generator
    {
        int bus;
        double Pg;
        double Qg;
        double Qmax;
        double Qmin;
        double Vg;
        double mBase;
        int status;
        double Pmax;
        double Pmin;
        double Pc1;
        double Pc2;
        double Qc1min;
        double Qc1max;
        double Qc2min;
        double Qc2max;
        double ramp_agc;
        double ramp_10;
        double ramp_30;
        double ramp_q;
        double apf;
    } *generator;

    struct Branch
    {
        int from;
        int to;
        double r;
        double x;
        double b;
        double rateA;
        double rateB;
        double rateC;
        double ratio;
        double angle;
        int status;
        double angmin;
        double angmax;
    } *branch;

    struct TrafoInfo
    {
        int bus_nr;           // nr. of the bus
        int num_hh;           // number of households served by this transformer
        class Household **hh_list; // list of households attached to this transformer
        int fraction_reduce;  // the fraction of households which receives a 'reduce' signal
        int fraction_raise;   // the fraction of households which receives a 'raise' signal
        int num_hh_reduced;   // number of households which have received a 'reduce' signal
        int num_hh_raised;    // number of households which have received a 'raise' signal
        int min_bus;          // nr. of bus with min. magnitude
        int max_bus;          // nr. of bus with max. magnitude
        double min_magnitude; // min. voltage magnitude of all household buses of a transformer
        double max_magnitude; // max. voltage magnitude of all household buses of a transformer
        double power_out;     // power transmitted by transformer
        FILE *file;
    } *trafo_info;

    struct BusInfo
    {
        int num_hh;        // numtar cvf res    ber of households sharing this bus
        class Household **hh_list; // list of households sharing this bus
        int num_neigh;     // number of neighbour buses
        int *neigh_list;   // list of neighbour buses
        int trafo_bus;     // bus nr. of the transformer which this bus is connected to
        TrafoInfo *trafo;  // the transformer info
        double magnitude;  // voltage magnitude at this bus
        double power_in;   // input power at this bus
        FILE *file;
    } *bus_info;

    void prepare_input_file();
    void create_case_file (const char file_name[], int num_households);
    void create_extension_file (const char file_name[], int num_households);
    void connect (int bus_nr, int trafo_nr);
    double sum_power_in_range (Household* list[], int list_length);
    double sum_production_in_range (Household* list[], int list_length);
    double max_power_in_range (Household* list[], int list_length);
    double Pd (Household* list[], int list_length);
    double Qd (Household* list[], int list_length);

public:
    Powerflow (int num_households);
    ~Powerflow();
    void simulate();
};

#endif
