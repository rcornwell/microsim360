/*
 * microsim360 - 2030 Front panel display.
 *
 * Copyright 2021, Richard Cornwell
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <SDL.h>
#include <SDL_timer.h>
#include <SDL_thread.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <stdio.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <SDL_main.h>
#endif
#include <fcntl.h>

#include "widgets.h"
#include "area.h"
#include "line.h"
#include "label.h"
#include "light.h"
#include "button.h"
#include "timer.h"
#include "lamp.h"
#include "hex_dial.h"
#include "store_dial.h"
#include "dial.h"
#include "ros_row.h"
#include "reg_row.h"
#include "cpu.h"
#include "logger.h"
#include "model2030.h"

/* ! is line between digits.
   | is line in place of digit.
*/
                   /*            1         2           */
                   /*  0123456789012345678901234567890 */
struct _ros_row row1 = {
                      "P!012345 ! P !L! P!1!8421| P!8421!8421",
                      " |       !   ! ! P!3!4567| P!0123!4567",
                      23, &c_on, &c_off};

                   /*            1         2           */
                   /*  0123456789012345678901234567890 */
struct _ros_row row2 = {
                      "P!0123|0123|A0123|01!012!01!A!P!0123",
                       NULL,
                       26, &c_on, &c_off};

                   /*            1         2           */
                   /*  0123456789012345678901234567890 */
struct _ros_row row3 = {
                      "P! 0123|    |012 |01|01!012!A0123",
                       NULL,
                       23, &c_on, &c_off};

struct _labels sel_labels[] = {
    { "CD", NULL },
    { "CC", NULL },
    { "SLI", NULL },
    { "SKIP", NULL },
    { "PCI", NULL },
    { "OP", "IN" },
    { "ADR", "IN" },
    { "STAT", "IN" },
    { "SERV", "IN" },
    { "SEL", "OUT" },
    { "ADR", "OUT" },
    { "CMND", "OUT" },
    { "SERV", "OUT" },
    { "SUP", "OUT" },
    { "IL", NULL },
    { "PROG", NULL },
    { "PROT", NULL },
    { "CHNL", "DATA" },
    { "CHNL", "CTRL" },
    { "INT", "FACE" },
};

