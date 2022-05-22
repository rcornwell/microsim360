/*
 * microsim360 - Microcode converter for Model 2844
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <sys/types.h>
#define CROS2844
#include "model2844.h"

struct ROS_2844 ros_2844[4096];

#if 0
 hex   address       number             ca   cb ck        cl   ch   pa ps cn     pn cd   cda cv cc  cs   pc aa bp

                     field bit position 1234 01 0123 4567 0123 0123 0  0  012345 0  0123 0   0  012 0123 0  0  0
000  0000 0000 0000  qy200   l1  -      0001 00 0000 0000 0000 0000 1  0  000000 1  0000 0   0  000 0000 1  1  0    413250
#endif

int
main(int argc, char *argv[])
{
    FILE      *in;
    FILE      *out;
    char      line[200];
    int       ln = 0;
    char      *p;
    int       i, j, b;
    int       fld;
    int       addr1, addr2;
    char      *q;
    int       parity;

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
    while (fgets(line, sizeof(line), in) != NULL) {
        ln++;
        /* Ignore lines that are blank */
        if (line[0] == ' ')
            goto loop;
        addr1 = addr2 = 0;

        /* Grab first address */
        for (p = line; *p != ' ' && *p != '\0'; p++) {
            if (!isxdigit(*p))
                break;
            addr1 <<= 4;
            if (*p >= '0' && *p <= '9')
                addr1 += (*p - '0');
            else if (tolower(*p) >= 'a' && tolower(*p) <= 'f')
                addr1 += (*p - 'a') + 0xa;
        }
        if (*p == '\n')
            goto loop;
        /* Grab next address */
        for (i = 0; i < 12 && *p != '\0'; p++) {
            if (*p == '0') {
               addr2 <<= 1;
               i++;
            } else if (*p == '1') {
               addr2 = (addr2 << 1) | 1;
               i++;
            } else if (*p != ' ') {
               break;
            }
        }
        if (*p == '\n')
            goto loop;
        if (i != 12) {
           fprintf(stderr, "Address2 not complete %d %d %03x %03x %s", ln, i, addr1, addr2, line);
           goto loop;
        }
        if (addr1 != addr2) {
           fprintf(stderr, "Address not match %d %03x %03x %s", ln, addr1, addr2, line);
           goto loop;
        }


        /* Skip a blank */
        while (*p == ' ') p++;
        q = ros_2844[addr1].NOTE;
        if (*p != '-') {
            /* Grab sheet and box */
            while ((*p != ' ' && *p != '\n')) *q++ = *p++;
            *q++ = '-';
            while (*p == ' ') p++;
            while ((*p != ' ' && *p != '\n')) *q++ = *p++;
            while (*p == ' ') p++;
        }
        p++;
        *q++ = '\0';
        while (*p == ' ') p++;
        /* Grab rest of line */
        j = b = 0;
        fld = 0;
        parity = 1;
        for (;*p != '\0' && *p != '\n' && j < 18; p++) {
            if (*p == '0') {
               b++;
               fld <<= 1;
            } else if (*p == '1') {
               b++;
               fld = (fld << 1) | 1;
               parity ^= 1;
            } else if (*p == ' ') {
               switch (j) {
               case 0:    ros_2844[addr1].CA = fld;
                          j++;
                          b = fld = 0;
                          break;
               case 1:    ros_2844[addr1].CB = fld;
                          j++;
                          b = fld = 0;
                          break;
               case 2:    j++;
                          break;
               case 3:    ros_2844[addr1].CK = fld;
                          j++;
                          b = fld = 0;
                          break;
               case 4:    ros_2844[addr1].CL = fld;
                          j++;
                          b = fld = 0;
                          break;
               case 5:    ros_2844[addr1].CH = fld;
                          j++;
                          b = fld = 0;
                          break;
               case 6:    ros_2844[addr1].PA = fld;
                          j++;
                          b = fld = 0;
                          break;
               case 7:    ros_2844[addr1].PS = fld;
                          j++;
                          b = fld = 0;
                          break;
               case 8:    ros_2844[addr1].CN = fld << 2;
                          j++;
                          b = fld = 0;
                          break;
               case 9:    ros_2844[addr1].PN = fld;
                          j++;
                          b = fld = 0;
                          break;
               case 10:   ros_2844[addr1].CD = fld;
                          j++;
                          b = fld = 0;
                          break;
               case 11:   ros_2844[addr1].CD |= (fld) ? 0x10 : 0;
                          j++;
                          b = fld = 0;
                          break;
               case 12:   ros_2844[addr1].CV = fld;
                          j++;
                          b = fld = 0;
                          break;
               case 13:   ros_2844[addr1].CC = fld;
                          j++;
                          b = fld = 0;
                          break;
               case 14:   ros_2844[addr1].CS = fld;
                          j++;
                          b = fld = 0;
                          break;
               case 15:   ros_2844[addr1].PC = fld;
                          j++;
                          b = fld = 0;
                          break;
               case 16:   ros_2844[addr1].CA |= (fld) ? 0x10 : 0;
                          j++;
                          b = fld = 0;
                          break;
               case 17:   ros_2844[addr1].BP = fld;
                          j++;
                          b = fld = 0;
                          break;
               }
               /* Skip blanks */
               while (p[1] == ' ') p++;
            } else {
               fprintf(stderr, "invalid char %d %c %s\n", ln, *p, line);
            }
        }
     }

     fprintf(out, "/*  CA   CB  CK  CL  CH  PA  PS  CN  PN  CD  CV  CC  CS  PC  BP  NOTE */\n");
     for (addr1 = 0; addr1 < 4096; addr1++) {
         struct ROS_2844  *r = &ros_2844[addr1];
                       /*  CA      CB     CK      CL    CH    PA    PS */
         fprintf(out, "{  0x%02x, 0x%x, 0x%02x, 0x%x, 0x%x, 0x%x, 0x%x,"
                       /*  CN      PN   CD      CV    CC      CS   PC  BP */
                       " 0x%02x, 0x%x, 0x%02x, 0x%x, 0x%x, 0x%02x, 0x%x, 0x%x,"
                       /* NOTE */
                       " \"%s\" },\n",
                       r->CA,  r->CB, r->CK, r->CL, r->CH, r->PA, r->PS,
                       r->CN,  r->PN, r->CD, r->CV, r->CC, r->CS, r->PC, r->BP,
                       r->NOTE);
    }
    return 0;
}


