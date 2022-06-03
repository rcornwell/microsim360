/*
 * microsim360 - Model 2065 CROS to C converter.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <sys/types.h>

#ifdef _MSC_VER
#define strncasecmp _strnicmp
#endif

#include "model2065.h"

struct ROS_2065 ros_2065[4096];

int
main(int argc, char *argv[])
{
    FILE   *in;
    FILE   *out;
    char   line[200];
    int    ln = 0;
    char   *p;
    int    i, j, b;
    int    io;
    int    addr1, addr2;
    char   *q;
    char   note[20];
    char   ec[20];
    uint32_t  bits[5];
    int    parity;

    /* Syntax, cros2065 input output */
    /*   or    cros2065 <input >output */
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
        /* Ignore header lines */
        if (strncasecmp(line, "    ", 4) == 0 ||
            strncasecmp(line, "hex ", 4) == 0)
            goto loop;
        /* Ignore lines that are blank */
        if (line[0] == ' ' && line[1] == ' ')
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
                addr1 += (tolower(*p) - 'a') + 0xa;
        }
        if (*p == '\n')
            goto loop;
        /* Grab binary next address */
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
           fprintf(stderr, "Address2 not complete %d %s\n", ln, line);
           goto loop;
        }
        if (addr1 != addr2) {
           fprintf(stderr, "Address not match %d %03x %03x %s\n", ln, addr1, addr2, line);
           goto loop;
        }

        /* Skip a blank */
        for (j = 0; *p == ' '; j++,p++);
        q = note;
        if (strncmp(p, "- ", 2) != 0 && strncmp(p, "er", 2) != 0 && strncmp(p, "sc", 2) != 0) {
            /* Grab sheet and box */
            while ((*p != ' ' && *p != '\n')) *q++ = *p++;
            *q++ = '-';
            while (*p == ' ') p++;
            while ((*p != ' ' && *p != '\n')) *q++ = *p++;
        *q = '\0';
        }
        *q++ = '\0';
        while (*p == ' ') p++;
        /* Grab mode */
        if (strncasecmp(p, "error", 5) == 0) {
            io = 2;
        } else if (strncasecmp(p, "scan", 4) == 0) {
            io = 1;
        } else {
            io = 0;
        }
        ros_2065[addr1].MODE = io;
        p += 5;
        while (*p == ' ') p++;
        strcpy(&ros_2065[addr1].note[0], &note[0]);
        /* Grab rest of line */
        j = b = 0;
        parity = 1;
        bits[0] = bits[1] = bits[2] = bits[4] = 0;
        for (;*p != '\0' && *p != '\n'; p++) {
            if (*p == '0') {
               b++;
               bits[j] <<= 1;
            } else if (*p == '1') {
               b++;
               bits[j] = (bits[j] << 1) | 1;
               parity ^= 1;
            } else if (*p != ' ') {
               fprintf(stderr, "invalid char %d %c %s\n", ln, *p, line);
            }
            if (b == 19 && j == 0) {
               j++;
               b = 0;
               parity = 1;
            } else if (b == 28 && j == 1) {
               j++;
               b = 0;
               parity = 1;
            } else if (b == 31 && j == 2) {
               j++;
               b = 0;
            } else if (b == 21 && j == 3) {
               break;
            }
        }
        q = ec;
        /* Grab EC */
        while ((*p != ' ' && *p != '\n')) *q++ = *p++;
        *q++ = '\0';
        strcpy(&ros_2065[addr1].ec[0], &ec[0]);

        ros_2065[addr1].A = (bits[0] >> 10) & 0xf;
        ros_2065[addr1].B = (bits[0] >> 8) & 0x3;
        ros_2065[addr1].C = (bits[0] >> 6) & 0xf;
        ros_2065[addr1].D = (bits[0] >> 0) & 7;
        ros_2065[addr1].E = ((bits[1] >> 22) & 0xf) | (((bits[3] >> 18) & 1) << 4);
        ros_2065[addr1].F = (bits[1] >> 16) & 0x1f;
        ros_2065[addr1].G = (bits[1] >> 11) & 0x1f;
        ros_2065[addr1].H = (bits[1] >> 4) & 0x3f;
        ros_2065[addr1].L = (bits[1] >> 0) & 0xf;
        ros_2065[addr1].J = (bits[2] >> 8) & 0x3f;
        ros_2065[addr1].K = (bits[2] >> 16) & 0x1f;
        ros_2065[addr1].M = (bits[2] >> 4) & 0xf;
        ros_2065[addr1].N = (bits[2] >> 0) & 0xf ;
        ros_2065[addr1].P = (bits[3] >> 19) & 7;
        ros_2065[addr1].Q = (bits[3] >> 15) & 7;
        ros_2065[addr1].R = (bits[3] >> 13) & 1;
        ros_2065[addr1].T = (bits[3] >> 9) & 7;
        ros_2065[addr1].U = ((bits[3] >> 4) & 3) | (bits[3] & 0x8);
        ros_2065[addr1].V = (bits[3] >> 0) & 3;
        ros_2065[addr1].W = (bits[0] >> 15) & 0xf;
        ros_2065[addr1].NX = ((bits[2] >> 21) & 0x1ff) << 2;
        ros_2065[addr1].row1 = bits[0];
        ros_2065[addr1].row2 = bits[1];
        ros_2065[addr1].row3 = bits[2];
        ros_2065[addr1].row4 = bits[3];
    }
    for (addr1 = 0; addr1 < 4096; addr1++) {
                          /*       MODE    A     B     C     D     E */
        fprintf(out, "/* %03x */ { 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, "
                   /*   F      G    H      J      K    L      M     N   P  */
                     " 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, "
                   /*    Q    R     T     U     V     W    NX  */
                     " 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, "
                   /*  row1    row2    row3     row4   note  ec */
                     " 0x%08x, 0x%08x, 0x%08x, 0x%08x, \"%s\", \"%s\"},\n",
                  addr1, ros_2065[addr1].MODE, ros_2065[addr1].A, ros_2065[addr1].B,
                         ros_2065[addr1].C, ros_2065[addr1].D, ros_2065[addr1].E,
                         ros_2065[addr1].F, ros_2065[addr1].G, ros_2065[addr1].H,
                         ros_2065[addr1].J, ros_2065[addr1].K, ros_2065[addr1].L,
                         ros_2065[addr1].M, ros_2065[addr1].N, ros_2065[addr1].P,
                         ros_2065[addr1].Q, ros_2065[addr1].R, ros_2065[addr1].T,
                         ros_2065[addr1].U, ros_2065[addr1].V, ros_2065[addr1].W,
                         ros_2065[addr1].NX, ros_2065[addr1].row1, ros_2065[addr1].row2,
                         ros_2065[addr1].row3, ros_2065[addr1].row4,
                         ros_2065[addr1].note, ros_2065[addr1].ec);
    }
    return 0;
}


