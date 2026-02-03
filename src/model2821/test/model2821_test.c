/*
 * microsim360 - Model 2821 Test for 2821 controller.
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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <sys/types.h>
#include <unistd.h>
#endif
#include "logger.h"
#include "device.h"
#include "test_chan.h"
#include "event.h"
#include "ctest.h"
#include "xlat.h"
#include "model2821.h"

uint64_t   step_count = 0;
int        verbose = 0;
char       *test_log_file = "model2821_debug.log";
char       *test_log_level = "info warn error trace device";


void
test_unit_xfer_done(struct _2821_dev_context *unit)
{
     unit->cmd = 0;
     unit->busy = 0;
     unit->cmd_done = 1;
     unit->status |= SNS_DEVEND;
     return;
}

/* Decode command to device */
static void
test_unit_device_cmd(struct _2821_dev_context *ctx, uint8_t bus_out)
{
    uint8_t cmd = bus_out & 0xff;

    log_device("2821: command %02x\n", bus_out);
    ctx->cmd = 0;
    ctx->status = 0;
    ctx->bptr = 0;
    switch (cmd & 07) {
    case 0: /* Test I/O */
           if (ctx->sense != 0) {
              ctx->status |= 0x02;
           }
           return;

    case 1: /* Write */
    case 2: /* Read */
           ctx->sense = 0;
           if ((cmd & 0xfc) != 0) {
               ctx->sense |= 0x80;
               break;
           }
           ctx->cmd = cmd;
           ctx->cmd_done = 0;
           ctx->busy = 1;
           break;

    case 3: /* Control */
           ctx->sense = 0;
           if ((cmd & 0xfc) != 0) {
               ctx->sense |= 0x80;
               break;
           }
           ctx->cmd = cmd;
           ctx->cmd_done = 1;
           ctx->busy = 0;
           ctx->bptr = ctx->blen;
           break;

    case 4:  /* Sense */
           if (cmd != 0x04) {
               ctx->sense |= 0x80;
               break;
           }
           log_device("2821: Sense %02x\n", ctx->sense);
           ctx->cmd = cmd;
           ctx->cmd_done = 0;
           ctx->busy = 1;
           return;

    default:
           ctx->sense |= 0x80;     /* Invalid command */
           break;
    }
    if (ctx->bptr == ctx->blen) {
        ctx->status |= SNS_CHNEND;
    }
    if (ctx->cmd_done) {
        ctx->status |= SNS_DEVEND;
        if (ctx->sense != 0) {
            ctx->status |= 0x02;
        }
    }
}

void
test_unit_init()
{
    struct _2821_dev_context *ctx;
    struct _device *unit;
    int            r;

    /* Allocate device structure */
    if ((unit = (struct _device *)calloc(1, sizeof(struct _device))) == NULL)
        return;
    /* Allocate structures to hold device information */
    if ((ctx = (struct _2821_dev_context *)calloc(1, sizeof(struct _2821_dev_context))) == NULL) {
        free(unit);
        return;
    }
    unit->dev = (void *)ctx;
    unit->type_name = "test";
    unit->n_units = 1;
    unit->addr = 0xc;
    ctx->device = unit;
    ctx->blen = 80;
    ctx->addr = 0xc;
    ctx->sense = 0;
    ctx->mpx_count = 2;
    ctx->device_cmd = &test_unit_device_cmd;
    ctx->device_xfer_done = &test_unit_xfer_done;
    add_chan(unit, unit->addr);
    if ((unit = find_chan_name("2821", 0)) == NULL) {
        free(ctx);
        printf("Unable to find a 2821 device\n");
        return;
    }
    printf("Type %s\n", unit->type_name);
    if ((r = model2821_register(unit, ctx, DEVICE_2821_READER)) != DEVICE_2821_OK) {
        printf("Unable to register to 2821 device\n");
    }
    printf("registered to 2821 device\n");
}


void
init_tests()
{
    init_event();
    (void)model2821_init(0);
    test_unit_init();
}

void
test_advance()
{
    step_count++;
    advance();
}

CTEST_DATA(model2821_test) {
    uint16_t                  addr;
    struct _device            *dev;
};

CTEST_SETUP(model2821_test) {
     log_trace("Init test\n");
     data->dev = find_chan_dev(0xc, 0xff);
     ASSERT_NOT_NULL(data->dev);
     data->addr = data->dev->addr;
}

