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

#include "panel.h"
#include "model2030.h"

#define DIG_LP   17
#define DIG_CD   18
#define DIG_CC   19
#define DIG_SLI  20
#define DIG_SKP  21
#define DIG_PCI  22
#define DIG_OP   23

static struct _labels labels[] = {
    { "0", NULL },
    { "1", NULL },
    { "2", NULL },
    { "3", NULL },
    { "4", NULL },
    { "5", NULL },
    { "6", NULL },
    { "7", NULL },
    { "8", NULL },
    { "9", NULL },
    { "A", NULL },
    { "B", NULL },
    { "C", NULL },
    { "D", NULL },
    { "E", NULL },
    { "F", NULL },
    { "P", NULL },
    { "LP", NULL },
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
    { "MAIN STOR", NULL },
    { "AUX STOR", NULL },
    { "EX", NULL },
    { "MATCH", NULL },
    { "ALLOW", "WRITE" },
    { "1050", "INTV" },
    { "1050", "REQ" },
    { "MPX", "CHNL" },
    { "SEL" "CHNL" },
    { "COMP", "MODE" },
    { "STOR", "ADR" },
    { "STOR", "DATA" },
    { "A", "REG" },
    { "B", "REG" },
    { "ALU", NULL },
    { "ROS", "ADR" },
    { "ROS", "SALS" },
    { "CTRL", "REG" },
    { NULL, NULL }
};


static struct _labels sw_labels[] = {
    { "SYSTEM", "RESET" },
    { "ROAR", "RESET" },
    { "START", NULL },
    { "SET", "IC" },
    { "CHECK", "RESET" },
    { "STOP", NULL },
    { "INT TMR", NULL },
    { "STORE", NULL },
    { "LAMP", "TEST" },
    { "DISPLAY", NULL },
    { "POWER", "ON" },
    { "POWER", "OFF" },
    { "INTERRUPT", NULL },
    { "LOAD", NULL },
    { NULL, NULL }
};

                   /*            1         2           */
                   /*  0123456789012345678901234567890 */
static char *row1 =    "P012345  P L P18421  P84218421";
static char *row1a=    "             P34567  P01234567";
static int   pos1[32];
                   /*            1         2           */
                   /*  0123456789012345678901234567890 */
static char *row2 =    "P0123 0123 A0123 0101201AP0123";
static int   pos2[32];
                   /*            1         2           */
                   /*  0123456789012345678901234567890 */
static char *row3 =    "P 0123      012  01 01012A0123";
static int   pos3[32];
static char *row_cnt = "     P84218421 P84217421      ";
static char *row_cnta= "     P01234576 P01234576      ";
static int   pos_cnt[32];

static char *chan_one = "     P84218421 P8421 8421     ";
static char *chan_onea= "     P01234576 P0123 4567     ";
static int   pos_chan1[32];
static int   pos_chan2[32];
static char *chan_two = "     P84218421 P8421 8421     ";
static char *chan_twoa= "     P01234576 P0123 4567     ";

static char *store_addr = "P84218421 P84218421 ";
static char *store_addra= "P01234567 P01234567 ";
static char *data_reg = "P84218421  84218421 ";
static int pos_mpx[32];
static int pos_store[32];
static int pos_main[32];
static int pos_data[32];
static int pos_breg[32];

