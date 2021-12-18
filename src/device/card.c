/* Generic Card read/punch routines for simulators.
 *
 * Copyright (c) 2021, Richard Cornwell
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

/* Generic Card read/punch routines for simulators.

   Input formats are accepted in a variaty of formats:
        Standard ASCII: one record per line.
                returns are ignored.
                tabs are expanded to modules 8 characters.
                ~ in first column is treated as a EOF.

        Binary Card format:
                Each record 160 characters.
                First characters 6789----
                Second character 21012345
                                 111
                Top 4 bits of second character are 0.
                It is unlikely that any other format could
                look like this.

    ASCII mode recognizes some additional forms of input which allows the
    intermixing of binary cards with text cards. 

    Lines beginning with ~raw are taken as a number of 4 digit octal values
    with represent each column of the card from 12 row down to 9 row. If there
    is not enough octal numbers to span a full card the remainder of the 
    card will not be punched.

    Also ~eor, will generate a 7/8/9 punch card. An ~eof will gernerate a
    6/7/9 punch card, and a ~eoi will generate a 6/7/8/9 punch.

    A single line of ~ will set the EOF flag when that card is read.

    For autodetection of card format, there can be no parity errors.
    All undeterminate formats are treated as ASCII.

    Auto output format is ASCII if card has only printable characters
    or card format binary.
*/


#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <memory.h>
#include "card.h"

/* Character conversion tables */

/* Set for IBM 029 codes */
static const uint16_t       ascii_to_hol_029[128] = {
   /* Control                              */
    0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,    /*0-37*/
   /*Control*/
    0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,
   /*Control*/
    0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,
   /*Control*/
    0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,
   /*  sp      !      "      #      $      %      &      ' */
   /* none   X28    78      38    Y38    T48    X      58  */
    0x000, 0x482, 0x006, 0x042, 0x442, 0x222, 0x800, 0x012,     /* 40 - 77 */
   /*   (      )      *      +      ,      -      .      / */
   /* X58    Y58    Y48    X68    T38    Y      X38    T1  */
    0x812, 0x412, 0x422, 0x80A, 0x242, 0x400, 0x842, 0x300,
   /*   0      1      2      3      4      5      6      7 */
   /* T      1      2      3      4      5      6      7   */
    0x200, 0x100, 0x080, 0x040, 0x020, 0x010, 0x008, 0x004,
   /*   8      9      :      ;      <      =      >      ? */
   /* 8      9      28     Y68    X48     68    T68    T78 */
    0x002, 0x001, 0x082, 0x40A, 0x822, 0x00A, 0x20A, 0x206,
   /*   @      A      B      C      D      E      F      G */
   /*  48    X1     X2     X3     X4     X5     X6     X7  */
    0x022, 0x900, 0x880, 0x840, 0x820, 0x810, 0x808, 0x804,     /* 100 - 137 */
   /*   H      I      J      K      L      M      N      O */
   /* X8     X9     Y1     Y2     Y3     Y4     Y5     Y6  */
    0x802, 0x801, 0x500, 0x480, 0x440, 0x420, 0x410, 0x408,
   /*   P      Q      R      S      T      U      V      W */
   /* Y7     Y8     Y9     T2     T3     T4     T5     T6  */
    0x404, 0x402, 0x401, 0x280, 0x240, 0x220, 0x210, 0x208,
   /*   X      Y      Z      [      \      ]      ^      _ */
   /* T7     T8     T9   TY028    T28  TY038    Y78    T58 */
    0x204, 0x202, 0x201, 0xE82, 0x282, 0xE42, 0x406, 0x212,
   /*   `      a      b      c      d      e      f      g */
    0x102 ,0xB00, 0xA80, 0xA40, 0xA20, 0xA10, 0xA08, 0xA04,     /* 140 - 177 */
   /*   h      i      j      k      l      m      n      o */
    0xA02, 0xA01, 0xD00, 0xC80, 0xC40, 0xC20, 0xC10, 0xC08,
   /*   p      q      r      s      t      u      v      w */
    0xC04, 0xC02, 0xC01, 0x680, 0x640, 0x620, 0x610, 0x608,
   /*   x      y      z      {      |      }      ~    del */
   /*                      Y78    X78    X79  XTY18        */
    0x604, 0x602, 0x601, 0x406, 0x806, 0x805, 0xF02,0xf000
};

