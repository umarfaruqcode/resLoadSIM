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

#include "element.H"
#include "globals.H"
#include "proto.H"

enum EnergyClass
{
    Ap,
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H
};


Element::Element (Category cat, double width, double height, double temp_int, int e_class, Element *parent_elem)
{
    double temp_ext = location->temperature;
    double u_ceiling, u_wall, u_floor, u_window, u_door;

    category = cat;
    area = width * height;

    // Values for h_ci, h_ce, h_ri, h_re are from ISO 13789, Table 8
    // The initial node temperatures are assumed to be distributed linearly
    if (category == WINDOW || category == DOOR)
    {
        num_nodes = 2;
        node_temp[0] = node_temp_prev[0] = temp_int;
        node_temp[1] = node_temp_prev[1] = temp_ext;
        parent_elem->adjust_area (-area);
    }
    else
    {
        num_nodes = 5;
        node_temp[0] = node_temp_prev[0] = temp_int;
        double delta_temp = (temp_ext - temp_int) * 0.25;
        for (int i=1; i<5; i++) node_temp[i] = node_temp_prev[i] = temp_int + i*delta_temp;
    }
    switch (category)
    {
        case WINDOW: h_ci = 2.5; break;
        case DOOR: h_ci = 2.5; break;
        case WALL: h_ci = 2.5; break;
        case FLOOR: h_ci = 0.7; break;
        case CEILING: h_ci = 5.0; break;
    }
    h_ce = 20.;
    h_ri = 5.13;
    h_re = 4.14;

    // Values for the thermal transmittance U depend on the used materials
    // and therefore on the energy class of the building

    switch (e_class)
    {
        case Ap:
            u_ceiling = 0.11;
            u_wall    = 0.14;
            u_floor   = 0.18;
            u_window  = 0.80;
            u_door    = 1.00;
            break;
        case A:
            u_ceiling = 0.20;
            u_wall    = 0.25;
            u_floor   = 0.30;
            u_window  = 1.00;
            u_door    = 1.20;
            break;
        case B:
            u_ceiling = 0.28;
            u_wall    = 0.33;
            u_floor   = 0.38;
            u_window  = 1.20;
            u_door    = 1.40;
            break;
        case C:
            u_ceiling = 0.36;
            u_wall    = 0.41;
            u_floor   = 0.46;
            u_window  = 1.40;
            u_door    = 1.60;
            break;
        case D:
            u_ceiling = 0.46;
            u_wall    = 0.50;
            u_floor   = 0.55;
            u_window  = 1.70;
            u_door    = 2.00;
            break;
        case E:
            u_ceiling = 0.55;
            u_wall    = 0.60;
            u_floor   = 0.65;
            u_window  = 2.00;
            u_door    = 2.40;
            break;
        case F:
            u_ceiling = 0.65;
            u_wall    = 0.75;
            u_floor   = 0.90;
            u_window  = 2.50;
            u_door    = 3.00;
            break;
        case G:
            u_ceiling = 0.85;
            u_wall    = 0.95;
            u_floor   = 1.05;
            u_window  = 3.0;
            u_door    = 3.5;
            break;
        case H:
            u_ceiling = 1.10;
            u_wall    = 1.20;
            u_floor   = 1.30;
            u_window  = 3.5;
            u_door    = 4.0;
            break;
    }
    for (int i=0; i<4; i++) h[i] = 0.;
    switch (category)
    {
        case WALL:
            h[0] = h[3] = 6. * u_wall;
            h[1] = h[2] = 3. * u_wall;
            break;
        case FLOOR:
            h[0] = h[3] = 6. * u_floor;
            h[1] = h[2] = 3. * u_floor;
            break;
        case CEILING:
            h[0] = h[3] = 6. * u_ceiling;
            h[1] = h[2] = 3. * u_ceiling;
            break;
        case WINDOW:
            h[0] = u_window;
            break;
        case DOOR:
            h[0] = u_door;
            break;
    }
    // Values for kappa from ISO-52016-1, Table B.4
    // assuming class D
    double kappa_m = get_random_number (50000., 250000.);
    kappa[0] = kappa[1] = kappa[2] = kappa_m/3.;

    if (category  == CEILING) phi_sky = h_re*11.;
    else if (category == FLOOR) phi_sky = 0.;
    else phi_sky = 0.5*h_re*11.;
}


void Element::adjust_area (double value)
{
    area -= value;
}

void Element::print_node_temp (void)
{
    printf ("Element::print_node_temp:  ");
    for (int i=0; i<num_nodes; i++) printf ("%lf ", node_temp[i]);
    printf ("\n");
}
