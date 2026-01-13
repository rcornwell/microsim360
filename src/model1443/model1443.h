/*
 * microsim360 - Model 1443 printer
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

#ifndef _MODEL1443_H_
#define _MODEL1443_H_
#include "device.h"

struct _1443_context {
    int                    addr;         /* Device address */
    int                    chan;         /* Channel address */
    int                    state;        /* Current channel state */
    int                    selected;     /* Device currently selected */
    int                    request;      /* Requested channel */
    int                    addressed;    /* Device has been addressed */
    int                    stacked;      /* Stacked status */
    int                    busy_flag;    /* If we returned fast busy */
    int                    sense;        /* Current sense value */
    int                    cmd;          /* Current command */
    int                    status;       /* Current bus status */
    int                    data;         /* Current byte to send/recieve */
    int                    data_rdy;     /* Data is valid */
    int                    data_end;     /* End of data transfer */
    int                    feed_done;    /* Done with paper feed */
    FILE                  *file;         /* Output file. */
    char                  *file_name;    /* Attached file name */
    int                    buf[144];     /* Line buffer */
    int                    col;          /* Current transfer column */
    int                    row;          /* Current print row */
    int                    lpp;          /* Number of lines per page */
    uint16_t               ready;        /* Printer in ready status */
    uint16_t               start;        /* Start printer */
    uint16_t               stop;         /* Stop printer after next cycle */
    uint16_t               single;       /* Single cycle printer */
    uint16_t               form;         /* Forms ready */
    int                    cnt;          /* Transfer count */
    int                    fcb_num;      /* Forms control number */
    const uint16_t        *fcb;          /* Form control block */
    uint8_t                output[15][120];
};


int print_line(struct _1443_context *ctx);

int model1443_create(struct _option *opt);

void model1443_dev(struct _device *unit, uint16_t *tags, uint16_t bus_out, uint16_t *bus_in);

void model1443_draw(struct _device *unit, void *rend, int u);

void *model1443_control(struct _device *unit, int u, int x, int y);

void model1443_init(struct _device *unit, void *rend);

int model1443_create(struct _option *opt);
#endif