/* Ascii codes to IBM EBCDIC punch codes */
static const uint16_t       ascii_to_hol_ebcdic[128] = {
   /* Control                              */
    0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,    /*0-37*/
   /*Control*/
    0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,
   /*Control*/
    0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,
   /*Control*/
    0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,0xf000,
   /*  sp      !      "      #      $      %      &      ' */
   /* none   Y28    78      38    Y38    T48    X      58  */
    0x000, 0x482, 0x006, 0x042, 0x442, 0x222, 0x800, 0x012,     /* 40 - 77 */
   /*   (      )      *      +      ,      -      .      / */
   /* X58    Y58    Y48    X      T38    Y      X38    T1  */
    0x812, 0x412, 0x422, 0x800, 0x242, 0x400, 0x842, 0x300,
   /*   0      1      2      3      4      5      6      7 */
   /* T      1      2      3      4      5      6      7   */
    0x200, 0x100, 0x080, 0x040, 0x020, 0x010, 0x008, 0x004,
   /*   8      9      :      ;      <      =      >      ? */
   /* 8      9      28     Y68    X48    68     T68    T78 */
    0x002, 0x001, 0x082, 0x40A, 0x822, 0x00A, 0x20A, 0x206,
   /*   @      A      B      C      D      E      F      G */
   /*  48    X1     X2     X3     X4     X5     X6     X7  */
    0x022, 0x900, 0x880, 0x840, 0x820, 0x810, 0x808, 0x804,     /* 100 - 137 */
   /*   H      I      J      K      L      M      N      O */
   /* X8     X9     Y1     Y2     Y3     Y4     Y5     Y6  */
    0x802, 0x801, 0x500, 0x480, 0x440, 0x420, 0x410, 0x408,
   /*   P      Q      R      S      T      U      V      W */
   /* Y7     Y8     Y9     T2     T3     T4     T5     T6  */
    0x404, 0x402, 0x401, 0x280, 0x240, 0x220, 0x210, 0x208,
   /*   X      Y      Z      [      \      ]      ^      _ */
   /* T7     T8     T9     X28    X68    Y28    Y78    X58 */
    0x204, 0x202, 0x201, 0x882, 0x20A, 0x482, 0x406, 0x212,
   /*   `      a      b      c      d      e      f      g */
    0x102, 0xB00, 0xA80, 0xA40, 0xA20, 0xA10, 0xA08, 0xA04,     /* 140 - 177 */
   /*   h      i      j      k      l      m      n      o */
    0xA02, 0xA01, 0xD00, 0xC80, 0xC40, 0xC20, 0xC10, 0xC08,
   /*   p      q      r      s      t      u      v      w */
    0xC04, 0xC02, 0xC01, 0x680, 0x640, 0x620, 0x610, 0x608,
   /*   x      y      z      {      |      }      ~    del */
   /*                     X18     X78    Y18  XYT18        */
    0x604, 0x602, 0x601, 0x902, 0x806, 0x502, 0xF02,0xf000
};