CTEST_TEARDOWN(model2821_test) {
     log_trace("teardown test\n");
}

/* Try to send Test I/O to controller */
CTEST2(model2821_test, test_io) {
    log_trace("TIO\n");
    ASSERT_EQUAL_X(0, test_io(data->addr));
}

/* Try to send Nop to controller */
CTEST2(model2821_test, nop) {
     uint16_t status;

     log_trace("Nop\n");
     set_mem(0x40,  0xffffffff);         /* Set CSW to zero */
     set_mem(0x44,  0xffffffff);
     set_mem(0x48,  0x500);   /* Set CAW */
     set_mem(0x500, 0x03000600); /* Set channel words */
     set_mem(0x504, 0x00000001);
     set_mem(0x600, 0xffffffff);
     status = start_io(data->addr, 0x500, 0, 0);
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000001, get_mem(0x44));
     ASSERT_EQUAL_X(0xffFFFFFF, get_mem(0x600));
}

/* Try to issue sense command */
CTEST2(model2821_test, sense) {
     uint16_t status;

     log_trace("Sense\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x04000700); /* Set channel words */
     set_mem(0x504, 0x00000001);
     set_mem(0x700, 0xffffffff);
     status = start_io(data->addr, 0x500, 0, 0);
     if (verbose) {
        printf("700=%08x 0x40=%08x %08x\n", get_mem(0x700),
                get_mem(0x40), get_mem(0x44));
     }

     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00ffffff, get_mem(0x700));
}

/* Try to read a card */
CTEST2(model2821_test, read) {
     struct _2821_dev_context *ctx = (struct _2821_dev_context *)(data->dev->dev);
     uint16_t status1 = 0;
     int      i;

     for (i = 0; i < 120; i++) {
         ctx->buffer[i] = i+1;
     }

     log_trace("Read\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0x00000050);
     set_mem(0x510, 0x04000700); /* Set channel words */
     set_mem(0x514, 0x00000001);
     for (i = 0; i < 0x60; i += 4) {
         set_mem(0x600 + i, 0xffffffff);
         set_mem(0x700 + i, 0xffffffff);
     }
     status1 = start_io(data->addr, 0x500, 0, 0);
     if (verbose) {
        printf("0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
        for(i = 0x600; i < 0x660; i+= 4) {
           printf("0x%03x=%08x ", i, get_mem(i));
        }
        printf("\n");

     }
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status1);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     for (i = 0; i < 80; i++) {
         ASSERT_EQUAL_X(i+1, get_mem_b(0x600+i));
     }

     /* Make sure sense is zero */
     status1 = start_io(data->addr, 0x510, 0, 0);
     if (verbose) {
        printf("700=%08x 0x40=%08x %08x\n", get_mem(0x700),
                get_mem(0x40), get_mem(0x44));
     }

     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status1);
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00ffffff, get_mem(0x700));
}

/* Try to read short */
CTEST2(model2821_test, read_short) {
     struct _2821_dev_context *ctx = (struct _2821_dev_context *)(data->dev->dev);
     uint16_t status1 = 0;
     int      i;

     for (i = 0; i < 120; i++) {
         ctx->buffer[i] = i+1;
     }

     log_trace("Read\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0x00000020);
     set_mem(0x510, 0x04000700); /* Set channel words */
     set_mem(0x514, 0x00000001);
     for (i = 0; i < 0x60; i += 4) {
         set_mem(0x600 + i, 0xffffffff);
         set_mem(0x700 + i, 0xffffffff);
     }
     status1 = start_io(data->addr, 0x500, 0, 0);
     if (verbose) {
        printf("0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
        for(i = 0x600; i < 0x660; i+= 4) {
           printf("0x%03x=%08x ", i, get_mem(i));
        }
        printf("\n");

     }
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status1);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c400000, get_mem(0x44));
     for (i = 0; i < 0x20; i++) {
         ASSERT_EQUAL_X(i+1, get_mem_b(0x600+i));
     }
     for (; i < 80; i++) {
         ASSERT_EQUAL_X(0xff, get_mem_b(0x600+i));
     }

     /* Make sure sense is zero */
     status1 = start_io(data->addr, 0x510, 0, 0);
     if (verbose) {
        printf("700=%08x 0x40=%08x %08x\n", get_mem(0x700),
                get_mem(0x40), get_mem(0x44));
     }

     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status1);
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00ffffff, get_mem(0x700));
}

