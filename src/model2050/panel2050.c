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
    { NULL, "SUPV", "STAT" },
    { "PROGRAM", "SCAN", "S2" },
    { "SUPV", "ENABLE", "STOP" },
    { NULL, "SEQ", "CNTR" },
    { NULL, "MAIN", "STOP" },
    { NULL, "ROS", "MODE" },
    { "ALT", "PRE-", "VSE" },
    { NULL, "HARD", "STOP" },
    { NULL, "LOG", "TGR" },
    { NULL, "BLOCK", "MODE" },
    { NULL, "SINGLE", "CYCLE" },
    { NULL, NULL, "CPU" },
    { NULL, NULL, "CHAN" },
    { NULL, NULL, "AUX" },
    { NULL, "MAIN", "STOP" },
    { "CHK","IRPT", "ENABLED" },
    { "LINK", "REG", "STATE" },
    { NULL, "CHK", "PEND" },
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
    { "ENTER", NULL },
    { NULL, NULL }
};


uint32_t      ADR_CMP;
uint32_t      INST_REP;
uint32_t      ROS_CMP;
uint32_t      ROS_REP;
uint32_t      SAR_CMP;
uint32_t      FORC_IND;
uint32_t      FLT_MODE;
uint32_t      CHN_MODE;
uint8_t       SEL_SW;
int           SEL_ENTER;


