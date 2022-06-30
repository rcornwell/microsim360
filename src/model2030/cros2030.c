/*
 * microsim360 - Microcode converter for Model 2030
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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "xlat.h"
#include "model2030.h"

struct ROS_2030 ros_2030[4096];

#if 0
uint16_t const odd_parity[256] = {
    /*    0    1    2    3    4    5    6    7 */
    /* 00x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 01x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100,
    /* 02x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100,
    /* 03x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 04x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100,
    /* 05x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 06x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 07x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100,
    /* 10x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100,
    /* 11x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 12x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 13x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100,
    /* 14x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 15x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100,
    /* 16x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100,
    /* 17x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 20x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100,
    /* 21x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 22x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 23x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100,
    /* 24x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 25x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100,
    /* 26x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100,
    /* 27x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 30x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 31x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100,
    /* 32x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100,
    /* 33x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 34x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100,
    /* 35x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 36x */ 0x100, 0x000, 0x000, 0x100, 0x000, 0x100, 0x100, 0x000,
    /* 37x */ 0x000, 0x100, 0x100, 0x000, 0x100, 0x000, 0x000, 0x100
};
#endif

int
main(int argc, char *argv[])
{
    FILE  *in;
    FILE  *out;
    char  line[200];
    char  *p;
    int   i;
    int   f;
    int   num, dig;
    char  c;
    int   base;
    int   shift;
    int   addr;

    /* Syntax, cros30 input output */
    /*   or    cros30 <input >output */
    if (argc > 1) {
       if ((in = fopen(argv[1], "r")) == NULL) {
           fprintf(stderr, "Unable to read: %s, ", argv[1]);
           perror("");
           exit(1);
       }
       if ((out = fopen(argv[2], "w")) == NULL) {
           fprintf(stderr, "Unable to create: %s, ", argv[2]);
           perror("");
           exit(1);
       }
    } else {
       in = stdin;
       out = stdout;
    }
loop:

    /* Read in a line and pack it into ros array */
    while (fgets(line, sizeof(line), in) != NULL) {
        p = line;
        f = 0;
        base = 16;
        shift = 4;
        addr = 0xffff;

        while (*p != '#') {
            num = 0;
            while (*p == ' ') p++;  /* Skip blanks */
            while (*p != ' ' && *p != '#') {
                c = *p++;
                if (c >= '0' && c <= '9')
                   dig = c - '0';
                else if (c >= 'A' && c <= 'F')
                   dig = (c - 'A') + 0xA;
                else if (c == '?')
                   dig = 0;
                if (dig >= base) {
                    fprintf(stderr, "Invalid digit %x in %s\n", dig, p);
                    goto loop;
                }
                num <<= shift;
                num += dig;
            }

            switch (f) {
            case 0:                 /* AAA */
                addr = num;
                break;
            case 1:                 /* CN */
                ros_2030[addr].CN = num & 0xFC;
                base = 2;
                shift = 1;
                break;
            case 2:                 /* CH */
                ros_2030[addr].CH = num;
                break;
            case 3:                 /* CL */
                ros_2030[addr].CL = num;
                break;
            case 4:                 /* CM */
                ros_2030[addr].CM = num;
                break;
            case 5:                 /* CU */
                ros_2030[addr].CU = num;
                break;
            case 6:                 /* CA */
                ros_2030[addr].CA = num;
                break;
            case 7:                 /* CB */
                ros_2030[addr].CB = num;
                break;
            case 8:                 /* CK */
                ros_2030[addr].CK = num;
                break;
            case 9:                 /* CD */
                ros_2030[addr].CD = num;
                break;
            case 10:                /* CF */
                ros_2030[addr].CF = num;
                break;
            case 11:                /* CG */
                ros_2030[addr].CG = num;
                break;
            case 12:                /* CV */
                ros_2030[addr].CV = num;
                break;
            case 13:                /* CC */
                ros_2030[addr].CC = num;
                break;
            case 14:                /* CS */
                ros_2030[addr].CS = num;
                break;
            case 15:                /* AA */
                ros_2030[addr].CA |= (num << 4);
                break;
            case 16:                /* AS */
                ros_2030[addr].CS |= (num << 4);
                break;
            case 17:                /* AK */
                ros_2030[addr].CK |= (num << 4);
                break;
            case 18:                /* PK */
                ros_2030[addr].PK = num;
                break;
            }
            f++;
        }
        if (f != 0 && addr == 0xffff) {
           fprintf(stderr, "Invalid address %s\n", line);
           continue;
        }
        if (*p == '#' && f != 0) {
           while (*++p == ' ');
           for (i = 0; i < 15; i++) {
              if (*p == '\n' || *p == '\0')
                 break;
              ros_2030[addr].note[i] = *p++;
           }
           ros_2030[addr].note[i] = '\0';
        }
    }

    /* Fill in the display values */
    for (i = 0; i < 4096; i++) {
        struct ROS_2030 *r = &ros_2030[i];
        int         t = (i >> 8) & 0x1f;
        int         x;

        /* Shift starts at 23 */
        /*  P on CN, ADR P, P W, P X*/
        r->row1 = odd_parity[i & 0xff] | (i & 0xff);  /* X */
        r->row1 |= (((odd_parity[t] != 0) ? 0x20 : 0) | t) << 9;   /* W */
        r->row1 |= (((odd_parity[r->CN] != 0) ? 0x40 : 0) | r->CN) << 17;

        /* Shift starts at 26 */
        /* SA, PK */
                                                      /* CK */
        r->row2 = (r->CK & 0xf) | (r->PK << 4) | ((r->CK & 0x10) << 1);
        r->row2 |= (r->CU << 6) | (r->CM << 8) | (r->CB << 11);
        r->row2 |= (r->CA << 13) | (r->CL << 18) | (r->CH << 22);
        x = odd_parity[r->row2 & 0xff];
        x ^= odd_parity[(r->row2 >> 8) & 0xff];
        x ^= odd_parity[(r->row2 >> 16) & 0xff];
        x ^= odd_parity[(r->row2 >> 24) & 0xff];
        r->row2 |= (x != 0) << 25;
        /* Shift starts at 19 */
        /* CR */
        r->row3 = (r->CS) | (r->CC << 5) | (r->CV << 8) | (r->CG << 10);
        r->row3 |= (r->CF << 12) | (r->CD << 15);
        x = odd_parity[r->row3 & 0xff];
        x ^= odd_parity[(r->row3 >> 8) & 0xff];
        x ^= odd_parity[(r->row3 >> 16) & 0xff];
        r->row3 |= (x != 0) << 19;
    }

    /* Dump out the ros image to a C file. */
    fprintf(out, "/*  CN   CH   CL   CM   CU    CA   CB    CK   CD    CF  CG   CV   CC    CS   PK        R1        R2        R3  Note  */\n");
    for (i = 0; i < 4096; i++) {
        struct ROS_2030 *r = &ros_2030[i];
                 /* CN     CH    CL    CM    CU    CA     CB    CK */
        fprintf(out, "{ 0x%02x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%02x, 0x%x, 0x%02x, ",
                  r->CN, r->CH, r->CL, r->CM, r->CU, r->CA, r->CB, r->CK);
                /* CD  CF    CG   CV    CC    CS    PK   */
        fprintf(out, "0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%02x, 0x%x, ",
                  r->CD, r->CF, r->CG, r->CV, r->CC, r->CS, r->PK);
        fprintf(out, "0x%06x, 0x%06x, 0x%06x, \"%s\" }, \n",
                  r->row1, r->row2, r->row3, (r->note[0] == '\0') ? " ": r->note);
    }
}

