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

#include "appliance.H"
#include "household.H"
#include "producer.H"
#include "proto.H"
#include "globals.H"
#include "constants.H"

#define JSMN_HEADER
#include "jsmn.h"

static double* extract_data (char *array, int size, double *sec);


WashingMachine::WashingMachine (Household *hh)
{
    num_energy_classes = config->wmachine.num_energy_classes;
    static double *sec_per_cycle = NULL;
    alloc_memory (&sec_per_cycle, num_energy_classes, "WashingMachine");
    if (config->variable_load && first_app == NULL) // the first washing machine inits the variable load
    {
        FILE *fp = NULL;
        char *buffer = NULL;
        int file_size, index = 0, num_tokens, t, n;
        jsmn_parser parser;
        jsmntok_t *tokens = NULL;
        char energy_class_names_2021[][2] = {"A", "B", "C", "D", "E", "F", "G"};
        char energy_class_names[][8] = {"A+++", "A++", "A+", "A", "B", "C", "D", "E", "F", "G"};
        char file_name[] = "varload.json";

        file_size = open_file (&fp, file_name, "r");
        buffer = (char *) malloc (file_size * sizeof (char) + 1);
        for (int i=0; i<file_size; i++) buffer[index++] = fgetc (fp);
        buffer[index] = '\0';
        fclose (fp);

        // Parsing with 'tokens' set to NULL returns the number of tokens.
        // num_tokens < 0 indicates a problem with the JSON file

        jsmn_init (&parser);
        num_tokens = jsmn_parse (&parser, buffer, strlen(buffer), tokens, 0);
        if (num_tokens < 0)  // num_tokens is interpreted as error code
        {
            switch (num_tokens)
            {
                case JSMN_ERROR_INVAL:
                    fprintf (stderr, "Bad JSON file '%s'. Please check the file's format.\n", file_name);
                    break;
                case JSMN_ERROR_NOMEM:
                    fprintf (stderr, "Not enough tokens for parsing JSON file '%s'.\n", file_name);
                    break;
                case JSMN_ERROR_PART:
                    fprintf (stderr, "JSON file '%s' is too short.\n", file_name);
                    break;
            }
            exit(1);
        }
        tokens = (jsmntok_t *) malloc (num_tokens * sizeof (jsmntok_t));
        jsmn_init (&parser);
        jsmn_parse (&parser, buffer, strlen(buffer), tokens, num_tokens);

        alloc_memory (&variable_load, num_energy_classes, "WashingMachine");
        for (int ec=0; ec<num_energy_classes; ec++) variable_load[ec] = NULL;

        for (t=0; t<num_tokens; t++)
        {
            if (buffer[tokens[t].end] == '"' || buffer[tokens[t].end+1] == '\n') buffer[tokens[t].end] = '\0';
        }
        t = 0;
        while (t<num_tokens && strcmp (buffer+tokens[t].start, "washing_machine")) t++;
        if (t == num_tokens)    // didn't find data for washing machines
        {
            fprintf (stderr, "Error in file '%s': didn't find any data for washing machines.\n", file_name);
            exit (1);
        }
        else
        {
            if (tokens[t+1].size < num_energy_classes)
            {
                fprintf (stderr, "Error in file '%s', washing_machine: there is not enough data for all %d energy classes.\n",
                         file_name, num_energy_classes);
                exit (1);
            }
            // Find for each energy class the corresponding load array
            t += 2;
            for (int ec=0; ec<num_energy_classes; ec++)
            {
                while (!(tokens[t].type == JSMN_STRING && tokens[t].size == 1)) t++;
                // lookup class name
                if (config->energy_classes_2021)
                {
                    for (n=0; n<num_energy_classes; n++)
                        if (!strcmp (buffer+tokens[t].start, energy_class_names_2021[n])) break;
                }
                else
                {
                    for (n=0; n<num_energy_classes; n++)
                        if (!strcmp (buffer+tokens[t].start, energy_class_names[n])) break;
                }
                if (n == num_energy_classes)
                {
                    fprintf (stderr, "Error in file '%s', washing_machine: there is no energy class named '%s'.\n", file_name, buffer+tokens[t].start);
                    exit (1);
                }
                // found data for energy class ec; find out how much memory we need to allocate for it
                if (variable_load[n] != NULL)
                {
                    fprintf (stderr, "Error in file '%s', washing_machine: double definition of data for energy class '%s'.\n", file_name, buffer+tokens[t].start);
                    exit (1);
                }
                variable_load[n] = extract_data (buffer+tokens[t+1].start+2, tokens[t+1].size, sec_per_cycle+n);
                t++;
            }
        }
        free (buffer);
        free (tokens);
    }
    count[hh->residents]++;
    next_app = first_app;
    first_app = this;
    household = hh;
    status = OFF;
    timer = 0;
    smart = smart_mode = false;
    sg_enabled =    config->wmachine.smartgrid_enabled > 0
                 && get_random_number (0., 100.) <= config->wmachine.smartgrid_enabled;
    capacity = config->wmachine.capacity[household->residents-1];   // in kg
    energy_class = random_energy_class (config->wmachine.energy_classes);

    if (config->energy_classes_2021)
    {
        /* The energy class dependent figures below and the formula to
        calculate the power are from the following source:
        COMMISSION DELEGATED REGULATION (EU) 2019/2014 of 11 March 2019
        It can be downloaded here:
        https://eur-lex.europa.eu/legal-content/EN/TXT/?qid=1575536811417&uri=CELEX:32019R2014
        */
        double EEI[] = {44, 52, 60, 69, 80, 91, 102, 113};
        double index = get_random_number (EEI[energy_class], EEI[energy_class+1]);
        consumption_per_cycle = (-0.0025*capacity*capacity + 0.0846*capacity + 0.3920) * index/100.;
        power.real =  consumption_per_cycle / config->wmachine.hours_per_cycle;
    }
    else
    {
        /* The source of 'factor' (kWh/kg) is
        http://www.blick.de/ratgeber/bauen-wohnen/energieeffiziente-waschmaschinen-sinnvoll-oder-augenwischerei-artikel8529019.php
        We took the mean value of each class
        */
        double factor[] = {0.12, 0.14, 0.16, 0.18, 0.21, 0.255, 0.295, 0.335, 0.375, 0.40};
        consumption_per_cycle = factor[energy_class] * capacity;
        power.real =  consumption_per_cycle / config->wmachine.hours_per_cycle;
    }
    power.reactive = sqrt ((power.real/config->wmachine.power_factor)*(power.real/config->wmachine.power_factor)
                           - power.real*power.real);

    if (config->variable_load) seconds_per_cycle = sec_per_cycle[energy_class];
    else seconds_per_cycle = config->wmachine.hours_per_cycle * 3600.;
}