/* IBM EBCDIC codes to IBM punch codes */
static uint16_t ebcdic_to_hol_table[256] = {
 /*  T918    T91    T92    T93    T94    T95    T96   T97   0x0x */
   0xB03,  0x901, 0x881, 0x841, 0x821, 0x811, 0x809, 0x805,
 /*  T98,   T189 , T289,  T389,  T489,  T589,  T689, T789   */
   0x803,  0x903, 0x883, 0x843, 0x823, 0x813, 0x80B, 0x807,
 /* TE189    E91    E92    E93    E94    E95    E96   E97   0x1x */
   0xD03,  0x501, 0x481, 0x441, 0x421, 0x411, 0x409, 0x405,
 /*  E98     E918   E928   E938   E948   E958   E968  E978   */
   0x403,  0x503, 0x483, 0x443, 0x423, 0x413, 0x40B, 0x407,
 /*  E0918   091    092    093    094    095    096   097   0x2x */
   0x703,  0x301, 0x281, 0x241, 0x221, 0x211, 0x209, 0x205,
 /*  098     0918  0928   0938    0948   0958   0968  0978   */
   0x203,  0x303, 0x283, 0x243, 0x223, 0x213, 0x20B, 0x207,
 /* TE0918   91    92     93      94     95     96     97   0x3x */
   0xF03,  0x101, 0x081, 0x041, 0x021, 0x011, 0x009, 0x005,
 /*  98      189    289    389    489    589    689    789   */
   0x003,  0x103, 0x083, 0x043, 0x023, 0x013, 0x00B, 0x007,
 /*          T091  T092   T093   T094   T095   T096    T097  0x4x */
   0x000,  0xB01, 0xA81, 0xA41, 0xA21, 0xA11, 0xA09, 0xA05,
 /* T098     T18    T28    T38    T48    T58    T68    T78    */
   0xA03,  0x902, 0x882, 0x842, 0x822, 0x812, 0x80A, 0x806,
 /* T        TE91  TE92   TE93   TE94   TE95   TE96    TE97  0x5x */
   0x800,  0xD01, 0xC81, 0xC41, 0xC21, 0xC11, 0xC09, 0xC05,
 /* TE98     E18    E28    E38    E48    E58    E68    E78   */
   0xC03,  0x502, 0x482, 0x442, 0x422, 0x412, 0x40A, 0x406,
 /* E        01    E092   E093   E094   E095   E096    E097  0x6x */
   0x400,  0x300, 0x681, 0x641, 0x621, 0x611, 0x609, 0x605,
 /* E098     018   TE     038    048     68    068     078    */
   0x603,  0x302, 0xC00, 0x242, 0x222, 0x212, 0x20A, 0x206,
 /* TE0    TE091  TE092  TE093  TE094  TE095  TE096  TE097   0x7x */
   0xE00,  0xF01, 0xE81, 0xE41, 0xE21, 0xE11, 0xE09, 0xE05,
 /* TE098    18     28     38    48      58      68     78    */
   0xE03,  0x102, 0x082, 0x042, 0x022, 0x012, 0x00A, 0x006,
 /* T018     T01    T02    T03    T04    T05    T06    T07   0x8x */
   0xB02,  0xB00, 0xA80, 0xA40, 0xA20, 0xA10, 0xA08, 0xA04,
 /* T08      T09   T028   T038    T048   T058   T068   T078   */
   0xA02,  0xA01, 0xA82, 0xA42, 0xA22, 0xA12, 0xA0A, 0xA06,
 /* TE18     TE1    TE2    TE3    TE4    TE5    TE6    TE7   0x9x */
   0xD02,  0xD00, 0xC80, 0xC40, 0xC20, 0xC10, 0xC08, 0xC04,
 /* TE8      TE9   TE28   TE38    TE48   TE58   TE68   TE78   */
   0xC02,  0xC01, 0xC82, 0xC42, 0xC22, 0xC12, 0xC0A, 0xC06,
 /* E018     E01    E02    E03    E04    E05    E06    E07   0xax */
   0x702,  0x700, 0x680, 0x640, 0x620, 0x610, 0x608, 0x604,
 /* E08      E09   E028   E038    E048   E058   E068   E078   */
   0x602,  0x601, 0x682, 0x642, 0x622, 0x612, 0x60A, 0x606,
 /* TE018    TE01   TE02   TE03   TE04   TE05   TE06   TE07  0xbx */
   0xF02,  0xF00, 0xE80, 0xE40, 0xE20, 0xE10, 0xE08, 0xE04,
 /* TE08     TE09   TE028  TE038  TE048  TE058  TE068  TE078  */
   0xE02,  0xE01, 0xE82, 0xE42, 0xE22, 0xE12, 0xE0A, 0xE06,
 /*  T0      T1     T2     T3     T4     T5     T6     T7    0xcx */
   0xA00,  0x900, 0x880, 0x840, 0x820, 0x810, 0x808, 0x804,
 /* T8       T9     T0928  T0938  T0948  T0958  T0968  T0978  */
   0x802,  0x801, 0xA83, 0xA43, 0xA23, 0xA13, 0xA0B, 0xA07,
 /* E0       E1     E2     E3     E4     E5     E6     E7    0xdx */
   0x600,  0x500, 0x480, 0x440, 0x420, 0x410, 0x408, 0x404,
 /* E8       E9     TE928  TE938  TE948  TE958  TE968  TE978  */
   0x402,  0x401, 0xC83, 0xC43, 0xC23, 0xC13, 0xC0B, 0xC07,
 /* 028      E091   02     03     04     05     06     07    0xex  */
   0x282,  0x701, 0x280, 0x240, 0x220, 0x210, 0x208, 0x204,
 /* 08       09     E0928  E0938  E0948  E0958  E0968  E0978  */
   0x202,  0x201, 0x683, 0x643, 0x623, 0x613, 0x60B, 0x607,
 /* 0        1      2      3      4      5      6      7     0xfx */
   0x200,  0x100, 0x080, 0x040, 0x020, 0x010, 0x008, 0x004,
 /* 8        9     TE0928 TE0938 TE0948 TE0958 TE0968 TE0978  */
   0x002,  0x001, 0xE83, 0xE43, 0xE23, 0xE13, 0xE0B, 0xE07
};

