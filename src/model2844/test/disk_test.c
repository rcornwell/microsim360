/*
 * microsim360 - Model 2844 disk controller test.
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
#include <stdlib.h>
#include "logger.h"
#include "device.h"
#include "ctest.h"
#include "model2844.h"

uint64_t   step_count = 0;
struct _device *chan[6]; 

CTEST_DATA(disk_test) {
    struct _device *dev;
};

CTEST_SETUP(disk_test) {
    data->dev = model2844_init(NULL, 0x90);
}

CTEST_TEARDOWN(disk_test) {
    if (data->dev->dev)
       free(data->dev->dev);
    if (data->dev)
       free(data->dev);
}

CTEST2(disk_test, reset) {
    int i;

    for (i = 0; i < 20; i++) {
        step_2844((struct _2844_context *)data->dev->dev);
        step_count++;
         if (((struct _2844_context *)data->dev)->WX == 0x5B6)
            break;
    }
}

/* Try to send Test I/O to controller */
CTEST2(disk_test, select) {
    uint16_t    tags;
    uint16_t    bus_out;
    uint16_t    bus_in;
    uint16_t    wx;
    int i;
    int         sel = 0;

    ((struct _2844_context *)(data->dev->dev))->WX = 0;
    tags = CHAN_OPR_OUT;
    for (i = 0; i < 200; i++) {
        step_2844((struct _2844_context *)data->dev->dev);
         wx = ((struct _2844_context *)(data->dev->dev))->WX;
         if (i > 40 && sel == 0 && (wx == 0x599 || wx == 0x5c2))
            break;
        step_2844((struct _2844_context *)data->dev->dev);
         wx = ((struct _2844_context *)(data->dev->dev))->WX;
         if (i > 40 && sel == 0 && (wx == 0x599 || wx == 0x5c2))
            break;
        step_count++;
        if (i == 30) {
           tags |= CHAN_ADR_OUT;
           bus_out = 0x190;
        }
        if (i == 31)
           sel = 1;
        if (sel)
           tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;
        data->dev->bus_func(data->dev, &tags, bus_out, &bus_in);
        if ((tags & CHAN_ADR_IN) != 0) {
           log_trace("Got address in\n");
           ASSERT_EQUAL_X(0x190, bus_in);
           tags &= ~CHAN_ADR_OUT;
           bus_out = 0x100;
           tags |= CHAN_CMD_OUT;
        }
        if ((tags & (CHAN_ADR_IN|CHAN_CMD_OUT)) == CHAN_CMD_OUT) {
           log_trace("Drop command out\n");
           bus_out = 0x100;
           tags &= ~CHAN_CMD_OUT;
        }
        if ((tags & (CHAN_STA_IN)) != 0) {
           log_trace("Status in\n");
           ASSERT_EQUAL_X(0x100, bus_in);
           bus_out = 0x100;
           tags |= CHAN_SRV_OUT;
        }
        if ((tags & (CHAN_STA_IN|CHAN_SRV_OUT)) == (CHAN_SRV_OUT)) {
           log_trace("Status in drop\n");
           bus_out = 0x100;
           tags &= ~(CHAN_SRV_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT);
           sel = 0;
        }
    }
}

/* Try to send Nop to controller */
CTEST2(disk_test, nop) {
    uint16_t    tags;
    uint16_t    bus_out;
    uint16_t    bus_in;
    uint16_t    wx;
    int i;
    int         sel = 0;

    ((struct _2844_context *)(data->dev->dev))->WX = 0;
    tags = CHAN_OPR_OUT;
    for (i = 0; i < 200; i++) {
        step_2844((struct _2844_context *)data->dev->dev);
         wx = ((struct _2844_context *)(data->dev->dev))->WX;
         if (i > 40 && sel == 0 && (wx == 0x599 || wx == 0x5c2))
            break;
log_trace("WX=%03x\n", wx);
        step_2844((struct _2844_context *)data->dev->dev);
         wx = ((struct _2844_context *)(data->dev->dev))->WX;
         if (i > 40 && sel == 0 && (wx == 0x599 || wx == 0x5c2))
            break;
log_trace("WX=%03x\n", wx);
        step_count++;
        if (i == 30) {
           tags |= CHAN_ADR_OUT;
           bus_out = 0x190;
        }
        if (i == 31)
           sel = 1;
        if (sel)
           tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;
        data->dev->bus_func(data->dev, &tags, bus_out, &bus_in);
        if ((tags & CHAN_ADR_IN) != 0) {
           log_trace("Got address in\n");
           ASSERT_EQUAL_X(0x190, bus_in);
           tags &= ~CHAN_ADR_OUT;
           bus_out = 0x103;
           tags |= CHAN_CMD_OUT;
        }
        if ((tags & (CHAN_ADR_IN|CHAN_CMD_OUT)) == CHAN_CMD_OUT) {
           log_trace("Drop command out\n");
           bus_out = 0x100;
           tags &= ~CHAN_CMD_OUT;
        }
        if ((tags & (CHAN_STA_IN)) != 0) {
           log_trace("Status in\n");
           ASSERT_EQUAL_X(0x10C, bus_in);   /* Device end and channel end */
           bus_out = 0x100;
           tags |= CHAN_SRV_OUT;
        }
        if ((tags & (CHAN_STA_IN|CHAN_SRV_OUT)) == (CHAN_SRV_OUT)) {
           log_trace("Status in drop\n");
           bus_out = 0x100;
           tags &= ~(CHAN_SRV_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT);
           sel = 0;
        }
    }
}