void WashingMachine::simulate (double time)
{
    double start, begin, length;
    double daytime = sim_clock->daytime;
    int intervals[20], num_intervals, i;
    int best_start, best_end;

    timer--;

    // If the machine is idle
    // and there is some laundry to wash
    // and someone is already awake and willing to fire up the machine,...
    if (   timer < 0
        && household->laundry >= capacity
        && almost_equal (daytime, household->wakeup)
        && get_random_number (1, 100) <= config->wmachine.random_limit)
    {
        // ...then we have to decide when to begin.
        // This decicion is based on several factors like the
        // smartness of the machine, user preferences and sunshine.

        // A smart machine will try to utilize a predicted surplus production
        // of the solarmodules.
        if (smart && household->solar_prediction(0)) smart_mode = true;

        // A smart grid enabled machine can wait for the best price, if the
        // user is willing to wait.
        else if (   sg_enabled
                 && config->control == PRICE
                 && get_random_number (1, 100) <= (100 - config->wmachine.ignore_price))
        {
            // In this case the machine checks for the best price in the
            // near future (e.g. the next 24 hours = 1440 minutes):
            Household::producer->best_price (time, config->wmachine.best_price_lookahead, &num_intervals, intervals);
            // It is possible that there is more than one interval with the
            // best price. The values stored in 'intervals' are in minutes.
            // For the time being take the first possible interval
            i = num_intervals * get_random_number (0., 0.99);
            begin = intervals[i*2] * 60;
            length = intervals[i*2+1] * 60;
            if (begin < daytime) begin += k_seconds_per_day;
            if (seconds_per_cycle > length)
                timer = (begin-daytime) / config->timestep_size;
            else
                timer = (begin-daytime + get_random_number (0., length-seconds_per_cycle))
                        / config->timestep_size;
        }
        // In all other cases the machine is turned on sometime during the day.
        else
        {
            start = household->get_random_start_time (household->wakeup, household->bedtime);
            timer = (start - daytime) / config->timestep_size;
        }
    }

    if (smart_mode)
    {
        if (household->has_enough_solar_power (power.real))
        {
            status = ON;
            timer = seconds_per_cycle / config->timestep_size;
            household->decrease_laundry (capacity);
            smart_mode = false;
        }
        else if (daytime > sim_clock->sunset)
        {
            timer = normal_distributed_random (config->wmachine.timer_mean, config->wmachine.timer_sigma) / config->timestep_size;
            smart_mode = false;
        }
    }

    if (timer == 0)  // It's time to switch
    {
        if (status == OFF)
        {
            if (sg_enabled && stop)
            {
                // It's peak time, so let us delay the start
                timer = config->wmachine.peak_delay / config->timestep_size;
            }
            else
            {
                status = ON;
                timer = seconds_per_cycle / config->timestep_size;
                household->decrease_laundry (capacity);
            }
        }
        else
        {
            status = OFF;
            // The machine has just finished one cycle.
            // Check whether there is enough laundry left, and if someone is at home
            // and awake to load the machine...
            if (   household->laundry >= capacity
                && (daytime < household->bedtime_old || (daytime > household->wakeup && daytime < household->bedtime))
                && household->residents_at_home (daytime))
            {
                if (   sg_enabled && config->control == PRICE
                    && get_random_number (1, 100) <= (100 - config->wmachine.ignore_price))
                {
                    household->producer->next_best_price_interval (time, time+k_seconds_per_day, &best_start, &best_end);
                    if ((best_end - best_start) >= seconds_per_cycle)
                    {
                        timer = (best_start + get_random_number (0., best_end - best_start - seconds_per_cycle))/config->timestep_size + 1;
                    }
                    else
                    {
                        timer = best_start/config->timestep_size + 1;
                    }
                }
                else  // start next timestep
                {
                    timer = 1;
                }
            }
            if (household->tumble_dryer) household->tumble_dryer->add_laundry (capacity);
        }
    }

    if (status == ON)
    {
        if (config->variable_load)
        {
            power.real = variable_load[energy_class][int(seconds_per_cycle/60) - timer] * consumption_per_cycle;
            power.reactive = sqrt ((power.real/config->wmachine.power_factor)*(power.real/config->wmachine.power_factor)
                                   - power.real*power.real);
        }
        household->increase_power (power.real, power.reactive);
        power_total[0] += power.real;
        power_total[household->residents] += power.real;
        increase_consumption();
        household->heat_loss_app += power.real*0.1;
    }
}