/* Back tables which are automatically generated */
static uint8_t  hol_to_ascii_table[4096];  /* Back conversion table */

static uint16_t hol_to_ebcdic_table[4096];

/* Convert EBCDIC character into hollerith code */
uint16_t
ebcdic_to_hol(uint8_t ebcdic)
{
   return ebcdic_to_hol_table[ebcdic];
}

/* Returns the BCD of the hollerith code or 0x7f if error */
uint16_t
hol_to_ebcdic(uint16_t hol)
{
    return hol_to_ebcdic_table[hol];
}

/* Return number of cards currently in hopper */
int
hopper_size(struct card_context *card_ctx)
{
    if (card_ctx == NULL)
        return 0;
    return card_ctx->hopper_cards - card_ctx->hopper_pos;
}

/* Return number of cards currently in stack */
int
stack_size(struct card_context *card_ctx)
{
    if (card_ctx == NULL)
        return 0;
    return card_ctx->hopper_cards;
}

/* Return next card off hopper */
int
read_card(struct card_context *card_ctx, uint16_t (*image)[80])
{
    int                   i;
    uint16_t             (*img)[80];
    uint8_t              out[81];
    int                   ok = 1;

    if (card_ctx->hopper_pos >= card_ctx->hopper_cards)
        return 0;

    img = &(*card_ctx->images)[card_ctx->hopper_pos];
    for (i = 0; i < 80; i++) {
        out[i] = hol_to_ebcdic_table[(int)(*img)[i]];
    }
    if (ok) {
        printf("Read hopper: [");
        for (i = 0; i < 80; i++) {
            printf("%02x,", out[i]);
        }
        printf("]\n");
    } else {
        printf("Read hopper binary\n");
    }
    card_ctx->hopper_pos++;
    memcpy(image, img, 80 * sizeof(uint16_t));
    return 1;
}


struct _card_buffer {
   uint8_t              buffer[8192+500];    /* Buffer data */
   int                   len;                 /* Amount of data in buffer */
   int                   size;                /* Size of last card read */
};

/* Helper function to check for special codes */
static int
_cmpcard(const uint8_t *p, const char *s)
{
   int  i;
   if (p[0] != '~')
        return 0;
   for(i = 0; i < 3; i++) {
        if (tolower(p[i+1]) != s[i])
           return 0;
   }
   return 1;
}

