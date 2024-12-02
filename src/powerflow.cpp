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
#include <stdio.h>
#include <string.h>
#include <float.h>

#include "globals.H"
#include "proto.H"
#include "household.H"
#include "solarmodule.H"
#include "battery.H"
#include "powerflow.H"

#define DELTA 10  // number of files to be saved before and after a signal point, e.g. a point at which the
                  // households receive a 'reduce consumption' or a 'raise consumption' signal

#define BR_MAX_LENGTH 50  // maximum number of households served by one transformator
                          // used only in case the user didn't provide any case data

static int compare_households_1 (const void *h1, const void *h2);
static int compare_households_2 (const void *h1, const void *h2);


Powerflow::Powerflow (int num_households)
{
    const char function_name[] = "Powerflow::Powerflow";
    index = 1;
    num_buses = 0;
    num_generators = 0;
    num_branches = 0;
    num_transformers = 0;
    num_files = 0;
    baseMVA = 100.;
    bus = NULL;
    generator = NULL;
    branch = NULL;
    bus_info = NULL;
    trafo_info = NULL;
    signal_points = NULL;
    hh_to_bus = NULL;

    // Delete directories from the previous calculation...
    shell_command ("rm -rf pfin pfout");
    // ...and create new ones
    if (config->powerflow.output_level > 1) shell_command ("mkdir pfin pfout");

    // Create options file for pf/power

    FILE *options_file = NULL;
#ifdef HAVE_PF
    open_file (&options_file, "pfoptions", "w");
#else
    open_file (&options_file, "poweroptions", "w");
#endif
    fprintf (options_file, "-snes_type newtonls\n");
    fprintf (options_file, "-snes_atol 1e-8\n");
    fprintf (options_file, "-snes_rtol 1e-20\n");
    fprintf (options_file, "-snes_linesearch_type basic\n");
    fprintf (options_file, "-ksp_type gmres\n");
    fprintf (options_file, "-pc_type bjacobi\n");
    fprintf (options_file, "-sub_pc_type lu\n");
    fprintf (options_file, "-sub_pc_factor_mat_ordering_type qmd\n");
    fprintf (options_file, "-sub_pc_factor_shift_type NONZERO\n");
    fclose (options_file);

    // Read the MATPOWER case file

    // If the user has not provided a case file name in resLoadSIM.json we
    // create our own case file on the fly, after informing the user about it.

    FILE *case_file = NULL;
    const char selfmade_case_file_name[] = "casedata.m";
    char *line = NULL;
    int line_nr = 1;
    int first_bus_line = 0;
    int first_generator_line = 0;
    int first_branch_line = 0;
    bool case_file_is_selfmade = false;

    case_file = fopen (config->powerflow.case_file_name, "r");
    if (!case_file)
    {
        fprintf (stderr, "\nWARNING: No case file name provided or case file not found.\n");
        fprintf (stderr, "         resLoadSIM continues by creating its own case file (%s) ...\n\n", selfmade_case_file_name);
        create_case_file (selfmade_case_file_name, num_households);
        case_file_is_selfmade = true;
        case_file = fopen (selfmade_case_file_name, "r");
    }
    while (read_line (case_file, &line) > 0)
    {
        if (strstr (line, "mpc.bus")) first_bus_line = line_nr + 1;
        if (strstr (line, "mpc.gen") && !strstr (line, "mpc.gencost")) first_generator_line = line_nr + 1;
        if (strstr (line, "mpc.branch")) first_branch_line = line_nr + 1;
        if (strstr (line, "];"))
        {
            if (first_bus_line && num_buses == 0) num_buses = line_nr - first_bus_line;
            if (first_generator_line && num_generators == 0) num_generators = line_nr - first_generator_line;
            if (first_branch_line && num_branches == 0) num_branches = line_nr - first_branch_line;
        }
        line_nr++;
    }
    alloc_memory (&bus, num_buses, function_name);
    alloc_memory (&generator, num_generators, function_name);
    alloc_memory (&branch, num_branches, function_name);
    rewind (case_file);
    line_nr = 1;
    int i=0, j=0, k=0;
    while (read_line (case_file, &line) > 0)
    {
        if (strstr (line, "mpc.baseMVA"))
        {
            sscanf (line, "mpc.baseMVA = %lf;", &baseMVA);
        }
        if (line_nr >= first_bus_line && line_nr < first_bus_line + num_buses)
        {
            sscanf (line, "%d %d %lf %lf %lf %lf %d %lf %lf %lf %d %lf %lf",
                    &bus[i].nr, &bus[i].type, &bus[i].Pd, &bus[i].Qd, &bus[i].Gs, &bus[i].Bs, &bus[i].area,
                    &bus[i].Vm, &bus[i].Va, &bus[i].baseKV, &bus[i].zone, &bus[i].Vmax, &bus[i].Vmin);
            i++;
        }
        if (line_nr >= first_generator_line && line_nr < first_generator_line + num_generators)
        {
            sscanf (line, "%d %lf %lf %lf %lf %lf %lf %d %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
                    &generator[j].bus, &generator[j].Pg, &generator[j].Qg, &generator[j].Qmax, &generator[j].Qmin,
                    &generator[j].Vg, &generator[j].mBase, &generator[j].status,
                    &generator[j].Pmax, &generator[j].Pmin, &generator[j].Pc1, &generator[j].Pc2,
                    &generator[j].Qc1min, &generator[j].Qc1max, &generator[j].Qc2min, &generator[j].Qc2max,
                    &generator[j].ramp_agc, &generator[j].ramp_10, &generator[j].ramp_30, &generator[j].ramp_q, &generator[j].apf);
            j++;
        }
        if (line_nr >= first_branch_line && line_nr < first_branch_line + num_branches)
        {
            sscanf (line, "%d %d %lf %lf %lf %lf %lf %lf %lf %lf %d %lf %lf",
                    &branch[k].from, &branch[k].to, &branch[k].r, &branch[k].x, &branch[k].b,
                    &branch[k].rateA, &branch[k].rateB, &branch[k].rateC, &branch[k].ratio,
                    &branch[k].angle, &branch[k].status, &branch[k].angmin, &branch[k].angmax);
            k++;
        }
        line_nr++;
    }
    fclose (case_file);

    // Read the file, which contains the relation between buses and households.
    // It is named like the case file with '.ext' as suffix.

    FILE *fp = NULL;
    char file_name[k_name_length+4];
    char *token, *tok_str;
    int bus_nr, hh_nr;  // bus number and household number, both starting with 1
    int num_hh;         // the number of households attached to a bus
    int solar_flag;     // 0 = no solar module, 1 = solar module, 2 = probability for solar module in households.json
    int battery_flag;   // 0 = no battery, 1 = battery, 2 = probability for battery in households.json
    double percent;
    class Household *hh_ptr;

    // If resLoadSIM has created its own case file, it has to improvise again at this point.
    if (case_file_is_selfmade)
    {
        snprintf (file_name, sizeof (file_name), "%s.ext", selfmade_case_file_name);
        create_extension_file (file_name, num_households);
    }
    else
    {
        snprintf (file_name, sizeof (file_name), "%s.ext", config->powerflow.case_file_name);
    }
    open_file (&fp, file_name, "r");
    alloc_memory (&bus_info, num_buses, function_name);
    for (i=0; i<num_buses; i++)
    {
        bus_info[i].num_hh = 0;
        bus_info[i].hh_list = NULL;
        bus_info[i].num_neigh = 0;
        bus_info[i].neigh_list = NULL;
        bus_info[i].trafo_bus = 0;
        bus_info[i].trafo = NULL;
        bus_info[i].magnitude = 0.;
        bus_info[i].power_in = 0.;
        bus_info[i].file = NULL;
    }
    int num_households_in_file = 0;  // The number of households according to the powerflow case data.
                                     // This must match the number of households given as an
                                     // argument to resLoadSIM.

    while (read_line (fp, &line) > 0)   // read the .ext file, which has the following columns:
                                        // 1. bus number
                                        // 2. number of households (num_hh) represented by this bus
                                        //    num_hh < 0  :  no households attached, no transformer
                                        //    num_hh == 0 :  it's a transformer, no further info stored
                                        //    num_hh > 0  :  a bus with num_hh households attached
                                        // 3. num_hh < 0  :  output flag (T/F)
                                        //    num_hh > 0  :  household number, solar flag (0/1/2) and
                                        //                   battery flag (0/1/2) for each household at this bus
                                        // 4. num_hh > 0  :  output flag (T/F)
    {
        tok_str = line;
        token = strtok (tok_str, " ");
        if (token && token[0] != '\0' && token[0] != '\n')
        {
            bus_nr = atoi (token);
            token = strtok (NULL, " ");
            num_hh = atoi (token);
            bus_info[bus_nr-1].num_hh = num_hh;

            if (num_hh < 0)     // it's a bus without any households connected
            {
                token = strtok (NULL, " ");
                if (token[1] == '\n') token[1] = '\0';
                if (strlen(token) == 1 && (token[0]=='t' || token[0]=='T'))
                {
                    char filename[k_max_path];
                    snprintf (filename, sizeof(filename), "bus.%d.%d", sim_clock->year, bus_nr);
                    open_file (&bus_info[bus_nr-1].file, filename, "w");
                }
                else if (strlen(token) == 1 && (token[0]=='f' || token[0]=='F')) bus_info[bus_nr-1].file = NULL;
                else
                {
                    fprintf (stderr, "Syntax error in file '%s': expected to read output flag ('f', 'F', 't' or 'T') at bus %d.\n", file_name, bus_nr);
                    exit(1);
                }
            }
            else if (num_hh == 0)  // it's a transformer
            {
                num_transformers++;
                bus_info[bus_nr-1].trafo_bus = bus_nr;
            }
            else // it's a household bus
            {
                num_households_in_file += num_hh;
                if (num_households_in_file > num_households)
                {
                    fprintf (stderr, "\nERROR: The number of households according to the power flow case data (%s)\n", file_name);
                    fprintf (stderr, "         exceeds the number of households provided as an argument (= %d).\n\n", num_households);
                    exit(1);
                }
                alloc_memory (&bus_info[bus_nr-1].hh_list, num_hh, function_name);
                for (i=0; i<num_hh; i++)
                {
                    token = strtok (NULL, " ");
                    hh_nr = atoi (token);
                    hh_ptr = Household::get_household_ptr (hh_nr);
                    bus_info[bus_nr-1].hh_list[i] = hh_ptr;
                    token = strtok (NULL, " ");
                    solar_flag = atoi (token);
                    if (solar_flag == 1) hh_ptr->add_solar_module();
                    else if (solar_flag == 2)
                    {
                        percent = config->household.prevalence.solar_module[hh_ptr->residents-1];
                        if (percent > 0 && get_random_number (0., 100.) <= percent)
                        {
                            hh_ptr->add_solar_module ();
                        }
                    }
                    token = strtok (NULL, " ");
                    battery_flag = atoi (token);
                    if (battery_flag == 1) hh_ptr->add_battery();
                    else if (battery_flag == 2)
                    {
                        if (hh_ptr->solar_module) percent = config->battery.frequency_solar;
                        else                      percent = config->battery.frequency_non_solar;
                        if (percent > 0 && get_random_number (0., 100.) <= percent)
                        {
                            hh_ptr->add_battery();
                        }
                    }
                }
                token = strtok (NULL, " ");
                if (token[1] == '\n') token[1] = '\0';
                if (strlen(token) == 1 && (token[0]=='t' || token[0]=='T'))
                {
                    char filename[k_max_path];
                    snprintf (filename, sizeof(filename), "bus.%d.%d", sim_clock->year, bus_nr);
                    open_file (&bus_info[bus_nr-1].file, filename, "w");
                }
                else if (strlen(token) == 1 && (token[0]=='f' || token[0]=='F')) bus_info[bus_nr-1].file = NULL;
                else
                {
                    fprintf (stderr, "Syntax error in file '%s': expected to read output flag ('f', 'F', 't' or 'T') at bus %d.\n", file_name, bus_nr);
                    exit(1);
                }
            }
        }
    }
    fclose (fp);

    if (num_households_in_file != num_households)
    {
        fprintf (stderr, "\nERROR: The number of households provided as an argument (= %d)\n", num_households);
        fprintf (stderr, "         does not match the number of households according to the power flow case data (= %d).\n\n", num_households_in_file);
        exit(1);
    }

    // Setup the inverse relation
    alloc_memory (&hh_to_bus, num_households, function_name);
    for (i=0; i<num_buses; i++)
    {
        for (j=0; j<bus_info[i].num_hh; j++)
        {
            hh_to_bus[bus_info[i].hh_list[j]->number-1] = i+1;
        }
    }

    // Find out how the buses are connected. Each bus stores its neighbours
    // in 'neigh_list'

    for (i=0; i<num_branches; i++)
    {
        bus_info[branch[i].from-1].num_neigh++;
        bus_info[branch[i].to-1].num_neigh++;
    }
    for (i=0; i<num_buses; i++)
    {
        if (bus_info[i].num_neigh > 0) alloc_memory (&bus_info[i].neigh_list, bus_info[i].num_neigh, function_name);
        bus_info[i].num_neigh = 0;
    }
    int fbus, tbus;
    for (i=0; i<num_branches; i++)
    {
        fbus = branch[i].from-1;
        tbus = branch[i].to-1;
        bus_info[fbus].neigh_list[bus_info[fbus].num_neigh++] = tbus+1;
        bus_info[tbus].neigh_list[bus_info[tbus].num_neigh++] = fbus+1;
    }

    // Now we can find out which bus is connected to which transformer.
    // First initialize the trafo info.

    alloc_memory (&trafo_info, num_transformers, function_name);
    for (int t=0; t<num_transformers; t++)
    {
        trafo_info[t].bus_nr = 0;
        trafo_info[t].num_hh = 0;
        trafo_info[t].hh_list = NULL;
        trafo_info[t].fraction_reduce = 0;
        trafo_info[t].fraction_raise = 0;
        trafo_info[t].num_hh_reduced = 0;
        trafo_info[t].num_hh_raised = 0;
        trafo_info[t].file = NULL;
    }
    int t_index = 0;
    for (i=0; i<num_buses; i++)
    {
        if (bus_info[i].trafo_bus > 0) // found a trafo
        {
            trafo_info[t_index++].bus_nr = i+1;
            bus_info[i].trafo_bus = 0;  // reset to 0 to make the function 'connect' work properly
        }
    }

    for (int t=0; t<num_transformers; t++)
    {
        connect (trafo_info[t].bus_nr-1, trafo_info[t].bus_nr);
    }


    // For each trafo:  compile the list of households, which are served by this trafo

    for (int t=0; t<num_transformers; t++)
    {
        for (i=0; i<num_buses; i++)
        {
            if (bus_info[i].trafo_bus == trafo_info[t].bus_nr && bus_info[i].num_hh > 0)
            {
                bus_info[i].trafo = trafo_info+t;
                trafo_info[t].num_hh += bus_info[i].num_hh;
            }
        }
    }

    for (int t=0; t<num_transformers; t++)
    {
        if (trafo_info[t].num_hh > 0) alloc_memory (&trafo_info[t].hh_list, trafo_info[t].num_hh, function_name);
        int idx = 0;
        for (i=0; i<num_buses; i++)
        {
            if (bus_info[i].trafo == trafo_info+t)
            // If bus i is attached to transformer t, add all households of bus i to
            // transformer t's household list
            {
                for (j=0; j<bus_info[i].num_hh; j++)
                {
                    trafo_info[t].hh_list[idx++] = bus_info[i].hh_list[j];
                }
            }
        }
    }

    // Just to be save: check whether all households got a connection to a transformer

    for (i=0; i<num_buses; i++)
    {
        if (bus_info[i].num_hh > 0)  // it's a household bus
        {
            if (!bus_info[i].trafo)
            {
                printf ("\nERROR: Household(s) at bus %d not connected to a transformer.\n", i+1);
                exit(1);
            }
        }
    }

    // Prepare transformer related output files

    if (config->powerflow.output_level > 0)
    {
        char filename[k_max_path];
        for (int t=0; t<num_transformers; t++)
        {
            snprintf (filename, sizeof(filename), "trafo.%d.%d", sim_clock->year, trafo_info[t].bus_nr);
            open_file (&trafo_info[t].file, filename, "w");
        }
    }

    if (config->powerflow.output_level == 2)
    {
        num_files = (floor(sim_clock->end_time/config->timestep_size)+1)/config->powerflow.step_size;
        alloc_memory (&signal_points, num_files, function_name);
        for (i=0; i<num_files; i++) signal_points[i] = 0;
    }
}


