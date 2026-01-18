/*
 * microsim360 - 2050 Front panel display.
 *
 * Copyright 2022, Richard Cornwell
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
#include "cpu.h"
#include "model2050.h"
#include "rollers2050.h"
#include "area.h"
#include "button.h"
#include "roller.h"
#include "lamp.h"
#include "line.h"
#include "label.h"
#include "lamp_row.h"
#include "lamp_data.h"
#include "switch.h"
#include "hex_dial.h"
#include "dial.h"
#include "rollers.xpm"


lamp_row lamp_row1[] = {
    { NULL, NULL, "PASS", LAMP_WHITE, NULL, 0 },
    { NULL, NULL, "FAIL", LAMP_WHITE, NULL, 0 },
    { NULL, "BINARY", "TGR", LAMP_WHITE, NULL, 0 },
    { "TEST", "CNTR", "=0", LAMP_WHITE, NULL, 0 },
    { NULL, NULL, "0" , LAMP_WHITE, NULL, 0},
    { NULL, NULL, "1" , LAMP_WHITE, NULL, 0},
    { NULL, NULL, "2" , LAMP_WHITE, NULL, 0},
    { NULL, NULL, "3" , LAMP_WHITE, NULL, 0},
    { NULL, NULL, "4" , LAMP_WHITE, NULL, 0},
    { NULL, NULL, "5" , LAMP_WHITE, NULL, 0},
    { NULL, NULL, "4" , LAMP_WHITE, NULL, 0},
    { NULL, NULL, "2" , LAMP_WHITE, NULL, 0},
    { NULL, NULL, "1" , LAMP_WHITE, NULL, 0},
    { NULL, NULL, "1" , LAMP_WHITE, NULL, 0},
    { NULL, NULL, "2" , LAMP_WHITE, NULL, 0},
    { NULL, NULL, "3" , LAMP_WHITE, NULL, 0},
    { NULL, NULL, "4" , LAMP_WHITE, NULL, 0},
    { "FLT", "LOAD", "CHK" , LAMP_WHITE, NULL, 0},
    { NULL, "SUPV", "STAT" , LAMP_WHITE, NULL, 0},
    { "PROGRAM", "SCAN", "S2" , LAMP_WHITE, NULL, 0},
    { "SUPV", "ENABLE", "STOP" , LAMP_WHITE, NULL, 0},
    { NULL, "SEQ", "CNTR" , LAMP_WHITE, NULL, 0},
    { NULL, "MAIN", "STOP" , LAMP_WHITE, NULL, 0},
    { NULL, "ROS", "MODE" , LAMP_WHITE, NULL, 0},
    { "ALT", "PRE-", "VSE" , LAMP_WHITE, NULL, 0},
    { NULL, "HARD", "STOP" , LAMP_WHITE, NULL, 0},
    { NULL, "LOG", "TGR" , LAMP_WHITE, NULL, 0},
    { NULL, "BLOCK", "MODE" , LAMP_WHITE, NULL, 0},
    { NULL, "SINGLE", "CYCLE" , LAMP_WHITE, NULL, 0},
    { NULL, NULL, "CPU" , LAMP_WHITE, NULL, 0},
    { NULL, NULL, "CHAN" , LAMP_WHITE, NULL, 0},
    { NULL, NULL, "AUX" , LAMP_WHITE, NULL, 0},
    { NULL, "MAIN", "STOP" , LAMP_WHITE, NULL, 0},
    { "CHK","IRPT", "ENABLED" , LAMP_WHITE, NULL, 0},
    { "LINK", "REG", "STATE" , LAMP_WHITE, NULL, 0},
    { NULL, "CHK", "PEND" , LAMP_WHITE, NULL, 0},
    { NULL, NULL, NULL , LAMP_WHITE, NULL, 0},
};


static struct _labels sw_labels[] = {
    { "SYSTEM", "RESET" },
    { "PSW", "RESTART" },
    { "START", NULL },
    { "SET", "IC" },
    { "CHECK", "RESET" },
    { "STOP", NULL },
    { "INT TMR", NULL },
    { "STORE", NULL },
    { "LOG", "OUT" },
    { "DISPLAY", NULL },
    { "POWER", "ON" },
    { "POWER", "OFF" },
    { "INTERRUPT", NULL },
    { "LOAD", NULL },
    { "ENTER", NULL },
    { NULL, NULL }
};

static int roller_offset[36] = {
   /* 0  1   2   3   4   5   6   7   8   9 */
     30, 23, 23, 23, 23, 23, 23, 23, 22, 23,
   /*10  11  12  13  14  15  16  17  18  19*/
     23, 23, 23, 23, 23, 23, 23, 23, 40, 23,
   /*20  21  22  23  24  25  26  27  28  29*/
     23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
   /*30  31  32  33  34  35  */
     23, 23, 23, 23, 23, 23
};