/* Parse a card from external form into hollerith punch codes */
static void
_parse_card(struct card_context *card_ctx, struct _card_buffer *buf, uint16_t (*image)[80])
{
    unsigned int          mode;
    uint16_t             temp;
    int                   i;
    char                  c;
    int                   col;

    memset(image, 0, 80 * sizeof(uint16_t));
    printf( "Read card ");
    if (card_ctx->mode == MODE_AUTO) {
        mode = MODE_TEXT;   /* Default is text */

        /* Check buffer to see if binary card in it. */
        for (i = 0, temp = 0; i < 160 && i <buf->len; i+=2)
            temp |= (uint16_t)(buf->buffer[i] & 0xFF);
        /* Check if every other char < 16 & full buffer */
        if ((temp & 0x0f) == 0 && i == 160)
            mode = MODE_BIN;        /* Probably binary */

        /* Check if modes match */
        if (card_ctx->mode != MODE_AUTO && card_ctx->mode != mode) {
            (*image)[0] = 0xfff;
            printf("invalid mode\n");
            return;
        }
    } else
        mode = card_ctx->mode;

    switch(mode) {
    default:
    case MODE_TEXT:
        printf("text: [");
        /* Check for special codes */
        if (buf->buffer[0] == '~') {
            int f = 1;
            for(col = i = 1; col < 80 && f && i < buf->len; i++) {
                c = buf->buffer[i];
                switch (c) {
                case '\n':
                case '\0':
                case '\r':
                    col = 80;
                case ' ':
                    break;              /* Ignore these */
                case '\t':
                    col = (col | 7) + 1;        /* Mult of 8 */
                    break;
                default:
                    f = 0;
                    break;
                }
             }
             if (f) {
                goto end_card;
             }
        }
        if (_cmpcard(&buf->buffer[0], "raw")) {
            int         j = 0;
            printf("-octal-");
            for(col = 0, i = 4; col < 80 && i < buf->len; i++) {
                if (buf->buffer[i] >= '0' && buf->buffer[i] <= '7') {
                    (*image)[col] = ((*image)[col] << 3) | (buf->buffer[i] - '0');
                    j++;
                } else if (buf->buffer[i] == '\n' || buf->buffer[i] == '\r') {
                   (*image)[0] |= 0xfff;
                } else {
                    break;
                }
                if (j == 4) {
                   col++;
                   j = 0;
                }
            }
        } else if (_cmpcard(&buf->buffer[0], "eor")) {
            printf("-eor-");
            (*image)[0] = 07;        /* 7/8/9 punch */
            i = 4;
        } else if (_cmpcard(&buf->buffer[0], "eof")) {
            printf("-eof-");
            (*image)[0] = 015;       /* 6/7/9 punch */
            i = 4;
        } else if (_cmpcard(&buf->buffer[0], "eoi")) {
            printf("-eoi-");
            (*image)[0] = 017;       /* 6/7/8/9 punch */
            i = 4;
        } else {
            /* Convert text line into card image */
            for (col = 0, i = 0; col < 80 && i < buf->len; i++) {
                c = buf->buffer[i];
                switch (c) {
                case '\0':
                case '\r':
                    break;              /* Ignore these */
                case '\t':
                    col = (col | 7) + 1;        /* Mult of 8 */
                    break;
                case '\n':
                    col = 80;
                    i--;
                    break;
                default:
                    printf("%c", c);
                    temp = ascii_to_hol_029[(int)c];
                    if (temp & 0xf000)
                        temp = 0xfff;
                    (*image)[col++] = temp & 0xfff;
                }
            }
        }
    end_card:
        printf("-%d-", i);

        /* Scan to end of line, ignore anything after last column */
        while (buf->buffer[i] != '\n' && buf->buffer[i] != '\r' && i < buf->len) {
            i++;
        }
        if (buf->buffer[i] == '\r')
            i++;
        if (buf->buffer[i] == '\n')
            i++;
        printf("]\n");
        break;

    case MODE_BIN:
        temp = 0;
        printf( "bin\n");
        if (buf->len < 160) {
            (*image)[0] = 0xfff;
            return;
        }
        /* Move data to buffer */
        for (col = i = 0; i < 160;) {
            temp |= (uint16_t)(buf->buffer[i] & 0xff);
            (*image)[col] = (buf->buffer[i++] >> 4) & 0xF;
            (*image)[col++] |= ((uint16_t)buf->buffer[i++] & 0xff) << 4;
        }
        /* Check if format error */
        if (temp & 0xF)
            (*image)[0] = 0xfff;

        break;

    case MODE_EBCDIC:
        printf("ebcdic\n");
        if (buf->len < 80)
            (*image)[0] |= 0xfff;
        /* Move data to buffer */
        for (i = 0; i < 80 && i < buf->len; i++) {
            temp = (uint16_t)(buf->buffer[i]) & 0xFF;
            (*image)[i] = ebcdic_to_hol_table[temp];
        }
        break;

    }
    buf->size = i;
}

/* Card punch routine

   Modifiers have been checked by the caller
   C modifier is recognized (column binary is implemented)
*/