Powerflow::~Powerflow()
{
    delete [] bus;
    delete [] generator;
    delete [] branch;
    delete [] hh_to_bus;
    for (int i=0; i<num_buses; i++)
    {
        if (bus_info[i].num_hh)
        {
            delete [] bus_info[i].hh_list;
        }
        if (bus_info[i].num_neigh) delete [] bus_info[i].neigh_list;
        if (bus_info[i].file) fclose (bus_info[i].file);
    }
    delete [] bus_info;

    if (config->powerflow.output_level > 0)
    {
        for (int t=0; t<num_transformers; t++) fclose (trafo_info[t].file);
    }

    for (int t=0; t<num_transformers; t++)
    {
        if (trafo_info[t].num_hh) delete [] trafo_info[t].hh_list;
    }
    delete [] trafo_info;

    if (config->powerflow.output_level == 2)  // Delete unwanted pfin/pfout files
    {
        int sum_left = 0, sum_right = 0;
        int length = num_files < DELTA+1 ? num_files : DELTA+1;
        char command[64];
        for (int i=0; i<length; i++) sum_right += signal_points[i];
        for (int i=0; i<num_files; i++)
        {
            if (sum_left == 0 && sum_right == 0)
            {
                snprintf (command, sizeof(command), "rm -rf pfin/pfin_%d", i+1);
                shell_command (command);
                snprintf (command, sizeof(command), "rm -rf pfout/pfout_%d", i+1);
                shell_command (command);
            }
            if (i>=DELTA) sum_left -= signal_points[i-DELTA];
            sum_left += signal_points[i];
            sum_right -= signal_points[i];
            if (i+DELTA+1<num_files) sum_right += signal_points[i+DELTA+1];
        }
        delete [] signal_points;
    }
}