void
setup_fp2030(void *rend)
{
    reg_row  reg;
    int	     i, j, k;
    int      h, w;
    int	     hx, wx;
    int      h1, w1;      /* Height and width of font 1 */
    int      start;       /* Start of register display boarders. */
    int      width;       /* Width of register display boarders. */
    int      reg_width;   /* Width of register areas */
    int      reg_start;   /* Start of register areas */
    int      pos;         /* Current vertical position */
    int      step;        /* Step for each row */
    int      control_row; /* Top of control panel position */
    int      s;           /* Temporary start position */
    int      e;           /* Temporary end position */
    int      p;           /* Temporary row position */
    int      pos_reg[32]; /* Position of digits in register */
    Uint32   f;
    dial_label label;

log_trace("Initialize panel\n");
    j = 0;

    /* Compute size of fonts */
    if (TTF_SizeText(font10, "M", &wx, &hx) != 0) {
        return;
    }
    if (TTF_SizeText(font1, "M", &w1, &h1) != 0) {
        return;
    }

    step = hx;
    pos = step;

    /* Draw top of display */
    add_area(cpu_panel, 0, 0, 975, 1100, &c_back);
    start = 8 * wx;
    reg_width = ros_row_width(cpu_panel, &row1, font10);
    reg_start = start + (3 * wx);
    width = reg_width + (wx * 4);

    /* Draw top ROS border  */
    add_area(cpu_panel, start, pos, hx + 2, width, &c_outline);
    add_area(cpu_panel, start, pos, (hx * 11) + 4, wx+3, &c_outline);
    add_area(cpu_panel, start + width, pos, (hx * 11) + 4, wx, &c_outline);
    add_label_center(cpu_panel, step, pos, width, "READ ONLY STORAGE",
                font10, &c_white);

    /* Draw ROS boxes */
    pos += (2 * step) - 4;
    add_area(cpu_panel, reg_start, pos-1, (hx * 3) + 6, reg_width, &c_label);
    add_line(cpu_panel, reg_start, pos + hx + 1, reg_width, &c_white);
    add_ros_row(cpu_panel, reg_start, pos+hx+3, &row1,
                font10, &cpu_2030.ros_row1, pos_reg, &c_white);

    add_label_center(cpu_panel, pos_reg[0], pos, pos_reg[2] - pos_reg[0],
                   "CN", font10, &c_white);
    add_mark(cpu_panel, pos_reg[2]-1, pos+1, hx-3, &c_white);
    add_label_center(cpu_panel, pos_reg[2], pos, pos_reg[3] - pos_reg[2],
                   "ADR", font10, &c_white);
    add_mark(cpu_panel, pos_reg[3]-1, pos+1, hx-3, &c_white);
    add_label_center(cpu_panel, pos_reg[4], pos, pos_reg[7] - pos_reg[4],
                   "W REGISTER", font10, &c_white);
    add_mark(cpu_panel, pos_reg[4]-1, pos+1, hx-3, &c_white);
    add_label_center(cpu_panel, pos_reg[7], pos, pos_reg[10] - pos_reg[7],
                   "X REGISTER", font10, &c_white);
    add_mark(cpu_panel, pos_reg[7]-1, pos+1, hx-3, &c_white);

    pos += step * 4;

    /* Second ROS row */
    add_area(cpu_panel, reg_start, pos - 1, (hx * 2) + 6, reg_width, &c_label);
    add_line(cpu_panel, reg_start, pos + hx + 1, reg_width, &c_white);

    add_ros_row(cpu_panel, reg_start, pos+hx+3,  &row2, font10, &cpu_2030.ros_row2,
                 &pos_reg[0], &c_white);
    add_label_center(cpu_panel, pos_reg[0], pos, pos_reg[1] - pos_reg[0],
                   "SA", font10, &c_white);
    add_mark(cpu_panel, pos_reg[1], pos+1, hx-3, &c_white);
    add_label_center(cpu_panel, pos_reg[1], pos, pos_reg[2] - pos_reg[1],
                   "CH", font10, &c_white);
    add_mark(cpu_panel, pos_reg[1], pos+1, hx-3, &c_white);
    add_label_center(cpu_panel, pos_reg[2], pos, pos_reg[3] - pos_reg[2],
                   "CL", font10, &c_white);
    add_mark(cpu_panel, pos_reg[2], pos+1, hx-3, &c_white);
    add_label_center(cpu_panel, pos_reg[3], pos, pos_reg[4] - pos_reg[3],
                   "CA", font10, &c_white);
    add_mark(cpu_panel, pos_reg[3], pos+1, hx-3, &c_white);
    add_label_center(cpu_panel, pos_reg[4], pos, pos_reg[5] - pos_reg[4],
                   "CB", font10, &c_white);
    add_mark(cpu_panel, pos_reg[4], pos+1, hx-3, &c_white);
    add_label_center(cpu_panel, pos_reg[5], pos, pos_reg[6] - pos_reg[5],
                   "CM", font10, &c_white);
    add_mark(cpu_panel, pos_reg[5], pos+1, hx-3, &c_white);
    add_label_center(cpu_panel, pos_reg[6], pos, pos_reg[7] - pos_reg[6],
                   "CU", font10, &c_white);
    add_mark(cpu_panel, pos_reg[6], pos+1, hx-3, &c_white);
    add_label_center(cpu_panel, pos_reg[7], pos, pos_reg[8] - pos_reg[7],
                   "CK", font10, &c_white);
    add_mark(cpu_panel, pos_reg[7], pos+1, hx-3, &c_white);

    pos += step * 3;

    /* 3rd ROS Row */
    add_area(cpu_panel, reg_start, pos - 1, (hx * 2) + 6, reg_width , &c_label);
    add_line(cpu_panel, reg_start, pos + hx, reg_width, &c_white);

    add_ros_row(cpu_panel, reg_start, pos+hx+3,  &row3, font10,
                &cpu_2030.ros_row3, pos_reg, &c_white);
    add_label_center(cpu_panel, pos_reg[0], pos, pos_reg[1] - pos_reg[0],
                   "CR", font10, &c_white);
    add_mark(cpu_panel, pos_reg[1], pos+1, hx-3, &c_white);
    add_label_center(cpu_panel, pos_reg[1], pos, pos_reg[2] - pos_reg[1],
                   "CD", font10, &c_white);
    add_mark(cpu_panel, pos_reg[2], pos+1, hx-3, &c_white);
    add_label_center(cpu_panel, pos_reg[3], pos, pos_reg[4] - pos_reg[3],
                   "CF", font10, &c_white);
    add_mark(cpu_panel, pos_reg[3], pos+1, hx-3, &c_white);
    add_label_center(cpu_panel, pos_reg[4], pos, pos_reg[5] - pos_reg[4],
                   "CG", font10, &c_white);
    add_mark(cpu_panel, pos_reg[4], pos+1, hx-3, &c_white);
    add_label_center(cpu_panel, pos_reg[5], pos, pos_reg[6] - pos_reg[5],
                   "CV", font10, &c_white);
    add_mark(cpu_panel, pos_reg[5], pos+1, hx-3, &c_white);
    add_label_center(cpu_panel, pos_reg[6], pos, pos_reg[7] - pos_reg[6],
                   "CC", font10, &c_white);
    add_mark(cpu_panel, pos_reg[6], pos+1, hx-3, &c_white);
    add_label_center(cpu_panel, pos_reg[7], pos, pos_reg[8] - pos_reg[7],
                   "CS", font10, &c_white);
    add_mark(cpu_panel, pos_reg[7], pos+1, hx-3, &c_white);

    pos += (step * 3) + (hx/2);

    /* Count register */
    reg.upper = "P!8421!8421|P!8421!8421";
    reg.lower = "P!0123!4576|P!0123!4576";
    reg.c_on = &c_on;
    reg.c_off = &c_off;
    reg.start_bit[0] = 8;
    reg.value[0] = &cpu_2030.GHZ;
    reg.start_bit[1] = 8;
    reg.value[1] = &cpu_2030.GHY;
    w = reg_row_width(cpu_panel, &reg, font10);
    s = (reg_width/2) - (w/2);
    add_area(cpu_panel, reg_start, pos - 1, (hx * 3) + 6, reg_width , &c_label);
    add_line(cpu_panel, reg_start, pos + hx, reg_width, &c_white);
    add_reg_row(cpu_panel, reg_start + s, pos+hx+3, &reg, font10, pos_reg, &c_white);
    add_label_center(cpu_panel, reg_start, pos, reg_width, "COUNT REGISTER",
                 font1, &c_white);

    pos += (step * 4);

    /* Selector channels */
    for (i = 0; i < 2; i++) {
        s = (reg_width/2) - (w/2);
        reg.start_bit[0] = 8;
        reg.value[0] = &cpu_2030.GR[i];
        reg.start_bit[1] = 4;
        reg.value[1] = &cpu_2030.GK[i];
        reg.start_bit[2] = 3;
        reg.value[2] = &cpu_2030.GG[i];
        /* Draw top Channel border */
        add_area(cpu_panel, start, pos, hx + 2, width, &c_outline);
        add_area(cpu_panel, start, pos, hx * 11, wx+3, &c_outline);
        add_area(cpu_panel, start + width, pos, (hx * 11) + 4, wx, &c_outline);
        if (i != 0) {
            add_label_center(cpu_panel, start, pos, width, "CHANNEL TWO",
                                font10, &c_white);
        } else {
            add_label_center(cpu_panel, start, pos, width, "CHANNEL ONE",
                                font10, &c_white);
        }

        pos += (2 * step) - 4;
        add_area(cpu_panel, reg_start, pos-1, (hx * 3) + 6, reg_width, &c_label);
        add_line(cpu_panel, reg_start, pos + hx + 1, reg_width, &c_white);
        add_reg_row(cpu_panel, s, pos+hx+3, &reg, font10, pos_reg, &c_white);

        add_mark(cpu_panel, pos_reg[0], pos+1, hx-3, &c_white);
        add_label_center(cpu_panel, pos_reg[0], pos, pos_reg[3] - pos_reg[0],
                       "DATA REGISTER", font10, &c_white);
        add_mark(cpu_panel, pos_reg[3]-1, pos+1, hx-3, &c_white);
        add_label_center(cpu_panel, pos_reg[3], pos, pos_reg[5] - pos_reg[3],
                       "KEY", font10, &c_white);
        add_mark(cpu_panel, pos_reg[5]-1, pos+1, hx-3, &c_white);
        add_label_center(cpu_panel, pos_reg[5], pos, pos_reg[6] - pos_reg[5],
                       "COMMAND", font10, &c_white);
        add_mark(cpu_panel, pos_reg[7]-1, pos+1, hx-3, &c_white);

        pos += step * 4;
        /* Channel Status */
        add_area(cpu_panel, reg_start, pos-1, (hx * 5) + (hx/2), reg_width, &c_label);
        add_line(cpu_panel, reg_start, pos + hx + 1, reg_width, &c_white);
        s = reg_start + wx;
        k = 7;
        for (j = 0; j < 5; j++) {
            add_light(cpu_panel, s, pos + (hx * 2) - 2, sel_labels[j].upper,
                   sel_labels[j].lower, &cpu_2030.GF[i], k, font10, &c_on, &c_off);
            s += wx * 5;
            k--;
        }
        add_label_center(cpu_panel, reg_start, pos, s - reg_start, "FLAGS", font10,
                        &c_white);
        add_mark(cpu_panel, s, pos+1, hx-3, &c_white);
        add_mark(cpu_panel, s, pos+hx+1, (hx * 5)-3, &c_white);
        k = 7;
        e = s;
        s += wx * 3;
        for (; j < 9; j++) {
            add_light(cpu_panel, s, pos + (hx * 2) - 2, sel_labels[j].upper,
                   sel_labels[j].lower, &cpu_2030.SEL_TI[i], k, font10, &c_on, &c_off);
            s += wx * 5;
            k--;
        }
        s = e + wx * 3;
        k = 15;
        for (; j < 14; j++) {
            add_light(cpu_panel, s, pos + (hx * 4) - 2, sel_labels[j].upper,
                   sel_labels[j].lower, &cpu_2030.SEL_TAGS[i], k, font10, &c_on, &c_off);
            s += wx * 5;
            k--;
        }
        add_label_center(cpu_panel, e, pos, s - e, "TAGS", font10, &c_white);
        add_mark(cpu_panel, s, pos+1, hx-3, &c_white);
        add_mark(cpu_panel, s, pos+hx+1, (hx * 5)-3, &c_white);
        e = s;
        k = 6;
        s += wx * 2;
        for (; j < 19; j++) {
            add_light(cpu_panel, s, pos + (hx * 2) - 2, sel_labels[j].upper,
                   sel_labels[j].lower, &cpu_2030.GE[i], k, font10, &c_red, &c_red_off);
            s += wx * 5;
            k--;
        }
        add_label_center(cpu_panel, e, pos, reg_width - e, "CHECKS", font10, &c_white);
        pos += (step * 6) + (hx / 2);
    }

    /* MPX Channel TAGS */
    add_area(cpu_panel, reg_start, pos-1, (hx * 3) + (hx/2), reg_width, &c_label);
    add_line(cpu_panel, reg_start, pos + hx + 1, reg_width, &c_white);
    s = reg_start + wx;
    k = 7;
    e = s;
    for (j = 5; j < 9; j++) {
        add_light(cpu_panel, s, pos + (hx * 2) - 2, sel_labels[j].upper,
                sel_labels[j].lower, &cpu_2030.MPX_TI, k, font10, &c_on, &c_off);
        s += wx * 5;
        k--;
    }
    k = 15;
    for (; j < 14; j++) {
        add_light(cpu_panel, s, pos + (hx * 2) - 2, sel_labels[j].upper,
                sel_labels[j].lower, &cpu_2030.MPX_TAGS, k, font10, &c_on, &c_off);
        s += wx * 5;
        k--;
    }
    add_label_center(cpu_panel, e, pos, s - e, "MPX CHANNEL TAGS", font10, &c_white);
    add_mark(cpu_panel, s, pos+1, hx-3, &c_white);
    add_mark(cpu_panel, s, pos+hx+1, (hx * 5)-3, &c_white);
    e = s;
    reg.upper = "P!8421!8421";
    reg.lower = "P!0123!4576";
    reg.c_on = &c_on;
    reg.c_off = &c_off;
    reg.start_bit[0] = 8;
    reg.value[0] = &cpu_2030.O_REG;
    add_reg_row_large(cpu_panel, s, pos+hx+3, &reg, font10, pos_reg, &c_white);
    add_label_center(cpu_panel, s, pos, reg_width - s,
               "MPX CHANNEL BUS-OUT REGISTER", font1, &c_white);
    control_row = pos;
    pos += (step * 4);

    /* Address register */
    reg.upper = "P!8421!8421|P!8421!8421";
    reg.lower = "P!0123!4576|P!0123!4576";
    reg.c_on = &c_on;
    reg.c_off = &c_off;
    reg.start_bit[0] = 8;
    reg.value[0] = &cpu_2030.N_REG;
    reg.start_bit[1] = 8;
    reg.value[1] = &cpu_2030.M_REG;
    add_area(cpu_panel, reg_start, pos - 1, (hx * 5) + 5, reg_width , &c_label);
    add_line(cpu_panel, reg_start, pos + hx, reg_width, &c_white);
    add_reg_row(cpu_panel, reg_start, pos+ (hx * 2), &reg, font10, pos_reg, &c_white);
    add_mark(cpu_panel, pos_reg[6] + wx, pos+hx+4, (4 * hx) - 3, &c_white);
    add_light(cpu_panel, pos_reg[6] + (wx * 3), pos+hx+(hx/2), "MAIN", "STOR",
                           &cpu_2030.store, 0, font10, &c_on, &c_off);
    add_light(cpu_panel, pos_reg[6] + (wx * 3), pos+(hx*4)-(hx/2), "AUX", "STOR",
                           &cpu_2030.store, 1, font10, &c_on, &c_off);
    add_label_center(cpu_panel, reg_start, pos, reg_width,
            "MAIN STOARAGE ADDRESS REGISTER", font1, &c_white);

    pos += (step * 6);

    /* CPU registers */
    reg.upper = "P!8421!8421|P!8421!8421";
    reg.lower = "P!0123!4576|P!0123!4576";
    reg.c_on = &c_on;
    reg.c_off = &c_off;
    reg.start_bit[0] = 8;
    reg.value[0] = &cpu_2030.R_REG;
    reg.start_bit[1] = 8;
    reg.value[1] = &cpu_2030.Alu_out;
    w = pos_reg[6]-reg_start;
    add_area(cpu_panel, reg_start, pos - 1, (hx * 3) + 5, w, &c_label);
    add_line(cpu_panel, reg_start, pos + hx, w, &c_white);
    add_reg_row(cpu_panel, reg_start, pos+hx, &reg, font10, pos_reg, &c_white);
    add_label_center(cpu_panel, pos_reg[0], pos, pos_reg[3],
            "MAIN STOARAGE DATA REGISTER", font1, &c_white);
    add_label_center(cpu_panel, pos_reg[3], pos, pos_reg[6]-pos_reg[3],
            "ALU OUTPUT", font1, &c_white);

    s = pos;  /* Save position for CPU Status and Check area's */
    e = w + (hx * 2); /* Save Start of area */

    pos += (step * 4) - (hx/2);

    reg.start_bit[0] = 8;
    reg.value[0] = &cpu_2030.Bbus;
    reg.start_bit[1] = 8;
    reg.value[1] = &cpu_2030.Abus;
    add_area(cpu_panel, reg_start, pos - 1, (hx * 3) + 5, w , &c_label);
    add_line(cpu_panel, reg_start, pos + hx, w, &c_white);
    add_reg_row(cpu_panel, reg_start, pos+hx, &reg, font10, pos_reg, &c_white);
    add_label_center(cpu_panel, pos_reg[0], pos, pos_reg[3],
            "B REGISTER", font1, &c_white);
    add_label_center(cpu_panel, pos_reg[3], pos, pos_reg[6]-pos_reg[3],
            "A REGISTER", font1, &c_white);

    pos += (step * 4) - (hx/2);

    /* CPU status */
    h = pos - s - (hx/2);
    w = ((reg_start + reg_width - e) / 2);
    w -= wx/2;
    i = s + hx - (hx/2);
    add_area(cpu_panel, e, s, h, w, &c_label);
    add_line(cpu_panel, e, s + hx, w, &c_white);
    add_label_center(cpu_panel, e, s, w, "CPU STATUS", font1, &c_white);
    add_light(cpu_panel, e + (wx), i + (hx), "EX", NULL, &cpu_2030.end_of_e_cycle, 0,
                font1, &c_on, &c_off);
    add_light(cpu_panel, e + (wx * 5), i + (hx), "MATCH", NULL, &cpu_2030.match, 0,
                font1, &c_on, &c_off);
    add_light(cpu_panel, e + (wx * 10), i + (hx), "ALLOW", "WRITE",
                &cpu_2030.allow_write, 0, font1, &c_on, &c_off);
    add_light(cpu_panel, e + (wx), i + (hx * 3), "1050", "INTV", &cpu_2030.TT, 2,
                font1, &c_on, &c_off);
    add_light(cpu_panel, e + (wx * 10), i + (hx * 3), "1050", "REQ",
                &cpu_2030.t_request, 0, font1, &c_on, &c_off);
    add_light(cpu_panel, e + (wx), i + (hx * 5), "MPX", "CHNL", &cpu_2030.FT, 5,
                font1, &c_on, &c_off);
    add_light(cpu_panel, e + (wx * 5), i + (hx * 5), "SEL", "CHNL", &cpu_2030.H_REG,
                3, font1, &c_on, &c_off);
    add_light(cpu_panel, e + (wx * 10), i + (hx * 5), "COMP", "MODE", 0, 0,
                font1, &c_on, &c_off);

    /* CPU Checks */
    e += w + wx;
    add_area(cpu_panel, e, s, h, w, &c_label);
    add_line(cpu_panel, e, s + hx, w, &c_white);
    add_label_center(cpu_panel, e, s, w, "CPU CHECKS", font1, &c_white);
    add_light(cpu_panel, e + (wx), i + (hx), "STOR", "ADR", &cpu_2030.MC_REG,
               1, font1, &c_red, &c_off);
    add_light(cpu_panel, e + (wx * 5), i + (hx), "STOR", "DATA", &cpu_2030.MC_REG,
               7, font1, &c_red, &c_off);
    add_light(cpu_panel, e + (wx), i + (hx * 3), "A", "REG", &cpu_2030.MC_REG,
               6, font1, &c_red, &c_off);
    add_light(cpu_panel, e + (wx * 5), i + (hx * 3), "B", "REG", &cpu_2030.MC_REG,
               0, font1, &c_red, &c_off);
    add_light(cpu_panel, e + (wx * 10), i + (hx * 3), "ALU", NULL, &cpu_2030.MC_REG,
               5, font1, &c_red, &c_off);
    add_light(cpu_panel, e + (wx), i + (hx * 5), "ROS", "ADR", &cpu_2030.MC_REG,
               2, font1, &c_red, &c_off);
    add_light(cpu_panel, e + (wx * 5), i + (hx * 5), "ROS", "SALS", &cpu_2030.MC_REG,
               3, font1, &c_red, &c_off);
    add_light(cpu_panel, e + (wx * 10), i + (hx * 5), "CTRL", "REG", &cpu_2030.MC_REG,
               4, font1, &c_red, &c_red_off);

    pos += step * 2;
    s = 10;
    /* Draw bottom switch panel */
    add_area(cpu_panel, 0, pos - hx - (hx/2) - 3, (2*hx) + (975 - pos), 1100, &c_label);

    add_button(cpu_panel, s, pos + hx, hx * 2, wx * 10, "SYSTEM", "RESET",
               &SYS_RST, font10, &c_white, &c_blue, 0);
    add_button(cpu_panel, s, pos + (hx * 3) + (hx/2), hx * 2, wx * 10, "ROAR", "RESET",
               &ROAR_RST, font10, &c_white, &c_blue, 0);
    add_blank(cpu_panel, s, pos + (hx * 6), hx * 2, wx * 10, &c_white);
    add_button(cpu_panel, s, pos + (hx * 8) + (hx/2), hx * 2, wx * 10, "START", NULL,
               &START, font10, &c_white, &c2, 0);

    s += wx * 12;
    add_blank(cpu_panel, s, pos + hx, hx * 2, wx * 10, &c_white);
    add_button(cpu_panel, s, pos + (hx * 3) + (hx/2), hx * 2, wx * 10, "SET", "IC",
               &SET_IC, font10, &c_white, &c_blue, 0);
    add_button(cpu_panel, s, pos + (hx * 6), hx * 2, wx * 10, "CHECK", "RESET",
               &CHECK_RST, font10, &c_white, &c_blue, 0);
    add_button(cpu_panel, s, pos + (hx * 8) + (hx/2), hx * 2, wx * 10, "STOP", NULL,
               &STOP, font10, &c_white, &c_red, 0);

    s += wx * 12;
    add_timer(cpu_panel, s, pos + hx, hx * 2, wx * 10, "INT_TMR", &INT_TMR,
              font10, &c_white, &c_blue);
    add_button(cpu_panel, s, pos + (hx * 3) + (hx/2), hx * 2, wx * 10, "STORE", NULL,
               &STORE, font10, &c_white, &c_blue, 0);
    add_button(cpu_panel, s, pos + (hx * 6), hx * 2, wx * 10, "LAMP", "TEST",
               &LAMP_TEST, font10, &c_white, &c_blue, 1);
    add_button(cpu_panel, s, pos + (hx * 8) + (hx/2), hx * 2, wx * 10, "DISPLAY", NULL,
               &START, font10, &c_white, &c_blue, 0);

    s += wx * 16;
    p = pos + (2 * hx) + (h1 * 4) + (h1/2);
    pos_reg[0] = s;
    add_hex_dial(cpu_panel, s, p, &A_SW);
    s += 80;
    pos_reg[1] = s;
    add_hex_dial(cpu_panel, s, p, &B_SW);
    s += 80;
    pos_reg[2] = s;
    add_hex_dial(cpu_panel, s, p, &C_SW);
    s += 80;
    pos_reg[3] = s;
    add_hex_dial(cpu_panel, s, p, &D_SW);
    s += 80;
    pos_reg[4] = s;
    add_store_dial(cpu_panel, s, p - 8, &E_SW);
    E_SW = 0x10;
    s += 80 + 8;
    pos_reg[5] = s;
    add_hex_dial(cpu_panel, s, p, &F_SW);
    s += 80;
    pos_reg[6] = s;
    add_hex_dial(cpu_panel, s, p, &G_SW);
    s += 80;
    pos_reg[7] = s;
    add_hex_dial(cpu_panel, s, p, &H_SW);
    s += 80;
    pos_reg[8] = s;
    add_hex_dial(cpu_panel, s, p, &J_SW);
    s += 80;
    pos_reg[9] = s;

    e = pos_reg[3] + 70;
    h = p;
    p = pos + (2 * hx);
    add_mark(cpu_panel, pos_reg[0] - 3, p, h1 + 2, &c_white);
    add_area(cpu_panel, pos_reg[0] - 3, p, h1, (e - (pos_reg[0] -3)), &c_white);
    add_label_center(cpu_panel, pos_reg[0] - 3, p,
            (e - (pos_reg[0] - 3)), "COMPARE ADDRSS", font1, &c_black);
    add_mark(cpu_panel, e, p, h1 + 2, &c_white);

    p += h1 + 4;
    add_mark(cpu_panel, pos_reg[0] - 3, p, h - p + 2, &c_white);
    add_area(cpu_panel, pos_reg[0] - 3, p, h1, (e - (pos_reg[0]-3)), &c_white);
    add_label_center(cpu_panel, pos_reg[0] - 3, p,
            (e - (pos_reg[0] - 3)), "MAIN STORAGE ADDRSS", font1, &c_black);
    add_mark(cpu_panel, e, p, h - p + 2, &c_white);

    e = pos_reg[4] + 80;
    p = pos + (2 * hx);
    add_mark(cpu_panel, pos_reg[4] - 3, p, h - p + 2, &c_white);
    add_area(cpu_panel, pos_reg[4] - 3, p, h1, (e - (pos_reg[4] -3)), &c_white);
    add_label_center(cpu_panel, pos_reg[4] - 3, p,
            (e - (pos_reg[4] - 3)), "DISPLAY STOR", font1, &c_black);
    add_mark(cpu_panel, e, p, h - p + 2, &c_white);

    e = pos_reg[8] + 70;
    add_mark(cpu_panel, pos_reg[5] - 3, p, h - p + 2, &c_white);
    add_area(cpu_panel, pos_reg[5] - 3, p, h1, (e - (pos_reg[5] -3)), &c_white);
    add_label_center(cpu_panel, pos_reg[5] - 3, p, (e - (pos_reg[5] - 3)),
                           "INSTRUCTION ADDRESS - ROS ADDRESS", font1, &c_black);
    add_mark(cpu_panel, e, p, h1 + 2, &c_white);

    p += (h1) + 4;
    add_mark(cpu_panel, pos_reg[6] - 3, p, h - p + 2, &c_white);
    add_area(cpu_panel, pos_reg[6] - 3, p, h1, (e - (pos_reg[6] -3)), &c_white);
    add_label_center(cpu_panel, pos_reg[6] - 3, p,
            (e - (pos_reg[6] - 3)), "LOAD UNIT", font1, &c_black);
    add_mark(cpu_panel, e, p, h1 + 2, &c_white);

    p += (h1) + 4;
    add_mark(cpu_panel, pos_reg[7] - 3, p, h - p + 2, &c_white);
    add_area(cpu_panel, pos_reg[7] - 3, p, h1, (e - (pos_reg[7] -3)), &c_white);
    add_label_center(cpu_panel, pos_reg[7] - 3, p,
            (e - (pos_reg[6] - 3)), "DATA", font1, &c_black);
    add_mark(cpu_panel, e, p, h - p + 2, &c_white);


    add_outline(cpu_panel, pos_reg[5] - 4, pos - hx - 3, (hx *3) - 2, 
              pos_reg[9] - pos_reg[5], &c_white);
    add_button(cpu_panel, pos_reg[5], pos - hx + 3, hx * 2, wx * 10, "POWER", "ON",
               &POWER, font10, &c_black, &c_white, 0);
    add_button(cpu_panel, pos_reg[8], pos -hx + 3, hx * 2, wx * 10, "POWER", "OFF",
               &POWER, font10, &c_white, &c_red, 0);
    add_outline(cpu_panel, pos_reg[5] - 6, h + 65, hx *3, pos_reg[9] - pos_reg[5],
                &c_black);
    add_button(cpu_panel, pos_reg[5], h + 70, hx * 2, wx * 10, "INTERRUPT", NULL,
               &INTR, font1, &c_white, &c_red, 0);
    add_button(cpu_panel, pos_reg[8], h + 70, hx * 2, wx * 10, "LOAD", NULL,
               &LOAD, font10, &c_white, &c_blue, 0);

    /* Add in status lights */
    add_lamp(cpu_panel, pos_reg[6], h + 80, "SYS", &cpu_2030.clock_start_lch, font1,
             LAMP_WHITE, &c_black);
    add_lamp(cpu_panel, pos_reg[6] + 30, h + 80, "MAN",
             &cpu_2030.allow_man_operation, font1, LAMP_WHITE, &c_black);
    add_lamp(cpu_panel, pos_reg[6] + 60, h + 80, "WAIT", &cpu_2030.wait, font1,
             LAMP_WHITE, &c_black);
    add_lamp(cpu_panel, pos_reg[6] + 90, h + 80, "TEST", &cpu_2030.test_mode,
             font1, LAMP_RED, &c_black);
    add_lamp(cpu_panel, pos_reg[6] + 120, h + 80, "LOAD", &cpu_2030.load_mode,
             font1, LAMP_WHITE, &c_black);

    /* Draw knobs */
    s = pos_reg[6];
    for (i = 0; i < 12; i++) {
        label.upper[i] = NULL;
        label.lower[i] = NULL;
    }

    label.upper[0] = "PROCESS";
    label.value[0] = 1;
    label.upper[1] = "ROS";
    label.lower[1] = "SCAN";
    label.value[1] = 2;
    label.value[2] = -1;
    label.value[10] = -1;
    label.upper[11] = "INHIBIT";
    label.lower[11] = "CF STOP";
    label.value[11] = 0;
    add_dial(cpu_panel, pos_reg[5] + 75, control_row + 50, 100, 150, 30, &label, &PROC_SW,
              1, 0, font1, &c_black);
    add_label(cpu_panel, pos_reg[5] + 75 - (5*wx), control_row, "ROS CONTROL",
              font10, &c_black);

    label.upper[0] = "PROCESS";
    label.value[0] = 1;
    label.upper[1] = "SINGLE CYCLE";
    label.lower[1] = NULL;
    label.value[1] = 2;
    label.value[2] = -1;
    label.value[10] = -1;
    label.upper[11] = "INSTR STEP";
    label.lower[11] = NULL;
    label.value[11] = 0;
    add_dial(cpu_panel, pos_reg[7] + 150, control_row + 50, 100, 175, 30, &label, &RATE_SW,
             1, 0, font1, &c_black);
    add_label(cpu_panel, pos_reg[7] + 150 - (2*wx) , control_row, "RATE",
              font10, &c_black);

    control_row += 90;
    label.upper[0] = "PROCESS";
    label.value[0] = 0;
    label.upper[1] = "SAR DELAYED";
    label.lower[1] = "STOP";
    label.value[1] = 1;
    label.upper[2] = "SAR STOP";
    label.value[2] = 2;
    label.upper[3] = NULL;
    label.upper[4] = "SAR RESTART";
    label.value[4] = 3;
    label.upper[5] = "ROAR RESTART";
    label.lower[5] = "STORE BYPASS";
    label.value[5] = 4;
    label.upper[6] = "ROAR";
    label.lower[6] = "RESTART";
    label.value[6] = 5;
    label.upper[7] = "ROAR RESTART";
    label.lower[7] = "WITHOUT RESET";
    label.value[7] = 6;
    label.upper[8] = "EARLY ROAR";
    label.lower[8] = "STOP";
    label.value[8] = 7;
    label.upper[9] = NULL;
    label.upper[10] = "ROAR STOP";
    label.value[10] = 8;
    label.upper[11] = "ROAR SYNC";
    label.value[11] = 9;
    add_dial(cpu_panel, pos_reg[5] + 75, control_row + 50, 150, 225, 40, &label, &MATCH_SW,
              0, 1, font1, &c_black);
    add_label(cpu_panel, pos_reg[5] + 75 - (7*wx), control_row, "ADDRESS COMPARE", font10, &c_black);


    for (i = 0; i < 12; i++) {
        label.upper[i] = NULL;
        label.lower[i] = NULL;
    }

    label.upper[0] = "PROCESS";
    label.value[0] = 2;
    label.upper[1] = "STOP";
    label.value[1] = 3;
    label.upper[2] = "RESTART";
    label.value[1] = 4;
    label.value[2] = -1;
    label.value[9] = -1;
    label.upper[10] = "DIAGNOSTIC";
    label.value[10] = 0;
    label.upper[11] = "DISABLE";
    label.value[11] = 1;
    add_dial(cpu_panel, pos_reg[7] + 150, control_row + 50, 150, 175, 30, &label, &CHK_SW,
              2, 0, font1, &c_black);
    add_label(cpu_panel, pos_reg[7] + 150 - (6*wx), control_row,
              "CHECK CONTROL", font10, &c_black);
}

