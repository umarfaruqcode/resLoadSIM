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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "version.H"
#include "proto.H"

void parse_arguments (int argc, char **argv, int *num_households, double *days, bool *silent_mode)
{
    *silent_mode = false;
    for (int i=1; i<argc; i++)
    {
        if (argv[i][0] == '-')
        {
            if (argv[i][1] == 'v')
            {
#ifdef DEBUG
                printf ("resLoadSIM version %s (DEBUG)\n", VERSION);
#else
                printf ("resLoadSIM version %s (OPTIMIZED)\n", VERSION);
#endif
                exit(0);
            }
            else if (argv[i][1] == 's') *silent_mode = true;
            else
            {
                fprintf (stderr, "Unknown option %s\n", argv[i]);
                exit(1);
            }
        }
        else  // non-option argument
        {
            if (*num_households == 0)
            {
                *num_households = atoi (argv[i]);
            }
            else
            {
                sscanf (argv[i], "%lf", days);
            }
        }
    }

    if (*num_households == 0 || *days <= 0)
    {
        fprintf (stderr, "usage:  resLoadSIM households days\n");
        exit(1);
    }
}