#if 0
p lu  mv zp     zf   zn  tr    zr ws  sf  p iv  al    wm   up md lb mb dg  ul ur p ce   lx  tc ry  ad   ab     bb    ux ss       92--97
2 222 22 222211 1111 111 10000 0  000 000 2 222 21111 1111 11 0  0  0  000 00 00 6 6555 555 5  444 333 3 333333 11111 1  111100 00 000000
0 987 65 432109 8765 432 10987 6  543 210 4 321 09876 5432 10 9  8  7  654 32 10 1 0987 654 3  210 987 6 543210 98765 4  321098 76 543210
0 000 00 000000 0000 100 00000 0  000 000 0 000 10111 0000 00 0  0  0  010 00 00 1 0000 000 0  000 0000 000000 00000 0  000000
0 000 00 101000 0000 011 11000 0  1  10 010 0 001 00000 000 0  000 00 011 01 01 0 0010 000 1  000 000 0 011011 11101 0  110100 00 000000
p lu  mv zp     zf   zn  tr    zr cs sa sf  p ct  al    wl  hc rs  cg mg  ul ur p ce   lx  tc ry  cl    ab     bb    ux ss       92--97
1 000 00 000000 1111 100 00000 0  0  00 111 0 011 00000 000 1  000 11 011 01 01 1 0000 001 0  001 101 0 000000 00000 0  000000





//plu mvzp    zf  zn tr   zws sf
//0000000000000000100000000000000
//plu mvzp    zf  zn tr   zcsasf
//1000000000001111100000000000111
//piv al   wm  upmlm dg u ur
//0000101110000000000100000
//pct al   wl h rs c mg u ur
//0011000000001000110110101
//p ce   lx  tc ry  cl    ab     bb    ux ss       92--97
//1 0000 001 0  001 101 0 000000 00000 0  000000
//p ce   lx  tc ry  ad   ab     bb    ux ss       92--97
//1 0000 000 0  000 0000 000000 00000 0  000000
//1000000100011010000000000000  000000
#endif
