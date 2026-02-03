/*
 * microsim360 - Model 1403 printer
 *
 * Copyright 2026, Richard Cornwell
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

/*
 *  Commands.
 *
 *                 01234567
 *  Write & Space  0LLLL001       L = 0000 to 0011
 *  Space Immedate 0LLLL011       L = 0000 to 0011
 *  Write & Skip   1CCCC001       C = 0001 to 1100
 *  Skip Immediate 1CCCC011       C = 0001 to 1100
 *  Sense          00000100
 */

#ifndef _MODEL1403_H_
#define _MODEL1403_H_
#include "device.h"

struct _1403_context {
    int                    feed_done;    /* Done with paper feed */
    FILE                  *file;         /* Output file. */
    char                  *file_name;    /* Attached file name */
    int                    row;          /* Current print row */
    int                    lpp;          /* Number of lines per page */
    int                    ch9;          /* Channel 9 detected */
    int                    ch12;         /* Channel 12 detected */
    uint16_t               ready;        /* Printer in ready status */
    uint16_t               start;        /* Start printer */
    uint16_t               stop;         /* Stop printer after next cycle */
    uint16_t               single;       /* Single cycle printer */
    uint16_t               form;         /* Forms ready */
    int                    fcb_num;      /* Forms control number */
    const uint16_t        *fcb;          /* Form control block */
    uint8_t                output[15][120];
};

extern char *model1403_type_label[];
extern const uint16_t *model1403_fcb[];

void model1403_feed(struct _1403_context *lpr, int num);

int model1403_create(struct _option *opt);

void model1403_draw(struct _device *unit, void *rend, int u);

void *model1403_control(struct _device *unit, int u, int x, int y);

void model1403_init_graphics(struct _device *unit, void *rend);

struct _device *model1403_init(uint16_t addr, uint16_t addr_2821);

#endif