void Powerflow::simulate()
{
    double time = sim_clock->cur_time;
    char command[64], *line = NULL;
    int from, to;               // bus numbers at both ends of a branch
    double pwr_from, pwr_to;    // power at both ends of a branch
    int t, count;

    // Prepare the input file (pf_input) for the power flow solver pf/power...
    prepare_input_file();
    // ...and start pf/power
#ifdef HAVE_PF
    snprintf (command, sizeof(command), "pf -pfdata pf_input");
#else
    snprintf (command, sizeof(command), "power -pfdata pf_input");
#endif
    shell_command (command);

    // Read the result file
    FILE *fp = NULL;
    open_file (&fp, "results", "r");

    // Bus data
    read_line (fp, &line);  // read the first 2 lines
    read_line (fp, &line);  // which don't contain any info we need
    for (int i=0; i<num_buses; i++)
    {
        read_line (fp, &line);
        if (bus_info[i].num_hh)  // it is a household bus
        {
            sscanf (line, "%*d %*s %lf %*s %*s %*s", &bus_info[i].magnitude);
        }
    }
    // Line data
    if (config->powerflow.output_level > 0)
    {
        for (int b=0; b<num_buses; b++) bus_info[b].power_in = 0.;
        for (int t=0; t<num_transformers; t++) trafo_info[t].power_out = 0.;
        // Read the 'Line data' line and the header line
        read_line (fp, &line);
        read_line (fp, &line);
        for (int i=0; i<num_branches; i++)
        {
            read_line (fp, &line);
            sscanf (line, "%d %d %lf %lf %*f %*d", &from, &to, &pwr_from, &pwr_to);
            // We are looking at branches with a transformer at the 'from' end
            // in order to get the output power
            if (bus_info[from-1].trafo_bus == from)
            {
               for (int t=0; t<num_transformers; t++)
               {
                   if (trafo_info[t].bus_nr == from)
                   {
                       trafo_info[t].power_out += pwr_from*1000.; // pwr is in MW, power values in resLoadSIM are in KW
                   }
               }
            }
            // For buses with an output file attached we need to calculate the input power
            if (bus_info[from-1].file)
            {
                if (pwr_from < pwr_to) bus_info[from-1].power_in += pwr_from*1000.;
            }
            if (bus_info[to-1].file)
            {
                if (pwr_to < pwr_from) bus_info[to-1].power_in += pwr_to*1000.;
            }
        }
    }
    fclose (fp); // Close the results file

    // Check all buses for min and max voltage and react if necessary
    // The buses are arranged in groups, with each group beeing served by one
    // transformer.

    for (t=0; t<num_transformers; t++)
    {
        trafo_info[t].min_magnitude = DBL_MAX;
        trafo_info[t].max_magnitude = DBL_MIN;
        trafo_info[t].min_bus = -1;
        trafo_info[t].max_bus = -1;
    }
    for (int i=0; i<num_buses; i++)
    {
        if (bus_info[i].num_hh > 0)  // a bus with household(s) attached
        {
            if (bus_info[i].magnitude < bus_info[i].trafo->min_magnitude)
            {
                bus_info[i].trafo->min_magnitude = bus_info[i].magnitude;
                bus_info[i].trafo->min_bus = i+1;
            }
            if (bus_info[i].magnitude > bus_info[i].trafo->max_magnitude)
            {
                bus_info[i].trafo->max_magnitude = bus_info[i].magnitude;
                bus_info[i].trafo->max_bus = i+1;
            }
        }
    }
    for (t=0; t<num_transformers; t++)
    {
        // If the voltage in a group of households drops below a given threshold and there are still
        // households left, which are not already in energy conservation mode, then send
        // a reduce consumption signal to those households

        if (trafo_info[t].min_magnitude <= config->powerflow.uv_lower_limit && trafo_info[t].fraction_reduce < 100)
        {
            trafo_info[t].fraction_reduce += 10;     // increase by 10%
            if (config->powerflow.uv_control)        // undervoltage control is enabled
            {
                count = trafo_info[t].num_hh * (double)trafo_info[t].fraction_reduce/100.;
                qsort (trafo_info[t].hh_list, (size_t)trafo_info[t].num_hh, sizeof(class Household*), &compare_households_1);
                int to_be_reduced = count - trafo_info[t].num_hh_reduced;
                int h = 0;
                while (to_be_reduced)
                {
                    if (!trafo_info[t].hh_list[h]->reduce_consumption && !trafo_info[t].hh_list[h]->raise_consumption)
                    {
                        trafo_info[t].hh_list[h]->reduce_consumption = true;
                        trafo_info[t].hh_list[h]->rc_timestamp = time;
                        to_be_reduced--;
                    }
                    h++;
                }
                trafo_info[t].num_hh_reduced = count;
            }
            if (config->powerflow.output_level == 2) signal_points[index-1] = 1;
        }
        // If the voltage level has recovered above the upper threshold and there are still
        // households in energy conservation mode, then reduce the number of households in that mode
        else if (trafo_info[t].min_magnitude >= config->powerflow.uv_upper_limit && trafo_info[t].fraction_reduce > 0)
        {
            trafo_info[t].fraction_reduce -= 10;     // decrease by 10%
            if (config->powerflow.uv_control)        // undervoltage control is enabled
            {
                count = trafo_info[t].num_hh * (double)trafo_info[t].fraction_reduce/100.;
                qsort (trafo_info[t].hh_list, (size_t)trafo_info[t].num_hh, sizeof(class Household*), &compare_households_2);
                double smallest_timestamp = trafo_info[t].hh_list[0]->rc_timestamp;
                int h = 0;
                while (trafo_info[t].hh_list[h]->rc_timestamp == smallest_timestamp)
                {
                    trafo_info[t].hh_list[h]->reduce_consumption = false;
                    trafo_info[t].hh_list[h]->rc_timestamp = DBL_MAX;
                    h++;
                }
                trafo_info[t].num_hh_reduced = count;
            }
            if (config->powerflow.output_level == 2) signal_points[index-1] = 1;
        }
        // If the voltage in a group of households exceeds a given threshold,
        // try to raise the consumption
        else if (trafo_info[t].max_magnitude >= config->powerflow.ov_upper_limit && trafo_info[t].fraction_raise < 100)
        {
            trafo_info[t].fraction_raise += 10;      // increase by 10%
            if (config->powerflow.ov_control)        // overvoltage control is enabled
            {
                count = trafo_info[t].num_hh * (double)trafo_info[t].fraction_raise/100.;
                qsort (trafo_info[t].hh_list, (size_t)trafo_info[t].num_hh, sizeof(class Household*), &compare_households_1);
                int to_be_raised = count - trafo_info[t].num_hh_raised;
                int h = trafo_info[t].num_hh-1;
                while (to_be_raised)
                {
                    if (!trafo_info[t].hh_list[h]->reduce_consumption && !trafo_info[t].hh_list[h]->raise_consumption)
                    {
                        trafo_info[t].hh_list[h]->raise_consumption = true;
                        trafo_info[t].hh_list[h]->rc_timestamp = time;
                        to_be_raised--;
                    }
                    h--;
                }
                trafo_info[t].num_hh_raised = count;
            }
            if (config->powerflow.output_level == 2) signal_points[index-1] = 1;
        }
        // Voltage is below the lower threshold, so we can send a signal to the households to
        // turn off the additional consumption
        else if (trafo_info[t].max_magnitude <= config->powerflow.ov_lower_limit && trafo_info[t].fraction_raise > 0)
        {
            trafo_info[t].fraction_raise -= 10;      // decrease by 10%
            if (config->powerflow.ov_control)        // overvoltage control is enabled
            {
                count = trafo_info[t].num_hh * (double)trafo_info[t].fraction_raise/100.;
                qsort (trafo_info[t].hh_list, (size_t)trafo_info[t].num_hh, sizeof(class Household*), &compare_households_2);
                double smallest_timestamp = trafo_info[t].hh_list[0]->rc_timestamp;
                int h = 0;
                while (trafo_info[t].hh_list[h]->rc_timestamp == smallest_timestamp)
                {
                    trafo_info[t].hh_list[h]->raise_consumption = false;
                    trafo_info[t].hh_list[h]->rc_timestamp = DBL_MAX;
                    h++;
                }
                trafo_info[t].num_hh_raised = count;
            }
            if (config->powerflow.output_level == 2) signal_points[index-1] = 1;
        }

        // Print results to transformer files
        if (config->powerflow.output_level > 0)
        {
            double max_power = 0.;
            // The following values are stored in the trafo output file:
            // - time
            // - number of the bus with minimum voltage
            // - minimum voltage magnitude
            // - fraction of households in energy conservation mode
            // - number of the bus with maximum voltage
            // - maximum voltage magnitude
            // - fraction of households in energy burst mode
            // - power transmitted by transformer
            // - maximum power (TBI)
            // - total power drawn by all households attached to this transformer
            // - total solar production of all households attached to this transformer
            // - maximum consumption of a household
            // - the bus to which the max. consumption household is attached to
            // - the id of the household with max. consumption
            fprintf (trafo_info[t].file, "%lf %d %lf %d %d %lf %d %lf %lf %lf %lf %lf %d %d\n",
                     time/3600.,
                     trafo_info[t].min_bus, trafo_info[t].min_magnitude, trafo_info[t].fraction_reduce,
                     trafo_info[t].max_bus, trafo_info[t].max_magnitude, trafo_info[t].fraction_raise,
                     trafo_info[t].power_out, max_power*1000.,
                     sum_power_in_range (trafo_info[t].hh_list, trafo_info[t].num_hh),
                     sum_production_in_range (trafo_info[t].hh_list, trafo_info[t].num_hh),
                     max_power_in_range (trafo_info[t].hh_list, trafo_info[t].num_hh),
                     hh_to_bus[trafo_info[t].hh_list[0]->number-1],
                     trafo_info[t].hh_list[0]->number);
        }
    }

    // Print results to bus files
    if (config->powerflow.output_level > 0)
    {
        for (int b=0; b<num_buses; b++)
        {
            if (bus_info[b].file)
            {
                // The following values are stored in the bus output file:
                // - time
                // - voltage magnitude
                // - input power at bus
                // - consumption at bus
                // - production at bus (solar modules)
                fprintf (bus_info[b].file, "%lf %lf %lf %lf %lf\n",
                         time/3600.,
                         bus_info[b].magnitude,
                         bus_info[b].power_in,
                         sum_power_in_range (bus_info[b].hh_list, bus_info[b].num_hh),
                         sum_production_in_range (bus_info[b].hh_list, bus_info[b].num_hh));
            }
        }
    }

    // Store the case data and the results in 'pfin' and 'pfout'
    if (config->powerflow.output_level > 1)
    {
        snprintf (command, sizeof(command), "mv pf_input pfin/pfin_%d", index);
        shell_command (command);
        snprintf (command, sizeof(command), "mv results pfout/pfout_%d", index);
        shell_command (command);
    }
    index++;
}


