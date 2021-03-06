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
#include "event.h"
#include "ctest.h"
#include "xlat.h"
#include "model2844.h"

uint64_t   step_count = 0;
struct _device *chan[6]; 

uint16_t
initial_select(struct _device *dev, uint16_t *tags, int cmd)
{
    uint16_t    bus_out;
    uint16_t    bus_in;
    uint16_t    status;
    int         i;
    int         sel = 0;
    int         sts = 0;

    *tags |= CHAN_OPR_OUT;
    for (i = 0; i < 200; i++) {
        step_2844((struct _2844_context *)dev->dev);
        step_2844((struct _2844_context *)dev->dev);
        advance();
        step_count++;
        if (i == 30) {
           *tags |= CHAN_ADR_OUT;
           bus_out = 0x190;
        }
        if (i == 31)
           sel = 1;
        if (sel) {
           *tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;
        }
        dev->bus_func(dev, tags, bus_out, &bus_in);
        if ((*tags & CHAN_ADR_IN) != 0) {
           log_trace("Got address in\n");
           ASSERT_EQUAL_X(0x190, bus_in);
           *tags &= ~CHAN_ADR_OUT;
           *tags &= ~CHAN_SUP_OUT;
           bus_out = (cmd & 0xff) | odd_parity[cmd & 0xff];
           *tags |= CHAN_CMD_OUT;
        }
        if ((*tags & (CHAN_ADR_IN|CHAN_CMD_OUT)) == CHAN_CMD_OUT) {
           log_trace("Drop command out\n");
           bus_out = 0x100;
           *tags &= ~CHAN_CMD_OUT;
        }
        if ((*tags & (CHAN_STA_IN)) != 0) {
           log_trace("Status in\n");
           bus_out = 0x100;
           status = bus_in;
           *tags |= CHAN_SRV_OUT;
           sts = 1;
        }
        if ((*tags & (CHAN_STA_IN|CHAN_SRV_OUT)) == (CHAN_SRV_OUT)) {
           log_trace("Status in drop\n");
           bus_out = 0x100;
           *tags &= ~(CHAN_SRV_OUT);
           sel = 0;
        }
        if (sts && (*tags & (CHAN_STA_IN|CHAN_SRV_IN)) == (0)) {
           log_trace("Initialize\n");
           bus_out = 0x100;
           *tags &= ~(CHAN_SRV_OUT);
           break;
        }
    }
    return status;
}

uint16_t
read_data(struct _device *dev, uint16_t *tags, uint8_t *data, int *num, int cc)
{
    uint16_t    bus_out;
    uint16_t    bus_in;
    uint16_t    status = 0;
    int         i;
    int         sel = 1;
    int         byte = 0;
    int         sta_in = 0;

    for (i = 0; i < 1000; i++) {
        step_2844((struct _2844_context *)dev->dev);
        step_2844((struct _2844_context *)dev->dev);
        step_count++;
        if (sel)
           *tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;
        dev->bus_func(dev, tags, bus_out, &bus_in);
        if ((*tags & (CHAN_STA_IN)) != 0) {
           log_trace("Status in\n");
           status = bus_in;   /* Device end and channel end */
           bus_out = 0x100;
           *tags |= CHAN_SRV_OUT;
           if (cc)
               *tags |= CHAN_SUP_OUT;
           sta_in = 1;
        }
        if ((*tags & (CHAN_STA_IN|CHAN_SRV_IN|CHAN_SRV_OUT)) == (CHAN_SRV_OUT)) {
           log_trace("Service in drop\n");
           bus_out = 0x100;
           *tags &= ~(CHAN_SRV_OUT);
           if (sta_in) {
               *tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
               sel = 0;
           }
        }
        if ((*tags & (CHAN_SRV_OUT|CHAN_SRV_IN)) == (CHAN_SRV_IN)) {
           if (byte <= *num) {
               data[byte] = bus_in;
           }
           log_trace("Service in %03x %02x\n", bus_in, byte);
           byte++;
           *tags |= (CHAN_SRV_OUT);
        }
        if ((*tags & (CHAN_OPR_IN)) == 0) {
           log_trace("Oper in drop\n");
           break;
        }
    }
    *num = byte;
    return status;
}

