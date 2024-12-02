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

#ifndef CONSTANTS_H
#define CONSTANTS_H

#define k_max_residents             6     // Households with 1, 2 ... up to 6 persons
#define k_max_files                 100
#define k_name_length               256
#define k_max_path                  4096
#define k_profile_length            100
#define k_max_sequence_length       100
#define k_seconds_per_day           86400.0
#define k_seconds_per_year          31536000  // 365 * 24 * 3600;
#define k_rls_json_file_name        "resLoadSIM.json"
#define k_rls_json_log_file_name    "resLoadSIM.json.log"
#define k_hh_json_file_name         "households.json"
#define k_hh_json_log_file_name     "households.json.log"
#define k_tech_json_file_name       "tech.json"
#define k_tech_json_log_file_name   "tech.json.log"
#define k_holidays_json_file_name   "public_holidays.json"
#define k_vacation_json_file_name   "vacation.json"
#define k_float_compare_eps         1.e-6

#define k_heat_capacity_H2O         4.184  // [kJ/(kg*K)] at 14.5°C

#define k_num_ec_household           9
#define k_max_energy_classes        10

#define k_num_ev_models             10
#define k_num_curve_points          21
#define k_max_ref_years             20
#define k_max_holidays              20
#endif