/* Try to read short */
CTEST2(model2821_test, read_long) {
     struct _2821_dev_context *ctx = (struct _2821_dev_context *)(data->dev->dev);
     uint16_t status1 = 0;
     int      i;

     for (i = 0; i < 120; i++) {
         ctx->buffer[i] = i+1;
     }

     log_trace("Read\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0x00000080);
     set_mem(0x510, 0x04000700); /* Set channel words */
     set_mem(0x514, 0x00000001);
     for (i = 0; i < 0x90; i += 4) {
         set_mem(0x600 + i, 0xffffffff);
         set_mem(0x700 + i, 0xffffffff);
     }
     status1 = start_io(data->addr, 0x500, 0, 0);
     if (verbose) {
        printf("0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
        for(i = 0x600; i < 0x660; i+= 4) {
           printf("0x%03x=%08x ", i, get_mem(i));
        }
        printf("\n");

     }
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status1);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c400030, get_mem(0x44));
     for (i = 0; i < 80; i++) {
         ASSERT_EQUAL_X(i+1, get_mem_b(0x600+i));
     }

     for (; i < 0x80; i++) {
         ASSERT_EQUAL_X(0xff, get_mem_b(0x600+i));
     }


     /* Make sure sense is zero */
     status1 = start_io(data->addr, 0x510, 0, 0);
     if (verbose) {
        printf("700=%08x 0x40=%08x %08x\n", get_mem(0x700),
                get_mem(0x40), get_mem(0x44));
     }

     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status1);
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00ffffff, get_mem(0x700));
}

/* Try to write command */
CTEST2(model2821_test, write) {
     struct _2821_dev_context *ctx = (struct _2821_dev_context *)(data->dev->dev);
     int     i;
     uint16_t status1 = 0;

     log_trace("Write\n");
     for (i = 0; i < 120; i++) {
         ctx->buffer[i] = 0;
     }

     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x01000600); /* Set channel words */
     set_mem(0x504, 0x00000050);
     set_mem(0x510, 0x04000700); /* Set channel words */
     set_mem(0x514, 0x00000001);
     set_mem(0x600, 0xbbbcbdbe);
     set_mem(0x604, 0xbfc0c1c2);
     set_mem(0x608, 0xc3c4c5c6);
     set_mem(0x60c, 0xc7c8c9d1);
     set_mem(0x610, 0xd2d3d4d5);
     set_mem(0x614, 0xd6d7d8d9);
     set_mem(0x618, 0xe2e3e4e5);
     set_mem(0x61c, 0xe6e7e8e9);
     set_mem(0x620, 0xf0f1f2f3);
     set_mem(0x624, 0xf4f5f6f7);
     set_mem(0x628, 0xf8f9c1c2);
     set_mem(0x62c, 0xc3c4c5c6);
     set_mem(0x630, 0xc7c8c9d1);
     set_mem(0x634, 0xd2d3d4d5);
     set_mem(0x638, 0xd6d7d8d9);
     set_mem(0x63c, 0xe2e3e4e5);
     set_mem(0x640, 0xe6e7e8e9);
     set_mem(0x644, 0xf0f1f2f3);
     set_mem(0x648, 0xf4f5f6f7);
     set_mem(0x64c, 0xf8f9fafb);
     status1 = start_io(data->addr, 0x500, 0, 0);

     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status1);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));

     /* Make sure sense is zero */
     set_mem(0x700, 0xffffffff);
     status1 = start_io(data->addr, 0x510, 0, 0);
     if (verbose) {
         for (i = 0; i < 120; i++) {
             printf("%02x, ", ctx->buffer[i]);
         }
         printf("\n");
     }
     for (i = 0; i < 80; i++) {
         ASSERT_EQUAL_X(get_mem_b(0x600+i), ctx->buffer[i]);
     }


     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status1);
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00ffffff, get_mem(0x700));
}