uint16_t
write_data(struct _device *dev, uint16_t *tags, uint8_t *data, int *num, int cc)
{
    uint16_t    bus_out;
    uint16_t    bus_in;
    uint16_t    status = 0;
    int         i;
    int         sel = 1;
    int         byte = 0;
    int         sta_in = 0;

    for (i = 0; i < 1000; i++) {
        step_2844((struct _2844_context *)dev->dev);
        step_2844((struct _2844_context *)dev->dev);
        step_count++;
        if (sel)
           *tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;
        dev->bus_func(dev, tags, bus_out, &bus_in);
        if ((*tags & (CHAN_STA_IN)) != 0) {
           status = bus_in;   /* Device end and channel end */
           log_trace("Status in %02x\n", status);
           bus_out = 0x100;
           *tags |= CHAN_SRV_OUT;
           if (cc)
               *tags |= CHAN_SUP_OUT;
           sta_in = 1;
        }
        if ((*tags & (CHAN_STA_IN|CHAN_SRV_IN|CHAN_SRV_OUT)) == (CHAN_SRV_OUT)) {
           log_trace("Service in drop\n");
           bus_out = 0x100;
           *tags &= ~(CHAN_SRV_OUT);
           if (sta_in) {
               *tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
               sel = 0;
           }
        }
        if ((*tags & (CHAN_SRV_OUT|CHAN_SRV_IN)) == (CHAN_SRV_IN)) {
           if (byte <= *num) {
               bus_out = data[byte];
               bus_out |= odd_parity[bus_out];
           }
           log_trace("Service in %03x %02x\n", bus_in, byte);
           byte++;
           *tags |= (CHAN_SRV_OUT);
        }
        if ((*tags & (CHAN_OPR_IN)) == 0) {
           log_trace("Oper in drop\n");
           break;
        }
    }
    *num = byte;
    return status;
}

uint16_t
wait_dev(struct _device *dev, uint16_t *tags, int cc)
{
    uint16_t    bus_out;
    uint16_t    bus_in;
    uint16_t    status = 0;
    int         i;
    int         sel = 1;

    *tags &= ~(CHAN_SRV_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT);
    for (i = 0; i < 300; i++) {
        step_2844((struct _2844_context *)dev->dev);
        step_2844((struct _2844_context *)dev->dev);
        step_count++;
        advance();
        dev->bus_func(dev, tags, bus_out, &bus_in);
        if (sel)
           *tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;
        if ((*tags & (CHAN_REQ_IN)) != 0) {
           sel = 1;
        }
        if ((*tags & (CHAN_ADR_IN)) != 0 && bus_in == 0x190) {
           *tags |= CHAN_CMD_OUT;
        }
        if ((*tags & (CHAN_ADR_IN)) == 0) {
           *tags &= ~CHAN_CMD_OUT;
        }
        if ((*tags & (CHAN_STA_IN)) != 0) {
           if (cc)
               *tags |= CHAN_SUP_OUT;
           *tags |= CHAN_SRV_OUT;
           status = bus_in;
        }
        if ((*tags & (CHAN_SRV_IN|CHAN_STA_IN|CHAN_SRV_OUT)) == (CHAN_SRV_OUT)) {
           *tags &= ~CHAN_SRV_OUT;
           sel = 0;
           i = 250;
        }
    }
    return status;
}

CTEST_DATA(disk_test) {
    struct _device *dev;
};

CTEST_SETUP(disk_test) {
    struct _2844_context *ctx;
    data->dev = model2844_init(NULL, 0x90);
    ctx = (struct _2844_context *)(data->dev->dev);
    ctx->disk[0] = (struct _dasd_t *)calloc(1, sizeof(struct _dasd_t));
    ctx->disk[7] = NULL;
    dasd_attach(ctx->disk[0], "test.ckd", 3, 1);
}

