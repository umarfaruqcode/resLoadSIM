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

#ifndef TYPES_H
#define TYPES_H

struct Power
{
    double real;
    double reactive;
};

enum Status
{
    OFF,
    ON,
    INTERRUPTED,
    IDLE,
    DRIVING,
    CHARGING,
    FORCED_CHARGING,
    FORCED_IDLE
};

enum ControlModes
{
    NONE = 0,
    PEAK_SHAVING,
    PROFILE,
    COMPENSATE,
    PRICE,
    NUM_CONTROL_MODES
};

enum HeatSourceType
{
    OIL = 0,                // Oil heating (both space heating (SH) and dom. hot water (DHW))
    GAS,                    // Gas heating (SH and DHW)
    DISTRICT,               // District heating (SH and DHW)
    HEAT_PUMP,              // Heat pump with auxiliary electrical heating for SH, and optional boiler for DHW
    SOLAR_COLLECTOR,        // Solar collector with auxiliary heat pump for SH and DHW, buffered by a heat storage
    NUM_HEAT_SOURCE_TYPES
};

enum DHW_activity  // Activities that influence the DHW model
{
    DO_NOTHING = 0,
    HANDWASH,
    SHOWER,
    BATH,
    COOK
};

enum Destination
{
    HOME = 0,
    WORK,
    SHOP,
    RECREATION,
    NUM_DESTINATIONS
};

enum Occupation
{
    STUDENT = 0,
    FULLTIME,
    PARTTIME,
    RETIRED
};

enum
{
    GRID = 0,
    SOLAR,
    NUM_PRICE_TABLES
};

enum HouseholdType
{
    URBAN = 0,
    RURAL,
    NUM_HOUSEHOLD_TYPES
};

struct Timer
{
    double duration;
    double heat_demand;
    Timer *next;
};

struct PriceInterval
{
    int begin;
    int length;
    double price;
};

#endif
