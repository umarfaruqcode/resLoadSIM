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

#ifndef HEAT_SOURCE_H
#define HEAT_SOURCE_H

#include "types.H"
#include "constants.H"
#include "proto.H"
#include "globals.H"
#ifdef PARALLEL
#include <mpi.h>
#endif

/******************************************************************************/

class HeatSource
{
private:
    double heat_sum;
    class Household *household;                 // pointer to the household this heat source belongs to
    static int count[NUM_HEAT_SOURCE_TYPES];

public:
    double max_heat_power;
    static double heat_power_SH_total[NUM_HEAT_SOURCE_TYPES][k_max_residents+1];   // power used for SH for all households [kW]
    static double heat_power_DHW_total[NUM_HEAT_SOURCE_TYPES][k_max_residents+1];  // power used for DHW for all households [kW]

    HeatSource (Household *hh);
    void simulate();
    static int global_count (int i)
    {
#ifdef PARALLEL
        MPI_Allreduce (MPI_IN_PLACE, count, NUM_HEAT_SOURCE_TYPES, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
#endif
        return count[i];
    }
};

#ifdef MAIN_MODULE
double HeatSource::heat_power_SH_total[NUM_HEAT_SOURCE_TYPES][k_max_residents+1];
double HeatSource::heat_power_DHW_total[NUM_HEAT_SOURCE_TYPES][k_max_residents+1];
int    HeatSource::count[NUM_HEAT_SOURCE_TYPES];
#endif

#endif