CTEST_TEARDOWN(disk_test) {
    struct _2844_context *ctx;
    int        i;
    if (data->dev->dev) {
       ctx = (struct _2844_context *)(data->dev->dev);
       for (i = 0; i < 8; i++) {
           if (ctx->disk[i] != NULL) {
               dasd_detach(ctx->disk[i]);
               free(ctx->disk[i]);
           }
       }
       free(data->dev->dev);
    }
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
CTEST2(disk_test, test_io) {
    uint16_t    tags;
    uint16_t    bus_out;
    uint16_t    bus_in;
    int         i;
    int         sel = 0;

    ((struct _2844_context *)(data->dev->dev))->WX = 0;
    tags = CHAN_OPR_OUT;
    for (i = 0; i < 200; i++) {
        step_2844((struct _2844_context *)data->dev->dev);
        step_2844((struct _2844_context *)data->dev->dev);
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
    uint16_t    tags = 0;
    uint16_t    status;

    ((struct _2844_context *)(data->dev->dev))->WX = 0;
    status = initial_select(data->dev, &tags, 0x3);
    ASSERT_EQUAL_X(0x10c, status);
}

/* Try to send Set file mask to controller */
CTEST2(disk_test, sense) {
    uint16_t    tags = 0;
    uint16_t    bus_out;
    uint16_t    bus_in;
    uint16_t    status;
    uint8_t     sense[6];
    int         num;

    ((struct _2844_context *)(data->dev->dev))->WX = 0;
    status = initial_select(data->dev, &tags, 0x4);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = read_data(data->dev, &tags, &sense[0], &num, 0);
    printf("Sense %02x -> %02x %02x %02x %02x\n", status, sense[0], sense[1],
            sense[2], sense[3]);
    ASSERT_EQUAL_X(0x0, sense[0]);
    ASSERT_EQUAL_X(0x0, sense[1]);
    ASSERT_EQUAL_X(0x0, sense[2]);
    ASSERT_EQUAL_X(0x40, sense[3]);
    ASSERT_EQUAL_X(0x0, sense[4]);
    ASSERT_EQUAL_X(0x0, sense[5]);
    ASSERT_EQUAL_X(0x10c, status);
}

/* Try to send Seek to controller */
CTEST2(disk_test, seek) {
    struct _2844_context *ctx = (struct _2844_context *)(data->dev->dev);
    static uint8_t  cmd[] = { 0, 0, 0, 0x10, 0, 5 };
    uint8_t         sense[6];
    uint16_t    tags = 0;
    uint16_t    bus_out;
    uint16_t    bus_in;
    uint16_t    status;
    int         i;
    int         sel = 0;
    int         byte = 0;
    int         sta_in = 0;

    ((struct _2844_context *)(data->dev->dev))->WX = 0;
    status = initial_select(data->dev, &tags, 0x7);
    ASSERT_EQUAL_X(0x100, status);
    i = 6;
    status = write_data(data->dev, &tags, &cmd[0], &i, 0);
    ASSERT_EQUAL_X(0x8, status);
    printf("Seek %02x %d\n", status, i);
    status = wait_dev(data->dev, &tags, 0);
    ASSERT_EQUAL_X(0x4, status);
    status = initial_select(data->dev, &tags, 0x4);
    ASSERT_EQUAL_X(0x100, status);
    i = 6;
    status = read_data(data->dev, &tags, &sense[0], &i, 0);
    ASSERT_EQUAL(5, ctx->disk[0]->head);
    ASSERT_EQUAL(0x10, ctx->disk[0]->cyl);
}

/* Try to send Set file mask to controller */
CTEST2(disk_test, read_ipl) {
    struct _2844_context *ctx = (struct _2844_context *)(data->dev->dev);
    uint16_t    tags;
    uint16_t    bus_out;
    uint16_t    bus_in;
    uint16_t    status;
    int         i;
    int         sel = 0;
    int         byte = 0;
    int         sta_in = 0;
    uint8_t     res[256];

    log_trace("Read IPL\n");
    bus_out = 0x100;
    tags = 0;
    ctx->WX = 0;
    ctx->disk[0]->cpos = 7000;
    status = initial_select(data->dev, &tags, 0x02);
    ASSERT_EQUAL_X(0x100, status);
    sel = 1;
    for (i = 0; i < 50000; i++) {
        step_2844((struct _2844_context *)data->dev->dev);
        step_2844((struct _2844_context *)data->dev->dev);
        step_count++;
        advance();
        if (sel)
           tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;
        data->dev->bus_func(data->dev, &tags, bus_out, &bus_in);
        if (sta_in == 0 && (tags & (CHAN_STA_IN)) != 0) {
           log_trace("Status in\n");
//           ASSERT_EQUAL_X(0x10c, bus_in);   /* Device end and channel end */
           bus_out = 0x100;
           tags |= CHAN_SRV_OUT;
           sta_in = 1;
        }
        if (sta_in == 1 && (tags & (CHAN_STA_IN)) != 0) {
           log_trace("Status final\n");
//           ASSERT_EQUAL_X(0x10c, bus_in);   /* Device end and channel end */
           tags |= CHAN_SRV_OUT;
           sta_in = 2;
        }
        if ((tags & (CHAN_STA_IN|CHAN_SRV_IN|CHAN_SRV_OUT)) == (CHAN_SRV_OUT)) {
           log_trace("Service in drop\n");
           bus_out = 0x100;
           tags &= ~(CHAN_SRV_OUT);
           if (sta_in) {
               tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
               sel = 0;
           }
           if (sta_in == 2)
               break;
        }
        if ((tags & (CHAN_SRV_OUT|CHAN_SRV_IN)) == (CHAN_SRV_IN)) {
           log_trace("Service in %03x %02x\n", bus_in, byte);
           bus_out = byte | odd_parity[byte];
           res[byte] = bus_in & 0xff;
           byte++;
           tags |= (CHAN_SRV_OUT);
        }
        if ((tags & (CHAN_OPR_IN)) == 0) {
           log_trace("Oper in drop\n");
           break;
        }
    }

    for (i = 0; i < byte; i++) {
        log_trace("Read %d: %02x\n", i, res[i]);
    }

    status = initial_select(data->dev, &tags, 0x4);
    ASSERT_EQUAL_X(0x100, status);
    sel = 1;
    sta_in = 0;
    byte = 0;
    for (i = 0; i < 500; i++) {
        step_2844((struct _2844_context *)data->dev->dev);
        step_2844((struct _2844_context *)data->dev->dev);
        step_count++;
        advance();
        if (sel)
           tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;
        data->dev->bus_func(data->dev, &tags, bus_out, &bus_in);
        if ((tags & (CHAN_STA_IN)) != 0) {
           log_trace("Status in\n");
           ASSERT_EQUAL_X(0x10c, bus_in);   /* Device end and channel end */
           bus_out = 0x100;
           tags |= CHAN_SRV_OUT;
           sta_in = 1;
        }
        if ((tags & (CHAN_STA_IN|CHAN_SRV_IN|CHAN_SRV_OUT)) == (CHAN_SRV_OUT)) {
           log_trace("Service in drop\n");
           bus_out = 0x100;
           tags &= ~(CHAN_SRV_OUT);
           if (sta_in) {
               tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
               sel = 0;
           }
        }
        if ((tags & (CHAN_SRV_OUT|CHAN_SRV_IN)) == (CHAN_SRV_IN)) {
           log_trace("Service in %03x %02x\n", bus_in, byte);
           bus_out = byte | odd_parity[byte];
           byte++;
           tags |= (CHAN_SRV_OUT);
        }
        if ((tags & (CHAN_OPR_IN)) == 0) {
           log_trace("Oper in drop\n");
           break;
        }
    }
}

/* Try to write to controller */
CTEST2(disk_test, write) {
    static uint8_t  cmd[] = { 0, 0, 0, 10, 0, 4 };
    static uint8_t  wha[] = { 0, 0, 1, 0, 4};
    static uint8_t  wr0[] = { 0, 1, 0, 4, 0, 0, 0, 8, 1, 2, 3, 4, 5, 6, 7, 8};
    uint8_t         wrk[512];
    uint16_t        tags;
    uint16_t        bus_out;
    uint16_t        bus_in;
    uint16_t        status;
    int             i, j;
    int             sel = 0;
    int             byte = 0;
    int             sta_in = 0;

    tags = 0;
    ((struct _2844_context *)(data->dev->dev))->WX = 0;
    status = initial_select(data->dev, &tags, 0x7);
    ASSERT_EQUAL_X(0x100, status);
    sel = 1;
    sta_in = 0;
    byte = 0;
    for (i = 0; i < 500; i++) {
        step_2844((struct _2844_context *)data->dev->dev);
        step_2844((struct _2844_context *)data->dev->dev);
        step_count++;
        advance();
        if (sel)
           tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;
        data->dev->bus_func(data->dev, &tags, bus_out, &bus_in);
        if ((tags & (CHAN_STA_IN)) != 0) {
           log_trace("Status in\n");
//           ASSERT_EQUAL_X(0x10c, bus_in);   /* Device end and channel end */
           bus_out = 0x100;
           tags |= CHAN_SRV_OUT|CHAN_SUP_OUT;
           sta_in = 1;
        }
        if ((tags & (CHAN_STA_IN|CHAN_SRV_IN|CHAN_SRV_OUT)) == (CHAN_SRV_OUT)) {
           log_trace("Service in drop\n");
           bus_out = 0x100;
           tags &= ~(CHAN_SRV_OUT);
           if (sta_in) {
               tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
               sel = 0;
           }
        }
        if ((tags & (CHAN_SRV_OUT|CHAN_SRV_IN)) == (CHAN_SRV_IN)) {
           log_trace("Service in %03x %02x\n", bus_in, byte);
           bus_out = cmd[byte];
           bus_out = bus_out | odd_parity[bus_out];
           byte++;
           tags |= (CHAN_SRV_OUT);
        }
        if ((tags & (CHAN_OPR_IN)) == 0) {
           log_trace("Oper in drop\n");
           break;
        }
    }
    tags &= ~(CHAN_SRV_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT);
    for (i = 0; i < 300; i++) {
        step_2844((struct _2844_context *)data->dev->dev);
        step_2844((struct _2844_context *)data->dev->dev);
        step_count++;
        advance();
        data->dev->bus_func(data->dev, &tags, bus_out, &bus_in);
        if (sel)
           tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;
        if ((tags & (CHAN_REQ_IN)) != 0) {
           sel = 1;
        }
        if ((tags & (CHAN_ADR_IN)) != 0 && bus_in == 0x190) {
           tags |= CHAN_CMD_OUT;
        }
        if ((tags & (CHAN_ADR_IN)) == 0) {
           tags &= ~CHAN_CMD_OUT;
        }
        if ((tags & (CHAN_STA_IN)) != 0) {
           tags |= CHAN_SRV_OUT|CHAN_SUP_OUT;
         ASSERT_EQUAL_X(0x4, bus_in);
        }
        if ((tags & (CHAN_SRV_IN|CHAN_STA_IN|CHAN_SRV_OUT)) == (CHAN_SRV_OUT)) {
           tags &= ~CHAN_SRV_OUT;
           sel = 0;
           i = 250;
        }
    }
    sta_in = 0;
    byte = 0;
    status = initial_select(data->dev, &tags, 0x1f);
    ASSERT_EQUAL_X(0x100, status);
    sel = 1;
    for (i = 0; i < 500; i++) {
        step_2844((struct _2844_context *)data->dev->dev);
        step_2844((struct _2844_context *)data->dev->dev);
        step_count++;
        advance();
        if (sel)
           tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;
        data->dev->bus_func(data->dev, &tags, bus_out, &bus_in);
        if ((tags & (CHAN_STA_IN)) != 0) {
           log_trace("Status in\n");
//           ASSERT_EQUAL_X(0x10c, bus_in);   /* Device end and channel end */
           bus_out = 0x100;
           tags |= CHAN_SRV_OUT|CHAN_SUP_OUT;
           sta_in = 1;
        }
        if ((tags & (CHAN_STA_IN|CHAN_SRV_IN|CHAN_SRV_OUT)) == (CHAN_SRV_OUT)) {
           log_trace("Service in drop\n");
           bus_out = 0x100;
           tags &= ~(CHAN_SRV_OUT);
           if (sta_in) {
               tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
               sel = 0;
           }
        }
        if ((tags & (CHAN_SRV_OUT|CHAN_SRV_IN)) == (CHAN_SRV_IN)) {
           log_trace("Service in %03x %02x\n", bus_in, byte);
           bus_out = 0xc0;
           bus_out = bus_out | odd_parity[bus_out];
           tags |= (CHAN_SRV_OUT);
        }
        if ((tags & (CHAN_OPR_IN)) == 0) {
           log_trace("Oper in drop\n");
           break;
        }
    }
    sta_in = 0;
    byte = 0;
    status = initial_select(data->dev, &tags, 0x19);
    if (status != 0x0) goto sense;
    ASSERT_EQUAL_X(0x100, status);
    sel = 1;
    for (i = 0; i < 50000; i++) {
        step_2844((struct _2844_context *)data->dev->dev);
        step_2844((struct _2844_context *)data->dev->dev);
        step_count++;
        advance();
        if (sel)
           tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;
        data->dev->bus_func(data->dev, &tags, bus_out, &bus_in);
        if ((tags & (CHAN_STA_IN)) != 0) {
           log_trace("Status in\n");
           ASSERT_EQUAL_X(0x10c, bus_in);   /* Device end and channel end */
           bus_out = 0x100;
           tags |= CHAN_SRV_OUT|CHAN_SUP_OUT;
           sta_in = 1;
        }
        if ((tags & (CHAN_STA_IN|CHAN_SRV_IN|CHAN_SRV_OUT)) == (CHAN_SRV_OUT)) {
           log_trace("Service in drop\n");
           bus_out = 0x100;
           tags &= ~(CHAN_SRV_OUT);
           if (sta_in) {
               tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
               sel = 0;
           }
        }
        if ((tags & (CHAN_SRV_OUT|CHAN_SRV_IN)) == (CHAN_SRV_IN)) {
           log_trace("Service in %03x %02x\n", bus_in, byte);
           bus_out = wha[byte] | odd_parity[wha[byte]];
           byte++;
           tags |= (CHAN_SRV_OUT);
        }
        if ((tags & (CHAN_OPR_IN)) == 0) {
           log_trace("Oper in drop\n");
           break;
        }
    }
    sta_in = 0;
    byte = 0;
    status = initial_select(data->dev, &tags, 0x15);
    if (status != 0x0) goto sense;
    ASSERT_EQUAL_X(0x100, status);
    sel = 1;
    for (i = 0; i < 30000; i++) {
        step_2844((struct _2844_context *)data->dev->dev);
        step_2844((struct _2844_context *)data->dev->dev);
        step_count++;
        advance();
        if (sel)
           tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;
        data->dev->bus_func(data->dev, &tags, bus_out, &bus_in);
        if ((tags & (CHAN_STA_IN)) != 0) {
           log_trace("Status in\n");
           ASSERT_EQUAL_X(0x10c, bus_in);   /* Device end and channel end */
           bus_out = 0x100;
           tags |= CHAN_SRV_OUT|CHAN_SUP_OUT;
           sta_in = 1;
        }
        if ((tags & (CHAN_STA_IN|CHAN_SRV_IN|CHAN_SRV_OUT)) == (CHAN_SRV_OUT)) {
           log_trace("Service in drop\n");
           bus_out = 0x100;
           tags &= ~(CHAN_SRV_OUT);
           if (sta_in) {
               tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
               sel = 0;
           }
        }
        if ((tags & (CHAN_SRV_OUT|CHAN_SRV_IN)) == (CHAN_SRV_IN)) {
           log_trace("Service in %03x %02x %d\n", bus_in, wr0[byte],  byte);
           bus_out = wr0[byte] | odd_parity[wr0[byte]];
           byte++;
           tags |= (CHAN_SRV_OUT);
        }
        if ((tags & (CHAN_OPR_IN)) == 0) {
           log_trace("Oper in drop\n");
           break;
        }
    }
    for (j = 1; j < 5; j++) {
        sta_in = 0;
        byte = 0;
        for (i = 0; i < 512; i++)
            wrk[i] = 0;
        wrk[1] = 1;
        wrk[3] = 4;
        wrk[4] = j;
        wrk[7] = 128;
        for (i = 0; i < 128; i++)
            wrk[8+i] = i;
        status = initial_select(data->dev, &tags, 0x29);
        if (status != 0x0) goto sense;
        ASSERT_EQUAL_X(0x100, status);
        sel = 1;
        for (i = 0; i < 30000; i++) {
            step_2844((struct _2844_context *)data->dev->dev);
            step_2844((struct _2844_context *)data->dev->dev);
            step_count++;
            advance();
            if (sel)
               tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;
            data->dev->bus_func(data->dev, &tags, bus_out, &bus_in);
            if ((tags & (CHAN_STA_IN)) != 0) {
               log_trace("Status in\n");
               ASSERT_EQUAL_X(0x10c, bus_in);   /* Device end and channel end */
               bus_out = 0x100;
               tags |= CHAN_SRV_OUT|CHAN_SUP_OUT;
               sta_in = 1;
            }
            if ((tags & (CHAN_STA_IN|CHAN_SRV_IN|CHAN_SRV_OUT)) == (CHAN_SRV_OUT)) {
               log_trace("Service in drop\n");
               bus_out = 0x100;
               tags &= ~(CHAN_SRV_OUT);
               if (sta_in) {
                   tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
                   sel = 0;
               }
            }
            if ((tags & (CHAN_SRV_OUT|CHAN_SRV_IN)) == (CHAN_SRV_IN)) {
               log_trace("Service in %03x %02x\n", bus_in, byte);
               bus_out = wrk[byte] | odd_parity[wrk[byte]];
               byte++;
               tags |= (CHAN_SRV_OUT);
            }
            if ((tags & (CHAN_OPR_IN)) == 0) {
               log_trace("Oper in drop\n");
               break;
            }
        }
    }
sense:
    status = initial_select(data->dev, &tags, 0x4);
    ASSERT_EQUAL_X(0x100, status);
    sel = 1;
    sta_in = 0;
    byte = 0;
    for (i = 0; i < 500; i++) {
        step_2844((struct _2844_context *)data->dev->dev);
        step_2844((struct _2844_context *)data->dev->dev);
        step_count++;
        advance();
        if (sel)
           tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;
        data->dev->bus_func(data->dev, &tags, bus_out, &bus_in);
        if ((tags & (CHAN_STA_IN)) != 0) {
           log_trace("Status in\n");
           ASSERT_EQUAL_X(0x10c, bus_in);   /* Device end and channel end */
           bus_out = 0x100;
           tags |= CHAN_SRV_OUT;
           sta_in = 1;
        }
        if ((tags & (CHAN_STA_IN|CHAN_SRV_IN|CHAN_SRV_OUT)) == (CHAN_SRV_OUT)) {
           log_trace("Service in drop\n");
           bus_out = 0x100;
           tags &= ~(CHAN_SRV_OUT);
           if (sta_in) {
               tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
               sel = 0;
           }
        }
        if ((tags & (CHAN_SRV_OUT|CHAN_SRV_IN)) == (CHAN_SRV_IN)) {
           log_trace("Service in %03x %02x\n", bus_in, byte);
           bus_out = byte | odd_parity[byte];
           byte++;
           tags |= (CHAN_SRV_OUT);
        }
        if ((tags & (CHAN_OPR_IN)) == 0) {
           log_trace("Oper in drop\n");
           break;
        }
    }
}

/* Try to send Set file mask to controller */
CTEST2_SKIP(disk_test, setmask) {
    uint16_t    tags;
    uint16_t    bus_out;
    uint16_t    bus_in;
    int i;
    int         sel = 0;

    ((struct _2844_context *)(data->dev->dev))->WX = 0;
    tags = CHAN_OPR_OUT;
    for (i = 0; i < 10000; i++) {
        step_2844((struct _2844_context *)data->dev->dev);
        step_2844((struct _2844_context *)data->dev->dev);
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
//           ASSERT_EQUAL_X(0x100, bus_in);   /* Device end and channel end */
           bus_out = 0x100;
           tags |= CHAN_SRV_OUT;
        }
        if ((tags & (CHAN_STA_IN|CHAN_SRV_OUT)) == (CHAN_SRV_OUT)) {
           log_trace("Status in drop\n");
           bus_out = 0x100;
           tags &= ~(CHAN_SRV_OUT|CHAN_HLD_OUT);
        }
        if ((tags & (CHAN_SRV_IN)) != 0) {
           log_trace("Service in\n");
           bus_out = 0x1c0;
           tags |= (CHAN_SRV_OUT);
        }
    }
}