void Powerflow::prepare_input_file()
{
    FILE *fp = NULL;
    open_file (&fp, "pf_input", "w");
    fprintf (fp, "function mpc = pf_input\n\n");
    fprintf (fp, "mpc.baseMVA = %lf;\n\n", baseMVA);

    // BUS data
    // id type Pd Qd Gs Bs area Vm Va baseKV zone Vmax Vmin
    fprintf (fp, "%%%% BUS data\n%%  bus_i     type           Pd           Qd           Gs           Bs  area       Vm       Va     baseKV  zone     Vmax     Vmin\n");
    fprintf (fp, "mpc.bus = [\n");
    for (int i=0; i<num_buses; i++)
    {
        if (bus_info[i].num_hh > 0)
        {
            bus[i].Pd = Pd (bus_info[i].hh_list, bus_info[i].num_hh);
            bus[i].Qd = Qd (bus_info[i].hh_list, bus_info[i].num_hh);
        }
        fprintf (fp, "%8d %8d     %.2E     %.2E     %.2E     %.2E %5d %8.2lf %8.2lf %10.2lf %5d %8.2lf %8.2lf;\n",
                 bus[i].nr, bus[i].type, bus[i].Pd, bus[i].Qd, bus[i].Gs, bus[i].Bs, bus[i].area,
                 bus[i].Vm, bus[i].Va, bus[i].baseKV, bus[i].zone, bus[i].Vmax, bus[i].Vmin);
    }
    fprintf (fp, "];\n\n");

    // GENERATOR data
    // bus, Pg, Qg, Qmax, Qmin, Vg, mBase, status, Pmax, Pmin, Pc1, Pc2,
    // Qc1min, Qc1max, Qc2min, Qc2max, ramp_agc, ramp_10, ramp_30, ramp_q, apf
    fprintf (fp, "%%%% GENERATOR data\n");
    fprintf (fp, "mpc.gen = [\n");
    for (int j=0; j<num_generators; j++)
    {
        fprintf (fp, "%8d %12.2lf %12.2lf %12.2lf %12.2lf %12.2lf %12.2lf %8d %12.2lf %12.2lf %12.2lf %12.2lf %12.2lf %12.2lf %12.2lf %12.2lf %12.2lf %12.2lf %12.2lf %12.2lf %12.2lf;\n",
                 generator[j].bus, generator[j].Pg, generator[j].Qg, generator[j].Qmax, generator[j].Qmin,
                 generator[j].Vg, generator[j].mBase, generator[j].status,
                 generator[j].Pmax, generator[j].Pmin, generator[j].Pc1, generator[j].Pc2,
                 generator[j].Qc1min, generator[j].Qc1max, generator[j].Qc2min, generator[j].Qc2max,
                 generator[j].ramp_agc, generator[j].ramp_10, generator[j].ramp_30, generator[j].ramp_q, generator[j].apf);
    }
    fprintf (fp, "];\n\n");

    // BRANCH data
    // from_bus, to_bus, r, x, b, rateA, rateB, rateC, ratio, angle, status, angmin, angmax
    fprintf (fp, "%%%% BRANCH data\n%%   fbus     tbus            r            x            b    rateA    rateB    rateC    ratio    angle   status   angmin   angmax\n");
    fprintf (fp, "mpc.branch = [\n");
    for (int k=0; k<num_branches; k++)
    {
        fprintf (fp, "%8d %8d     %.2E     %.2E     %.2E %8.2lf %8.2lf %8.2lf %8.2lf %8.2lf %8d %8.2lf %8.2lf;\n",
                 branch[k].from, branch[k].to, branch[k].r, branch[k].x, branch[k].b,
                 branch[k].rateA, branch[k].rateB, branch[k].rateC, branch[k].ratio,
                 branch[k].angle, branch[k].status, branch[k].angmin, branch[k].angmax);
    }
    fprintf (fp, "];\n\n");

    fclose (fp);
}