static void
_punch_card(struct card_context *card_ctx, uint16_t (*image)[80])
{
/* Convert word record into column image */
/* Check output type, if auto or text, try and convert record to bcd first */
/* If failed and text report error and dump what we have */
/* Else if binary or not convertable, dump as image */

    /* Try to convert to text */
    uint8_t             out[512];
    int                  i;
    int                  outp = 0;
    int                  mode = card_ctx->mode;
    int                  ok = 1;


    /* Fix mode if in auto mode */
    if (mode == MODE_AUTO) {
         /* Try to convert each column to ascii */
         for (i = 0; i < 80; i++) {
             out[i] = hol_to_ascii_table[(*image)[i]];
             if (out[i] == 0xff) {
                ok = 0;
             }
         }
         mode = ok?MODE_TEXT:MODE_OCTAL;
    }

    switch(mode) {
    default:
    case MODE_TEXT:
        /* Scan each column */
        printf("text: [");
        for (i = 0; i < 80; i++, outp++) {
            out[outp] = hol_to_ascii_table[(*image)[i]];
            if (out[outp] == 0xff) {
               out[outp] = '?';
            }
            printf("%c", out[outp]);
        }
        printf("]\n");
        /* Trim off trailing spaces */
        while (outp > 0 && out[--outp] == ' ') ;
        out[++outp] = '\n';
        out[++outp] = '\0';
        break;

    case MODE_OCTAL:
        printf("octal: [");
        out[outp++] = '~';
        for (i = 79; i >= 0; i--) {
            if ((*image)[i] != 0)
               break;
        }
        /* Check if special card */
        if (i == 0) {
            out[outp++] = 'e';
            out[outp++] = 'o';
            if ((*image)[0] == 07) {
               out[outp++] = 'r';
               out[outp++] = '\n';
               printf( "eor\n");
               break;
            }
            if ((*image)[0] == 015) {
               out[outp++] = 'f';
               out[outp++] = '\n';
               printf( "eof\n");
               break;
            }
            if ((*image)[0] == 017) {
               out[outp++] = 'f';
               out[outp++] = '\n';
               printf( "eoi\n");
               break;
            }
        }
        out[outp++] = 'r';
        out[outp++] = 'a';
        out[outp++] = 'w';
        for (i = 0; i < 80; i++) {
            uint16_t col = (*image)[i];
            out[outp++] = ((col >> 9) & 07) + '0';
            out[outp++] = ((col >> 6) & 07) + '0';
            out[outp++] = ((col >> 3) & 07) + '0';
            out[outp++] = (col & 07) + '0';
        }
        out[outp++] = '\n';
        printf( "%s", &out[4]);
        break;


    case MODE_BIN:
        printf( "bin\n");
        for (i = 0; i < 80; i++) {
            uint16_t      col = (*image)[i];
            out[outp++] = (col & 0x00f) << 4;
            out[outp++] = (col & 0xff0) >> 4;
        }
        break;

    case MODE_EBCDIC:
        printf( "ebcdic\n");
        /* Fill buffer */
        for (i = 0; i < 80; i++, outp++) {
            uint16_t      col = (*image)[i];
            out[outp] = 0xff & hol_to_ebcdic_table[col];
        }
        break;
    }
    card_ctx->hopper_pos++;
    fwrite(out, 1, outp, card_ctx->file);
}


