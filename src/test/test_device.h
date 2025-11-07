/*
 * microsim360 - Test device controller.
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
#include <stdint.h>
#include "device.h"

#define SENSE_MAX       1

struct _test_context {
    device_state_t         state;             /* Current channel state */
    int                    selected;          /* Device currently selected */
    int                    sense[SENSE_MAX];  /* Current sense value */
    int                    sense_cnt;         /* Sense counter */
    int                    cmd;               /* Current command */
    int                    cmd_done;          /* Command finished */
    int                    chaining;          /* Command chaining */
    int                    busy;              /* Device in operation */
    int                    status;            /* Current bus status */
    int                    data;              /* Current byte to send/recieve */
    int                    data_rdy;          /* Data is valid */
    int                    data_end;          /* Data transfer over */
    int                    data_end_post;     /* Data end sent to CPU */
    int                    data_cnt;          /* Data counter */
    int                    disconnect;        /* Disconnect device if in operation */
    int                    delay;             /* Delay */
    uint8_t                buffer[256];       /* Data buffer. */
    int                    max_data;          /* Max counter. */
    int                    burst;             /* Transfer bytes in burst mode */
    int                    burst_cnt;         /* Number of bytes to transfer per burst */
    int                    burst_max;         /* Max number of bytes per burst */
    int                    sms;               /* Return SMS status */
};

void test_step(struct _device *unit);

void test_dev(struct _device *unit, uint16_t *tags, uint16_t bus_out, uint16_t *bus_in);