void Powerflow::connect (int i, int trafo_bus_nr)
{
    int index;
    if (bus_info[i].trafo_bus > 0 ) return;      // this node has already been connected to a trafo
    bus_info[i].trafo_bus = trafo_bus_nr;        // node i connects to trafo at bus 'trafo_bus_nr'
    for (int n=0; n<bus_info[i].num_neigh; n++)  // connect all neighbours from node i too (unless it's a generator)
    {
        index = bus_info[i].neigh_list[n]-1;
        if (bus[index].type != 3) connect (index, trafo_bus_nr);
    }
}


void Powerflow::create_case_file (const char file_name[], int num_households)
{
    FILE *fp = NULL;
    int id, from, to;
    const double r = 2.0;    // resistance (p.u.)
    const double x = 0.33;   // reactance (p.u.)
    const double b = 0.0;

    int num_trafos = (num_households-1)/BR_MAX_LENGTH+1;

    open_file (&fp, file_name, "w");
    fprintf (fp, "function mpc = pf_input\n\n");
    fprintf (fp, "mpc.baseMVA = %lf;\n\n", baseMVA);

    // BUS data
    // id type Pd Qd Gs Bs area Vm Va baseKV zone Vmax Vmin
    fprintf (fp, "%%%% BUS data\n%%  bus_i     type           Pd           Qd           Gs           Bs  area       Vm       Va     baseKV  zone     Vmax     Vmin\n");
    fprintf (fp, "mpc.bus = [\n");
    // Bus 1 is a slack bus (bus_type = 3)
    fprintf (fp, "       1        3            0            0            0            0     1        1        0         11     1      1.1      0.9;\n");
    // The following num_households/br_max_length+1 buses are the start points of the transfomer branches
    for (id=2; id<num_trafos+2; id++)
    {
        fprintf (fp, "%8d        1            0            0            0            0     1        1        0         11     1      1.1      0.9;\n", id);
    }
    // The rest are load buses
    for (int i=id; i<num_households+id; i++)
    fprintf (fp, "%8d        1            0            0            0            0     1        1        0      0.400     1      1.1      0.9;\n", i);
    fprintf (fp, "];\n\n");

    // GENERATOR data
    // bus, Pg, Qg, Qmax, Qmin, Vg, mBase, status, Pmax, Pmin, Pc1, Pc2,
    // Qc1min, Qc1max, Qc2min, Qc2max, ramp_agc, ramp_10, ramp_30, ramp_q, apf
    // There is only 1 generator, which is the substation at bus 1
    fprintf (fp, "mpc.gen = [\n");
    fprintf (fp, "       1            0            0          300         -300            1          100        1          300           10");
    for (int j=0; j<11; j++) fprintf (fp, "            0");
    fprintf (fp, ";\n];\n\n");

    // BRANCH data
    // from_bus, to_bus, r, x, b, rateA, rateB, rateC, ratio, angle, status, angmin, angmax
    fprintf (fp, "%%%% BRANCH data\n%%   fbus     tbus            r            x            b    rateA    rateB    rateC    ratio    angle   status   angmin   angmax\n");
    fprintf (fp, "mpc.branch = [\n");
    // Start with the branches that are the connections between the substation (bus 1) and
    // the distribution transformers
    for (to=2; to<num_trafos+2; to++)
    {
        fprintf (fp, "       1 %8d          0.1            0            0      250      250      250        0        0        1     -360      360;\n", to);
    }
    // Now the branches which represent the distribution transformers
    to = 2+num_trafos;
    for (from=2; from<num_trafos+2; from++)
    {
        fprintf (fp, "%8d %8d          0.1            0            0      250      250      250        1        0        1     -360      360;\n", from, to);
        to += BR_MAX_LENGTH;
    }
    // The rest of the branches are connections between the consumers
    from = 2+num_trafos;
    while (from < 1+num_trafos+num_households)
    {
        fprintf (fp, "%8d %8d %12.5lf %12.5lf %12.5lf      250      250      250        0        0        1     -360      360;\n", from, from+1, r, x, b);
        if ((from-num_trafos) % BR_MAX_LENGTH == 0) from += 2; else from += 1;
    }
    fprintf (fp, "];\n\n");
    fclose (fp);
}