double* extract_data (char *array, int size, double *sec_per_cycle)
{
    int count = 0, length = 0, *time;  // values in time[] are in minutes (read from varload.json)
    char *str;
    double *load, *power, value;

    alloc_memory (&time, size, "extract_data");
    alloc_memory (&power, size, "extract_data");
    str = strtok (array, "[], ");
    while (str)
    {
        count++;
        if (count%2)
        {
            time[(count-1)/2] = atoi(str);
            length += time[(count-1)/2];
        }
        else
        {
            sscanf (str, "%lf", &value);
            power[(count-2)/2] = value;
        }
        str = strtok (NULL, "[], ");
    }
    *sec_per_cycle = length * 60.;

    // Normalization to 1 kWh for one whole washing cycle
    double sum = 0., mult;
    for (int i=0; i<size; i++) sum += time[i]*power[i];
    mult = 60./sum;
    for (int i=0; i<size; i++) power[i] *= mult;

    // Save values in load array
    alloc_memory (&load, *sec_per_cycle/config->timestep_size, "extract_data");
    int index = 0;
    for (int i=0; i<size; i++)
    {
        for (int j=0; j<time[i]*60./config->timestep_size; j++) load[index++] = power[i];
    }
    delete [] time;
    delete [] power;
    return load;
}