static int switch_offset[33] = {
   /* 0  1   2   3   4   5   6   7   8   9 */
     60, 24, 23, 24, 23, 24, 23, 24, 47, 23,
   /*10  11  12  13  14  15  16  17  18  19*/
     24, 23, 25, 23, 24, 23, 76, 25, 24, 25,
   /*20  21  22  23  24  25  26  27  28  29*/
     24, 25, 24, 25, 48, 24, 24, 23, 23, 23,
   /*30  31  32  33  34  35  */
     23, 23, 23
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

static int roller_shift[36] = {
     24,     31, 30, 29, 28, 27, 26, 25, 24,
     16,     23, 22, 21, 20, 19, 18, 17, 16,
      8,     15, 14, 13, 12, 11, 10,  9,  8,
      0,      7,  6,  5,  4,  3,  2,  1,  0
};

static int roller_mask[36] = {
     0xff,    0,  0,  0,  0,  0,  0,  0,  0,
     0xff,    0,  0,  0,  0,  0,  0,  0,  0,
     0xff,    0,  0,  0,  0,  0,  0,  0,  0,
     0xff,    0,  0,  0,  0,  0,  0,  0,  0
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
    int	     hx, wx;
    int      h2, w2;
    int      mark[40];
    char     buffer[10];
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
    text = TTF_RenderText_Shaded(font1, "ON", c1, c);
    on = SDL_CreateTextureFromSurface( render, text);
    SDL_FreeSurface(text);
    text = TTF_RenderText_Shaded(font1, "OFF", c1, c);
    off = SDL_CreateTextureFromSurface( render, text);
    SDL_FreeSurface(text);

    text = IMG_ReadXPMFromArray(rollers_img);
    roll = SDL_CreateTextureFromSurface( render, text);
    SDL_FreeSurface(text);
    cpu_2050.L_REG = 0xaa55aa55;
    cpu_2050.M_REG = 0x55aa55aa;

    /* Draw top of display */
    ADD_AREA(0, 0, 975, 1100, &cc);

    /* Draw bottom switch panel */
    ADD_AREA(0, (65 * h2) - (f1_hd/4), (32 * h2), 1100, &cl);

    roller[0].rollers = roll;
    roller[0].pos.x = 150;
    roller[0].pos.y = 100;
    roller[0].sel = 0;
    roller[0].ystart = 0;
    SET_INDICATOR8(&roller[0].disp[0].ind[0], &cpu_2050.CHCTL, 0, 0);
    SET_INDICATOR8(&roller[0].disp[0].ind[1], &cpu_2050.CHCTL, 2, 0);
    SET_INDICATOR8(&roller[0].disp[0].ind[2], &cpu_2050.CHCTL, 1, 0);
    SET_INDICATOR8(&roller[0].disp[0].ind[3], &cpu_2050.CHCTL, 3, 0);
    SET_INDICATOR8(&roller[0].disp[0].ind[4], &cpu_2050.CH, 2, 0);
    SET_INDICATOR8(&roller[0].disp[0].ind[5], &cpu_2050.CH, 1, 0);
    SET_INDICATOR8(&roller[0].disp[0].ind[6], &cpu_2050.CH, 0, 0);
    SET_INDICATOR8(&roller[0].disp[0].ind[7], NULL, 3, 0);     /* Instruction reply 0 */
    SET_INDICATOR8(&roller[0].disp[0].ind[8], NULL, 3, 0);     /* Instruction reply 1 */
    SET_INDICATOR8(&roller[0].disp[0].ind[9], NULL, 3, 0);     /* Instruction reply 2 */
    SET_INDICATOR8(&roller[0].disp[0].ind[10], NULL, 3, 0);    /* Instruction reply 3 */
    SET_INDICATOR8(&roller[0].disp[0].ind[11], NULL, 3, 0);    /* Reply, BCHI */
    SET_INDICATOR8(&roller[0].disp[0].ind[12], NULL, 3, 0);    /* Interrupt pending */
    SET_INDICATOR8(&roller[0].disp[0].ind[13], NULL, 3, 0);    /* Reply to BCHI */
    SET_INDICATOR8(&roller[0].disp[0].ind[15], NULL, 3, 0);    /* Time out */
    SET_INDICATOR8(&roller[0].disp[0].ind[16], NULL, 3, 0);    /* Time out check */
    SET_INDICATOR8(&roller[0].disp[0].ind[17], &cpu_2050.CHCTL, 1, 0);    /* Foul */
    roller[1].rollers = roll;
    roller[1].pos.x = 150;
    roller[1].pos.y = 200;
    roller[1].sel = 0;
    roller[1].ystart = 8 * 25;
    roller[2].rollers = roll;
    roller[2].pos.x = 150;
    roller[2].pos.y = 300;
    roller[2].sel = 0;
    roller[2].ystart = 16 * 25;
    roller[3].rollers = roll;
    roller[3].pos.x = 150;
    roller[3].pos.y = 400;
    roller[3].sel = 0;
    roller[3].ystart = 24 * 25;
    SET_INDICATOR8(&roller[3].disp[2].ind[0], &cpu_2050.SYLS1, 0, 0);
    SET_INDICATOR8(&roller[3].disp[2].ind[1], &cpu_2050.REFETCH, 0, 0);
    SET_INDICATOR16(&roller[3].disp[2].ind[6], &cpu_2050.NROAR, 11, 0);
    SET_INDICATOR16(&roller[3].disp[2].ind[7], &cpu_2050.NROAR, 10, 0);
    SET_INDICATOR16(&roller[3].disp[2].ind[8], &cpu_2050.NROAR, 9, 0);
    SET_INDICATOR16(&roller[3].disp[2].ind[9], &cpu_2050.NROAR, 8, 0);
    SET_INDICATOR16(&roller[3].disp[2].ind[10], &cpu_2050.NROAR, 7, 0);
    SET_INDICATOR16(&roller[3].disp[2].ind[11], &cpu_2050.NROAR, 6, 0);
    SET_INDICATOR16(&roller[3].disp[2].ind[12], &cpu_2050.NROAR, 5, 0);
    SET_INDICATOR16(&roller[3].disp[2].ind[13], &cpu_2050.NROAR, 4, 0);
    SET_INDICATOR16(&roller[3].disp[2].ind[14], &cpu_2050.NROAR, 3, 0);
    SET_INDICATOR16(&roller[3].disp[2].ind[15], &cpu_2050.NROAR, 2, 0);
    SET_INDICATOR16(&roller[3].disp[2].ind[16], &cpu_2050.NROAR, 1, 0);
    SET_INDICATOR16(&roller[3].disp[2].ind[17], &cpu_2050.NROAR, 0, 0);
    /* Bits 18-23 External interrupt */
    SET_INDICATOR8(&roller[3].disp[2].ind[24], &cpu_2050.ILC, 1, 0);
    SET_INDICATOR8(&roller[3].disp[2].ind[25], &cpu_2050.ILC, 0, 0);
    SET_INDICATOR8(&roller[3].disp[2].ind[26], &cpu_2050.CC, 1, 0);
    SET_INDICATOR8(&roller[3].disp[2].ind[27], &cpu_2050.CC, 0, 0);
    SET_INDICATOR8(&roller[3].disp[2].ind[28], &cpu_2050.PMASK, 3, 0);
    SET_INDICATOR8(&roller[3].disp[2].ind[29], &cpu_2050.PMASK, 2, 0);
    SET_INDICATOR8(&roller[3].disp[2].ind[30], &cpu_2050.PMASK, 1, 0);
    SET_INDICATOR8(&roller[3].disp[2].ind[31], &cpu_2050.PMASK, 0, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[0], &cpu_2050.io_mode, 0, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[1], &cpu_2050.IO_REG, 0, 3);
    SET_INDICATOR8(&roller[3].disp[3].ind[2], &cpu_2050.IO_REG, 1, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[3], &cpu_2050.IO_REG, 0, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[4], NULL, 0, 0);   /* Timer Irpt */
    SET_INDICATOR8(&roller[3].disp[3].ind[5], NULL, 0, 0);   /* Cons Irpt */
    SET_INDICATOR8(&roller[3].disp[3].ind[6], &cpu_2050.LB_REG, 0, 3);
    SET_INDICATOR8(&roller[3].disp[3].ind[7], &cpu_2050.LB_REG, 1, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[8], &cpu_2050.LB_REG, 0, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[9], &cpu_2050.MB_REG, 0, 3);
    SET_INDICATOR8(&roller[3].disp[3].ind[10], &cpu_2050.MB_REG, 1, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[11], &cpu_2050.MB_REG, 0, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[12], &cpu_2050.F_REG, 0, 0xf);
    SET_INDICATOR8(&roller[3].disp[3].ind[13], &cpu_2050.F_REG, 3, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[14], &cpu_2050.F_REG, 2, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[15], &cpu_2050.F_REG, 1, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[16], &cpu_2050.F_REG, 0, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[17], &cpu_2050.Q_REG, 0, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[18], NULL, 1, 0);  /* Edit stats */
    SET_INDICATOR8(&roller[3].disp[3].ind[19], NULL, 0, 0);  /* Edit stats */
    SET_INDICATOR8(&roller[3].disp[3].ind[20], &cpu_2050.S_REG, 7, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[21], &cpu_2050.S_REG, 6, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[22], &cpu_2050.S_REG, 5, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[23], &cpu_2050.S_REG, 4, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[24], &cpu_2050.S_REG, 3, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[25], &cpu_2050.S_REG, 2, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[26], &cpu_2050.S_REG, 1, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[27], &cpu_2050.S_REG, 0, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[28], &cpu_2050.LSGNS, 0, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[29], &cpu_2050.RSGNS, 0, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[30], &cpu_2050.CAR, 0, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[31], NULL, 0, 0);  /* RTL */
    SET_INDICATOR8(&roller[3].disp[3].ind[32], &cpu_2050.mem_state, 0, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[33], &cpu_2050.mem_state, 1, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[34], &cpu_2050.mem_state, 2, 0);
    SET_INDICATOR8(&roller[3].disp[3].ind[35], &cpu_2050.mem_state, 3, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[0], &cpu_2050.LSA, 0, 0x3f);
    SET_INDICATOR8(&roller[3].disp[4].ind[1], &cpu_2050.LSA, 5, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[2], &cpu_2050.LSA, 4, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[3], &cpu_2050.LSA, 3, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[4], &cpu_2050.LSA, 2, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[5], &cpu_2050.LSA, 1, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[6], &cpu_2050.LSA, 0, 0);
    SET_INDICATOR16(&roller[3].disp[4].ind[7], &cpu_2050.FN, 1, 0);
    SET_INDICATOR16(&roller[3].disp[4].ind[8], &cpu_2050.FN, 0, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[9], &cpu_2050.J_REG, 0, 0xf);
    SET_INDICATOR8(&roller[3].disp[4].ind[10], &cpu_2050.J_REG, 3, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[11], &cpu_2050.J_REG, 2, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[12], &cpu_2050.J_REG, 1, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[13], &cpu_2050.J_REG, 0, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[14], &cpu_2050.MD_REG, 0, 0x7);
    SET_INDICATOR8(&roller[3].disp[4].ind[15], &cpu_2050.MD_REG, 2, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[16], &cpu_2050.MD_REG, 1, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[17], &cpu_2050.MD_REG, 0, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[18], NULL, 0, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[19], &cpu_2050.G1NEG, 0, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[20], &cpu_2050.G_REG, 0, 0xf0);
    SET_INDICATOR8(&roller[3].disp[4].ind[21], &cpu_2050.G_REG, 7, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[22], &cpu_2050.G_REG, 6, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[23], &cpu_2050.G_REG, 5, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[24], &cpu_2050.G_REG, 4, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[25], &cpu_2050.G2NEG, 0, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[26], &cpu_2050.G_REG, 0, 0xf);
    SET_INDICATOR8(&roller[3].disp[4].ind[27], &cpu_2050.G_REG, 3, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[28], &cpu_2050.G_REG, 2, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[29], &cpu_2050.G_REG, 1, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[30], &cpu_2050.G_REG, 0, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[31], NULL, 0, 0);  /* RTL */
    SET_INDICATOR8(&roller[3].disp[4].ind[32], NULL, 0, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[33], NULL, 0, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[34], NULL, 0, 0);
    SET_INDICATOR8(&roller[3].disp[4].ind[35], NULL, 0, 0);
    SET_INDICATOR16(&roller[3].disp[6].ind[6], &cpu_2050.ROAR, 11, 0);
    SET_INDICATOR16(&roller[3].disp[6].ind[7], &cpu_2050.ROAR, 10, 0);
    SET_INDICATOR16(&roller[3].disp[6].ind[8], &cpu_2050.ROAR, 9, 0);
    SET_INDICATOR16(&roller[3].disp[6].ind[9], &cpu_2050.ROAR, 8, 0);
    SET_INDICATOR16(&roller[3].disp[6].ind[10], &cpu_2050.ROAR, 7, 0);
    SET_INDICATOR16(&roller[3].disp[6].ind[11], &cpu_2050.ROAR, 6, 0);
    SET_INDICATOR16(&roller[3].disp[6].ind[12], &cpu_2050.ROAR, 5, 0);
    SET_INDICATOR16(&roller[3].disp[6].ind[13], &cpu_2050.ROAR, 4, 0);
    SET_INDICATOR16(&roller[3].disp[6].ind[14], &cpu_2050.ROAR, 3, 0);
    SET_INDICATOR16(&roller[3].disp[6].ind[15], &cpu_2050.ROAR, 2, 0);
    SET_INDICATOR16(&roller[3].disp[6].ind[16], &cpu_2050.ROAR, 1, 0);
    SET_INDICATOR16(&roller[3].disp[6].ind[17], &cpu_2050.ROAR, 0, 0);
    SET_INDICATOR16(&roller[3].disp[7].ind[6], &cpu_2050.PROAR, 11, 0);
    SET_INDICATOR16(&roller[3].disp[7].ind[7], &cpu_2050.PROAR, 10, 0);
    SET_INDICATOR16(&roller[3].disp[7].ind[8], &cpu_2050.PROAR, 9, 0);
    SET_INDICATOR16(&roller[3].disp[7].ind[9], &cpu_2050.PROAR, 8, 0);
    SET_INDICATOR16(&roller[3].disp[7].ind[10], &cpu_2050.PROAR, 7, 0);
    SET_INDICATOR16(&roller[3].disp[7].ind[11], &cpu_2050.PROAR, 6, 0);
    SET_INDICATOR16(&roller[3].disp[7].ind[12], &cpu_2050.PROAR, 5, 0);
    SET_INDICATOR16(&roller[3].disp[7].ind[13], &cpu_2050.PROAR, 4, 0);
    SET_INDICATOR16(&roller[3].disp[7].ind[14], &cpu_2050.PROAR, 3, 0);
    SET_INDICATOR16(&roller[3].disp[7].ind[15], &cpu_2050.PROAR, 2, 0);
    SET_INDICATOR16(&roller[3].disp[7].ind[16], &cpu_2050.PROAR, 1, 0);
    SET_INDICATOR16(&roller[3].disp[7].ind[17], &cpu_2050.PROAR, 0, 0);
    roller_ptr = 4;
    j = 150 + 35;
    for (i = 0; i < 36; i++) {
        SET_INDICATOR32(&roller[2].disp[0].ind[i], &cpu_2050.L_REG, roller_shift[i], roller_mask[i]);
        SET_INDICATOR32(&roller[2].disp[1].ind[i], &cpu_2050.R_REG, roller_shift[i], roller_mask[i]);
        SET_INDICATOR32(&roller[2].disp[2].ind[i], &cpu_2050.M_REG, roller_shift[i], roller_mask[i]);
        SET_INDICATOR32(&roller[2].disp[3].ind[i], &cpu_2050.H_REG, roller_shift[i], roller_mask[i]);
        if (i < 27) {
            SET_INDICATOR32(&roller[2].disp[4].ind[i], &cpu_2050.SAR_REG, roller_shift[i+8], roller_mask[i+8]);
        } else if (i > 28 && i < 32) {
            SET_INDICATOR8(&roller[2].disp[4].ind[i], &cpu_2050.BS_REG, 3 - (i - 29), 0);
        }
        if (i < 28) {  /* CE - UX */
            SET_INDICATOR32(&roller[2].disp[5].ind[i], &cpu_2050.ros_row3, 28 - i, 0);
        } else if (i < 34) {  /* SS */
            SET_INDICATOR32(&roller[2].disp[5].ind[i], &cpu_2050.ros_row4, 32 - i, 0);
        }
        if (i < 30) { /* LU - ZN, skip 1, TR-SF */
            SET_INDICATOR32(&roller[3].disp[0].ind[i], &cpu_2050.ros_row1, 32 - i, 0);
        }
        if (i < 28) {  /* IV - UR */
            SET_INDICATOR32(&roller[3].disp[1].ind[i], &cpu_2050.ros_row2, 30 - i, 0);
        }
        if (i > 28 && i < 30) {
            SET_INDICATOR8(&roller[3].disp[1].ind[i], &cpu_2050.mvfnc, 30 - i, 0);
        }
        if (i > 30 && i < 31) {
            SET_INDICATOR8(&roller[3].disp[1].ind[i], &cpu_2050.io_mvfnc, 30 - i, 0);
        }
        j += lamp_offset[i];
    //    sprintf(buffer, "%d", i);
     //   ADD_LABEL(j, (h2 * 20), f1_wd*2, buffer, c1, cc);
    }

    add_switch(&SEL_ENTER, 15, (h2 * 11), f1_wd * 10, f1_hd * 2, SW, &c3, &sw_labels[14], font1);
    ADD_LINE(10, (h2 * 8), 120, c);
    ADD_LINE(10, (h2 * 9), 120, c);
    ADD_MARK(10, (h2 * 8), (h2 * 7), c);
    ADD_LINE(10, (h2 * 14), 120, c);
    ADD_LINE(10, (h2 * 15), 120, c);
    ADD_MARK(130, (h2 * 8), (h2 * 7), c);
    ADD_LABEL0(30, (h2 * 8), f1_wd*14, "CHANNEL CONTROL", c1, cc);
    ADD_LABEL0(60, (h2 * 14) + 1, f1_wd*14, "MANUAL OP", c1, cc);
    add_toggle(&CHN_MODE, 0, 80, (h2*10), THREE);
    ADD_LINE(88, (h2 * 9) + (h2/2), (f1_wd*2) - (f1_wd/2), c1);
    ADD_LINE(88, (h2 * 13) + (h2/2), (f1_wd*2) - (f1_wd/2), c1);
    ADD_MARK(88, (h2 * 9) + (h2/2), h2 * (13-9), c1);
    ADD_LABEL(110, (h2 * 9),  4, "MPX", c1, cc);
    ADD_LABEL(110, (h2 * 11), 4, "OFF", c1, cc);
    ADD_LABEL(110, (h2 * 13), 4, "SEL", c1, cc);
    ADD_LABEL(15, (h2 * 19) - (h2/2), f1_wd*14, "SELECTOR", c1, cc);
    ADD_LABEL(15, (h2 * 20) - (h2/2), f1_wd*14, "CHANNEL", c1, cc);
    ADD_LABEL(15, (h2 * 21) - (h2/2), f1_wd*14, "DISPLAY", c1, cc);
    ADD_LABEL0(100, (h2 * 19) - (h2/2), f1_wd*12, "SC1", c1, cc);
    ADD_LABEL0(100, (h2 * 20) - (h2/2), f1_wd*12, "SC2", c1, cc);
    ADD_LABEL0(100, (h2 * 21) - (h2/2), f1_wd*12, "SC3", c1, cc);
    ADD_LINE(110,(h2 * 19), 10, c1);
    ADD_LINE(110,(h2 * 21), 10, c1);
    dial[4].boxd.x = 40;
    dial[4].boxd.y = (h2 * 19);
    dial[4].boxd.w = 50;
    dial[4].boxd.h = (h2 * 4);
    dial[4].boxu.x = 90;
    dial[4].boxu.y = (h2 * 19);
    dial[4].boxu.w = 50;
    dial[4].boxu.h = (h2 * 4);
    dial[4].center_x = 90;
    dial[4].center_y = (h2 * 20);
    dial[4].pos_x[0] = 110;
    dial[4].pos_x[1] = 120;
    dial[4].pos_x[2] = 110;
    dial[4].pos_y[0] = h2 * 19;
    dial[4].pos_y[1] = (h2 * 20);
    dial[4].pos_y[2] = (h2 * 21);
    dial[4].init = 0;
    dial[4].value = &SEL_SW;
    dial[4].max = 2;
    dial[4].wrap = 0;

    j = 150 + 35;
    ADD_AREA(150,(h2 * 42) - (h2/2), h2, 931, &c1);
    ADD_LABEL(150,(h2 * 42) - (h2/2), 931, "FLT OP REG", c, c1);
    ADD_LINE(150,(h2 * 47) - (h2/2), 930, c1);
    ADD_MARK(150, (h2 * 42), h2 * (47-42) - (h2/2), c1);
    ADD_MARK(150+929, (h2 * 42), h2 * (47-42) - (h2/2), c1);
    for (i = 1; i <= 36; i++) {
//        sprintf(buffer, "%d", i);
 //       ADD_LABEL(j, (h2 * 46), f1_wd*2, buffer, c1, cc);
        lamp[lamp_ptr].rect.x = j;
        lamp[lamp_ptr].rect.y = 45 * h2;
        lamp[lamp_ptr].rect.w = 15;
        lamp[lamp_ptr].rect.h = 15;
        lamp[lamp_ptr].col = 0;
        SET_INDICATORNON(&lamp[lamp_ptr].ind);
        lamp_ptr++;
        if (label3[i-1].upper != NULL) {
            ADD_LABEL0(j, (h2 * 43), f1_wd*2, label3[i-1].upper, c1, cc);
        }
        if (label3[i-1].middle != NULL) {
            ADD_LABEL0(j, (h2 * 43) + (h2/2), f1_wd*2, label3[i-1].middle, c1, cc);
        }
        if (label3[i-1].lower != NULL) {
            ADD_LABEL0(j, (h2 * 44), f1_wd*2, label3[i-1].lower, c1, cc);
        }
        mark[i] = j;
        j += lamp_offset[i];
    }
    ADD_MARK(mark[0] - f1_wd, (h2 * 42), h2 * (47-42) - (h2/2), c1);
    ADD_MARK(mark[5] - f1_wd, (h2 * 42), h2 * (47-42) - (h2/2), c1);
    ADD_MARK(mark[11] - f1_wd, (h2 * 42), h2 * (47-42) - (h2/2), c1);
    ADD_MARK(mark[14] - f1_wd, (h2 * 42), h2 * (47-42) - (h2/2), c1);
    ADD_MARK(mark[18] - f1_wd, (h2 * 42), h2 * (47-42) - (h2/2), c1);
    ADD_MARK(mark[22] - f1_wd, (h2 * 42), h2 * (47-42) - (h2/2), c1);
    ADD_MARK(mark[25] - f1_wd, (h2 * 42), h2 * (47-42) - (h2/2), c1);
    ADD_MARK(mark[30] - f1_wd, (h2 * 42), h2 * (47-42) - (h2/2), c1);
    ADD_MARK(mark[34] - f1_wd, (h2 * 42), h2 * (47-42) - (h2/2), c1);

    ADD_AREA(150,(h2 * 48) - (h2/2), h2, 931, &c1);
    ADD_LABEL(150,(h2 * 48) - (h2/2), 931, "STORAGE DATA REGISTER", c, c1);
    ADD_LINE(150,(h2 * 56), 930, c1);
    ADD_MARK(150, (h2 * 48), h2 * (56-48), c1);
    ADD_MARK(150+929, (h2 * 48), h2 * (56-48), c1);
    j = 150 + 35;
    for (i = 0; i <= 36; i++) {
        mark[i] = j;
        j += lamp_offset[i];
        lamp[lamp_ptr].rect.x = j;
        lamp[lamp_ptr].rect.y = 49 * h2;
        lamp[lamp_ptr].rect.w = 15;
        lamp[lamp_ptr].rect.h = 15;
        lamp[lamp_ptr].col = 0;
        SET_INDICATOR32(&lamp[lamp_ptr].ind, &cpu_2050.SDR_REG, roller_shift[i], roller_mask[i]);
        lamp_ptr++;
        if (roller_mask[i] != 0) {
            sprintf(buffer, "%d-%d", 32 - (roller_shift[i] + 8), 31 - roller_shift[i]);
            ADD_LABEL0(j, (h2 * 50), f1_wd*2, buffer, c1, cc);
            ADD_LABEL0(j, (h2 * 52), f1_wd*2, "P", c1, cc);
        }
    }

    ADD_AREA(150,(h2 * 51), h2, 931, &c);
    ADD_LABEL(mark[5], (h2 * 51), 10, "BYTE 0", c1, c);
    ADD_LABEL(mark[14], (h2 * 51), 10, "BYTE 1", c1, c);
    ADD_LABEL(mark[23], (h2 * 51), 10, "BYTE 2", c1, c);
    ADD_LABEL(mark[32], (h2 * 51), 10, "BYTE 3", c1, c);
    ADD_MARK(mark[9] + (w2 * 4), (h2 * 49), h2 * 3, c);
    ADD_MARK(mark[19] - (w2 * 4), (h2 * 49), h2 * 3, c);
    ADD_MARK(mark[28] - (w2 * 2), (h2 * 49), h2 * 3, c);

    ADD_AREA(mark[2] + 5, (h2 * 49)+5, 5, mark[5] - mark[2], &c);
    ADD_AREA(mark[6] + 5, (h2 * 49)+5, 5, mark[9] - mark[6], &c1);
    ADD_AREA(mark[11] + 5, (h2 * 49)+5, 5, mark[14] - mark[11], &c);
    ADD_AREA(mark[15] + 5, (h2 * 49)+5, 5, mark[18] - mark[15], &c1);
    ADD_AREA(mark[20] + 5, (h2 * 49)+5, 5, mark[23] - mark[20], &c);
    ADD_AREA(mark[24] + 5, (h2 * 49)+5, 5, mark[27] - mark[24], &c1);
    ADD_AREA(mark[29] + 5, (h2 * 49)+5, 5, mark[32] - mark[29], &c);
    ADD_AREA(mark[33] + 5, (h2 * 49)+5, 5, mark[36] - mark[33], &c1);

    add_toggle(NULL, 0, 150, 53*h2, ON_OFF);
    j = 150;
    for (i = 31; i >=0; i--) {
        j += switch_offset[31-i];
        add_toggle(&cpu_2050.DKEYS, i, j, 53*h2, ON_OFF);
        sprintf(buffer, "%d", 31 - i);
        ADD_LABEL0(j, (h2 * 52), f1_wd*2, buffer, c1, cc);
        mark[31 - i] = j;
    }

    ADD_LABEL0(595,(h2 * 54), 70, "DATA", c1, cc);
    ADD_AREA(155, (h2 * 53), h2, mark[7] - 148, &c1);
    ADD_LABEL0(150,(h2 * 53), 70, "DATA BUS", c, c1);
    ADD_LABEL0(595,(h2 * 55), 70, "SAR COMPARE", c1, c);
    ADD_AREA(mark[8] + 5, (h2 * 55), h2, mark[31] - mark[8], &c);
    ADD_LABEL0(mark[23],(h2 * 53) + (h2/2), 0, "ROS COMPARE", c, c1);
    ADD_AREA(mark[20] + 5, (h2 * 53) + (h2/2), h2, mark[31] - mark[20], &c1);

    j = 150 + 35;
    ADD_AREA(150,(h2 * 57) - (h2/2), h2, 931, &c1);
    ADD_LABEL(150,(h2 * 57) - (h2/2), 931, "INSTRUCTION ADDRESS REGISTER", c, c1);
    for (i = 0; i <= 36; i++) {
        mark[i] = j;
        j += lamp_offset[i];
        if (i > 8) {
            lamp[lamp_ptr].rect.x = j;
            lamp[lamp_ptr].rect.y = 58 * h2;
            lamp[lamp_ptr].rect.w = 15;
            lamp[lamp_ptr].rect.h = 15;
            lamp[lamp_ptr].col = 0;
            SET_INDICATOR32(&lamp[lamp_ptr].ind, &cpu_2050.SAR_REG, roller_shift[i], roller_mask[i]);
            lamp_ptr++;
            if (roller_mask[i] != 0) {
                sprintf(buffer, "%d-%d", 32 - (roller_shift[i] + 8), 31 - roller_shift[i]);
                ADD_LABEL0(j, (h2 * 59), f1_wd*2, buffer, c1, cc);
                ADD_LABEL0(j, (h2 * 60), f1_wd*2, "P", c1, cc);
            }
        }
    }
    ADD_AREA(mark[11] + 5, (h2 * 58)+5, 5, mark[14] - mark[11], &c);
    ADD_AREA(mark[15] + 5, (h2 * 58)+5, 5, mark[18] - mark[15], &c1);
    ADD_AREA(mark[20] + 5, (h2 * 58)+5, 5, mark[23] - mark[20], &c);
    ADD_AREA(mark[24] + 5, (h2 * 58)+5, 5, mark[27] - mark[24], &c1);
    ADD_AREA(mark[29] + 5, (h2 * 58)+5, 5, mark[32] - mark[29], &c);
    ADD_AREA(mark[33] + 5, (h2 * 58)+5, 5, mark[36] - mark[33], &c1);
    j = 150;
    for (i = 31; i >=0; i--) {
        j += switch_offset[31-i];
        if (i < 24) {
            add_toggle(&cpu_2050.AKEYS, i, j, 61*h2, ON_OFF);
            sprintf(buffer, "%d", 31 - i);
            ADD_LABEL(j, (h2 * 90), f1_wd*2, buffer, c1, cc);
            sprintf(buffer, "%d", 31 - i);
            ADD_LABEL0(j, (h2 * 60), f1_wd*2, buffer, c1, cc);
        }
        mark[i] = j;
    }
    ADD_LABEL0(595,(h2 * 61), 70, "ADDRESS", c1, cc);
    ADD_LABEL0(595,(h2 * 63) - (h2/2), 70, "STORAGE PROTECT", c1, c);
    ADD_LINE(150,(h2 * 64), 930, c1);
    ADD_MARK(150, (h2 * 57), h2 * (64-57), c1);
    ADD_MARK(150+929, (h2 * 57), h2 * (64-57), c1);
    ADD_AREA(mark[23], (h2 * 63) - (h2/2), h2, mark[11] - mark[23] + 5, &c);
    ADD_LABEL(mark[29], (h2 * 58), f1_wd*14, "STORAGE SELECT", c1, cc);
    ADD_LABEL0(mark[29] - (f1_wd * 12), (h2 * 62) - (h2/4), f1_wd*12, "22-27 LOCAL", c1, cc);
    ADD_LABEL0(mark[29] - (f1_wd * 14) - (f1_wd/2), (h2 * 62) + (h2/4), f1_wd*12, "L REG", c1, cc);
    ADD_LABEL0(mark[29] - (f1_wd * 13), (h2 * 60) - (h2/4), f1_wd*12, "4-25 MAIN", c1, cc);
    ADD_LABEL0(mark[29] - (f1_wd * 15) - (f1_wd/2), (h2 * 60) + (h2/4), f1_wd*12, "SDR", c1, cc);
    ADD_LINE(mark[28] - (f1_wd * 5), (h2 * 60) + (h2/2), (f1_wd * 2), c1);
    ADD_LABEL0(mark[29] + (f1_wd * 11) - (f1_wd/2), (h2 * 60) - (h2/4), f1_wd*12, "PROTECT 8-26", c1, cc);
    ADD_LABEL0(mark[29] + (f1_wd * 14), (h2 * 60) + (h2/4), f1_wd*12, "F REG", c1, cc);
    ADD_LINE(mark[28] + (f1_wd * 4), (h2 * 60) + (h2/2), (f1_wd * 2), c1);
    ADD_LABEL0(mark[29] + (f1_wd * 12), (h2 * 62) - (h2/4), f1_wd*12, "MPX 22-31", c1, cc);
    ADD_LABEL0(mark[29] + (f1_wd * 14) + (f1_wd/2), (h2 * 62) + (h2/4), f1_wd*12, "SDR", c1, cc);
    dial[3].boxd.x = mark[28] - (f1_wd * 12);
    dial[3].boxd.y = h2 * 60;
    dial[3].boxd.w = (f1_wd * 12);
    dial[3].boxd.h = h2 * 5;
    dial[3].boxu.x = mark[28];
    dial[3].boxu.y = h2 * 60;
    dial[3].boxu.w = (f1_wd * 12);
    dial[3].boxu.h = h2 * 5;
    dial[3].center_x = mark[28],
    dial[3].center_y = (h2 * 62);
    dial[3].pos_x[0] = mark[28] - (f1_wd * 5);
    dial[3].pos_x[1] = mark[28] - (f1_wd * 3);
    dial[3].pos_x[2] = mark[28] + (f1_wd * 4);
    dial[3].pos_x[3] = mark[28] + (f1_wd * 7);
    dial[3].pos_y[0] = h2 * 62;
    dial[3].pos_y[1] = (h2 * 60) + (h2/2);
    dial[3].pos_y[2] = (h2 * 60) + (h2/2);
    dial[3].pos_y[3] = (h2 * 62);
    dial[3].init = 1;
    dial[3].value = &E_SW;
    dial[3].max = 3;
    dial[3].wrap = 0;

    add_switch(&SYS_RST, 610, (h2 * 73), f1_wd * 10, f1_hd * 2, SW, &c3, &sw_labels[0], font1);
    add_switch(&SET_IC, 610, (h2 * 75) + (h2/2), f1_wd * 10, f1_hd * 2, SW, &c3, &sw_labels[3], font1);
    add_switch(&STOP, 610, (h2 * 78), f1_wd * 10, f1_hd * 2, SW, &c5, &sw_labels[5], font1);
    add_switch(&ROAR_RST,670, (h2 * 73), f1_wd * 10, f1_hd * 2, SW, &c3, &sw_labels[1], font1);
    add_switch(&STORE, 670, (h2 * 75) + (h2/2), f1_wd * 10, f1_hd * 2, SW, &c3, &sw_labels[7], font1);
    add_switch(&LAMP_TEST, 670, (h2 * 78), f1_wd * 10, f1_hd * 2, SW, &c, NULL, NULL);
    add_switch(&CHECK_RST, 730, (h2 * 73), f1_wd * 10, f1_hd * 2, SW, &c3, &sw_labels[4], font1);
    add_switch(&DISPLAY, 730, (h2 * 75) + (h2/2), f1_wd * 10, f1_hd * 2, SW, &c3, &sw_labels[9], font1);
    add_switch(NULL, 730, (h2 * 78), f1_wd * 10, f1_hd * 2, SW, &c3, &sw_labels[8], font1);
    add_switch(NULL, 810, (h2 * 65), f1_wd * 10, f1_hd * 2, SW, &c, &sw_labels[10], font1);
    add_switch(&POWER, 1000, (h2 * 65), f1_wd * 10, f1_hd * 2, SW, &c5, &sw_labels[11], font1);
    add_switch(&INTR, 810, (h2 * 78), f1_wd * 10, f1_hd * 2, SW, &c5, &sw_labels[12], font1);
    add_switch(&LOAD, 1000, (h2 * 78), f1_wd * 10, f1_hd * 2, SW, &c3, &sw_labels[13], font1);
    add_switch(&START, 515, (h2 * 78), f1_wd * 10, f1_hd * 2, SW, &c2, &sw_labels[2], font1);
    ADD_LINE(515 + (f1_wd * 10), (h2 * 79), (f1_wd * 10), c1);
    marks[mrk_ptr].x1 = 800;
    marks[mrk_ptr].y1 = (h2 * 65) - 5;
    marks[mrk_ptr].x2 = 1065;
    marks[mrk_ptr].y2 = (h2 * 65) - 5;
    marks[mrk_ptr].c = &c;
    mrk_ptr++;
    marks[mrk_ptr].x1 = 800;
    marks[mrk_ptr].y1 = (h2 * 67) + 3;
    marks[mrk_ptr].x2 = 1065;
    marks[mrk_ptr].y2 = (h2 * 67) + 3;
    marks[mrk_ptr].c = &c;
    mrk_ptr++;
    marks[mrk_ptr].x1 = 800;
    marks[mrk_ptr].y1 = (h2 * 65) - 5;
    marks[mrk_ptr].x2 = 800;
    marks[mrk_ptr].y2 = (h2 * 67) + 3;
    marks[mrk_ptr].c = &c;
    mrk_ptr++;
    marks[mrk_ptr].x1 = 1065;
    marks[mrk_ptr].y1 = (h2 * 65) - 5;
    marks[mrk_ptr].x2 = 1065;
    marks[mrk_ptr].y2 = (h2 * 67) + 3;
    marks[mrk_ptr].c = &c;
    mrk_ptr++;
    marks[mrk_ptr].x1 = 800;
    marks[mrk_ptr].y1 = (h2 * 78) - 5;
    marks[mrk_ptr].x2 = 1065;
    marks[mrk_ptr].y2 = (h2 * 78) - 5;
    marks[mrk_ptr].c = &c1;
    mrk_ptr++;
    marks[mrk_ptr].x1 = 800;
    marks[mrk_ptr].y1 = (h2 * 78) - 5;
    marks[mrk_ptr].x2 = 800;
    marks[mrk_ptr].y2 = (h2 * 80) + 3;
    marks[mrk_ptr].c = &c1;
    mrk_ptr++;
    marks[mrk_ptr].x1 = 800;
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

    marks[mrk_ptr].x1 = 605;
    marks[mrk_ptr].y1 = (h2 * 73) - 5;
    marks[mrk_ptr].x2 = 785;
    marks[mrk_ptr].y2 = (h2 * 73) - 5;
    marks[mrk_ptr].c = &c;
    mrk_ptr++;
    marks[mrk_ptr].x1 = 605;
    marks[mrk_ptr].y1 = (h2 * 80) + 3;
    marks[mrk_ptr].x2 = 785;
    marks[mrk_ptr].y2 = (h2 * 80) + 3;
    marks[mrk_ptr].c = &c;
    mrk_ptr++;
    marks[mrk_ptr].x1 = 605;
    marks[mrk_ptr].y1 = (h2 * 73) - 5;
    marks[mrk_ptr].x2 = 605;
    marks[mrk_ptr].y2 = (h2 * 80) + 3;
    marks[mrk_ptr].c = &c;
    mrk_ptr++;
    marks[mrk_ptr].x1 = 785;
    marks[mrk_ptr].y1 = (h2 * 73) - 5;
    marks[mrk_ptr].x2 = 785;
    marks[mrk_ptr].y2 = (h2 * 80) + 3;
    marks[mrk_ptr].c = &c;
    mrk_ptr++;
    marks[mrk_ptr].x1 = 485;
    marks[mrk_ptr].y1 = (h2 * 73) - 5;
    marks[mrk_ptr].x2 = 595;
    marks[mrk_ptr].y2 = (h2 * 73) - 5;
    marks[mrk_ptr].c = &c;
    mrk_ptr++;
    marks[mrk_ptr].x1 = 485;
    marks[mrk_ptr].y1 = (h2 * 73) - 5;
    marks[mrk_ptr].x2 = 485;
    marks[mrk_ptr].y2 = (h2 * 80) + 3;
    marks[mrk_ptr].c = &c;
    mrk_ptr++;
    marks[mrk_ptr].x1 = 485;
    marks[mrk_ptr].y1 = (h2 * 80) + 3;
    marks[mrk_ptr].x2 = 595;
    marks[mrk_ptr].y2 = (h2 * 80) + 3;
    marks[mrk_ptr].c = &c;
    mrk_ptr++;
    marks[mrk_ptr].x1 = 595;
    marks[mrk_ptr].y1 = (h2 * 73) - 5;
    marks[mrk_ptr].x2 = 595;
    marks[mrk_ptr].y2 = (h2 * 80) + 3;
    marks[mrk_ptr].c = &c;
    mrk_ptr++;
    ADD_MARK(795, (h2 * 64), (h2 * 20), c1);

    ADD_LABEL(485,(h2 * 65), (f1_wd * 23), "FLT CONTROL", c1, c4);
    ADD_LABEL(485 + (f1_wd * 18), (h2 * 66) + (h2/2), f1_wd * 4, "AUTO", c1, c4);
    ADD_LABEL(480 + (f1_wd * 18), (h2 * 67) + (h2/2), f1_wd * 7, "REREAD", c1, c4);
    ADD_LABEL(480, (h2 * 66), f1_wd * 23, "PROCESS", c1, c4);
    ADD_LABEL(480 + (f1_wd * 18), (h2 * 69), f1_wd * 4, "HALT", c1, c4);
    ADD_LABEL(480 + (f1_wd * 18), (h2 * 70), f1_wd * 5, "AFTER", c1, c4);
    ADD_LABEL(480 + (f1_wd * 18), (h2 * 71), f1_wd * 4, "LOAD", c1, c4);
    ADD_LABEL(480, (h2 * 67), f1_wd * 6, "REPEAT", c1, c4);
    ADD_LABEL(480, (h2 * 70), f1_wd * 4, "STOP", c1, c4);
//    ADD_LINE(425 + (f1_wd * 11), (h2 * 66), (f1_wd * 3), c1);
//    ADD_LINE(425 + (f1_wd * 26)+1, (h2 * 71), (f1_wd * 3) - 1, c1);
    dial[0].boxd.x = 480;
    dial[0].boxd.y = h2 * 65;
    dial[0].boxd.w = (f1_wd * 12);
    dial[0].boxd.h = h2 * 6;
    dial[0].boxu.x = 480 + (f1_wd * 12);
    dial[0].boxu.y = h2 * 65;
    dial[0].boxu.w = (f1_wd * 12);
    dial[0].boxu.h = h2 * 6;
    dial[0].center_x = 480 + (f1_wd * 12);
    dial[0].center_y = (h2 * 68) + (h2/2);
    dial[0].pos_x[0] = 480 + (f1_wd * 6);
    dial[0].pos_x[1] = 480 + (f1_wd * 12);
    dial[0].pos_x[2] = 480 + (f1_wd * 18);
    dial[0].pos_x[3] = 480 + (f1_wd * 18);
    dial[0].pos_x[4] = 480 + (f1_wd * 6);
    dial[0].pos_y[0] = h2 * 68 - (h2/2);
    dial[0].pos_y[1] = h2 * 67;
    dial[0].pos_y[2] = h2 * 68 - (h2/2);
    dial[0].pos_y[3] = h2 * 70;
    dial[0].pos_y[4] = h2 * 70;
    dial[0].init = 1;
    dial[0].value = &PROC_SW;
    dial[0].max = 4;
    dial[0].wrap = 0;

    ADD_LABEL(485, (h2 * 73), (f1_wd * 23), "RATE", c1, c4);
    ADD_LABEL(485, (h2 * 74), f1_wd * 23, "PROCESS", c1, c4);
    ADD_LABEL(490, (h2 * 75) - (h2/2), (f1_wd * 5), "INSTR", c1, c4);
    ADD_LABEL(490, (h2 * 76) - (h2/2), (f1_wd * 4), "STEP", c1, c4);
    ADD_LABEL(480 + (f1_wd * 17), (h2 * 75) - (h2/2), f1_wd * 5, "SINGLE", c1, c4);
    ADD_LABEL(480 + (f1_wd * 17) - (f1_wd/2), (h2 * 76) - (h2/2), f1_wd * 5, "CYCLE", c1, c4);
//    ADD_LINE(425 + (f1_wd * 9), (h2 * 75), (f1_wd * 1), c1);
//    ADD_LINE(425 + (f1_wd * 16), (h2 * 75), (f1_wd * 2), c1);
    dial[1].boxd.x = 480;
    dial[1].boxd.y = h2 * 69;
    dial[1].boxd.w = (f1_wd * 12);
    dial[1].boxd.h = h2 * 6;
    dial[1].boxu.x = 480 + (f1_wd * 12);
    dial[1].boxu.y = h2 * 69;
    dial[1].boxu.w = (f1_wd * 12);
    dial[1].boxu.h = h2 * 6;
    dial[1].center_x = 480 + (f1_wd * 12);
    dial[1].center_y = (h2 * 76) + (h2/2);
    dial[1].pos_x[0] = 480 + (f1_wd * 8);
    dial[1].pos_x[1] = 480 + (f1_wd * 12);
    dial[1].pos_x[2] = 480 + (f1_wd * 17);
    dial[1].pos_y[0] = h2 * 75;
    dial[1].pos_y[1] = h2 * 75;
    dial[1].pos_y[2] = h2 * 75;
    dial[1].init = 1;
    dial[1].value = &RATE_SW;
    dial[1].max = 2;
    dial[1].wrap = 0;

    ADD_LABEL(630, (h2 * 65), f1_wd*23, "CHECK CONTROL", c1, c4);
    ADD_LABEL(630, (h2 * 66), f1_wd*23, "PROCESS", c1, c4);
    ADD_LABEL(630 + (f1_wd * 20), (h2 * 67) - (h2/2), f1_wd * 7, "DISABLE", c1, c4);
    ADD_LINE(630 + (f1_wd * 17), (h2 * 67), (f1_wd * 3), c1);
    ADD_LABEL(630 + (f1_wd * 20), (h2 * 69) + (h2 / 2), f1_wd * 4, "STOP", c1, c4);
    ADD_LINE(630 + (f1_wd * 17), (h2 * 70), (f1_wd * 3), c1);
    ADD_LABEL(630, (h2 * 69), f1_wd * 4, "CHAN", c1, c4);
    ADD_LABEL(630, (h2 * 70), f1_wd * 4, "STOP", c1, c4);
    ADD_LINE(630 + (f1_wd * 4), (h2 * 70), (f1_wd * 2), c1);
    dial[2].boxd.x = 630;
    dial[2].boxd.y = h2 * 65;
    dial[2].boxd.w = (f1_wd * 12);
    dial[2].boxd.h = h2 * 6;
    dial[2].boxu.x = 650 + (f1_wd * 12);
    dial[2].boxu.y = h2 * 65;
    dial[2].boxu.w = (f1_wd * 12);
    dial[2].boxu.h = h2 * 6;
    dial[2].center_x = 630 + (f1_wd * 12);
    dial[2].center_y = (h2 * 68) + (h2/2);
    dial[2].pos_x[0] = 630 + (f1_wd * 12);
    dial[2].pos_x[1] = 630 + (f1_wd * 17);
    dial[2].pos_x[2] = 630 + (f1_wd * 17);
    dial[2].pos_x[3] = 630 + (f1_wd * 6);;
    dial[2].pos_y[0] = h2 * 67;
    dial[2].pos_y[1] = (h2 * 67);
    dial[2].pos_y[2] = (h2 * 70);
    dial[2].pos_y[3] = (h2 * 70);
    dial[2].init = 0;
    dial[2].value = &CHK_SW;
    dial[2].max = 3;
    dial[2].wrap = 0;

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
        rect.x += (f1_wd * 18);
    }

    hex_dial[0].digit = &A_SW;
    hex_dial[1].digit = &B_SW;
    hex_dial[2].digit = &C_SW;

    ADD_LABEL2(820+(f1_wd * 10), (h2 * 77) + (h2/2), "SYS");
    ADD_LABEL2(820+(f1_wd * 15), (h2 * 77) + (h2/2), "MAN");
    ADD_LABEL2(820+(f1_wd * 20), (h2 * 77) + (h2/2), "WAIT");
    ADD_LABEL2(820+(f1_wd * 25), (h2 * 77) + (h2/2), "TEST");
    ADD_LABEL2(820+(f1_wd * 30), (h2 * 77) + (h2/2), "LOAD");

    /* Add in status lights */
    lamp[lamp_ptr].rect.x = 820 + (f1_wd * 10);
    lamp[lamp_ptr].rect.y = h2 * 78 + (h2/2);
    lamp[lamp_ptr].rect.h = 15;
    lamp[lamp_ptr].rect.w = 15;
    lamp[lamp_ptr].col = 0;
    SET_INDICATOR8(&lamp[lamp_ptr].ind, &clock_start_lch, 0, 0);
    lamp_ptr++;
    lamp[lamp_ptr].rect.x = 820 + (f1_wd * 15);
    lamp[lamp_ptr].rect.y = h2 * 78 + (h2/2);
    lamp[lamp_ptr].rect.h = 15;
    lamp[lamp_ptr].rect.w = 15;
    lamp[lamp_ptr].col = 0;
    SET_INDICATOR8(&lamp[lamp_ptr].ind, &allow_man_operation, 0, 0);
    lamp_ptr++;
    lamp[lamp_ptr].rect.x = 820 + (f1_wd * 20);
    lamp[lamp_ptr].rect.y = h2 * 78 + (h2/2);
    lamp[lamp_ptr].rect.h = 15;
    lamp[lamp_ptr].rect.w = 15;
    lamp[lamp_ptr].col = 0;
    SET_INDICATOR8(&lamp[lamp_ptr].ind, &wait, 0, 0);
    lamp_ptr++;
    lamp[lamp_ptr].rect.x = 820 + (f1_wd * 25);
    lamp[lamp_ptr].rect.y = h2 * 78 + (h2/2);
    lamp[lamp_ptr].rect.h = 15;
    lamp[lamp_ptr].rect.w = 15;
    lamp[lamp_ptr].col = 1;
    SET_INDICATOR8(&lamp[lamp_ptr].ind, &test_mode, 0, 0);
    lamp_ptr++;
    lamp[lamp_ptr].rect.x = 820 + (f1_wd * 30);
    lamp[lamp_ptr].rect.y = h2 * 78 + (h2/2);
    lamp[lamp_ptr].rect.h = 15;
    lamp[lamp_ptr].rect.w = 15;
    lamp[lamp_ptr].col = 0;
    SET_INDICATOR8(&lamp[lamp_ptr].ind, &load_mode, 0, 0);
    lamp_ptr++;
   /* Pass
      Fail
      Binary Tgr
      Test cntr=0
      FLT OP REG 012345
      SEQ CNTR 421
      SEQ STAT 1234
      FLT LOAD CHECK
      empty
      SUPV STAT
      PROGSYS SCAN STAT
      SUPV ENABL STOP
      SEQ CNTR
      MAIN STOR
      ROS
      ALT PRE- FIX
      HARD STOP
      LOG TGR
      BLOCK INDIC
      SINGLE CYCLE
      CPU
      CHAN
      ROS
      MAIN STOR
      CHK IRPT ENABLD
      CHK REG GATED
      CHK PEND
    */

    ADD_LINE(8, (h2 * 65) + (h2/3), 470, c1);
    ADD_LINE(8, (h2 * 66) + (h2/3), 470, c1);
    ADD_LINE(8, (h2 * 72), 470, c1);
    ADD_LINE(8, (h2 * 75), 470, c1);
    ADD_MARK(8, (h2 * 65) + (h2/3), (h2 * 10) - (h2/3), c1);

    ADD_LABEL(8, (h2 * 66) - (h2/2), 96, "IAR", c1, c4);
    /* Mark/lines for switch */
    ADD_MARK(22, (h2 * 68) - (h2/2), (h2 * 3) + (h2/2), c1);
    ADD_LINE(22, (h2 * 68) - (h2/2), (f1_wd*2) - (f1_wd/2), c1);
    ADD_LINE(22, (h2 * 71), (f1_wd*2) - (f1_wd/2), c1);
    ADD_LABEL(38, (h2 * 67), 4, "SYNC", c1, c4);
    ADD_LABEL(38, (h2 * 69), 4, "PROC", c1, c4);
    ADD_LABEL(38, (h2 * 71) - (h2/2), 4, "STOP", c1, c4);
    ADD_LABEL(24, (h2 * 72), 10, "ADDRESS", c1, c4);
    ADD_LABEL(24, (h2 * 73), 10, "COMPARE", c1, c4);
    add_toggle(&ADR_CMP, 0, 15, 68*h2, THREE);

    ADD_MARK(55, (h2 * 66) + (h2/3), (h2 * 8) + (h2/2), c1);

    ADD_LABEL(110, (h2 * 66) - (h2/2), 80, "ROS", c1, c4);
    ADD_LABEL(74, (h2 * 72), 10, "REPEAT", c1, c4);
    ADD_LABEL(74, (h2 * 73), 10, "INSN", c1, c4);
    add_toggle(&INST_REP, 0, 62, 68*h2, ON_OFF);

    ADD_MARK(102, (h2 * 65) + (h2/3), (h2 * 10) - (h2/3), c1);

    /* Mark/line for switch */
    ADD_MARK(115, (h2 * 69), (h2 * 2), c1);
    ADD_LINE(114, (h2 * 71), (f1_wd*2), c1);
    ADD_LABEL(134, (h2 * 69), 4, "SYNC", c1, c4);
    ADD_LABEL(134, (h2 * 71) - (h2/2), 4, "STOP", c1, c4);
    ADD_LABEL(120, (h2 * 72), 10, "ADDRESS", c1, c4);
    ADD_LABEL(120, (h2 * 73), 10, "COMPARE", c1, c4);
    add_toggle(&ROS_CMP, 0, 109, 68*h2, THREE);

    ADD_MARK(149, (h2 * 66) + (h2/3), (h2 * 8) + (h2/2), c1);

    /* Mark/line for switch */
    ADD_MARK(162, (h2 * 68) - (h2/2), (h2 * 3) + (h2/2), c1);
    ADD_LINE(162, (h2 * 68) - (h2/2), (f1_wd*2) - (f1_wd/2), c1);
    ADD_LINE(162, (h2 * 71), (f1_wd*2) - (f1_wd/2), c1);
    ADD_LABEL(179, (h2 * 67), 3, "I/O", c1, c4);
    ADD_LABEL(179, (h2 * 68), 4, "MODE", c1, c4);
    ADD_LABEL(179, (h2 * 70), 3, "CPU", c1, c4);
    ADD_LABEL(179, (h2 * 71), 4, "MODE", c1, c4);
    ADD_LABEL(164, (h2 * 72), 10, "REPEAT", c1, c4);
    ADD_LABEL(164, (h2 * 73), 10, "INSN", c1, c4);
    add_toggle(&ROS_REP, 0, 156, 68*h2, ON_OFF);
    ADD_MARK(196, (h2 * 65) + (h2/3), (h2 * 10) - (h2/3), c1);

    add_toggle(NULL, 0, 203, 68*h2, ON_OFF);  /* Empty */
    ADD_MARK(243, (h2 * 65) + (h2/3), (h2 * 10) - (h2/3), c1);

    ADD_LABEL(250, (h2 * 66) - (h2/2), 40, "SAR", c1, c4);
    ADD_LABEL(260, (h2 * 72), 10, "ADDRESS", c1, c4);
    ADD_LABEL(260, (h2 * 73), 10, "COMPARE", c1, c4);
    add_toggle(&SAR_CMP, 0, 250, 68*h2, ON_OFF);
    ADD_MARK(290, (h2 * 65) + (h2/3), (h2 * 10) - (h2/3), c1);

    ADD_LABEL(310, (h2 * 72), 10, "DISABLE", c1, c4);
    ADD_LABEL(310, (h2 * 73), 10, "INTERVAL", c1, c4);
    ADD_LABEL(310, (h2 * 74), 10, "TIMER", c1, c4);
    add_toggle(&INT_TMR, 0, 297, 68*h2, ON_OFF);
    ADD_MARK(337, (h2 * 65) + (h2/3), (h2 * 10) - (h2/3), c1);

    add_toggle(&LAMP_TEST, 0, 344, 68*h2, ON_OFF_MOM);
    ADD_LABEL(340, (h2 * 72), 30, "LAMP", c1, c4);
    ADD_LABEL(340, (h2 * 73), 30, "TEST", c1, c4);
    ADD_MARK(384, (h2 * 65) + (h2/3), (h2 * 10) - (h2/3), c1);

    ADD_LABEL(392, (h2 * 72), 30, "FORCE", c1, c4);
    ADD_LABEL(392, (h2 * 73), 30, "INDICATOR", c1, c4);
    add_toggle(&FORC_IND, 0, 391, 68*h2, THREE);
    ADD_MARK(429, (h2 * 66) + (h2/3), (h2 * 8) + (h2/2), c1);

    ADD_LABEL(463, (h2 * 67) - (h2/2), 4, "FR", c1, c4);
    ADD_LABEL(465, (h2 * 68) - (h2/2), 4, "PASS", c1, c4);
    ADD_MARK(447, (h2 * 68) - (h2/2), (h2 * 3) + (h2/2), c1);
    ADD_LINE(447, (h2 * 68) - (h2/2), (f1_wd*2), c1);
    ADD_LINE(440, (h2 * 71), (f1_wd*4), c1);
    ADD_LABEL(465, (h2 * 69), 4, "OFF", c1, c4);
    ADD_LABEL(465, (h2 * 71) - (h2/2), 4, "LOAD", c1, c4);
    ADD_LABEL(436, (h2 * 71) - (h2/2), 3, "STR", c1, c4);
    ADD_LABEL(432, (h2 * 72), 30, "FLT", c1, c4);
    ADD_LABEL(432, (h2 * 73), 30, "MODE", c1, c4);
    add_toggle(&FLT_MODE, 0, 440, 68*h2, THREE);
    ADD_MARK(478, (h2 * 65) + (h2/3), (h2 * 10) - (h2/3), c1);

}