void Powerflow::create_extension_file (const char file_name[], int num_households)
{
    FILE *fp = NULL;
    int hh_nr = 1;
    int num_trafos = (num_households-1)/BR_MAX_LENGTH+1;

    open_file (&fp, file_name, "w");
    for (int i=2; i<num_trafos+2; i++)
    {
        fprintf (fp, "%d 0\n", i);
    }
    for (int i=num_trafos+2; i<=num_buses; i++)
    {
        fprintf (fp, "%d 1 %d 2 2 F\n", i, hh_nr++);
    }
    fclose (fp);
}

double Powerflow::sum_power_in_range (Household* list[], int list_length)
{
    double sum = 0.;
    for (int i=0; i<list_length; i++)
    {
        sum += list[i]->power.real;
    }
    return sum;
}

double Powerflow::sum_production_in_range (Household* list[], int list_length)
{
    double sum = 0.;
    for (int i=0; i<list_length; i++)
    {
        if (list[i]->solar_module) sum += list[i]->solar_module->power.real;
    }
    return sum;
}

double Powerflow::max_power_in_range (Household* list[], int list_length)
{
    double max = 0.;
    for (int i=0; i<list_length; i++)
    {
        if (list[i]->power.real > max) max = list[i]->power.real;
    }
    return max;
}