/* Try to send Set file mask to controller */
CTEST2(disk_test, sense) {
    uint16_t    tags;
    uint16_t    bus_out;
    uint16_t    bus_in;
    uint16_t    wx;
    int i;
    int         sel = 0;

    ((struct _2844_context *)(data->dev->dev))->WX = 0;
    tags = CHAN_OPR_OUT;
    for (i = 0; i < 500; i++) {
        step_2844((struct _2844_context *)data->dev->dev);
//         wx = ((struct _2844_context *)(data->dev->dev))->WX;
 //        if (i > 40 && sel == 0 && (wx == 0x599 || wx == 0x5c2))
  //          break;
        step_2844((struct _2844_context *)data->dev->dev);
   //      wx = ((struct _2844_context *)(data->dev->dev))->WX;
    //     if (i > 40 && sel == 0 && (wx == 0x599 || wx == 0x5c2))
     //       break;
        step_count++;
        if (i == 30) {
           tags |= CHAN_ADR_OUT;
           bus_out = 0x190;
        }
        if (i == 31)
           sel = 1;
        if (sel)
           tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;
        data->dev->bus_func(data->dev, &tags, bus_out, &bus_in);
        if ((tags & CHAN_ADR_IN) != 0) {
           log_trace("Got address in\n");
           ASSERT_EQUAL_X(0x190, bus_in);
           tags &= ~CHAN_ADR_OUT;
           bus_out = 0x004;
           tags |= CHAN_CMD_OUT;
        }
        if ((tags & (CHAN_ADR_IN|CHAN_CMD_OUT)) == CHAN_CMD_OUT) {
           log_trace("Drop command out\n");
           bus_out = 0x100;
           tags &= ~CHAN_CMD_OUT;
        }
        if ((tags & (CHAN_STA_IN)) != 0) {
           log_trace("Status in\n");
           ASSERT_EQUAL_X(0x100, bus_in);   /* Device end and channel end */
           bus_out = 0x100;
           tags |= CHAN_SRV_OUT;
        }
        if ((tags & (CHAN_STA_IN|CHAN_SRV_OUT)) == (CHAN_SRV_OUT)) {
           log_trace("Status in drop\n");
           bus_out = 0x100;
           tags &= ~(CHAN_SRV_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT);
        }
    }
}

/* Try to send Set file mask to controller */
CTEST2(disk_test, setmask) {
    uint16_t    tags;
    uint16_t    bus_out;
    uint16_t    bus_in;
    uint16_t    wx;
    int i;
    int         sel = 0;

    ((struct _2844_context *)(data->dev->dev))->WX = 0;
    tags = CHAN_OPR_OUT;
    for (i = 0; i < 500; i++) {
        step_2844((struct _2844_context *)data->dev->dev);
//         wx = ((struct _2844_context *)(data->dev->dev))->WX;
 //        if (i > 40 && sel == 0 && (wx == 0x599 || wx == 0x5c2))
  //          break;
        step_2844((struct _2844_context *)data->dev->dev);
   //      wx = ((struct _2844_context *)(data->dev->dev))->WX;
    //     if (i > 40 && sel == 0 && (wx == 0x599 || wx == 0x5c2))
     //       break;
        step_count++;
        if (i == 30) {
           tags |= CHAN_ADR_OUT;
           bus_out = 0x190;
        }
        if (i == 31)
           sel = 1;
        if (sel)
           tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;
        data->dev->bus_func(data->dev, &tags, bus_out, &bus_in);
        if ((tags & CHAN_ADR_IN) != 0) {
           log_trace("Got address in\n");
           ASSERT_EQUAL_X(0x190, bus_in);
           tags &= ~CHAN_ADR_OUT;
           bus_out = 0x01f;
           tags |= CHAN_CMD_OUT;
        }
        if ((tags & (CHAN_ADR_IN|CHAN_CMD_OUT)) == CHAN_CMD_OUT) {
           log_trace("Drop command out\n");
           bus_out = 0x100;
           tags &= ~CHAN_CMD_OUT;
        }
        if ((tags & (CHAN_STA_IN)) != 0) {
           log_trace("Status in\n");
           ASSERT_EQUAL_X(0x100, bus_in);   /* Device end and channel end */
           bus_out = 0x100;
           tags |= CHAN_SRV_OUT;
        }
        if ((tags & (CHAN_STA_IN|CHAN_SRV_OUT)) == (CHAN_SRV_OUT)) {
           log_trace("Status in drop\n");
           bus_out = 0x100;
           tags &= ~(CHAN_SRV_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT);
        }
    }
}