void
setup_fp2030(SDL_Renderer *render)
{
    SDL_Rect rect, rect2;
    SDL_Surface *text;
    SDL_Texture *txt;
    int      shf;
    int	     i, j, k;
    int      h, w;
    int      wb;
    int	     hx, wx;
    int      h2, w2;
    Uint32   f;

    f1_hd = 0;
    f1_wd = 0;
    j = 0;

    /* Create textures for labels and buttons */
    for (i = 0; labels[i].upper != NULL; i++) {
       if ((i >= 32 && i <= 37) || (i >= 48 && i <= 55) ) {
          text = TTF_RenderText_Shaded(font1, labels[i].upper, c5o, cl);
       } else {
          text = TTF_RenderText_Shaded(font1, labels[i].upper, cof, cl);
       }
       digit_off[i] = SDL_CreateTextureFromSurface( render, text);
       SDL_FreeSurface(text);
       if (labels[i].lower != NULL) {
           if ((i >= 32 && i <= 37) || (i >= 48 && i <= 55) ) {
               text = TTF_RenderText_Shaded(font1, labels[i].lower, c5o, cl);
           } else {
               text = TTF_RenderText_Shaded(font1, labels[i].lower, cof, cl);
           }
           digit2_off[i] = SDL_CreateTextureFromSurface( render, text);
           SDL_FreeSurface(text);
       } else {
           digit2_off[i] = NULL;
       }
       if ((i >= 32 && i <= 37) || (i >= 48 && i <= 55) ) {
           text = TTF_RenderText_Shaded(font1, labels[i].upper, c5, cl);
       } else {
           text = TTF_RenderText_Shaded(font1, labels[i].upper, con, cl);
       }
       digit_on[i] = SDL_CreateTextureFromSurface( render, text);
       SDL_FreeSurface(text);
       if (labels[i].lower != NULL) {
           if ((i >= 32 && i <= 37) || (i >= 48 && i <= 55) ) {
               text = TTF_RenderText_Shaded(font1, labels[i].lower, c5, cl);
           } else {
               text = TTF_RenderText_Shaded(font1, labels[i].lower, con, cl);
           }
           digit2_on[i] = SDL_CreateTextureFromSurface( render, text);
           SDL_FreeSurface(text);
       } else {
           digit2_on[i] = NULL;
       }
       if (strlen(labels[i].upper) == 1) {
           SDL_QueryTexture(digit_on[i], &f, &k, &w, &h);
           if (w > f1_wd)
              f1_wd = w;
           if (h > f1_hd)
              f1_hd = h;
       }
    }

    text = TTF_RenderText_Shaded(font1, "M", c, cb);
    txt = SDL_CreateTextureFromSurface( render, text);
    SDL_QueryTexture(txt, &f, &k, &w2, &h2);
    SDL_FreeSurface(text);
    text = TTF_RenderText_Shaded(font1, "ON", c1, c);
    on = SDL_CreateTextureFromSurface( render, text);
    SDL_FreeSurface(text);
    text = TTF_RenderText_Shaded(font1, "OFF", c1, c);
    off = SDL_CreateTextureFromSurface( render, text);
    SDL_FreeSurface(text);
    wb = (strlen(row1) + 1) * (3 * f1_wd);

    /* Draw top of display */
    ADD_AREA(0, 0, 975, 1100, &cc);

    /* Draw bottom switch panel */
    ADD_AREA(0, (66 * h2) - (f1_hd/4), (31 * h2), 1100, &cl);

    /* Draw top box */
    ADD_AREA(10, 10, h2 + 10, wb + 40, &cb);

    ADD_AREA(10, 10 + h2, (h2 * 12) + 10, 10, &cb);

    ADD_AREA(10+(wb+30), 10 + h2, (h2 * 12) + 10, 10, &cb);
    ADD_LABEL(10, 10 + (h2/2), wb, "READ ONLY STORAGE", c, cb)

    /* Draw ROS boxes */
    ADD_AREA(30, 10 + (h2 * 2), h2 * 4, wb, &cl);
    ADD_LINE(30, 10 + (h2 * 3), wb, c);
    rect.x = 50 + f1_wd;
    rect.y = 15 + (h2 * 3);
    rect.h = f1_hd;
    rect.w = f1_wd;
    ros_ptr = 0;
    shf = 23;
    for (i = 0; row1[i] != '\0'; i++) {
        pos1[i] = rect.x;
        switch (row1[i]) {
        case 'L': j = DIG_LP; pos1[i] += f1_wd/2; break;
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': pos1[i] = rect.x + f1_wd + (f1_wd/2); goto next_row1;
        }
        ros_bits[ros_ptr].rect.x = pos1[i];
        ros_bits[ros_ptr].rect.y = rect.y;
        ros_bits[ros_ptr].rect.h = f1_hd;
        ros_bits[ros_ptr].rect.w = f1_wd;
        ros_bits[ros_ptr].digit_on = digit_on[j];
        ros_bits[ros_ptr].digit_off = digit_off[j];
        ros_bits[ros_ptr].shift = shf;
        ros_bits[ros_ptr].row = 0;
        if (j == DIG_LP) {
           ros_bits[ros_ptr].rect.x -= f1_wd/2;
           ros_bits[ros_ptr].rect.w = 2*f1_wd;
        }
        shf --;
        ros_ptr++;
next_row1:
        rect.x += 3*f1_wd;
        switch (row1a[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': continue;
        }
        ctl_label[ctl_ptr].rect.x = pos1[i];
        ctl_label[ctl_ptr].rect.y = rect.y + f1_hd;
        ctl_label[ctl_ptr].rect.h = f1_hd;
        ctl_label[ctl_ptr].rect.w = f1_wd;
        ctl_label[ctl_ptr].text = digit_off[j];
        ctl_ptr++;
    }
    rect.y = 15 + (h2 * 3);

    ADD_LABEL1(pos1[3] + f1_wd, 10 + (h2 * 2), "CN");
    ADD_LABEL1(pos1[9] - f1_wd, 10 + (h2 * 2), "ADR");
    ADD_LABEL1(pos1[14] - f1_wd, 10 + (h2 * 2), "W REGISTER");
    ADD_LABEL1(pos1[23] + f1_wd, 10 + (h2 * 2), "X REGISTER");
    ADD_MARK(pos1[1] - f1_wd, rect.y, f1_hd*2, c);
    ADD_MARK(pos1[7], rect.y, f1_hd*2, c);
    ADD_MARK(pos1[11] - f1_wd, rect.y, f1_hd*2, c);
    ADD_MARK(pos1[12] - f1_wd, rect.y, f1_hd*2, c);
    ADD_MARK(pos1[14] - f1_wd, rect.y, f1_hd*2, c);
    ADD_MARK(pos1[19] - f1_wd, rect.y, f1_hd*2, c);
    ADD_MARK(pos1[22] - f1_wd, rect.y, f1_hd*2, c);
    ADD_MARK(pos1[26] - f1_wd, rect.y, f1_hd*2, c);
    ADD_MARK(pos1[7], rect.y - h2 - (h2/4), f1_hd - (h2/4), c);
    ADD_MARK(pos1[11] - f1_wd, rect.y - h2 - (h2/4), f1_hd - (h2/4), c);
    ADD_MARK(pos1[12] - f1_wd, rect.y - h2 - (h2/4), f1_hd - (h2/4), c);
    ADD_MARK(pos1[19] - f1_wd, rect.y - h2 - (h2/4), f1_hd - (h2/4), c);

    /* Second ROS row */
    ADD_AREA(30, 10 +(h2 * 7), h2 * 3, wb, &cl);
    ADD_LINE(30, 10 + (h2 * 8), wb, c);

    rect.x = 30 + f1_wd;
    rect.y = 15 + (h2 * 8);
    rect.h = f1_hd;
    rect.w = f1_wd;
    shf = 26;
    for (i = 0; row2[i] != '\0'; i++) {
        pos2[i] = rect.x;
        switch (row2[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': pos2[i] = rect.x + f1_wd + (f1_wd/2); rect.x += 3*f1_wd; continue;
        }
        ros_bits[ros_ptr].rect.x = rect.x;
        ros_bits[ros_ptr].rect.y = rect.y;
        ros_bits[ros_ptr].rect.h = f1_hd;
        ros_bits[ros_ptr].rect.w = f1_wd;
        ros_bits[ros_ptr].digit_on = digit_on[j];
        ros_bits[ros_ptr].digit_off = digit_off[j];
        ros_bits[ros_ptr].shift = shf;
        ros_bits[ros_ptr].row = 1;
        ros_ptr++;
        rect.x = pos2[i] + 3*f1_wd;
        shf--;
    }
             /*            1         2           */
             /*  0123456789012345678901234567890 */
             /* "P0123 0123 A0123 0101201AP0123" */
             /* SA CH   CL   CA   CB CMCU  CK */

    ADD_LABEL1(pos2[0] - (f1_wd / 2), 10 + (h2 * 7), "SA");
    ADD_LABEL1(pos2[3] - (f1_wd*2), 10 + (h2 * 7), "CH");
    ADD_LABEL1(pos2[8] - (f1_wd*2), 10 + (h2 * 7), "CL");
    ADD_LABEL1(pos2[13] - f1_wd, 10 + (h2 * 7), "CA");
    ADD_LABEL1(pos2[17] + f1_wd, 10 + (h2 * 7), "CB");
    ADD_LABEL1(pos2[20] - f1_wd, 10 + (h2 * 7), "CM");
    ADD_LABEL1(pos2[22] + f1_wd, 10 + (h2 * 7), "CU");
    ADD_LABEL1(pos2[26] + f1_wd, 10 + (h2 * 7), "CK");

    ADD_MARK(pos2[1] - f1_wd, rect.y, f1_hd, c);
    ADD_MARK(pos2[5] - f1_wd, rect.y, f1_hd, c);
    ADD_MARK(pos2[10] - f1_wd, rect.y, f1_hd, c);
    ADD_MARK(pos2[16],  rect.y, f1_hd, c);
    ADD_MARK(pos2[19] - f1_wd, rect.y, f1_hd, c);
    ADD_MARK(pos2[22] - f1_wd, rect.y, f1_hd, c);
    ADD_MARK(pos2[24] - f1_wd, rect.y, f1_hd, c);
    ADD_MARK(pos2[25] - f1_wd, rect.y, f1_hd, c);
    ADD_MARK(pos2[26] - f1_wd, rect.y, f1_hd, c);

    ADD_MARK(pos2[1] - f1_wd, rect.y - h2 - (h2/4), f1_hd - (h2/4), c);
    ADD_MARK(pos2[5] - f1_wd, rect.y - h2 - (h2/4), f1_hd - (h2/4), c);
    ADD_MARK(pos2[10] - f1_wd, rect.y - h2 - (h2/4), f1_hd - (h2/4), c);
    ADD_MARK(pos2[16], rect.y - h2 - (h2/4), f1_hd - (h2/4), c);
    ADD_MARK(pos2[19] - f1_wd, rect.y - h2 - (h2/4), f1_hd - (h2/4), c);
    ADD_MARK(pos2[22] - f1_wd, rect.y - h2 - (h2/4), f1_hd - (h2/4), c);
    ADD_MARK(pos2[24] - f1_wd, rect.y - h2 - (h2/4), f1_hd - (h2/4), c);

    /* 3rd ROS Row */
    ADD_AREA(30, 10 + (h2 * 11), h2 * 3, wb , &cl);
    ADD_LINE(30, 10 + (h2 * 12), wb, c);

    rect.x = 30 + f1_wd;
    rect.y = 15 + (h2 * 12);
    rect.h = f1_hd;
    rect.w = f1_wd;
    shf = 19;
    for (i = 0; row3[i] != '\0'; i++) {
        pos3[i] = rect.x;
        switch (row3[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': pos3[i] = rect.x + f1_wd + (f1_wd/2); rect.x += 3*f1_wd; continue;
        }
        ros_bits[ros_ptr].rect.x = rect.x;
        ros_bits[ros_ptr].rect.y = rect.y;
        ros_bits[ros_ptr].rect.h = f1_hd;
        ros_bits[ros_ptr].rect.w = f1_wd;
        ros_bits[ros_ptr].digit_on = digit_on[j];
        ros_bits[ros_ptr].digit_off = digit_off[j];
        ros_bits[ros_ptr].shift = shf;
        ros_bits[ros_ptr].row = 2;
        ros_ptr++;
        rect.x = pos3[i] + 3*f1_wd;
        shf--;
    }

    ADD_LABEL1(pos3[0] - (f1_wd / 2), 10 + (h2 * 11), "CR");
    ADD_LABEL1(pos3[3], 10 + (h2 * 11), "CD");
    ADD_LABEL1(pos3[13] + f1_wd, 10 + (h2 * 11), "CF");
    ADD_LABEL1(pos3[17] + f1_wd, 10 + (h2 * 11), "CG");
    ADD_LABEL1(pos3[20], 10 + (h2 * 11), "CV");
    ADD_LABEL1(pos3[23] - f1_wd, 10 + (h2 * 11), "CC");
    ADD_LABEL1(pos3[27] - f1_wd, 10 + (h2 * 11), "CS");

    ADD_MARK(pos3[1] - f1_wd, rect.y, f1_hd, c);
    ADD_MARK(pos3[6] - f1_wd, rect.y, f1_hd, c);
    ADD_MARK(pos3[11] - f1_wd, rect.y, f1_hd, c);
    ADD_MARK(pos3[16] - f1_wd, rect.y, f1_hd, c);
    ADD_MARK(pos3[19] - f1_wd, rect.y, f1_hd, c);
    ADD_MARK(pos3[22] - f1_wd, rect.y, f1_hd, c);
    ADD_MARK(pos3[25] - f1_wd, rect.y, f1_hd, c);
    ADD_MARK(pos3[1] - f1_wd, rect.y - h2 - (h2/4), f1_hd - (h2/4), c);
    ADD_MARK(pos3[6] - f1_wd, rect.y - h2 - (h2/4), f1_hd - (h2/4), c);
    ADD_MARK(pos3[11] - f1_wd, rect.y - h2 - (h2/4), f1_hd - (h2/4), c);
    ADD_MARK(pos3[16] - f1_wd, rect.y - h2 - (h2/4), f1_hd - (h2/4), c);
    ADD_MARK(pos3[19] - f1_wd, rect.y - h2 - (h2/4), f1_hd - (h2/4), c);
    ADD_MARK(pos3[22] - f1_wd, rect.y - h2 - (h2/4), f1_hd - (h2/4), c);
    ADD_MARK(pos3[25] - f1_wd, rect.y - h2 - (h2/4), f1_hd - (h2/4), c);

    /* Count register */
    ADD_AREA(30, 10 + (h2 * 15), h2 * 4, wb, &cl);
    /* Draw Count title */
    ADD_LABEL(30, 15 + (h2 * 14) + (h2/2), wb, "COUNT REGISTER", c, cl);
    ADD_LINE(30, 10 + (h2 * 16), wb, c);
    rect.x = 30 + f1_wd;
    rect.y = 15 + (h2 * 16);
    rect.h = f1_hd;
    rect.w = f1_wd;
    shf = 8;
    j = 0;
    for (i = 0; row_cnt[i] != '\0'; i++) {
        pos_cnt[i] = rect.x;
        switch (row_cnt[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': pos_cnt[i] = rect.x + f1_wd + (f1_wd/2); rect.x += 3 * f1_wd; continue;
        }
        if (j)
            (void)add_led(&labels[j], &cpu_2030.GHY, shf, pos_cnt[i], rect.y, j);
        else
            (void)add_led(&labels[j], &cpu_2030.GHZ, shf, pos_cnt[i], rect.y, j);
        shf--;
        if (shf == -1) {
           j++;
           shf = 8;
        }
        rect.x += 3*f1_wd;
        switch (row_cnta[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': continue;
        }
        ctl_label[ctl_ptr].rect.x = pos_cnt[i];
        ctl_label[ctl_ptr].rect.y = rect.y + f1_hd;
        ctl_label[ctl_ptr].rect.h = f1_hd;
        ctl_label[ctl_ptr].rect.w = f1_wd;
        ctl_label[ctl_ptr].text = digit_off[j];
        ctl_ptr++;
    }

    ADD_MARK(pos_cnt[6] - f1_wd, rect.y, f1_hd * 2, c);
    ADD_MARK(pos_cnt[10] - f1_wd, rect.y, f1_hd * 2, c);
    ADD_MARK(pos_cnt[14] - f1_wd, rect.y, f1_hd * 2, c);
    ADD_MARK(pos_cnt[16] - f1_wd, rect.y, f1_hd * 2, c);
    ADD_MARK(pos_cnt[20] - f1_wd, rect.y, f1_hd * 2, c);



    /* Channel number one */
    ADD_AREA(10, 10 + (h2 * 20), h2 + 10, wb + 40, &cb);
    ADD_AREA(10, 10 + (h2 * 21), (h2 * 10), 10, &cb);
    ADD_AREA(10 + (wb + 30), 10 + (h2 * 21), (h2 * 10), 10, &cb);

    /* Channel one title */
    ADD_LABEL(10, 10 + (h2 * 20) + (h2 /2), wb, "CHANNEL NUMBER ONE", c, cb);

    /* Channel one register */
    ADD_AREA(30, 10 + (h2 * 22), h2 * 4, wb, &cl);
    ADD_LINE(30, 10 + (h2 * 23), wb, c);

    rect.x = 20 + f1_wd;
    rect.y = 15 + (h2 * 23);
    rect.h = f1_hd;
    rect.w = f1_wd;
    shf = 8;
    k = 0;
    for (i = 0; chan_one[i] != '\0'; i++) {
        pos_chan1[i] = rect.x;
        switch (chan_one[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': pos_chan1[i] = rect.x + f1_wd + (f1_wd/2); rect.x += 3 * f1_wd; continue;
        }
        switch (k) {
        case 0:  (void)add_led(&labels[j], &cpu_2030.GR[0], shf, pos_chan1[i], rect.y, j); break;
        case 1:  (void)add_led(&labels[j], &cpu_2030.GK[0], shf, pos_chan1[i], rect.y, j); break;
        case 2:  (void)add_led(&labels[j], &cpu_2030.GG[0], shf, pos_chan1[i], rect.y, j); break;
        }
        shf--;
        if (shf == -1) {
           k++;
           if (k == 1)
              shf = 4;
           else
              shf = 3;
        }
        rect.x += 3*f1_wd;
        switch (chan_onea[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': continue;
        }
        ctl_label[ctl_ptr].rect.x = pos_chan1[i];
        ctl_label[ctl_ptr].rect.y = rect.y + f1_hd;
        ctl_label[ctl_ptr].rect.h = f1_hd;
        ctl_label[ctl_ptr].rect.w = f1_wd;
        ctl_label[ctl_ptr].text = digit_off[j];
        ctl_ptr++;
    }

    ADD_LABEL1(pos_chan1[8] - (f1_wd / 2), 10 + (h2 * 22), "DATA REGISTER");
    ADD_LABEL1(pos_chan1[17] - f1_wd, 10 + (h2 * 22), "KEY");
    ADD_LABEL1(pos_chan1[22], 10 + (h2 * 22), "COMMAND");
    ADD_MARK(pos_chan1[5] - f1_wd, rect.y, f1_hd * 2, c);
    ADD_MARK(pos_chan1[6] - f1_wd, rect.y, f1_hd * 2, c);
    ADD_MARK(pos_chan1[10] - f1_wd, rect.y, f1_hd * 2, c);
    ADD_MARK(pos_chan1[14] - f1_wd, rect.y, f1_hd * 2, c);
    ADD_MARK(pos_chan1[16] - f1_wd, rect.y, f1_hd * 2, c);
    ADD_MARK(pos_chan1[20] - f1_wd, rect.y, f1_hd * 2, c);
    ADD_MARK(pos_chan1[25] - f1_wd, rect.y, f1_hd * 2, c);
    ADD_MARK(pos_chan1[5] - f1_wd, rect.y - h2 - (h2/4), f1_hd - (h2/4), c);
    ADD_MARK(pos_chan1[14] - f1_wd, rect.y - h2 - (h2/4), f1_hd - (h2/4), c);
    ADD_MARK(pos_chan1[20] - f1_wd, rect.y - h2 - (h2/4), f1_hd - (h2/4), c);
    ADD_MARK(pos_chan1[25] - f1_wd, rect.y - h2 - (h2/4), f1_hd - (h2/4), c);

    /* Channel one Status */
    ADD_AREA(30, 10 + (h2 * 26) + (f1_hd/4), h2 * 6, wb, &cl);
    ADD_LINE(30, 15 + (h2 * 27) + (f1_hd/4), wb, c);
    rect.x = 30 + f1_wd;
    rect.y = 15 + (h2 * 28);
    rect.h = f1_hd;
    rect.w = f1_wd;
    pos_chan2[0] = rect.x;
    (void) add_led(&labels[DIG_CD], &cpu_2030.GF[0], 7, rect.x, rect.y, DIG_CD);
    rect.x += f1_wd * 5;
    pos_chan2[1] = rect.x;
    (void) add_led(&labels[DIG_CC], &cpu_2030.GF[0], 6, rect.x, rect.y, DIG_CC);
    rect.x += f1_wd * 5;
    pos_chan2[2] = rect.x;
    (void) add_led(&labels[DIG_SLI], &cpu_2030.GF[0], 5, rect.x, rect.y, DIG_SLI);
    rect.x += f1_wd * 5;
    pos_chan2[3] = rect.x;
    (void) add_led(&labels[DIG_SKP], &cpu_2030.GF[0], 4, rect.x, rect.y, DIG_SKP);
    rect.x += f1_wd * 5;
    pos_chan2[4] = rect.x;
    (void) add_led(&labels[DIG_PCI], &cpu_2030.GF[0], 3, rect.x, rect.y, DIG_PCI);
    rect.x += f1_wd * 5;
    pos_chan2[5] = rect.x;
    (void)add_led(&labels[DIG_OP], &cpu_2030.SEL_TI[0], 7, rect.x, rect.y, DIG_OP);
    rect.x += f1_wd * 6;
    pos_chan2[6] = rect.x;
    (void)add_led(&labels[24], &cpu_2030.SEL_TI[0], 6, rect.x, rect.y, 24);
    rect.x += f1_wd * 6;
    pos_chan2[7] = rect.x;
    (void)add_led(&labels[25], &cpu_2030.SEL_TI[0], 5, rect.x, rect.y, 25);
    rect.x += f1_wd * 6;
    pos_chan2[8] = rect.x;
    (void)add_led(&labels[26], &cpu_2030.SEL_TI[0], 4, rect.x, rect.y, 26);
    rect.x += f1_wd * 6;
    rect.y += f1_hd * 2;
    rect.x = pos_chan2[5];
    (void)add_led(&labels[27], &cpu_2030.SEL_TAGS[0], 15, rect.x, rect.y, 27);
    rect.x += f1_wd * 6;
    (void)add_led(&labels[28], &cpu_2030.SEL_TAGS[0], 14, rect.x, rect.y, 28);
    rect.x += f1_wd * 6;
    (void)add_led(&labels[29], &cpu_2030.SEL_TAGS[0], 13, rect.x, rect.y, 29);
    rect.x += f1_wd * 6;
    (void)add_led(&labels[30], &cpu_2030.SEL_TAGS[0], 12, rect.x, rect.y, 30);
    rect.x += f1_wd * 6;
    (void)add_led(&labels[31], &cpu_2030.SEL_TAGS[0], 11, rect.x, rect.y, 31);
    rect.y -= f1_hd * 2;
    rect.x += f1_wd * 6;
    pos_chan2[9] = rect.x;
    (void)add_led(&labels[32], &cpu_2030.GE[0], 7, rect.x, rect.y, 32);
    rect.x += f1_wd * 6;
    pos_chan2[10] = rect.x;
    (void)add_led(&labels[33], &cpu_2030.GE[0], 6, rect.x, rect.y, 33);
    rect.x += f1_wd * 6;
    pos_chan2[11] = rect.x;
    (void)add_led(&labels[34], &cpu_2030.GE[0], 5, rect.x, rect.y, 34);
    rect.x += f1_wd * 6;
    pos_chan2[12] = rect.x;
    (void)add_led(&labels[35], &cpu_2030.GE[0], 4, rect.x, rect.y, 35);
    rect.x += f1_wd * 6;
    pos_chan2[13] = rect.x;
    (void)add_led(&labels[36], &cpu_2030.GE[0], 3, rect.x, rect.y, 36);
    rect.x += f1_wd * 6;
    pos_chan2[13] = rect.x;
    (void)add_led(&labels[37], &cpu_2030.GE[0], 2, rect.x, rect.y, 37);
    ADD_LABEL1(pos_chan2[2], 15 + (h2 * 26), "FLAGS");
    ADD_LABEL1(pos_chan2[7], 15 + (h2 * 26), "TAGS");
    ADD_LABEL1(pos_chan2[11], 15 + (h2 * 26), "CHECKS");
    ADD_MARK(pos_chan2[5] - f1_wd, rect.y - (h2/2), f1_hd * 4, c);
    ADD_MARK(pos_chan2[9] - f1_wd, rect.y - (h2/2), f1_hd * 4, c);
    ADD_MARK(pos_chan2[5] - f1_wd, rect.y - (h2 * 2), f1_hd - (h2/4), c);
    ADD_MARK(pos_chan2[9] - f1_wd, rect.y - (h2 * 2), f1_hd - (h2/4), c);

    /* Channel number two */
    ADD_AREA(10, 10 + (h2 * 33), h2 + 10, wb + 40, &cb);
    ADD_AREA(10, 10 + (h2 * 34), (h2 * 10), 10, &cb);
    ADD_AREA(10 + (wb + 30), 10 + (h2 * 34), (h2 * 10), 10, &cb);

    /* Channel two title */
    ADD_LABEL(10, 10 + (h2 * 33) + (h2 /2), wb, "CHANNEL NUMBER TWO", c, cb);
    ADD_AREA(30, 10 + (h2 * 35), (h2 * 4), wb, &cl);
    ADD_LINE(30, 10 + (h2 * 36), wb, c);

    rect.y = 15 + (h2 * 36);
    rect.h = f1_hd;
    rect.w = f1_wd;
    shf = 8;
    k = 0;
    for (i = 0; chan_one[i] != '\0'; i++) {
        rect.x = pos_chan1[i];
        switch (chan_one[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': rect.x += 3 * f1_wd; continue;
        }
        switch (k) {
        case 0:  (void)add_led(&labels[j], &cpu_2030.GR[1], shf, pos_chan1[i], rect.y, j); break;
        case 1:  (void)add_led(&labels[j], &cpu_2030.GK[1], shf, pos_chan1[i], rect.y, j); break;
        case 2:  (void)add_led(&labels[j], &cpu_2030.GG[1], shf, pos_chan1[i], rect.y, j); break;
        }
        shf--;
        if (shf == -1) {
           k++;
           if (k == 1)
              shf = 4;
           else
              shf = 3;
        }
        rect.x += 3*f1_wd;
        switch (chan_onea[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': continue;
        }
        ctl_label[ctl_ptr].rect.x = pos_chan1[i];
        ctl_label[ctl_ptr].rect.y = rect.y + h2;
        ctl_label[ctl_ptr].rect.h = f1_hd;
        ctl_label[ctl_ptr].rect.w = f1_wd;
        ctl_label[ctl_ptr].text = digit_off[j];
        ctl_ptr++;
    }

    ADD_LABEL1(pos_chan1[8] - (f1_wd / 2), 10 + (h2 * 35), "DATA REGISTER");
    ADD_LABEL1(pos_chan1[17] - f1_wd, 10 + (h2 * 35), "KEY");
    ADD_LABEL1(pos_chan1[22], 10 + (h2 * 35), "COMMAND");
    ADD_MARK(pos_chan1[5] - f1_wd, rect.y, f1_hd * 2, c);
    ADD_MARK(pos_chan1[6] - f1_wd, rect.y, f1_hd * 2, c);
    ADD_MARK(pos_chan1[10] - f1_wd, rect.y, f1_hd * 2, c);
    ADD_MARK(pos_chan1[14] - f1_wd, rect.y, f1_hd * 2, c);
    ADD_MARK(pos_chan1[16] - f1_wd, rect.y, f1_hd * 2, c);
    ADD_MARK(pos_chan1[20] - f1_wd, rect.y, f1_hd * 2, c);
    ADD_MARK(pos_chan1[25] - f1_wd, rect.y, f1_hd * 2, c);
    ADD_MARK(pos_chan1[5] - f1_wd, rect.y - h2 - (h2/4), f1_hd - (h2/4), c);
    ADD_MARK(pos_chan1[14] - f1_wd, rect.y - h2 - (h2/4), f1_hd - (h2/4), c);
    ADD_MARK(pos_chan1[20] - f1_wd, rect.y - h2 - (h2/4), f1_hd - (h2/4), c);
    ADD_MARK(pos_chan1[25] - f1_wd, rect.y - h2 - (h2/4), f1_hd - (h2/4), c);

    /* Channel Status */
    ADD_AREA(30, 10 + (h2 * 39) + (f1_hd/4), h2 * 6, wb, &cl);
    ADD_LINE(30, 15 + (h2 * 40) + (f1_hd/4), wb, c);
    rect.x = 30 + f1_wd;
    rect.y = 10 + (h2 * 41) + (f1_hd/2);
    rect.h = f1_hd;
    rect.w = f1_wd;
    rect.x = pos_chan2[0];
    (void) add_led(&labels[DIG_CD], &cpu_2030.GF[1], 7, rect.x, rect.y, DIG_CD);
    rect.x += f1_wd * 5;
    (void) add_led(&labels[DIG_CC], &cpu_2030.GF[1], 6, rect.x, rect.y, DIG_CC);
    rect.x += f1_wd * 5;
    (void) add_led(&labels[DIG_SLI], &cpu_2030.GF[1], 5, rect.x, rect.y, DIG_SLI);
    rect.x += f1_wd * 5;
    (void) add_led(&labels[DIG_SKP], &cpu_2030.GF[1], 4, rect.x, rect.y, DIG_SKP);
    rect.x += f1_wd * 5;
    (void) add_led(&labels[DIG_PCI], &cpu_2030.GF[1], 3, rect.x, rect.y, DIG_PCI);
    rect.x += f1_wd * 5;
    (void)add_led(&labels[DIG_OP], &cpu_2030.SEL_TI[1], 7, rect.x, rect.y, DIG_OP);
    rect.x += f1_wd * 6;
    (void)add_led(&labels[24], &cpu_2030.SEL_TI[1], 6, rect.x, rect.y, 24);
    rect.x += f1_wd * 6;
    (void)add_led(&labels[25], &cpu_2030.SEL_TI[1], 5, rect.x, rect.y, 25);
    rect.x += f1_wd * 6;
    (void)add_led(&labels[26], &cpu_2030.SEL_TI[1], 4, rect.x, rect.y, 26);
    rect.x += f1_wd * 6;
    rect.x = pos_chan2[5];
    rect.y += f1_hd * 2;
    (void)add_led(&labels[27], &cpu_2030.SEL_TAGS[1], 15, rect.x, rect.y, 27);
    rect.x += f1_wd * 6;
    (void)add_led(&labels[28], &cpu_2030.SEL_TAGS[1], 14, rect.x, rect.y, 28);
    rect.x += f1_wd * 6;
    (void)add_led(&labels[29], &cpu_2030.SEL_TAGS[1], 13, rect.x, rect.y, 29);
    rect.x += f1_wd * 6;
    (void)add_led(&labels[30], &cpu_2030.SEL_TAGS[1], 12, rect.x, rect.y, 30);
    rect.x += f1_wd * 6;
    (void)add_led(&labels[31], &cpu_2030.SEL_TAGS[1], 11, rect.x, rect.y, 31);

    rect.y -= f1_hd * 2;
    rect.x += f1_wd * 6;
    rect.x = pos_chan2[9];
    (void)add_led(&labels[32], &cpu_2030.GE[1], 7, rect.x, rect.y, 32);
    rect.x += f1_wd * 6;
    (void)add_led(&labels[33], &cpu_2030.GE[1], 6, rect.x, rect.y, 33);
    rect.x += f1_wd * 6;
    pos_chan2[11] = rect.x;
    (void)add_led(&labels[34], &cpu_2030.GE[1], 5, rect.x, rect.y, 34);
    rect.x += f1_wd * 6;
    pos_chan2[12] = rect.x;
    (void)add_led(&labels[35], &cpu_2030.GE[1], 4, rect.x, rect.y, 35);
    rect.x += f1_wd * 6;
    pos_chan2[13] = rect.x;
    (void)add_led(&labels[36], &cpu_2030.GE[1], 3, rect.x, rect.y, 36);
    rect.x += f1_wd * 6;
    pos_chan2[13] = rect.x;
    (void)add_led(&labels[37], &cpu_2030.GE[1], 2, rect.x, rect.y, 37);
    ADD_LABEL1(pos_chan2[2], 15 + (h2 * 39), "FLAGS");
    ADD_LABEL1(pos_chan2[7], 15 + (h2 * 39), "TAGS");
    ADD_LABEL1(pos_chan2[11], 15 + (h2 * 39), "CHECKS");
    ADD_MARK(pos_chan2[5] - f1_wd, rect.y - (h2/2), f1_hd * 4, c);
    ADD_MARK(pos_chan2[9] - f1_wd, rect.y - (h2/2), f1_hd * 4, c);
    ADD_MARK(pos_chan2[5] - f1_wd, rect.y - (h2 * 2), f1_hd - (h2/4), c);
    ADD_MARK(pos_chan2[9] - f1_wd, rect.y - (h2 * 2), f1_hd - (h2/4), c);

    /* MPX register */
    ADD_AREA(30, 10 + (h2 * 46) + (f1_hd/4), h2 * 4, wb, &cl);
    ADD_LINE(30, 15 + (h2 * 47) + (f1_hd/4), wb, c);
    rect.x = 30 + f1_wd;
    rect.y = 15 + (h2 * 48);
    rect.h = f1_hd;
    rect.w = f1_wd;
    pos_mpx[0] = rect.x;
    (void) add_led(&labels[DIG_OP], &cpu_2030.MPX_TI, 7, rect.x, rect.y, DIG_OP);  /* OP IN */
    rect.x += f1_wd * 6;
    pos_mpx[1] = rect.x;
    (void) add_led(&labels[24], &cpu_2030.MPX_TI, 6, rect.x, rect.y, 24);   /* ADR IN */
    rect.x += f1_wd * 6;
    pos_mpx[2] = rect.x;
    (void) add_led(&labels[25], &cpu_2030.MPX_TI, 5, rect.x, rect.y, 25);  /* STAT IN */
    rect.x += f1_wd * 6;
    pos_mpx[3] = rect.x;
    (void) add_led(&labels[26], &cpu_2030.MPX_TI, 4, rect.x, rect.y, 26);  /* SERV IN */
    rect.x += f1_wd * 6;
    pos_mpx[4] = rect.x;
    (void) add_led(&labels[27], &cpu_2030.MPX_TAGS, 15, rect.x, rect.y, 27); /* SEL OUT */
    rect.x += f1_wd * 6;
    pos_mpx[5] = rect.x;
    (void)add_led(&labels[28], &cpu_2030.MPX_TAGS, 14, rect.x, rect.y, 28); /* ADR OUT */
    rect.x += f1_wd * 6;
    pos_mpx[6] = rect.x;
    (void)add_led(&labels[29], &cpu_2030.MPX_TAGS, 13, rect.x, rect.y, 29);  /* CMD OUT */
    rect.x += f1_wd * 6;
    pos_mpx[7] = rect.x;
    (void)add_led(&labels[30], &cpu_2030.MPX_TAGS, 12, rect.x, rect.y, 30);  /* SERV OUT */
    rect.x += f1_wd * 6;
    pos_mpx[8] = rect.x;
    (void)add_led(&labels[31], &cpu_2030.MPX_TAGS, 11, rect.x, rect.y, 31);  /* SUP OUT */
    rect.x += f1_wd * 6;
    pos_mpx[9] = rect.x;
    rect.x += f1_wd * 3;
    rect.y = 15 + (h2 * 48) - (f1_hd/2);
    shf = 8;
    for (i = 15; chan_one[i] != '\0'; i++) {
        pos_mpx[i] = rect.x;
        switch (chan_one[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': rect.x += 3 * f1_wd; continue;
        }
        (void)add_led(&labels[j], &cpu_2030.O_REG, shf, pos_mpx[i], rect.y, j);
        shf--;
        rect.x += 3*f1_wd;
        switch (chan_onea[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': continue;
        }
        ctl_label[ctl_ptr].rect.x = pos_mpx[i];
        ctl_label[ctl_ptr].rect.y = rect.y + f1_hd;
        ctl_label[ctl_ptr].rect.h = f1_hd;
        ctl_label[ctl_ptr].rect.w = f1_wd;
        ctl_label[ctl_ptr].text = digit_off[j];
        ctl_ptr++;
    }
    ADD_LABEL1(pos_mpx[3], 15 + (h2 * 46), "MPX CHANNEL TAGS");
    ADD_LABEL1(pos_mpx[15], 15 + (h2 * 46), "MPX CHANNEL BUS-OUT REGISTER");
    ADD_MARK(pos_mpx[9] - f1_wd, rect.y, f1_hd * 2, c);
    ADD_MARK(pos_mpx[9] - f1_wd, rect.y - h2 - (h2/4), f1_hd - (f1_hd/4), c);

    /* Seperator */
    ADD_AREA(30, 15 + (h2 * 50) + 4, f1_hd * 4, wb, &cl);
    ADD_LINE(30, 15 + (h2 * 51) + 4, wb, c);
    rect.x = 30 + f1_wd;
    rect.y = 10 + (h2 * 52) + (f1_hd/4);
    rect.h = f1_hd;
    rect.w = f1_wd;
    shf = 8;
    k = 0;
    for (i = 0; store_addr[i] != '\0'; i++) {
        pos_store[i] = rect.x;
        switch (store_addr[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': pos_store[i] = rect.x + f1_wd + (f1_wd/2); rect.x += 3 * f1_wd; continue;
        }
        if (k)
            (void)add_led(&labels[j], &cpu_2030.N_REG, shf, pos_store[i], rect.y, j);
        else
            (void)add_led(&labels[j], &cpu_2030.M_REG, shf, pos_store[i], rect.y, j);
        shf--;
        if (shf == -1) {
           k++;
           shf = 8;
        }
        rect.x += 3*f1_wd;
        switch (store_addra[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': continue;
        }
        ctl_label[ctl_ptr].rect.x = pos_store[i];
        ctl_label[ctl_ptr].rect.y = rect.y + f1_hd;
        ctl_label[ctl_ptr].rect.h = f1_hd;
        ctl_label[ctl_ptr].rect.w = f1_wd;
        ctl_label[ctl_ptr].text = digit_off[j];
        ctl_ptr++;
    }

    shf--;
    (void)add_led(&labels[38], &store, 0, rect.x + f1_wd, 10 + rect.y - f1_hd, 38);
    shf--;
    (void)add_led(&labels[39], &store, 1, rect.x + f1_wd, 10 + rect.y + f1_hd - 5, 39);
    ADD_LABEL1(pos_store[11] - f1_wd, 15 + (h2 * 50) + 4, "MAIN STORAGE ADDRESS REGISTER");
    ADD_MARK(pos_store[1] - f1_wd, rect.y + (f1_hd/4), f1_hd * 2, c);
    ADD_MARK(pos_store[5] - f1_wd, rect.y + (f1_hd/4), f1_hd * 2, c);
    ADD_MARK(pos_store[9] - f1_wd, rect.y - h2, f1_hd * 3, c);
    ADD_MARK(pos_store[11] - f1_wd, rect.y + (f1_hd/4), f1_hd * 2, c);
    ADD_MARK(pos_store[15] - f1_wd, rect.y + (f1_hd/4), f1_hd * 2, c);
    ADD_MARK(pos_store[19] - f1_wd, rect.y + (f1_hd/4), f1_hd * 2, c);

    /* Seperator */
    ADD_AREA(30, 10 + (h2 * 55), h2 * 4, f1_wd * 59, &cl);
    ADD_LINE(30, 15 + (h2 * 56), f1_wd * 59, c);
    rect.x = 40 + f1_wd;
    rect.y = 10 + (h2 * 57) + (f1_hd/2);
    rect.h = f1_hd;
    rect.w = f1_wd;
    shf = 8;
    k = 0;
    for (i = 0; data_reg[i] != '\0'; i++) {
        pos_data[i] = rect.x;
        switch (data_reg[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': pos_data[i] = rect.x + f1_wd + (f1_wd/2); rect.x += 3*f1_wd; continue;
        }
        if (k)
            (void)add_led(&labels[j], &cpu_2030.Alu_out, shf, pos_data[i], rect.y, j);
        else
            (void)add_led(&labels[j], &cpu_2030.R_REG, shf, pos_data[i], rect.y, j);
        rect.x = pos_data[i] + 3*f1_wd;
        shf--;
        if (shf == -1) {
           shf = 7;
           k++;
        }
    }
    ADD_LABEL1(pos_data[0] + f1_wd, 15 + (h2 * 55), "MAIN STORAGE DATA REGISTER");
    ADD_LABEL1(pos_data[13] - f1_wd, 15 + (h2 * 55), "ALU OUTPUT");
    ADD_MARK(pos_data[1] - f1_wd, rect.y, f1_hd, c);
    ADD_MARK(pos_data[5] - f1_wd, rect.y, f1_hd, c);
    ADD_MARK(pos_data[9] - f1_wd, rect.y - (h2 * 2) - (h2/4), f1_hd, c);
    ADD_MARK(pos_data[9] - f1_wd, rect.y - h2, f1_hd * 2, c);
    ADD_MARK(pos_data[11] - f1_wd, rect.y, f1_hd, c);
    ADD_MARK(pos_data[15] - f1_wd, rect.y, f1_hd, c);
    ADD_AREA(30, 10 + (h2 * 60) - (f1_hd/2), (h2 * 4) + f1_hd, f1_wd * 59, &cl);
    ADD_LINE(30, 15 + (h2 * 61), f1_wd * 59, c);
    rect.x = 30 + f1_wd;
    rect.y = 15 + (h2 * 62);
    rect.h = f1_hd;
    rect.w = f1_wd;
    shf = 8;
    k = 0;
    for (i = 0; store_addr[i] != '\0'; i++) {
        pos_breg[i] = rect.x;
        switch (store_addr[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': pos_breg[i] = rect.x + f1_wd + (f1_wd/2); rect.x += 3 * f1_wd; continue;
        }
        if (k)
            (void)add_led(&labels[j], &cpu_2030.Bbus, shf, pos_breg[i], rect.y, j);
        else
            (void)add_led(&labels[j], &cpu_2030.Abus, shf, pos_breg[i], rect.y, j);
        shf--;
        if (shf == -1) {
           shf = 8;
           k++;
        }
        rect.x += 3*f1_wd;
        switch (store_addra[i]) {
        case '0': j = 0;  break;
        case '1': j = 1;  break;
        case '2': j = 2;  break;
        case '3': j = 3;  break;
        case '4': j = 4;  break;
        case '5': j = 5;  break;
        case '6': j = 6;  break;
        case '7': j = 7;  break;
        case '8': j = 8;  break;
        case 'A': j = 10;  break;
        case 'P': j = 16;  break;
        case ' ': continue;
        }
        ctl_label[ctl_ptr].rect.x = pos_breg[i];
        ctl_label[ctl_ptr].rect.y = rect.y + f1_hd;
        ctl_label[ctl_ptr].rect.h = f1_hd;
        ctl_label[ctl_ptr].rect.w = f1_wd;
        ctl_label[ctl_ptr].text = digit_off[j];
        ctl_ptr++;
    }
    ADD_LABEL1(pos_breg[3] + f1_wd, 15 + (h2 * 60) - (f1_hd/2), "B REGISTER");
    ADD_LABEL1(pos_breg[13] - f1_wd, 15 + (h2 * 60) - (f1_hd/2), "A REGISTER");
    ADD_MARK(pos_breg[1] - f1_wd, rect.y, f1_hd * 2, c);
    ADD_MARK(pos_breg[5] - f1_wd, rect.y, f1_hd * 2, c);
    ADD_MARK(pos_breg[9] - f1_wd, rect.y - (h2 * 2) - (h2/4), f1_hd, c);
    ADD_MARK(pos_breg[9] - f1_wd, rect.y - h2, f1_hd * 3, c);
    ADD_MARK(pos_breg[11] - f1_wd, rect.y, f1_hd * 2, c);
    ADD_MARK(pos_breg[15] - f1_wd, rect.y, f1_hd * 2, c);

    rect.x = 40 + pos_data[17];
    rect.y = 10 + (h2 * 55);
    rect.h = (h2 * 9) + (h2/2);
    rect.w = (30 + wb) - rect.x;
    ADD_AREA(rect.x, rect.y, rect.h, rect.w, &cl);
    rect.y = 15 + (h2 * 56);
    ADD_LINE(rect.x, 15 + (h2 * 56), rect.w, c);
    ADD_LABEL1(rect.x + (f1_wd * 3), 15 + (h2 * 55), "CPU STATUS");
    rect.x += f1_wd;
    pos_data[18] = rect.x;
    rect.y += h2;       /* EX */
    (void)add_led(&labels[40], &end_of_e_cycle, 0, rect.x, rect.y, 40);
    rect.x += (f1_wd * 4); /* MATCH */
    (void)add_led(&labels[41], &match, 0, rect.x, rect.y, 41);
    rect.x += (f1_wd * 7);  /* ALLOW WRITE */
    (void)add_led(&labels[42], &allow_write, 0, rect.x, rect.y, 42);
    rect.y += 2*h2 + (f1_hd/2); /* 1050 Intv */
    rect.x = pos_data[18];
    (void)add_led(&labels[43], &cpu_2030.TT, 2, rect.x, rect.y, 43);
    rect.x += (f1_wd * 4);
    rect.x += (f1_wd * 7);    /* 1050 req */
    (void)add_led(&labels[44], &t_request, 0, rect.x, rect.y, 44);
    rect.y += 2*h2 + (f1_hd/2);
    rect.x = pos_data[18];  /* MPX CHNL */
    (void)add_led(&labels[45], &cpu_2030.FT, 5, rect.x, rect.y, 45);
    rect.x += (f1_wd * 4);     /* SELCH */
    (void)add_led(&labels[46], &cpu_2030.H_REG, 3, rect.x, rect.y, 46);
    rect.x += (f1_wd * 7);    /* Compute */
    (void)add_led(&labels[47], 0, 0, rect.x, rect.y, 47);
    rect.x += (f1_wd * 8);
    pos_data[19] = rect.x;
    rect.y = 15 + (h2 * 57);
    ADD_LABEL1(rect.x + (f1_wd*2), 15 + (h2 * 55), "CPU CHECKS");
    ADD_MARK(rect.x - f1_wd, 15 + (h2 * 55), (f1_hd * 8) + (f1_hd/2), c);
    /* STORE ADR */
    (void)add_led(&labels[48], &cpu_2030.MC_REG, 5, rect.x, rect.y, 48);
    rect.x += f1_wd * 4;
    /* STORE DATA */
    (void)add_led(&labels[49], &cpu_2030.MC_REG, 1, rect.x, rect.y, 49);
    rect.x = pos_data[19];
    rect.y += 2*h2 + (f1_hd/2);
    /* B reg */
    (void)add_led(&labels[50], &cpu_2030.MC_REG, 7, rect.x, rect.y, 50);
    rect.x += f1_wd * 4;
    /* A reg */
    (void)add_led(&labels[51], &cpu_2030.MC_REG, 6, rect.x, rect.y, 51);
    rect.x += f1_wd * 4;
    /* ALU */
    (void)add_led(&labels[52], &cpu_2030.MC_REG, 0, rect.x, rect.y, 52);
    rect.x = pos_data[19];
    rect.y += 2*h2 + (f1_hd/2);
    /* ROS ADDR */
    (void)add_led(&labels[53], &cpu_2030.MC_REG, 2, rect.x, rect.y, 53);
    rect.x += f1_wd * 4;
    /* ROS SALS */
    (void)add_led(&labels[54], &cpu_2030.MC_REG, 3, rect.x, rect.y, 54);
    rect.x += f1_wd * 4;
    /* CTL REG */
    (void)add_led(&labels[55], &cpu_2030.MC_REG, 4, rect.x, rect.y, 55);

    add_switch(&SYS_RST, 10, (h2 * 67), f1_wd * 10, f1_hd * 2, SW, &c3, &sw_labels[0], font1);
    add_switch(&ROAR_RST,10, (h2 * 70), f1_wd * 10, f1_hd * 2, SW, &c3, &sw_labels[1], font1);
    add_switch(NULL, 10, (h2 * 73), f1_wd * 10, f1_hd * 2, SW, &c, NULL, NULL);
    add_switch(&START, 10, (h2 * 76), f1_wd * 10, f1_hd * 2, SW, &c2, &sw_labels[2], font1);
    add_switch(NULL, 85, (h2 * 67), f1_wd * 10, f1_hd * 2, SW, &c, NULL, font1);
    add_switch(&SET_IC, 85, (h2 * 70), f1_wd * 10, f1_hd * 2, SW, &c3, &sw_labels[3], font1);
    add_switch(&CHECK_RST, 85, (h2 * 73), f1_wd * 10, f1_hd * 2, SW, &c3, &sw_labels[4], font1);
    add_switch(&STOP, 85, (h2 * 76), f1_wd * 10, f1_hd * 2, SW, &c5, &sw_labels[5], font1);
    add_switch(&INT_TMR, 160, (h2 * 67), f1_wd * 10, f1_hd * 2, ONOFF, &c3, &sw_labels[6], font1);
    add_switch(&STORE, 160, (h2 * 70), f1_wd * 10, f1_hd * 2, SW, &c3, &sw_labels[7], font1);
    add_switch(&LAMP_TEST, 160, (h2 * 73), f1_wd * 10, f1_hd * 2, SW, &c3, &sw_labels[8], font1);
    add_switch(&DISPLAY, 160, (h2 * 76), f1_wd * 10, f1_hd * 2, SW, &c3, &sw_labels[9], font1);
    add_switch(NULL, 780, (h2 * 66), f1_wd * 10, f1_hd * 2, SW, &c, &sw_labels[10], font1);
    add_switch(&POWER, 1000, (h2 * 66), f1_wd * 10, f1_hd * 2, SW, &c5, &sw_labels[11], font1);
    add_switch(&INTR, 780, (h2 * 79), f1_wd * 10, f1_hd * 2, SW, &c5, &sw_labels[12], font1);
    add_switch(&LOAD, 1000, (h2 * 79), f1_wd * 10, f1_hd * 2, SW, &c3, &sw_labels[13], font1);
    marks[mrk_ptr].x1 = 770;
    marks[mrk_ptr].y1 = (h2 * 66) - 3;
    marks[mrk_ptr].x2 = 1065;
    marks[mrk_ptr].y2 = (h2 * 66) - 3;
    marks[mrk_ptr].c = &c;
    mrk_ptr++;
    marks[mrk_ptr].x1 = 770;
    marks[mrk_ptr].y1 = (h2 * 68) + 3;
    marks[mrk_ptr].x2 = 1065;
    marks[mrk_ptr].y2 = (h2 * 68) + 3;
    marks[mrk_ptr].c = &c;
    mrk_ptr++;
    marks[mrk_ptr].x1 = 770;
    marks[mrk_ptr].y1 = (h2 * 66) - 3;
    marks[mrk_ptr].x2 = 770;
    marks[mrk_ptr].y2 = (h2 * 68) + 3;
    marks[mrk_ptr].c = &c;
    mrk_ptr++;
    marks[mrk_ptr].x1 = 1065;
    marks[mrk_ptr].y1 = (h2 * 66) - 3;
    marks[mrk_ptr].x2 = 1065;
    marks[mrk_ptr].y2 = (h2 * 68) + 3;
    marks[mrk_ptr].c = &c;
    mrk_ptr++;
    marks[mrk_ptr].x1 = 770;
    marks[mrk_ptr].y1 = (h2 * 79) - 5;
    marks[mrk_ptr].x2 = 1065;
    marks[mrk_ptr].y2 = (h2 * 79) - 5;
    marks[mrk_ptr].c = &c1;
    mrk_ptr++;
    marks[mrk_ptr].x1 = 770;
    marks[mrk_ptr].y1 = (h2 * 79) - 5;
    marks[mrk_ptr].x2 = 770;
    marks[mrk_ptr].y2 = (h2 * 81) + 3;
    marks[mrk_ptr].c = &c1;
    mrk_ptr++;
    marks[mrk_ptr].x1 = 770;
    marks[mrk_ptr].y1 = (h2 * 81) + 3;
    marks[mrk_ptr].x2 = 1065;
    marks[mrk_ptr].y2 = (h2 * 81) + 3;
    marks[mrk_ptr].c = &c1;
    mrk_ptr++;
    marks[mrk_ptr].x1 = 1065;
    marks[mrk_ptr].y1 = (h2 * 79) - 5;
    marks[mrk_ptr].x2 = 1065;
    marks[mrk_ptr].y2 = (h2 * 81) + 3;
    marks[mrk_ptr].c = &c1;
    mrk_ptr++;

    ADD_LABEL(620,(h2 * 34), (f1_wd * 40), "ROS CONTROL", c1, cc);
    ADD_LABEL(620 + (f1_wd * 4), (h2 * 36), f1_wd*6, "INHBIT", c1, cc);
    ADD_LABEL(620 + (f1_wd * 4), (h2 * 37), f1_wd * 7, "CF STOP", c1, cc);
    ADD_LABEL(620, (h2 * 36), f1_wd * 40, "PROCESS", c1, cc);
    ADD_LABEL(620 + (f1_wd * 30), (h2 * 36), f1_wd * 3, "ROS", c1, cc);
    ADD_LABEL(620 + (f1_wd * 30), (h2 * 37), f1_wd*4, "SCAN", c1, cc);
    ADD_LINE(620 + (f1_wd * 11), (h2 * 37), (f1_wd * 3), c1);
    ADD_LINE(620 + (f1_wd * 26)+1, (h2 * 37), (f1_wd * 3) - 1, c1);
    dial[0].boxd.x = 620 + (f1_wd * 10);
    dial[0].boxd.y = h2 * 36;
    dial[0].boxd.w = (f1_wd * 20);
    dial[0].boxd.h = h2 * 5;
    dial[0].boxu.x = 620 + (f1_wd * 20);
    dial[0].boxu.y = h2 * 36;
    dial[0].boxu.w = (f1_wd * 20);
    dial[0].boxu.h = h2 * 5;
    dial[0].center_x = 620 + (f1_wd * 20);
    dial[0].center_y = (h2 * 40);
    dial[0].pos_x[0] = 620 + (f1_wd * 14);
    dial[0].pos_x[1] = 620 + (f1_wd * 20);
    dial[0].pos_x[2] = 620 + (f1_wd * 26);
    dial[0].pos_y[0] = h2 * 37;
    dial[0].pos_y[1] = h2 * 37;
    dial[0].pos_y[2] = h2 * 37;
    dial[0].init = 1;
    dial[0].value = &PROC_SW;
    dial[0].max = 2;
    dial[0].wrap = 0;

    ADD_LABEL(900, (h2 * 34), (f1_wd * 23), "RATE", c1, cc);
    ADD_LABEL(900, (h2 * 36), (f1_wd * 5), "INSTR", c1, cc);
    ADD_LABEL(900, (h2 * 37), (f1_wd * 4), "STEP", c1, cc);
    ADD_LABEL(900, (h2 * 36), f1_wd * 23, "PROCESS", c1, cc);
    ADD_LABEL(900 + (f1_wd * 21), (h2 * 36), f1_wd * 5, "SINGLE", c1, cc);
    ADD_LABEL(900 + (f1_wd * 21), (h2 * 37), f1_wd * 5, "CYCLE", c1, cc);
    ADD_LINE(900 + (f1_wd * 5), (h2 * 37), (f1_wd * 2), c1);
    ADD_LINE(900 + (f1_wd * 16), (h2 * 37), (f1_wd * 3), c1);
    dial[1].boxd.x = 900;
    dial[1].boxd.y = h2 * 36;
    dial[1].boxd.w = (f1_wd * 12);
    dial[1].boxd.h = h2 * 5;
    dial[1].boxu.x = 900 + (f1_wd * 12);
    dial[1].boxu.y = h2 * 36;
    dial[1].boxu.w = (f1_wd * 12);
    dial[1].boxu.h = h2 * 5;
    dial[1].center_x = 900 + (f1_wd * 12);
    dial[1].center_y = (h2 * 40);
    dial[1].pos_x[0] = 900 + (f1_wd * 7);
    dial[1].pos_x[1] = 900 + (f1_wd * 12);
    dial[1].pos_x[2] = 900 + (f1_wd * 16);
    dial[1].pos_y[0] = h2 * 37;
    dial[1].pos_y[1] = h2 * 37;
    dial[1].pos_y[2] = h2 * 37;
    dial[1].init = 1;
    dial[1].value = &RATE_SW;
    dial[1].max = 2;
    dial[1].wrap = 0;

    ADD_LABEL(620, (h2 * 46), (f1_wd * 40), "ADDRESS COMPARE", c1, cc);
    ADD_LABEL(620, (h2 * 48) - (f1_hd/2), f1_wd*40, "PROCESS", c1, cc);
    ADD_LABEL3(620, (h2 * 48), "ROAR SYNC", c1, cc);
    ADD_LINE(620 + (f1_wd * 10), (h2 * 49), (f1_wd * 5), c1);
    ADD_LABEL3(620, (h2 * 50), "ROAR STOP", c1, cc);
    ADD_LINE(620 + (f1_wd * 10), (h2 * 51), (f1_wd * 4), c1);
    ADD_LABEL3(620, (h2 * 52), "EARLY ROAR", c1, cc);
    ADD_LINE(620 + (f1_wd * 10), (h2 * 53), (f1_wd * 4), c1);
    ADD_LABEL3(620, (h2 * 53), "STOP", c1, cc);
    ADD_LABEL3(620, (h2 * 55), "ROAR RESTART", c1, cc);
    ADD_LABEL3(620, (h2 * 56), "WITHOUT RESET", c1, cc);
    ADD_LABEL(620, (h2 * 56), f1_wd*40, "ROAR", c1, cc);
    ADD_LABEL(620, (h2 * 57), f1_wd*40, "RESTART", c1, cc);
    ADD_LABEL3(620 + (f1_wd * 29), (h2 * 48), "SAR DELAYED", c1, cc);
    ADD_LINE(620 + (f1_wd * 26) + 1, (h2 * 49), (f1_wd * 2), c1);
    ADD_LABEL3(620 + (f1_wd * 29), (h2 * 49), "STOP", c1, cc);
    ADD_LABEL3(620 + (f1_wd * 32), (h2 * 51) - (f1_hd/2), "SAR STOP", c1, cc);
    ADD_LINE(620 + (f1_wd * 26) + 1, (h2 * 51), (f1_wd * 4), c1);
    ADD_LABEL3(620 + (f1_wd * 29), (h2 * 53) - (f1_hd/2), "SAR RESTART", c1, cc);
    ADD_LINE(620 + (f1_wd * 26) + 1, (h2 * 53), (f1_wd * 2), c1);
    ADD_LABEL3(620 + (f1_wd * 28), (h2 * 55), "ROAR RESTART", c1, cc);
    ADD_LABEL3(620 + (f1_wd * 28), (h2 * 56), "STORE BYPASS", c1, cc);
    dial[2].boxd.x = 620 + (f1_wd * 10);
    dial[2].boxd.y = h2 * 48;
    dial[2].boxd.w = (f1_wd * 10);
    dial[2].boxd.h = h2 * 5;
    dial[2].boxu.x = 620 + (f1_wd * 20);
    dial[2].boxu.y = h2 * 48;
    dial[2].boxu.w = (f1_wd * 10);
    dial[2].boxu.h = h2 * 5;
    dial[2].center_x = 620 + (f1_wd * 20);
    dial[2].center_y = (h2 * 52);
    dial[2].pos_x[0] = 620 + (f1_wd * 20);
    dial[2].pos_x[1] = 620 + (f1_wd * 26);
    dial[2].pos_x[2] = 620 + (f1_wd * 26);
    dial[2].pos_x[3] = 620 + (f1_wd * 26);
    dial[2].pos_x[4] = 620 + (f1_wd * 26);
    dial[2].pos_x[5] = 620 + (f1_wd * 20);
    dial[2].pos_x[6] = 620 + (f1_wd * 14);
    dial[2].pos_x[7] = 620 + (f1_wd * 14);
    dial[2].pos_x[8] = 620 + (f1_wd * 14);
    dial[2].pos_x[9] = 620 + (f1_wd * 15);
    dial[2].pos_y[0] = h2 * 49;
    dial[2].pos_y[1] = (h2 * 49);
    dial[2].pos_y[2] = (h2 * 51);
    dial[2].pos_y[3] = (h2 * 53);
    dial[2].pos_y[4] = (h2 * 55);
    dial[2].pos_y[5] = (h2 * 56);
    dial[2].pos_y[6] = (h2 * 56);
    dial[2].pos_y[7] = (h2 * 53);
    dial[2].pos_y[8] = (h2 * 51);
    dial[2].pos_y[9] = h2 * 49;
    dial[2].init = 0;
    dial[2].value = &MATCH_SW;
    dial[2].max = 9;
    dial[2].wrap = 1;

    ADD_LABEL(900, (h2 * 46), f1_wd*23, "CHECK CONTROL", c1, cc);
    ADD_LABEL3(900 - (f1_wd *5), (h2 * 48) + (f1_hd/2), "DISABLE", c1, cc);
    ADD_LINE(900 + (f1_wd * 3), (h2 * 49), (f1_wd * 4), c1);
    ADD_LABEL3(900 - (f1_wd*5), (h2 * 50) + (f1_hd/2), "DIAGNOSTIC", c1, cc);
    ADD_LABEL(900, (h2 * 48) - (f1_hd/2), f1_wd*23, "PROCESS", c1, cc);
    ADD_LABEL(900 + (f1_wd * 24), (h2 * 48) + (f1_hd/2), f1_wd*4, "STOP", c1, cc);
    ADD_LINE(900 + (f1_wd * 16), (h2 * 49), (f1_wd * 6), c1);
    ADD_LABEL(900 + (f1_wd * 21), (h2 * 50) + (f1_hd/2), f1_wd*7, "RESTART", c1, cc);
    dial[3].boxd.x = 900;
    dial[3].boxd.y = h2 * 48;
    dial[3].boxd.w = (f1_wd * 12);
    dial[3].boxd.h = h2 * 5;
    dial[3].boxu.x = 900 + (f1_wd * 12);
    dial[3].boxu.y = h2 * 48;
    dial[3].boxu.w = (f1_wd * 12);
    dial[3].boxu.h = h2 * 5;
    dial[3].center_x = 900 + (f1_wd * 12);
    dial[3].center_y = (h2 * 52);
    dial[3].pos_x[0] = 900 + (f1_wd * 6);
    dial[3].pos_x[1] = 900 + (f1_wd * 7);
    dial[3].pos_x[2] = 900 + (f1_wd * 12);
    dial[3].pos_x[3] = 900 + (f1_wd * 16);
    dial[3].pos_x[4] = 900 + (f1_wd * 18);
    dial[3].pos_y[0] = h2 * 51;
    dial[3].pos_y[1] = (h2 * 49);
    dial[3].pos_y[2] = (h2 * 49);
    dial[3].pos_y[3] = (h2 * 49);
    dial[3].pos_y[4] = (h2 * 51);
    dial[3].init = 2;
    dial[3].value = &CHK_SW;
    dial[3].max = 4;
    dial[3].wrap = 0;
    PROC_SW = dial[0].init;
    RATE_SW = dial[1].init;
    MATCH_SW = dial[2].init;
    CHK_SW = dial[3].init;

    rect.x = 250;
    for (i = 0; i < 8; i++ ) {
        hex_dial[i].rect.x = rect.x + f1_wd;
        hex_dial[i].rect.y = (h2 * 73);
        hex_dial[i].rect.w = 64;
        hex_dial[i].rect.h = 64;
        hex_dial[i].boxu.x = rect.x + f1_wd;
        hex_dial[i].boxu.y = (h2 * 73);
        hex_dial[i].boxu.w = 32;
        hex_dial[i].boxu.h = 64;
        hex_dial[i].boxd.x = rect.x + f1_wd + 32;
        hex_dial[i].boxd.y = (h2 * 73);
        hex_dial[i].boxd.w = 32;
        hex_dial[i].boxd.h = 64;
        if (i == 3) {
            rect.x += (f1_wd * 15);
        }
        hex_ptr++;
        rect.x += (f1_wd * 15);
    }

    rect.x = 250 + (f1_wd * 58);
    store_dial[0].rect.x = rect.x + f1_wd;
    store_dial[0].rect.y = (h2 * 73);
    store_dial[0].rect.w = 80;
    store_dial[0].rect.h = 80;
    store_dial[0].boxu.x = rect.x;
    store_dial[0].boxu.y = (h2 * 73);
    store_dial[0].boxu.w = 40;
    store_dial[0].boxu.h = 80;
    store_dial[0].boxd.x = rect.x + 32;
    store_dial[0].boxd.y = (h2 * 73);
    store_dial[0].boxd.w = 40;
    store_dial[0].boxd.h = 80;
    store_ptr = 1;

    hex_dial[0].digit = &A_SW;
    hex_dial[1].digit = &B_SW;
    hex_dial[2].digit = &C_SW;
    hex_dial[3].digit = &D_SW;
    hex_dial[4].digit = &F_SW;
    hex_dial[5].digit = &G_SW;
    hex_dial[6].digit = &H_SW;
    hex_dial[7].digit = &J_SW;
    store_dial[0].digit = &E_SW;
    store_dial[0].sel = 0;
    *store_dial[0].digit = 0;

    rect.x= hex_dial[0].boxu.x;
    rect.y=h2 * 69 - (h2 / 2);
    rect.h=h2;
    rect.w= (hex_dial[3].boxd.x + hex_dial[3].boxd.w) - rect.x;
    ADD_AREA(rect.x, rect.y, rect.h, rect.w, &c);
    ADD_LABEL(rect.x, rect.y, rect.w, "COMPARE ADDRESS", c1, c);
    ADD_AREA(rect.x, rect.y, h2 + h2/3, 2, &c);
    ADD_AREA(rect.x + rect.w, rect.y, h2 + h2/3, 2, &c);

    rect.x= hex_dial[0].boxu.x;
    rect.y=h2 * 70;
    rect.h=h2;
    rect.w= (hex_dial[3].boxd.x + hex_dial[3].boxd.w) - rect.x;
    ADD_AREA(rect.x, rect.y, rect.h, rect.w, &c);
    ADD_LABEL(rect.x, rect.y, rect.w, "MAIN STORAGE ADDRESS", c1, c);
    ADD_AREA(rect.x, rect.y, h2 + (3 * f1_hd), 2, &c);
    ADD_AREA(rect.x + rect.w, rect.y, h2 + (3 * f1_hd), 2, &c);

    rect.x= store_dial[0].rect.x;
    rect.y=h2 * 69 - (h2 / 2);
    rect.h=h2;
    rect.w= store_dial[0].rect.w;
    ADD_AREA(rect.x, rect.y, rect.h, rect.w, &c);
    ADD_LABEL3(rect.x + (f1_wd/2), rect.y, "DISPLAY STOR SEL", c1, c);
    ADD_AREA(rect.x, rect.y, h2 + (5*f1_hd), 2, &c);
    ADD_AREA(rect.x + rect.w,  rect.y, h2 + (5*f1_hd), 2, &c);

    rect.x= hex_dial[4].boxu.x;
    rect.y=h2 * 69 - (h2/2);
    rect.h=h2;
    rect.w= (hex_dial[7].boxd.x + hex_dial[7].boxd.w) - rect.x;
    ADD_AREA(rect.x, rect.y, rect.h, rect.w, &c);
    ADD_LABEL(rect.x, rect.y, rect.w, "INSTRUCTION ADDRESS - ROS ADDRESS", c1, c);
    ADD_AREA(rect.x, rect.y, h2 + (5 * f1_hd), 2, &c);
    ADD_AREA(rect.x + rect.w, rect.y, h2 + (h2 / 3), 2, &c);
    rect.x= hex_dial[5].boxu.x;
    rect.y=h2 * 70;
    rect.h=h2;
    rect.w= (hex_dial[7].boxd.x + hex_dial[7].boxd.w) - rect.x;
    ADD_AREA(rect.x, rect.y, rect.h, rect.w, &c);
    ADD_LABEL(rect.x, rect.y, rect.w, "LOAD UNIT", c1, c);
    ADD_AREA(rect.x, rect.y, h2 + (3 * f1_hd), 2, &c);
    ADD_AREA(rect.x + rect.w, rect.y, h2 + (h2 / 3), 2, &c);

    rect.x= hex_dial[6].boxu.x;
    rect.y=h2 * 71 + (h2/2);
    rect.h=h2;
    rect.w= (hex_dial[7].boxd.x + hex_dial[7].boxd.w) - rect.x;
    ADD_AREA(rect.x, rect.y, rect.h, rect.w, &c);
    ADD_LABEL(rect.x, rect.y, rect.w, "DATA", c1, c);
    ADD_AREA(rect.x, rect.y, h2 + f1_hd, 2, &c);
    ADD_AREA(rect.x + rect.w, rect.y, h2 + f1_hd, 2, &c);
    ADD_LABEL2(790+(f1_wd * 10), (h2 * 78) + (h2/2), "SYS");
    ADD_LABEL2(790+(f1_wd * 15), (h2 * 78) + (h2/2), "MAN");
    ADD_LABEL2(790+(f1_wd * 20), (h2 * 78) + (h2/2), "WAIT");
    ADD_LABEL2(790+(f1_wd * 25), (h2 * 78) + (h2/2), "TEST");
    ADD_LABEL2(790+(f1_wd * 30), (h2 * 78) + (h2/2), "LOAD");

    /* Add in status lights */
    lamp[0].rect.x = 790 + (f1_wd * 10);
    lamp[0].rect.y = h2 * 79 + (h2/2);
    lamp[0].rect.h = 15;
    lamp[0].rect.w = 15;
    lamp[0].col = 0;
    lamp[0].value = &clock_start_lch;
    lamp[0].shift = 0;
    lamp[1].rect.x = 790 + (f1_wd * 15);
    lamp[1].rect.y = h2 * 79 + (h2/2);
    lamp[1].rect.h = 15;
    lamp[1].rect.w = 15;
    lamp[1].col = 0;
    lamp[1].value = &allow_man_operation;
    lamp[1].shift = 0;
    lamp[2].rect.x = 790 + (f1_wd * 20);
    lamp[2].rect.y = h2 * 79 + (h2/2);
    lamp[2].rect.h = 15;
    lamp[2].rect.w = 15;
    lamp[2].col = 0;
    lamp[2].value = &wait;
    lamp[2].shift = 0;
    lamp[3].rect.x = 790 + (f1_wd * 25);
    lamp[3].rect.y = h2 * 79 + (h2/2);
    lamp[3].rect.h = 15;
    lamp[3].rect.w = 15;
    lamp[3].col = 1;
    lamp[3].value = &test_mode;
    lamp[3].shift = 0;
    lamp[4].rect.x = 790 + (f1_wd * 30);
    lamp[4].rect.y = h2 * 79 + (h2/2);
    lamp[4].rect.h = 15;
    lamp[4].rect.w = 15;
    lamp[4].col = 0;
    lamp[4].value = &load_mode;
    lamp[4].shift = 0;
    lamp_ptr = 5;
}