/* Read file into hopper */
int
read_deck(struct card_context *card_ctx, char *file_name)
{
    struct _card_buffer   buf;
    int                   i;
    int                   j;
    int                   l;
    int                   cards = 0;
    int                   r = 1;

    buf.len = 0;
    buf.size = 0;
    buf.buffer[0] = 0; /* Initialize bufer to empty */

    free (card_ctx->file_name);
	card_ctx->file_name = NULL;
    card_ctx->file = fopen(file_name, "rb");
    if (card_ctx->file == NULL)
       return -1;

	if ((card_ctx->file_name = (char*)malloc(strlen(file_name)+1)) == NULL) {
		fclose(card_ctx->file);
		return -1;
	}

	strcpy(card_ctx->file_name, file_name);

    /* Move stack down if any cards in it */
    if (card_ctx->hopper_pos > 0) {
       for ( i = 0; card_ctx->hopper_pos < card_ctx->hopper_cards; i++) {
           memcpy(&card_ctx->images[i], &card_ctx->images[card_ctx->hopper_pos], 80 * sizeof(uint16_t));
           card_ctx->hopper_pos++;
       }
       /* At end hopper_pos is our new size */
       card_ctx->hopper_cards = i;
       card_ctx->hopper_pos = 0;
    }

    /* Slurp up requested file */
    do {
        if (buf.len < 500 && !feof(card_ctx->file)) {
            l = fread(&buf.buffer[buf.len], 1, 8192, card_ctx->file);
            if (l < 0)
                r = -1;
            else
                buf.len += l;
        }

        /* Allocate space for some more cards if needed */
        if (card_ctx->hopper_cards >= card_ctx->hopper_size) {
            card_ctx->hopper_size += DECK_SIZE;
            card_ctx->images = (uint16_t (*)[1][80])realloc((void *)card_ctx->images,
                       (size_t)card_ctx->hopper_size * sizeof(*(card_ctx->images)));
			if (card_ctx->images == NULL) {
				card_ctx->hopper_size = 0;
				card_ctx->hopper_cards = 0;
				return -1;
			}
            memset((void *)&card_ctx->images[card_ctx->hopper_cards], 0,
                       (size_t)(card_ctx->hopper_size - card_ctx->hopper_cards) *
                             sizeof(*(card_ctx->images)));
        }

        /* Process one card */
        cards++;
        _parse_card(card_ctx, &buf, &(*card_ctx->images)[card_ctx->hopper_cards]);
        card_ctx->hopper_cards++;
        /* Move data to start at begining of buffer */
        /* Data is moved down to simplify the decoding of one card */
        l = buf.len - buf.size;
        j = buf.size;
        for(i = 0; i < l; i++, j++)
            buf.buffer[i] = buf.buffer[j];
        buf.len -= buf.size;
    } while (buf.len > 0 && r == 1);

    /* If there is an error, free just read deck */
    fclose (card_ctx->file);
    card_ctx->file = 0;
    return r;
}


/*
 * Empty hopper of cards.
 */
void
empty_cards(struct card_context *card_ctx)
{
    /* Flush any cards in hopper out to file */
    if (card_ctx->file != NULL) {
        while (card_ctx->hopper_pos < card_ctx->hopper_cards) {
            _punch_card(card_ctx, &(*card_ctx->images)[card_ctx->hopper_cards]);
        }
    }

    /* Reduce size of stack to DECK_SIZE */
    if (card_ctx->hopper_size > DECK_SIZE) {
        card_ctx->hopper_size = DECK_SIZE;
        card_ctx->images = (uint16_t (*)[1][80])realloc((void *)card_ctx->images,
                   (size_t)card_ctx->hopper_size * sizeof(*(card_ctx->images)));
		if (card_ctx->images == NULL) {
			card_ctx->hopper_size = 0;
			card_ctx->hopper_cards = 0;
			return;
		}
        memset((void *)&card_ctx->images[0], 0,
                   (size_t)(card_ctx->hopper_size) * sizeof(*(card_ctx->images)));
    }

    /* Position to head of hopper */
    card_ctx->hopper_pos = 0;
    card_ctx->hopper_cards = 0;
}

/*
 * Add into hopper cards blank cards.
 */
void
blank_deck(struct card_context *card_ctx, int cards)
{

	if (card_ctx->images == NULL)
		return;

    /* Move stack down if any cards in it */
    if (card_ctx->hopper_pos > 0) {
       int   i;
       for (i = 0; card_ctx->hopper_pos < card_ctx->hopper_cards; i++) {
           memcpy(&card_ctx->images[i], &card_ctx->images[card_ctx->hopper_pos], 80 * sizeof(uint16_t));
           card_ctx->hopper_pos++;
       }
       /* At end hopper_pos is our new size */
       card_ctx->hopper_cards = i;
       card_ctx->hopper_pos = 0;
    }

    /* Append cards until done */
    while (cards > 0) {
        /* Allocate space for some more cards if needed */
        if (card_ctx->hopper_cards >= card_ctx->hopper_size) {
            card_ctx->hopper_size += DECK_SIZE;
            card_ctx->images = (uint16_t (*)[1][80])realloc((void *)card_ctx->images,
                       (size_t)card_ctx->hopper_size * sizeof(*(card_ctx->images)));
			if (card_ctx->images == NULL) {
				card_ctx->hopper_size = 0;
				card_ctx->hopper_cards = 0;
				return;
			}
            memset((void *)&card_ctx->images[card_ctx->hopper_cards], 0,
                       (size_t)(card_ctx->hopper_size - card_ctx->hopper_cards) *
                             sizeof(*(card_ctx->images)));
        }

        /* Process one card */
        cards--;
        memset((void *)&(*card_ctx->images)[card_ctx->hopper_cards], 0, 80 * sizeof(uint16_t));
        card_ctx->hopper_cards++;
    }
}