/* Try to write short command */
CTEST2(model2821_test, write_short) {
     struct _2821_dev_context *ctx = (struct _2821_dev_context *)(data->dev->dev);
     int     i;
     uint16_t status1 = 0;

     log_trace("Write\n");
     for (i = 0; i < 120; i++) {
         ctx->buffer[i] = 0;
     }

     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x01000600); /* Set channel words */
     set_mem(0x504, 0x00000020);
     set_mem(0x510, 0x04000700); /* Set channel words */
     set_mem(0x514, 0x00000001);
     set_mem(0x600, 0xbbbcbdbe);
     set_mem(0x604, 0xbfc0c1c2);
     set_mem(0x608, 0xc3c4c5c6);
     set_mem(0x60c, 0xc7c8c9d1);
     set_mem(0x610, 0xd2d3d4d5);
     set_mem(0x614, 0xd6d7d8d9);
     set_mem(0x618, 0xe2e3e4e5);
     set_mem(0x61c, 0xe6e7e8e9);
     set_mem(0x620, 0xf0f1f2f3);
     set_mem(0x624, 0xf4f5f6f7);
     set_mem(0x628, 0xf8f9c1c2);
     set_mem(0x62c, 0xc3c4c5c6);
     set_mem(0x630, 0xc7c8c9d1);
     set_mem(0x634, 0xd2d3d4d5);
     set_mem(0x638, 0xd6d7d8d9);
     set_mem(0x63c, 0xe2e3e4e5);
     set_mem(0x640, 0xe6e7e8e9);
     set_mem(0x644, 0xf0f1f2f3);
     set_mem(0x648, 0xf4f5f6f7);
     set_mem(0x64c, 0xf8f9fafb);
     status1 = start_io(data->addr, 0x500, 0, 0);

     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status1);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c400000, get_mem(0x44));

     /* Make sure sense is zero */
     set_mem(0x700, 0xffffffff);
     status1 = start_io(data->addr, 0x510, 0, 0);
     if (verbose) {
         for (i = 0; i < 120; i++) {
             printf("%02x, ", ctx->buffer[i]);
         }
         printf("\n");
     }
     for (i = 0; i < 0x20; i++) {
         ASSERT_EQUAL_X(get_mem_b(0x600+i), ctx->buffer[i]);
     }
     for (; i < 120; i++) {
         ASSERT_EQUAL_X(0, ctx->buffer[i]);
     }


     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status1);
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00ffffff, get_mem(0x700));
}

/* Try to write short command */
CTEST2(model2821_test, write_long) {
     struct _2821_dev_context *ctx = (struct _2821_dev_context *)(data->dev->dev);
     int     i;
     uint16_t status1 = 0;

     log_trace("Write\n");
     for (i = 0; i < 120; i++) {
         ctx->buffer[i] = 0;
     }

     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x01000600); /* Set channel words */
     set_mem(0x504, 0x00000080);
     set_mem(0x510, 0x04000700); /* Set channel words */
     set_mem(0x514, 0x00000001);
     set_mem(0x600, 0xbbbcbdbe);
     set_mem(0x604, 0xbfc0c1c2);
     set_mem(0x608, 0xc3c4c5c6);
     set_mem(0x60c, 0xc7c8c9d1);
     set_mem(0x610, 0xd2d3d4d5);
     set_mem(0x614, 0xd6d7d8d9);
     set_mem(0x618, 0xe2e3e4e5);
     set_mem(0x61c, 0xe6e7e8e9);
     set_mem(0x620, 0xf0f1f2f3);
     set_mem(0x624, 0xf4f5f6f7);
     set_mem(0x628, 0xf8f9c1c2);
     set_mem(0x62c, 0xc3c4c5c6);
     set_mem(0x630, 0xc7c8c9d1);
     set_mem(0x634, 0xd2d3d4d5);
     set_mem(0x638, 0xd6d7d8d9);
     set_mem(0x63c, 0xe2e3e4e5);
     set_mem(0x640, 0xe6e7e8e9);
     set_mem(0x644, 0xf0f1f2f3);
     set_mem(0x648, 0xf4f5f6f7);
     set_mem(0x64c, 0xf8f9fafb);
     status1 = start_io(data->addr, 0x500, 0, 0);

     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status1);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c400030, get_mem(0x44));

     /* Make sure sense is zero */
     set_mem(0x700, 0xffffffff);
     status1 = start_io(data->addr, 0x510, 0, 0);
     if (verbose) {
         for (i = 0; i < 120; i++) {
             printf("%02x, ", ctx->buffer[i]);
         }
         printf("\n");
     }
     for (i = 0; i < 80; i++) {
         ASSERT_EQUAL_X(get_mem_b(0x600+i), ctx->buffer[i]);
     }
     for (; i < 120; i++) {
         ASSERT_EQUAL_X(0, ctx->buffer[i]);
     }


     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status1);
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00ffffff, get_mem(0x700));
}


