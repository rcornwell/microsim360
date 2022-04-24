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

#include "panel.h"
#include "model2050.h"
#include "rollers.xpm"

#define DIG_LP   17
#define DIG_CD   18
#define DIG_CC   19
#define DIG_SLI  20
#define DIG_SKP  21
#define DIG_PCI  22
#define DIG_OP   23

static struct _label3 {
    char  *upper;
    char  *middle;
    char  *lower;
} label3[] = {
    { "0", NULL, NULL },
    { "1", NULL, NULL },
    { "2", NULL, NULL },
    { "3", NULL, NULL },
    { "4", NULL, NULL },
    { "5", NULL, NULL },
    { "6", NULL, NULL },
    { "7", NULL, NULL },
    { "8", NULL, NULL },
    { "9", NULL, NULL },
    { "A", NULL, NULL },
    { "B", NULL, NULL },
    { "C", NULL, NULL },
    { "D", NULL, NULL },
    { "E", NULL, NULL },
    { "F", NULL, NULL },
    { NULL, NULL, "PASS" },
    { NULL, NULL, "FAIL" },
    { NULL, "BINARY", "TGR" },
    { "TEST", "CNTR", "=0" },
    { NULL, NULL, "0" },
    { NULL, NULL, "1" },
    { NULL, NULL, "2" },
    { NULL, NULL, "3" },
    { NULL, NULL, "4" },
    { NULL, NULL, "5" },
    { NULL, NULL, "4" },
    { NULL, NULL, "2" },
    { NULL, NULL, "1" },
    { NULL, NULL, "1" },
    { NULL, NULL, "2" },
    { NULL, NULL, "3" },
    { NULL, NULL, "4" },
    { "FLT", "LOAD", "CHK" },
    { NULL, NULL, NULL },
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
    { NULL, NULL }
};

             /*            1         2           */
             /*  0123456789012345678901234567890 */
char *row1 =    "P012345  P L P18421  P84218421";
char *row1a=    "             P34567  P01234567";
int   pos1[32];
             /*            1         2           */
             /*  0123456789012345678901234567890 */
char *row2 =    "P0123 0123 A0123 0101201AP0123";
int   pos2[32];
             /*            1         2           */
             /*  0123456789012345678901234567890 */
char *row3 =    "P 0123      012  01 01012A0123";
int   pos3[32];
char *row_cnt = "     P84218421 P84217421      ";
char *row_cnta= "     P01234576 P01234576      ";
int   pos_cnt[32];

char *chan_one = "     P84218421 P8421 8421     ";
char *chan_onea= "     P01234576 P0123 4567     ";
int   pos_chan1[32];
int   pos_chan2[32];
char *chan_two = "     P84218421 P8421 8421     ";
char *chan_twoa= "     P01234576 P0123 4567     ";

char *store_addr = "P84218421 P84218421 ";
char *store_addra= "P01234567 P01234567 ";
char *data_reg = "P84218421  84218421 ";
int pos_mpx[32];
int pos_store[32];
int pos_main[32];
int pos_data[32];
int pos_breg[32];

static int switch_offset[33] = {
   /* 0  1   2   3   4   5   6   7   8   9 */
     60, 24, 23, 24, 23, 24, 23, 24, 47, 23,
   /*10  11  12  13  14  15  16  17  18  19*/
     24, 23, 25, 23, 24, 23, 76, 25, 24, 25,
   /*20  21  22  23  24  25  26  27  28  29*/
     24, 25, 24, 25, 24, 24+ 24, 24, 23, 23,
   /*30  31  32  33  34  35  */
     23, 23, 23, 23
};

static int lamp_offset[36] = {
   /* 0  1   2   3   4   5   6   7   8   9 */
      0, 24, 23, 24, 23, 24, 23, 24, 23, 24,
   /*10  11  12  13  14  15  16  17  18  19*/
     23, 24, 23, 25, 23, 24, 23, 24, 52, 25,
   /*20  21  22  23  24  25  26  27  28  29*/
     24, 25, 24, 25, 24, 25, 24, 24, 24, 24,
   /*30  31  32  33  34  35  */
     23, 23, 23, 23, 23, 23
};