/*
 * Put a card image on stacker.
 */
int
stack_card(struct card_context *card_ctx, uint16_t (*image)[80])
{

    /* Allocate space for some more cards if needed */
    if (card_ctx->hopper_cards >= card_ctx->hopper_size) {
        card_ctx->hopper_size += DECK_SIZE;
        card_ctx->images = (uint16_t (*)[1][80])realloc((void *)card_ctx->images,
                   (size_t)card_ctx->hopper_size * sizeof(*(card_ctx->images)));
		if (card_ctx->images == NULL) {
			card_ctx->hopper_size = 0;
			card_ctx->hopper_cards = 0;
			return -1;
		}
        memset((void *)&card_ctx->images[card_ctx->hopper_cards], 0,
                   (size_t)(card_ctx->hopper_size - card_ctx->hopper_cards) *
                         sizeof(*(card_ctx->images)));
    }

    /* Process one card */
    memcpy(&(*card_ctx->images)[card_ctx->hopper_cards], image, 80 * sizeof(uint16_t));
    card_ctx->hopper_cards++;
    if (card_ctx->file != NULL) {
        while (card_ctx->hopper_pos < card_ctx->hopper_cards) {
            _punch_card(card_ctx, &(*card_ctx->images)[card_ctx->hopper_cards]);
        }
    }
	return 0;
}

/*
 * Put a card image on stacker.
 */
int
save_deck(struct card_context *card_ctx, char *file_name)
{
    if (card_ctx->file) {
        fclose(card_ctx->file);
    }
    free(card_ctx->file_name);

	card_ctx->file = fopen(file_name, "wb");
	if (card_ctx->file == NULL)
		return -1;

	if ((card_ctx->file_name = (char*)malloc(strlen(file_name)+1)) == NULL) {
		fclose(card_ctx->file);
		return -1;
	}

	strcpy(card_ctx->file_name, file_name);

    while (card_ctx->hopper_pos < card_ctx->hopper_cards) {
        _punch_card(card_ctx, &(*card_ctx->images)[card_ctx->hopper_cards]);
    }
	return 0;
}

/*
 * Initialize a stacker. On first time also initialize back
 * conversion tables.
 */
struct card_context *
init_card_context()
{
    unsigned int         i;
    static int           ebcdic_init = 0;
    struct card_context *card_ctx;

	if ((card_ctx = (struct card_context*)malloc(sizeof(struct card_context))) == NULL)
		return NULL;
	memset((void*)card_ctx, 0, sizeof(struct card_context));

    /* Initialize reverse mapping if not initialized */
    if (!ebcdic_init) {
        for (i = 0; i < 4096; i++)
            hol_to_ebcdic_table[i] = 0x100;
        for (i = 0; i < 256; i++) {
            uint16_t     temp = ebcdic_to_hol_table[i];
            if (hol_to_ebcdic_table[temp] != 0x100) {
                fprintf(stderr, "Translation error %02x is %03x and %03x\n",
                    i, temp, hol_to_ebcdic_table[temp]);
            } else {
                hol_to_ebcdic_table[temp] = i;
            }
        }
        memset(&hol_to_ascii_table[0], 0xff, sizeof(hol_to_ascii_table));
        for(i = 0; i < (sizeof(ascii_to_hol_029)/sizeof(uint16_t)); i++) {
             uint16_t          temp;
             temp = ascii_to_hol_029[i];
             if ((temp & 0xf000) == 0) {
                hol_to_ascii_table[temp] = i;
             }
        }
        ebcdic_init = 1;
    }



    card_ctx->hopper_size = 0;
    card_ctx->hopper_cards = 0;
    card_ctx->hopper_pos = 0;
    free(card_ctx->images);
    card_ctx->images = NULL;
    free(card_ctx->file_name);
    card_ctx->file_name = NULL;
    card_ctx->file = NULL;
    return card_ctx;
}