double Powerflow::Pd (Household* list[], int list_length)
{
    double solar_real, bat_real;
    double sum = 0.;

    for (int i=0; i<list_length; i++)
    {
        if (list[i]->solar_module)
        {
            solar_real = list[i]->solar_module->power.real;
            // The solar contribution is reduced by the amount of power used for charging batteries
            if (list[i]->battery && list[i]->battery->is_solar_charging)
            {
                solar_real -= list[i]->battery->power_charging;
            }
        }
        else
        {
            solar_real = 0.;
        }
        // The contribution of discharging batteries
        if (list[i]->battery)
        {
            bat_real = list[i]->battery->power_discharging;
        }
        else
        {
            bat_real = 0.;
        }
        // Net amount of power drawn from the grid
        sum += (list[i]->power.real - solar_real - bat_real) * 0.001;
    }
    return sum;
}


double Powerflow::Qd (Household* list[], int list_length)
{
    double sum = 0.;
    double solar_reactive;

    for (int i=0; i<list_length; i++)
    {
        if (list[i]->solar_module)
        {
            solar_reactive = list[i]->solar_module->power.reactive;
        }
        else
        {
            solar_reactive = 0.;
        }
        sum += (list[i]->power.reactive - solar_reactive) * 0.001;
    }
    return sum;
}


static int compare_households_1 (const void *h1, const void *h2)
{
    Household *ptr1 = *(Household**)h1;
    Household *ptr2 = *(Household**)h2;
    double delta = ptr1->power.real - ptr2->power.real;
    if (delta > 0) return -1;
    else if (delta < 0) return 1;
    return 0;
}

static int compare_households_2 (const void *h1, const void *h2)
{
    Household *ptr1 = *(Household**)h1;
    Household *ptr2 = *(Household**)h2;
    if (ptr1->rc_timestamp < ptr2->rc_timestamp) return -1;
    else if (ptr1->rc_timestamp > ptr2->rc_timestamp) return 1;
    else return 0;
}