void
setup_fp2050(SDL_Renderer *render)
{
    SDL_Rect rect, rect2;
    SDL_Surface *text;
    SDL_Texture *txt;
    SDL_Texture *roll;
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

    text = TTF_RenderText_Shaded(font1, "M", c, cb);
    txt = SDL_CreateTextureFromSurface( render, text);
    SDL_QueryTexture(txt, &f, &k, &w2, &h2);
    f1_wd = w2;
    f1_hd = h2;
    SDL_FreeSurface(text);
   printf(" H1=%d W1=%d\n", f1_wd, f1_hd);
    text = TTF_RenderText_Shaded(font1, "ON", c1, c);
    on = SDL_CreateTextureFromSurface( render, text);
    SDL_FreeSurface(text);
    text = TTF_RenderText_Shaded(font1, "OFF", c1, c);
    off = SDL_CreateTextureFromSurface( render, text);
    SDL_FreeSurface(text);
    wb = (strlen(row1) + 1) * (3 * f1_wd);

    text = IMG_ReadXPMFromArray(rollers_img);
    roll = SDL_CreateTextureFromSurface( render, text);
    SDL_FreeSurface(text);

    /* Draw top of display */
    ADD_AREA(0, 0, 975, 1100, &cc);

    /* Draw bottom switch panel */
    ADD_AREA(0, (65 * h2) - (f1_hd/4), (32 * h2), 1100, &cl);

    roller[0].rollers = roll;
    roller[0].pos.x = 150;
    roller[0].pos.y = 100;
    roller[0].sel = 0;
    roller[0].ystart = 0;
    roller_ptr++;
    roller[1].rollers = roll;
    roller[1].pos.x = 150;
    roller[1].pos.y = 200;
    roller[1].sel = 0;
    roller[1].ystart = 8 * 25;
    roller_ptr++;
    roller[2].rollers = roll;
    roller[2].pos.x = 150;
    roller[2].pos.y = 300;
    roller[2].sel = 0;
    roller[2].ystart = 16 * 25;
    roller_ptr++;
    roller[3].rollers = roll;
    roller[3].pos.x = 150;
    roller[3].pos.y = 400;
    roller[3].sel = 0;
    roller[3].ystart = 24 * 25;
    roller_ptr++;

#if 0
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
#endif

    add_switch(&SYS_RST, 560, (h2 * 74), f1_wd * 10, f1_hd * 2, SW, &c3, &sw_labels[0], font1);
    add_switch(&SET_IC, 560, (h2 * 76) + (h2/2), f1_wd * 10, f1_hd * 2, SW, &c3, &sw_labels[3], font1);
    add_switch(&STOP, 560, (h2 * 79), f1_wd * 10, f1_hd * 2, SW, &c5, &sw_labels[5], font1);
    add_switch(&ROAR_RST,630, (h2 * 74), f1_wd * 10, f1_hd * 2, SW, &c3, &sw_labels[1], font1);
    add_switch(&STORE, 630, (h2 * 76) + (h2/2), f1_wd * 10, f1_hd * 2, SW, &c3, &sw_labels[7], font1);
    add_switch(&LAMP_TEST, 630, (h2 * 79), f1_wd * 10, f1_hd * 2, SW, &c, NULL, NULL);
    add_switch(&CHECK_RST, 700, (h2 * 74), f1_wd * 10, f1_hd * 2, SW, &c3, &sw_labels[4], font1);
    add_switch(&DISPLAY, 700, (h2 * 76) + (h2/2), f1_wd * 10, f1_hd * 2, SW, &c3, &sw_labels[9], font1);
    add_switch(NULL, 700, (h2 * 79), f1_wd * 10, f1_hd * 2, SW, &c3, &sw_labels[8], font1);
    add_switch(NULL, 780, (h2 * 65), f1_wd * 10, f1_hd * 2, SW, &c, &sw_labels[10], font1);
    add_switch(&POWER, 1000, (h2 * 65), f1_wd * 10, f1_hd * 2, SW, &c5, &sw_labels[11], font1);
    add_switch(&INTR, 780, (h2 * 78), f1_wd * 10, f1_hd * 2, SW, &c5, &sw_labels[12], font1);
    add_switch(&LOAD, 1000, (h2 * 78), f1_wd * 10, f1_hd * 2, SW, &c3, &sw_labels[13], font1);
    add_switch(&START, 460, (h2 * 79), f1_wd * 10, f1_hd * 2, SW, &c2, &sw_labels[2], font1);
    ADD_LINE(460 + (f1_wd * 10), (h2 * 80), (f1_wd * 10), c1);
    marks[mrk_ptr].x1 = 770;
    marks[mrk_ptr].y1 = (h2 * 65) - 3;
    marks[mrk_ptr].x2 = 1065;
    marks[mrk_ptr].y2 = (h2 * 65) - 3;
    marks[mrk_ptr].c = &c;
    mrk_ptr++;
    marks[mrk_ptr].x1 = 770;
    marks[mrk_ptr].y1 = (h2 * 67) + 3;
    marks[mrk_ptr].x2 = 1065;
    marks[mrk_ptr].y2 = (h2 * 67) + 3;
    marks[mrk_ptr].c = &c;
    mrk_ptr++;
    marks[mrk_ptr].x1 = 770;
    marks[mrk_ptr].y1 = (h2 * 65) - 3;
    marks[mrk_ptr].x2 = 770;
    marks[mrk_ptr].y2 = (h2 * 67) + 3;
    marks[mrk_ptr].c = &c;
    mrk_ptr++;
    marks[mrk_ptr].x1 = 1065;
    marks[mrk_ptr].y1 = (h2 * 65) - 3;
    marks[mrk_ptr].x2 = 1065;
    marks[mrk_ptr].y2 = (h2 * 67) + 3;
    marks[mrk_ptr].c = &c;
    mrk_ptr++;
    marks[mrk_ptr].x1 = 770;
    marks[mrk_ptr].y1 = (h2 * 78) - 5;
    marks[mrk_ptr].x2 = 1065;
    marks[mrk_ptr].y2 = (h2 * 78) - 5;
    marks[mrk_ptr].c = &c1;
    mrk_ptr++;
    marks[mrk_ptr].x1 = 770;
    marks[mrk_ptr].y1 = (h2 * 78) - 5;
    marks[mrk_ptr].x2 = 770;
    marks[mrk_ptr].y2 = (h2 * 80) + 3;
    marks[mrk_ptr].c = &c1;
    mrk_ptr++;
    marks[mrk_ptr].x1 = 770;
    marks[mrk_ptr].y1 = (h2 * 80) + 3;
    marks[mrk_ptr].x2 = 1065;
    marks[mrk_ptr].y2 = (h2 * 80) + 3;
    marks[mrk_ptr].c = &c1;
    mrk_ptr++;
    marks[mrk_ptr].x1 = 1065;
    marks[mrk_ptr].y1 = (h2 * 78) - 5;
    marks[mrk_ptr].x2 = 1065;
    marks[mrk_ptr].y2 = (h2 * 80) + 3;
    marks[mrk_ptr].c = &c1;
    mrk_ptr++;

    ADD_LABEL(430,(h2 * 65), (f1_wd * 23), "FLT CONTROL", c1, c4);
    ADD_LABEL(430 + (f1_wd * 18), (h2 * 67), f1_wd * 4, "AUTO", c1, c4);
    ADD_LABEL(425 + (f1_wd * 18), (h2 * 68), f1_wd * 7, "REREAD", c1, c4);
    ADD_LABEL(425, (h2 * 66), f1_wd * 23, "PROCESS", c1, c4);
    ADD_LABEL(425 + (f1_wd * 18), (h2 * 70), f1_wd * 4, "HALT", c1, c4);
    ADD_LABEL(425 + (f1_wd * 18), (h2 * 71), f1_wd * 5, "AFTER", c1, c4);
    ADD_LABEL(425 + (f1_wd * 18), (h2 * 72), f1_wd * 4, "LOAD", c1, c4);
    ADD_LABEL(425, (h2 * 67), f1_wd * 6, "REPEAT", c1, c4);
    ADD_LABEL(425, (h2 * 71), f1_wd * 4, "STOP", c1, c4);
//    ADD_LINE(425 + (f1_wd * 11), (h2 * 66), (f1_wd * 3), c1);
//    ADD_LINE(425 + (f1_wd * 26)+1, (h2 * 71), (f1_wd * 3) - 1, c1);
    dial[0].boxd.x = 425;
    dial[0].boxd.y = h2 * 65;
    dial[0].boxd.w = (f1_wd * 12);
    dial[0].boxd.h = h2 * 6;
    dial[0].boxu.x = 425 + (f1_wd * 12);
    dial[0].boxu.y = h2 * 65;
    dial[0].boxu.w = (f1_wd * 12);
    dial[0].boxu.h = h2 * 6;
    dial[0].center_x = 425 + (f1_wd * 12);
    dial[0].center_y = (h2 * 70);
    dial[0].pos_x[0] = 425 + (f1_wd * 6);
    dial[0].pos_x[1] = 425 + (f1_wd * 12);
    dial[0].pos_x[2] = 425 + (f1_wd * 18);
    dial[0].pos_x[3] = 425 + (f1_wd * 18);
    dial[0].pos_x[4] = 425 + (f1_wd * 6);
    dial[0].pos_y[0] = h2 * 68;
    dial[0].pos_y[1] = h2 * 67;
    dial[0].pos_y[2] = h2 * 68;
    dial[0].pos_y[3] = h2 * 71;
    dial[0].pos_y[4] = h2 * 72;
    dial[0].init = 1;
    dial[0].value = &PROC_SW;
    dial[0].max = 4;
    dial[0].wrap = 0;

    ADD_LABEL(430, (h2 * 72), (f1_wd * 23), "RATE", c1, c4);
    ADD_LABEL(430, (h2 * 73), f1_wd * 23, "PROCESS", c1, c4);
    ADD_LABEL(425, (h2 * 74), (f1_wd * 5), "INSTR", c1, c4);
    ADD_LABEL(425, (h2 * 75), (f1_wd * 4), "STEP", c1, c4);
    ADD_LABEL(425 + (f1_wd * 18), (h2 * 74), f1_wd * 5, "SINGLE", c1, c4);
    ADD_LABEL(425 + (f1_wd * 18) - (f1_wd/2), (h2 * 75), f1_wd * 5, "CYCLE", c1, c4);
//    ADD_LINE(425 + (f1_wd * 9), (h2 * 75), (f1_wd * 1), c1);
//    ADD_LINE(425 + (f1_wd * 16), (h2 * 75), (f1_wd * 2), c1);
    dial[1].boxd.x = 425;
    dial[1].boxd.y = h2 * 71;
    dial[1].boxd.w = (f1_wd * 12);
    dial[1].boxd.h = h2 * 6;
    dial[1].boxu.x = 425 + (f1_wd * 12);
    dial[1].boxu.y = h2 * 71;
    dial[1].boxu.w = (f1_wd * 12);
    dial[1].boxu.h = h2 * 6;
    dial[1].center_x = 425 + (f1_wd * 12);
    dial[1].center_y = (h2 * 76) + (h2/2);
    dial[1].pos_x[0] = 425 + (f1_wd * 6);
    dial[1].pos_x[1] = 425 + (f1_wd * 12);
    dial[1].pos_x[2] = 425 + (f1_wd * 18);
    dial[1].pos_y[0] = h2 * 75;
    dial[1].pos_y[1] = h2 * 74;
    dial[1].pos_y[2] = h2 * 75;
    dial[1].init = 1;
    dial[1].value = &RATE_SW;
    dial[1].max = 2;
    dial[1].wrap = 0;

    ADD_LABEL(600, (h2 * 65), f1_wd*23, "CHECK CONTROL", c1, c4);
    ADD_LABEL(600, (h2 * 66), f1_wd*23, "PROCESS", c1, c4);
    ADD_LABEL(600 + (f1_wd * 20), (h2 * 67) + (f1_hd/2), f1_wd * 7, "DISABLE", c1, c4);
    ADD_LINE(600 + (f1_wd * 17), (h2 * 68), (f1_wd * 3), c1);
    ADD_LABEL(600 + (f1_wd * 20), (h2 * 71) + (h2 / 2), f1_wd * 4, "STOP", c1, c4);
    ADD_LINE(600 + (f1_wd * 17), (h2 * 72), (f1_wd * 3), c1);
    ADD_LABEL(600, (h2 * 70), f1_wd * 4, "CHAN", c1, c4);
    ADD_LABEL(600, (h2 * 71), f1_wd * 4, "STOP", c1, c4);
    ADD_LINE(600 + (f1_wd * 4), (h2 * 71), (f1_wd * 2), c1);
    dial[2].boxd.x = 600;
    dial[2].boxd.y = h2 * 65;
    dial[2].boxd.w = (f1_wd * 12);
    dial[2].boxd.h = h2 * 6;
    dial[2].boxu.x = 600 + (f1_wd * 12);
    dial[2].boxu.y = h2 * 65;
    dial[2].boxu.w = (f1_wd * 12);
    dial[2].boxu.h = h2 * 6;
    dial[2].center_x = 600 + (f1_wd * 12);
    dial[2].center_y = (h2 * 70);
    dial[2].pos_x[0] = 600 + (f1_wd * 12);
    dial[2].pos_x[1] = 600 + (f1_wd * 17);
    dial[2].pos_x[2] = 600 + (f1_wd * 17);
    dial[2].pos_x[3] = 600 + (f1_wd * 6);;
    dial[2].pos_y[0] = h2 * 67;
    dial[2].pos_y[1] = (h2 * 68);
    dial[2].pos_y[2] = (h2 * 72);
    dial[2].pos_y[3] = (h2 * 71);
    dial[2].init = 0;
    dial[2].value = &CHK_SW;
    dial[2].max = 3;
    dial[2].wrap = 0;

    ADD_LABEL(10, (h2 * 50), f1_wd*23, "CHECK CONTROL", c1, cc);
    ADD_LABEL(10 - (f1_wd *5), (h2 * 50) + (f1_hd/2), f1_wd*7, "DISABLE", c1, cc);
    ADD_LINE(10 + (f1_wd * 3), (h2 * 50), (f1_wd * 4), c1);
    ADD_LABEL(10, (h2 * 48) - (f1_hd/2), f1_wd*23, "PROCESS", c1, cc);
    ADD_LABEL(10 + (f1_wd * 24), (h2 * 50) + (f1_hd/2), f1_wd*4, "STOP", c1, cc);
    ADD_LINE(10 + (f1_wd * 16), (h2 * 50), (f1_wd * 6), c1);
    ADD_LABEL(10 + (f1_wd * 21), (h2 * 50) + (f1_hd/2), f1_wd*7, "RESTART", c1, cc);
    dial[3].boxd.x = 10;
    dial[3].boxd.y = h2 * 66;
    dial[3].boxd.w = (f1_wd * 12);
    dial[3].boxd.h = h2 * 5;
    dial[3].boxu.x = 10 + (f1_wd * 12);
    dial[3].boxu.y = h2 * 66;
    dial[3].boxu.w = (f1_wd * 12);
    dial[3].boxu.h = h2 * 5;
    dial[3].center_x = 10 + (f1_wd * 12);
    dial[3].center_y = (h2 * 51);
    dial[3].pos_x[0] = 10 + (f1_wd * 6);
    dial[3].pos_x[1] = 10 + (f1_wd * 7);
    dial[3].pos_x[2] = 10 + (f1_wd * 12);
    dial[3].pos_x[3] = 10 + (f1_wd * 16);
    dial[3].pos_x[4] = 10 + (f1_wd * 18);
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
    CHK_SW = dial[2].init;

    rect.x = 800;
    for (i = 0; i < 3; i++ ) {
        hex_dial[i].rect.x = rect.x + f1_wd;
        hex_dial[i].rect.y = (h2 * 70);
        hex_dial[i].rect.w = 64;
        hex_dial[i].rect.h = 64;
        hex_dial[i].boxu.x = rect.x + f1_wd;
        hex_dial[i].boxu.y = (h2 * 70);
        hex_dial[i].boxu.w = 32;
        hex_dial[i].boxu.h = 64;
        hex_dial[i].boxd.x = rect.x + f1_wd + 32;
        hex_dial[i].boxd.y = (h2 * 70);
        hex_dial[i].boxd.w = 32;
        hex_dial[i].boxd.h = 64;
        hex_ptr++;
        rect.x += (f1_wd * 15);
    }

    hex_dial[0].digit = &A_SW;
    hex_dial[1].digit = &B_SW;
    hex_dial[2].digit = &C_SW;

    ADD_LABEL2(790+(f1_wd * 10), (h2 * 77) + (h2/2), "SYS");
    ADD_LABEL2(790+(f1_wd * 15), (h2 * 77) + (h2/2), "MAN");
    ADD_LABEL2(790+(f1_wd * 20), (h2 * 77) + (h2/2), "WAIT");
    ADD_LABEL2(790+(f1_wd * 25), (h2 * 77) + (h2/2), "TEST");
    ADD_LABEL2(790+(f1_wd * 30), (h2 * 77) + (h2/2), "LOAD");

    /* Add in status lights */
    lamp[0].rect.x = 790 + (f1_wd * 10);
    lamp[0].rect.y = h2 * 78 + (h2/2);
    lamp[0].rect.h = 15;
    lamp[0].rect.w = 15;
    lamp[0].col = 0;
    lamp[0].value = &clock_start_lch;
    lamp[0].shift = 0;
    lamp[1].rect.x = 790 + (f1_wd * 15);
    lamp[1].rect.y = h2 * 78 + (h2/2);
    lamp[1].rect.h = 15;
    lamp[1].rect.w = 15;
    lamp[1].col = 0;
    lamp[1].value = &allow_man_operation;
    lamp[1].shift = 0;
    lamp[2].rect.x = 790 + (f1_wd * 20);
    lamp[2].rect.y = h2 * 78 + (h2/2);
    lamp[2].rect.h = 15;
    lamp[2].rect.w = 15;
    lamp[2].col = 0;
    lamp[2].value = &wait;
    lamp[2].shift = 0;
    lamp[3].rect.x = 790 + (f1_wd * 25);
    lamp[3].rect.y = h2 * 78 + (h2/2);
    lamp[3].rect.h = 15;
    lamp[3].rect.w = 15;
    lamp[3].col = 1;
    lamp[3].value = &test_mode;
    lamp[3].shift = 0;
    lamp[4].rect.x = 790 + (f1_wd * 30);
    lamp[4].rect.y = h2 * 78 + (h2/2);
    lamp[4].rect.h = 15;
    lamp[4].rect.w = 15;
    lamp[4].col = 0;
    lamp[4].value = &load_mode;
    lamp[4].shift = 0;
    lamp_ptr = 5;
    j = 150 + 35;
    for (i = 0; i <= 36; i++) {
            lamp[lamp_ptr].rect.x = j;
            lamp[lamp_ptr].rect.y = 44 * h2;
            lamp[lamp_ptr].rect.w = 15;
            lamp[lamp_ptr].rect.h = 15;
            lamp[lamp_ptr].col = 0;
            lamp[lamp_ptr].value = 0;
            lamp[lamp_ptr].shift = 0;
            lamp_ptr++;
        j += lamp_offset[i];
    }
    j = 150 + 35;
    for (i = 0; i <= 36; i++) {
            lamp[lamp_ptr].rect.x = j;
            lamp[lamp_ptr].rect.y = 49 * h2;
            lamp[lamp_ptr].rect.w = 15;
            lamp[lamp_ptr].rect.h = 15;
            lamp[lamp_ptr].col = 0;
            lamp[lamp_ptr].value = 0;
            lamp[lamp_ptr].shift = 0;
            lamp_ptr++;
        j += lamp_offset[i];
    }
    j = 150;
    for (i = 31; i >=0; i--) {
        j += switch_offset[31-i];
        add_toggle(&cpu_2050.DKEYS, i, j, 52*h2, ON_OFF);
    }

    j = 150 + 35;
    for (i = 0; i <= 36; i++) {
        if (i > 9) {
            lamp[lamp_ptr].rect.x = j;
            lamp[lamp_ptr].rect.y = 58 * h2;
            lamp[lamp_ptr].rect.w = 15;
            lamp[lamp_ptr].rect.h = 15;
            lamp[lamp_ptr].col = 0;
            lamp[lamp_ptr].value = 0;
            lamp[lamp_ptr].shift = 0;
            lamp_ptr++;
        }
        j += lamp_offset[i];
    }
    j = 150;
    for (i = 31; i >=0; i--) {
        j += switch_offset[31-i];
        if (i < 24) {
            add_toggle(&cpu_2050.AKEYS, i, j, 61*h2, ON_OFF);
        }
    }

}

