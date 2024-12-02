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
#include <time.h>
#ifdef _WIN32
#   define random rand
#   define srandom srand
#endif

#include "proto.H"
#include "globals.H"
#include "constants.H"


int get_random_number (int min, int max)
{
    return random() % (max - min + 1) + min;
}

double get_random_number (double min, double max)
{
    return (max - min)*(double)random()/(double)RAND_MAX + min;
}

double normal_distributed_random (int mean, int sigma)
{
    double u1, u2, q, ndr;
    double factor = 1./(double)(RAND_MAX);

    // Polar method from George Marsaglia. u1 and u2 are uniformly
    // distributed in the interval [-1,1]

    u1 = 2 * ((double)random()*factor) - 1;
    u2 = 2 * ((double)random()*factor) - 1;
    q = u1*u1 + u2*u2;
    while (q == 0. || q > 1.)
    {
        u1 = 2 * ((double)random()*factor) - 1;
        u2 = 2 * ((double)random()*factor) - 1;
        q = u1*u1 + u2*u2;
    }
    ndr = sqrt (-2.*log(q)/q) * u1;

    // ndr is a standard normal distributed random N(0,1).
    // Let's convert it to N(mean,sigma^2):

    ndr = sigma * ndr + mean;
    if (ndr < 1.) ndr = 1.;
    return ndr;
}


double normal_distributed_random_with_limits (double mean, double sigma,
                                              double lower, double upper)
{
    double u1, u2, q, ndr=0.;
    double factor = 1./(double)(RAND_MAX);

    // Polar method from George Marsaglia. u1 and u2 are uniformly
    // distributed in the interval [-1,1]

    while (ndr < lower || ndr > upper)
    {
        u1 = 2 * ((double)random()*factor) - 1;
        u2 = 2 * ((double)random()*factor) - 1;
        q = u1*u1 + u2*u2;
        while (q == 0. || q > 1.)
        {
            u1 = 2 * ((double)random()*factor) - 1;
            u2 = 2 * ((double)random()*factor) - 1;
            q = u1*u1 + u2*u2;
        }
        ndr = sqrt (-2.*log(q)/q) * u1;

        // ndr is a standard normal distributed random N(0,1).
        // Let's convert it to N(mean,sigma^2):
        ndr = sigma * ndr + mean;
    }
    return ndr;
}


int random_energy_class (double percentage[])
{
    int x = get_random_number (1, 100 * 100);
    double sum = percentage[0];
    int i = 0;
    while (x > sum*100)
    {
        i++;
        sum += percentage[i];
    }
    return i;
}

void init_random()
{
    FILE *fp = NULL;
    char file_name[k_name_length];
    char *line = NULL;

    snprintf (file_name, k_name_length, "countries/%s/table_saturday", location->country);
    open_file (&fp, file_name, "r");
    for (int i=0; i<4; i++) read_line (fp, &line);
    for (int i=0; i<1440; i++)
    {
        read_line (fp, &line);
        sscanf (line, "%*f %lf %*c", table_DHW_saturday+i);
    }
    fclose (fp);

    snprintf (file_name, k_name_length, "countries/%s/table_sunday", location->country);
    open_file (&fp, file_name, "r");
    for (int i=0; i<4; i++) read_line (fp, &line);
    for (int i=0; i<1440; i++)
    {
        read_line (fp, &line);
        sscanf (line, "%*f %lf %*c", table_DHW_sunday+i);
    }
    fclose (fp);

    snprintf (file_name, k_name_length, "countries/%s/table_weekday", location->country);
    open_file (&fp, file_name, "r");
    for (int i=0; i<4; i++) read_line (fp, &line);
    for (int i=0; i<1440; i++)
    {
        read_line (fp, &line);
        sscanf (line, "%*f %lf %*c", table_DHW_weekday+i);
    }
    fclose (fp);
    free (line);

    if (config->seed == 0) srandom ((unsigned)(time(NULL)+rank));
    else srandom ((unsigned)config->seed);
}
