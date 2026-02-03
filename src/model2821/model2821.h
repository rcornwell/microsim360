/*
 * microsim360 - Model 2821 controller.
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

#include <stdio.h>
#include <stdint.h>
#include "device.h"

#ifndef _2821_H_
#define _2821_h_

#define DEVICE_2821_READER  0
#define DEVICE_2821_PUNCH   1
#define DEVICE_2821_PRINTER 2

#define DEVICE_2821_OK      0
#define DEVICE_2821_DUP     1
#define DEVICE_2821_FULL    2
#define DEVICE_2821_UNKN    3

struct _2821_dev_context {
    uint8_t                buffer[256];       /* Data buffer. */
    int                    blen;              /* Length of buffer. */
    int                    sense;             /* Sense data byte */
    uint16_t               addr;              /* Address of device */
    int                    request;           /* Device requesting channel */
    int                    stacked;           /* Device has stacked status */
    int                    xfer_count;        /* Amount of data transfered. */
    int                    mpx_count;         /* Amount of data to xfer on non burst xfer */
    int                    bptr;              /* Pointer in buffer for transfer */
    int                    busy;              /* Device in operation */
    int                    status;            /* Current bus status */
    int                    cmd;               /* Current command */
    int                    cmd_done;          /* Current command */
    void                   (*device_cmd)(struct _2821_dev_context *unit, uint16_t bus_out);
    void                   (*device_init)(struct _2821_dev_context *unit, void *rend);
    void                   (*device_xfer_done)(struct _2821_dev_context *unit);
    void                   (*device_halt)(struct _2821_dev_context *unit);
    struct _2821_context   *unit;             /* Pointer to 2821 context */
    struct _device         *device;           /* Device */
    void                   *ctx;              /* Private data context */
};

struct _2821_context {
    device_state_t         state;             /* Current channel state */
    int                    selected;          /* Device currently selected */
    int                    disconnect;        /* Disconnect device if in operation */
    uint16_t               data;              /* Current data byte */
    int                    mode;              /* Burst or byte mode */
    struct _2821_dev_context *sel_device;     /* Pointer to currently selected device */
    struct _2821_dev_context *device[5];      /* Pointer to device contexts */
};

void model2821_dev(struct _device *unit, uint16_t *tags, uint16_t bus_out, uint16_t *bus_in);

int  model2821_create(struct _option *opt);

struct _device *model2821_init(uint16_t addr);

struct _2821_dev_context *model2821_get_type(struct _device *unit, int dev_type);

int  model2821_register(struct _device *unit, struct _2821_dev_context *dev, int dev_type);

#endif