static int switch_offset[33] = {
   /* 0  1   2   3   4   5   6   7 P 8 */
     54, 23, 23, 23, 23, 24, 23, 23, 45, 23,
   /* 9  10  11  12  13  14  15 P16  17  18  19*/
     23, 23, 23, 23, 23, 23, 63, 23, 23, 23,
   /*20  21  22  23  24  25  26  27  28  29*/
     23, 23, 23, 23, 45, 23, 23, 23, 23, 23,
   /*30  31  32  33  34  35  */
     23, 23, 23
};


void *
setup_fp2050(char *title)
{
    SDL_Rect rect, rect2;
    SDL_Surface *text;
    SDL_Texture *txt;
    SDL_Texture *roll;
    int      shf;
    int	     i, j, k;
    int      h, w;
    int      s, p, t, t2;
    int	     hx, wx;
    int      h1, w1;
    int      h2, w2;
    int      rw;           /* Roller width */
    int      rw2;
    int      pos;          /* Position on display */
    int      roffset;      /* Offset to Rollers */
    int      mark[40];
    int      pos_reg[20];
    char     buffer[40];
    dial_label label;
    Panel    cpu_panel;

    j = 0;

    /* Compute size of fonts */
    if (TTF_SizeText(font10, "M", &wx, &hx) != 0) {
        return NULL;
    }

    if (TTF_SizeText(font1, "M", &w1, &h1) != 0) {
        return NULL;
    }

    cpu_panel = create_window(title, 1100, 975, 0);
    if (cpu_panel == NULL) {
        return NULL;
    }

    text = IMG_ReadXPMFromArray(rollers_img);
    roll = SDL_CreateTextureFromSurface( cpu_panel->render, text);
    SDL_FreeSurface(text);
    SDL_QueryTexture(roll, NULL, NULL, &rw, NULL);

    /* Draw top of display */
    add_area(cpu_panel, 0, 0, 975, 1100, &c_label);

    /* Draw bottom switch panel */
    rw2 = 40;
    for (i = 0; i < 36; i++) {
        rw2 += roller_offset[i];
    }

    rect.x = 0;
    rect.y = 0;
    rect.w = rw;
    rect.h = 25;
    pos = 100;

    add_outline(cpu_panel, 10, pos, (hx * 8), 140, &c_white);
    add_label_center(cpu_panel, 10, pos, 140, "CHANNEL CONTROL", font10, &c_black);
    add_line(cpu_panel, 10, pos+hx, 140, &c_white);
    add_button(cpu_panel, 20, pos+(3*hx)+(hx/2), hx*2, wx*10, "ENTER", NULL,
      NULL, font10, &c_white, &c_blue, 1);
    add_mark(cpu_panel, 110, pos + (2*hx), (4*hx)+(hx/2), &c_black);
    add_label(cpu_panel, 120, pos+(2*hx)-(hx/2), "MPX", font1, &c_black);
    add_line(cpu_panel, 110, pos + (2*hx), wx, &c_black);
    add_label(cpu_panel, 120, pos+(4*hx), "OFF", font1, &c_black);
    add_line(cpu_panel, 110, pos + (4*hx)+(hx/2), wx, &c_black);
    add_label(cpu_panel, 120, pos+(6*hx), "SEL", font1, &c_black);
    add_line(cpu_panel, 110, pos + (6*hx)+(hx/2), wx, &c_black);
    add_switch_three(cpu_panel, 103, pos + (3*hx)+2, wx*2, &CHN_MODE, 0);
    add_label(cpu_panel, 90, pos + (hx*7), "MANUAL OP", font10, &c_black);
    add_line(cpu_panel, 10, pos+(hx*7), 140, &c_white);

    roffset = 180;

    add_roller(cpu_panel, roffset, pos, &rect, roll, 7, &roller_1, 36, roller_offset, LAMP_WHITE);
    rect.y += 8 * rect.h;
    pos += (rect.h * 3) + 25;


    /* Draw Selector channel knob */
    for (i = 0; i < 12; i++) {
        label.upper[i] = NULL;
        label.lower[i] = NULL;
        label.value[i] = -1;
    }

    label.upper[2] = "SC1";
    label.value[2] = 0;
    label.upper[3] = "SC2";
    label.value[3] = 1;
    label.upper[4] = "SC3";
    label.value[4] = 2;
    p = pos + (hx*2);
    add_dial(cpu_panel, 20 + (wx*11), p+(hx), 60, 120, 30, &label, &cpu_2050.SEL_CHAN_SEL, 0, 0, font1, &c_black);
    add_label_center(cpu_panel, wx*2, p-(h1/2)-2, wx*8, "SELECTOR", font1, &c_black);
    add_label_center(cpu_panel, wx*2, p, wx*8, "CHANNEL", font1, &c_black);
    add_label_center(cpu_panel, wx*2, p+(h1/2)+2, wx*8, "DISPLAY", font1, &c_black);


    add_roller(cpu_panel, roffset, pos, &rect, roll, 7, &roller_2, 36, roller_offset, LAMP_WHITE);
    rect.y += 8 * rect.h;
    pos += (rect.h * 3) + 25;
    add_roller(cpu_panel, roffset, pos, &rect, roll, 7, &roller_3, 36, roller_offset, LAMP_WHITE);
    rect.y += 8 * rect.h;
    pos += (rect.h * 3) + 25;
    add_roller(cpu_panel, roffset, pos, &rect, roll, 7, &roller_4, 36, roller_offset, LAMP_WHITE);
    pos += (rect.h * 3);
    add_area(cpu_panel, roffset, pos, hx, rw2, &c_black);
    h = hx*4 + (hx/2);
    add_outline(cpu_panel, roffset, pos, h, rw2, &c_black);
    p = roffset - 6;
    for (i = 0; i < sizeof(lamp_row1) / sizeof(lamp_row); i++) {
        p += roller_offset[i];
        switch (i) {
        case 4:
               add_mark(cpu_panel, p, pos, h,  &c_black);
               s = p;
               break;
        case 10:
               add_mark(cpu_panel, p, pos, h,  &c_black);
               add_label_center(cpu_panel, s, pos, p - s, "FLT OP REG", font10, &c_white);
               s = p;
               break;
        case 13:
               add_mark(cpu_panel, p, pos, h,  &c_black);
               add_label_center(cpu_panel, s, pos, p - s, "SEQ CNTR", font10, &c_white);
               s = p;
               break;
        case 15:
               add_mark(cpu_panel, p, pos, h,  &c_black);
               add_label_center(cpu_panel, s, pos, p - s, "SEQ STAT", font10, &c_white);
               s = p;
               break;
        case 17:
               add_mark(cpu_panel, p, pos, h,  &c_black);
               s = p;
               break;
        case 21:
               add_mark(cpu_panel, p, pos, h,  &c_black);
               s = p;
               break;
        case 24:
               add_mark(cpu_panel, p, pos, h,  &c_black);
               add_label_center(cpu_panel, s, pos, p - s, "MODE", font10, &c_white);
               s = p;
               break;
        case 29:
               add_mark(cpu_panel, p, pos, h,  &c_black);
               s = p;
               break;
        case 33:
               add_mark(cpu_panel, p, pos, h,  &c_black);
               add_label_center(cpu_panel, s, pos, p - s, "CLOCK", font10, &c_white);
               s = p;
               break;
        }
    }
    pos += hx;
    add_lamp_row(cpu_panel, roffset, pos, lamp_row1, sizeof(lamp_row1) / sizeof(lamp_row), roller_offset, font0, &c_black);
    pos += (rect.h * 2);
    add_area(cpu_panel, roffset, pos, hx, rw2, &c_black);
    h = hx*4 + (hx/2);
    add_outline(cpu_panel, roffset, pos, (hx * 8) + (hx/2), rw2, &c_black);
    add_label_center(cpu_panel, roffset, pos, rw2, "STORAGE DATA", font10, &c_white);
    pos += hx + (hx/2);
    t = pos + (hx/2);
    add_lamp_data(cpu_panel, roffset, pos, &cpu_2050.SDR_REG, 31, roller_offset, LAMP_WHITE, font0, &c_black);
    pos += 2*hx;
    p = roffset - 6;
    add_area(cpu_panel, roffset, pos, hx, rw2, &c_white);
    for (i = 0; i < 36; i++) {
        int   t3, t4;
        p += roller_offset[i];
        switch (i) {
        case 0:
              s = p;
              t = p + 3;
              add_label(cpu_panel, s+7, pos + hx, "P", font0, &c_black);
              break;
        case 7:
              add_area(cpu_panel, roffset+7, pos + hx+h1+1, 8, p - 140, &c_black);
              add_label(cpu_panel, roffset+15, pos + hx+h1+1, "DATA BUS", font0, &c_white);
              break;

        case 9:
               add_mark(cpu_panel, p, pos-(hx/2), hx*3,  &c_white);
               add_label_center(cpu_panel, s, pos, p - s, "BYTE 0", font10, &c_black);
               add_label(cpu_panel, s+7, pos + hx, "P", font0, &c_black);
               s = p;
               break;
        case 11:
               t4 = p;
               break;
        case 17:
               break;
        case 18:
               add_label_center(cpu_panel, p, pos + (3*hx) - (hx/2), roller_offset[i+1], "DATA", font10, &c_black);
               add_mark(cpu_panel, p, pos-(hx/2), hx*3,  &c_white);
               add_label_center(cpu_panel, s, pos, p - s, "BYTE 1", font10, &c_black);
               add_label(cpu_panel, s+7, pos + hx, "P", font0, &c_black);
               s = p;
               t3 = p;
               break;
        case 24:
               t = p;
               break;
        case 27:
               add_mark(cpu_panel, p, pos-(hx/2), hx*3,  &c_white);
               add_label_center(cpu_panel, s, pos, p - s, "BYTE 2", font10, &c_black);
               add_label(cpu_panel, s+7, pos + hx, "P", font0, &c_black);
               s = p;
               t2 = p;
               break;
        case 35:
               add_label_center(cpu_panel, s, pos, p - s, "BYTE 3", font10, &c_black);
               add_area(cpu_panel, t-8, pos + hx+h1, 9, p - t + 23, &c_black);
               add_label(cpu_panel, t2-2, pos+hx+h1, "ROS ADDRESS", font0, &c_white);
               add_area(cpu_panel, t4-10, pos + (4*hx)-5, 8, p - t4 + 23, &c_white);
               add_label(cpu_panel, t3-10, pos+(4*hx)-5, "SAR COMPARE", font0, &c_black);
               s = p;
               break;
        }
    }
    pos += hx;
    s = roffset;
    add_switch_on_off(cpu_panel, s, pos + hx, wx*2, NULL, 0);
    for (i = 31; i >=0; i--) {
        s += switch_offset[31-i];
        add_switch_on_off(cpu_panel, s, pos + hx, wx*2, &cpu_2050.DKEYS, i);
        sprintf(buffer, "%d", 31-i);
        if (i < 20) {
            add_label(cpu_panel, s+5, pos, buffer, font0, &c_black);
        } else {
            add_label(cpu_panel, s-1, pos, buffer, font0, &c_black);
        }
    }

    h = hx*4 + (hx/2);
    pos += (4*hx) + (hx/2);
    add_area(cpu_panel, roffset, pos, hx, rw2, &c_black);
    add_outline(cpu_panel, roffset, pos, hx * 8, rw2, &c_black);
    add_label_center(cpu_panel, roffset, pos, rw2, "INSTRUCTION ADDRESS REGISTER", font10, &c_white);
    pos += hx + (hx/2);
    t = pos + (hx/2);
    add_lamp_data(cpu_panel, roffset, pos, &cpu_2050.SAR_REG, 23, roller_offset, LAMP_WHITE, font0, &c_black);
    pos += hx + (hx/2);
    p = roffset - 6;
    for (i = 0; i < 36; i++) {
        int   t3;
        p += roller_offset[i];
        switch (i) {
        case 9:
               add_label(cpu_panel, p+7, pos + hx, "P", font0, &c_black);
               break;
        case 18:
               add_label(cpu_panel, p+7, pos + hx, "P", font0, &c_black);
               add_label_center(cpu_panel, p, pos + (3*hx) - (hx/2), roller_offset[i+1], "ADDRESS", font10, &c_black);
               break;
        case 27:
               add_label(cpu_panel, p+7, pos + hx, "P", font0, &c_black);
               break;
        }
    }

    /* Draw knobs */
    for (i = 0; i < 12; i++) {
        label.upper[i] = NULL;
        label.lower[i] = NULL;
        label.value[i] = -1;
    }

    label.upper[1] = "PROTECT 8-20";
    label.lower[1] = "F REG";
    label.value[1] = 2;
    label.upper[3] = "MPX 22-31";
    label.lower[3] = "SDR";
    label.value[3] = 3;
    label.upper[11] = "8-23 MAIN";
    label.lower[11] = "SDR";
    label.value[11] = 0;
    label.upper[9] = "22-27 LOCAL";
    label.lower[9] = "L REG";
    label.value[9] = 1;
    add_dial(cpu_panel, roffset+110, pos+40, 100, 175, 30, &label, &E_SW,
              0, 0, font1, &c_black);
    add_label_center(cpu_panel, roffset+20, pos, 200, "STORAGE SELECT",
              font10, &c_black);

    pos += hx;
    s = roffset;
    for (i = 31; i >=0; i--) {
        s += switch_offset[31-i];
        if (i <= 23) {
            add_switch_on_off(cpu_panel, s, pos + hx, wx*2, &cpu_2050.AKEYS, i);
            sprintf(buffer, "%d", 31-i);
            if (i < 20) {
                add_label(cpu_panel, s+5, pos, buffer, font0, &c_black);
            } else {
                add_label(cpu_panel, s-1, pos, buffer, font0, &c_black);
            }
        }
    }

   pos += 6*hx;



    s = 30;
    add_outline(cpu_panel, s-wx, pos, 8*hx, 70*wx, &c_black);
    add_line(cpu_panel, s-wx, pos + hx, 70*wx, &c_black);
    add_line(cpu_panel, s-wx, pos + (6*hx), 70*wx, &c_black);
    /* IAR Compare */
    add_label_center(cpu_panel, s, pos, 14*wx, "IAR", font1, &c_black);
    add_mark(cpu_panel, s+wx+6, pos + (1*hx) + h1, (3*hx), &c_black);
    add_label(cpu_panel, s+wx+8 + wx, pos+ (1*hx) + (h1), "SYNC", font0, &c_black);
    add_line(cpu_panel, s+wx+6, pos + (1*hx) + (h1/2), wx, &c_black);
    add_label(cpu_panel, s+wx+8 + wx, pos+(5*hx)-(h1/2), "STOP", font0, &c_black);
    add_line(cpu_panel, s+wx+6, pos + (5*hx), wx, &c_black);
    add_switch_three(cpu_panel, s+wx, pos + (2*hx), wx*2, NULL, 0);
    add_label_center(cpu_panel, s, pos + (6*hx), 7 * wx, "ADDRESS", font1, &c_black);
    add_label_center(cpu_panel, s, pos + (6*hx) + h1, 7 * wx, "COMPARE", font1, &c_black);
    s += 7*wx;
    add_mark(cpu_panel, s, pos + hx, 7*hx, &c_black);
    add_mark(cpu_panel, s+wx+6, pos + (4*hx), (1*hx), &c_black);
    add_label(cpu_panel, s+wx+6+wx, pos+(5*hx)-(h1/2), "RPT", font0, &c_black);
    add_line(cpu_panel, s+wx+6, pos + (5*hx), wx, &c_black);
    add_switch_on_off(cpu_panel, s+wx, pos + (2*hx), wx*2, NULL, 0);
    add_label_center(cpu_panel, s, pos + (6*hx), 7*wx, "REPEAT", font1, &c_black);
    add_label_center(cpu_panel, s, pos + (6*hx) + h1, 7*wx, "INSN", font1, &c_black);

    s += 7*wx;
    /* ROS Compare */
    add_mark(cpu_panel, s, pos, 8*hx, &c_black);
    add_label_center(cpu_panel, s, pos, 14*wx, "ROS", font1, &c_black);
    add_mark(cpu_panel, s+wx +6, pos + (4*hx), 1*hx, &c_black);
    add_label(cpu_panel, s+wx+8+wx, pos+(5*hx)-(h1/2), "STOP", font0, &c_black);
    add_line(cpu_panel, s+wx+6, pos + (5*hx), wx, &c_black);
    add_switch_three(cpu_panel, s+wx, pos + (2*hx), wx*2, NULL, 0);
    add_label_center(cpu_panel, s, pos + (6*hx), 7*wx,  "ADDRESS", font1, &c_black);
    add_label_center(cpu_panel, s, pos + (6*hx) + h1, 7*wx,  "COMPARE", font1, &c_black);
    s += 7*wx;
    add_mark(cpu_panel, s, pos+hx, 7*hx, &c_black);
    add_switch_three(cpu_panel, s+wx, pos + (2*hx), wx*2, NULL, 0);
    add_label_center(cpu_panel, s, pos + (6*hx), 7*wx, "REPEAT", font1, &c_black);
    add_label_center(cpu_panel, s, pos + (6*hx) + h1, 7*wx, "INSN", font1, &c_black);
    s += 7*wx;
    /* Unused */
    add_mark(cpu_panel, s, pos, 8*hx, &c_black);
    add_switch_three(cpu_panel, s+wx, pos + (2*hx), wx*2, NULL, 0);
    s += 7*wx;
    /* SAR Compare */
    add_mark(cpu_panel, s, pos, 8*hx, &c_black);
    add_mark(cpu_panel, s+wx+6, pos + (4*hx), (1*hx), &c_black);
    add_label(cpu_panel, s+wx+8+wx, pos+(5*hx)-(h1/2), "STOP", font0, &c_black);
    add_line(cpu_panel, s+wx+6, pos + (5*hx), wx, &c_black);
    add_switch_on_off(cpu_panel, s+wx, pos + (2*hx), wx*2, NULL, 0);
    add_label_center(cpu_panel, s, pos + (6*hx), 7*wx, "SAR", font1, &c_black);
    add_label_center(cpu_panel, s, pos + (6*hx) + h1, 7*wx, "COMPARE", font1, &c_black);
    s += 7*wx;
    /* Disable interval timer */
    add_mark(cpu_panel, s, pos, 8*hx, &c_black);
    add_switch_on_off(cpu_panel, s+wx, pos + (2*hx), wx*2, NULL, 0);
    add_label_center(cpu_panel, s, pos + (6*hx), 7*wx, "DISABLE", font1, &c_black);
    add_label_center(cpu_panel, s, pos + (6*hx) + (h1/2), 7*wx, "INTERVAL", font1, &c_black);
    add_label_center(cpu_panel, s, pos + (7*hx), 7*wx, "TIMER", font1, &c_black);
    s += 7*wx;
    /* Lamp test */
    add_mark(cpu_panel, s, pos, 8*hx, &c_black);
    add_switch_momentary(cpu_panel, s+wx, pos + (2*hx), wx*2, &LAMP_TEST, 0);
    add_label_center(cpu_panel, s, pos + (6*hx), 7*wx, "LAMP", font1, &c_black);
    add_label_center(cpu_panel, s, pos + (7*hx), 7*wx, "TEST", font1, &c_black);
    s += 7*wx;
    /* Force indicator */
    add_mark(cpu_panel, s, pos, 8*hx, &c_black);
    add_switch_momentary(cpu_panel, s+wx, pos + (2*hx), wx*2, NULL, 0);
    add_label_center(cpu_panel, s, pos + (6*hx), 7*wx, "FORCE", font1, &c_black);
    add_label_center(cpu_panel, s, pos + (7*hx), 7*wx, "INDICATOR", font1, &c_black);
    s += 7*wx;
    /* FLT Mode */
    add_mark(cpu_panel, s, pos, 8*hx, &c_black);
    add_switch_three(cpu_panel, s+wx, pos + (2*hx), wx*2, NULL, 0);
    add_label_center(cpu_panel, s, pos + (6*hx), 7*wx, "FTL", font1, &c_black);
    add_label_center(cpu_panel, s, pos + (7*hx), 7*wx, "MODE", font1, &c_black);

    s += 7*wx;
    t = pos;
    /* Draw knobs */
    for (i = 0; i < 12; i++) {
        label.upper[i] = NULL;
        label.lower[i] = NULL;
        label.value[i] = -1;
    }
    label.upper[0] = "PROCESS";
    label.value[0] = 1;
    label.upper[1] = "AUTO";
    label.lower[1] = "REREAD";
    label.value[1] = 0;
    label.upper[5] = "HALT";
    label.lower[5] = "AFTER";
    label.value[5] = 2;
    label.upper[7] = "STOP";
    label.value[7] = 3;
    label.upper[11] = "REPEAT";
    label.value[11] = 2;
    add_dial(cpu_panel, s+60, t+(2*hx), 50, 125, 30, &label, &PROC_SW, 1, 0, font1, &c_black);
    add_label(cpu_panel, s+50-(5*wx), t - hx - (hx/2), "FLT CONTROL", font10, &c_black);

    for (i = 0; i < 12; i++) {
        label.upper[i] = NULL;
        label.lower[i] = NULL;
        label.value[i] = -1;
    }
    label.upper[0] = "PROCESS";
    label.value[0] = 1;
    label.upper[1] = "SINGLE";
    label.lower[1] = "CYCLE";
    label.value[1] = 2;
    label.upper[11] = "INSN";
    label.lower[11] = "STEP";
    label.value[11] = 0;
    add_dial(cpu_panel, s+60, pos + (hx*3) + 70, 100, 100, 25, &label, &RATE_SW, 1, 0, font1, &c_black);
    add_label(cpu_panel, s+60-(2*wx), pos + (hx*5), "RATE", font10, &c_black);
    add_outline(cpu_panel, s+5, pos + (hx*5) - 5, (hx*9), 110, &c_white);

    p = pos + (5 * hx);

    add_button(cpu_panel, s + 25, p + (hx * 6), hx * 2, wx * 10, "START", NULL,
               &START, font10, &c_white, &c_blue, 0);

    s += 125;
    add_outline(cpu_panel, s-5, p-5, hx * 9, (wx * 35)+10, &c_white);
   /* Draw bottom switch panel */
    add_button(cpu_panel, s, p, hx * 2, wx * 10, "SYSTEM", "RESET",
               &SYS_RST, font10, &c_white, &c_blue, 0);
    add_button(cpu_panel, s, p + (hx * 3), hx * 2, wx * 10, "SET IC", NULL,
               &SET_IC, font10, &c_white, &c_blue, 0);
    add_button(cpu_panel, s, p + (hx * 6), hx * 2, wx * 10, "STOP", NULL,
               &STOP, font10, &c_white, &c_red, 0);

    s += wx * 12;
    for (i = 0; i < 12; i++) {
        label.upper[i] = NULL;
        label.lower[i] = NULL;
        label.value[i] = -1;
    }
    label.upper[0] = "PROCESS";
    label.value[0] = 1;
    label.upper[1] = "DISABLE";
    label.value[1] = 0;
    label.upper[5] = "STOP";
    label.value[5] = 2;
    label.upper[7] = "CHAN";
    label.lower[7] = "STOP";
    label.value[7] = 3;
    add_dial(cpu_panel, s+(6*wx), t + (2*hx), 50, 120, 30, &label, &CHK_SW, 1, 0, font1, &c_black);
    add_label(cpu_panel, s, t - hx - (hx/2), "CHECK CONTROL", font10, &c_black);

    add_button(cpu_panel, s, p, hx * 2, wx * 10, "PSW", "RESTART",
               NULL, font10, &c_white, &c_blue, 0);
    add_button(cpu_panel, s, p + (hx * 3), hx * 2, wx * 10, "STORE", NULL,
               &STORE, font10, &c_white, &c_blue, 0);
    add_blank(cpu_panel, s, p + (hx * 6), hx * 2, wx * 10, &c_white);

    s += wx * 12;
    add_button(cpu_panel, s, p, hx * 2, wx * 10, "CHECK", "RESET",
              NULL, font10, &c_white, &c_blue, 0);
    add_button(cpu_panel, s, p + (hx * 3), hx * 2, wx * 10, "DISPLAY", NULL,
               &DISPLAY, font10, &c_white, &c_blue, 0);
    add_button(cpu_panel, s, p + (hx * 6), hx * 2, wx * 10, "LOG", "OUT",
               NULL, font10, &c_white, &c_blue, 0);


    s = 800;
    pos_reg[0] = s;
    add_hex_dial(cpu_panel, s, p-20, &A_SW);
    s += 100;
    pos_reg[1] = s;
    add_hex_dial(cpu_panel, s, p-20, &B_SW);
    s += 100;
    pos_reg[2] = s;
    add_hex_dial(cpu_panel, s, p-20, &C_SW);
    pos_reg[3] = s + (wx * 12);
    h = pos + (5 * hx);

    add_outline(cpu_panel, pos_reg[0] - 6, pos - 15, (hx*2) + 10,
              pos_reg[3] - pos_reg[0], &c_white);
    add_button(cpu_panel, pos_reg[0], pos - hx + 3, hx * 2, wx * 10, "POWER", "ON",
               &POWER, font10, &c_black, &c_white, 0);
    add_button(cpu_panel, pos_reg[2], pos -hx + 3, hx * 2, wx * 10, "POWER", "OFF",
               &POWER, font10, &c_white, &c_red, 0);
    add_outline(cpu_panel, pos_reg[0] - 6, p + (6*hx) - 4, (hx *2) + 10, pos_reg[3] - pos_reg[0],
                &c_black);
    add_button(cpu_panel, pos_reg[0], p+(6*hx), hx * 2, wx * 10, "INTERRUPT", NULL,
               &INTR, font1, &c_white, &c_red, 0);
    add_button(cpu_panel, pos_reg[2], p+(6*hx), hx * 2, wx * 10, "LOAD", NULL,
               &LOAD, font10, &c_white, &c_blue, 0);

    /* Add in status lights */
    s = pos_reg[0] + (wx * 12);
    add_lamp(cpu_panel, s, p + (7*hx), "SYS", &cpu_2050.clock_start_lch, font1,
             LAMP_WHITE, &c_black);
    add_lamp(cpu_panel, s + 25, p + (7*hx), "MAN",
             &cpu_2050.allow_man_operation, font1, LAMP_WHITE, &c_black);
    add_lamp(cpu_panel, s + 50, p + (7*hx), "WAIT", &cpu_2050.wait, font1,
             LAMP_WHITE, &c_black);
    add_lamp(cpu_panel, s + 75, p + (7*hx), "TEST", &cpu_2050.test_mode,
             font1, LAMP_RED, &c_black);
    add_lamp(cpu_panel, s + 100, p + (7*hx), "LOAD", &cpu_2050.load_mode,
             font1, LAMP_WHITE, &c_black);

   return (void *)cpu_panel;

}

