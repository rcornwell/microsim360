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
#include "event.h"
#include "ctest.h"
#include "xlat.h"
#include "model2844.h"

uint64_t   step_count = 0;

void
print_bin(struct _device *dev, int unit)
{
    struct _2844_context *ctx = (struct _2844_context *)dev->dev;
    struct _dasd_t *dasd = ctx->disk[unit];
    int             pos;
    uint8_t         *rec;
//    uint8_t         *da;
//    int             dlen;
//    int             klen;
//    int             i;
//    int             end = 0;

    pos = (dasd->tsize * dasd->head);
    rec = &dasd->cbuf[pos];
    log_trace("HA %02x %02x %02x %02x %02x\n", rec[0], rec[1], rec[2], rec[3], rec[4]);
    rec = &rec[5];
    log_trace("RECa c=%02x%02x h=%02x%02x r=%02x\n", rec[0], rec[1], rec[2], rec[3], rec[4]);
    rec = &rec[7];
    log_trace("RECb c=%02x%02x h=%02x%02x r=%02x\n", rec[0], rec[1], rec[2], rec[3], rec[4]);
}

void
print_track(struct _device *dev, int unit)
{
    struct _2844_context *ctx = (struct _2844_context *)dev->dev;
    struct _dasd_t *dasd = ctx->disk[unit];
    int             pos;
    uint8_t         *rec;
    int             dlen;
    int             klen;
    int             i;
    int             end = 0;

    pos = (dasd->tsize * dasd->head);

    rec = &dasd->cbuf[pos];
    log_trace("HA %02x %02x %02x %02x %02x\n", rec[0], rec[1], rec[2], rec[3], rec[4]);
    rec = &rec[5];
    for (i = 0; !end; i++) {
         if ((rec[0] & rec[1] & rec[2] & rec[3]) == 0xff) {
            log_trace("End\n");
            end = 1;
            klen = dlen = 0;
         } else {
            klen = rec[5];
            dlen = (rec[6] << 8) | rec[7];
         }
         log_trace("REC%d c=%02x%02x h=%02x%02x r=%02x k=%d d=%d\n", i, rec[0], rec[1], rec[2],
                 rec[3], rec[4], klen, dlen);
         rec += 8 + klen + dlen;
    }
}

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
    status = 0x100;
    bus_out = 0x100;
    log_trace("Initial select\n");
    for (i = 0; i < 200; i++) {
        step_disk();
        step_disk();
        advance();
        step_count++;
        if (i == 30) {
           *tags |= CHAN_ADR_OUT;
           bus_out = 0x91;
        }
        if (i == 31)
           sel = 1;
        if (sel) {
           *tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;
        }
        dev->bus_func(dev, tags, bus_out, &bus_in);
        if ((*tags & CHAN_ADR_IN) != 0) {
           log_trace("Got address in\n");
           ASSERT_EQUAL_X(0x91, bus_in);
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
        if ((*tags & (CHAN_STA_IN|CHAN_ADR_OUT)) == (CHAN_STA_IN|CHAN_ADR_OUT)) {
           log_trace("Unit busy %02x\n", bus_in);
           status = bus_in;
           bus_out = 0x100;
           sts = 0;
           *tags &= ~(CHAN_ADR_OUT);
           *tags |= CHAN_SRV_OUT;
           for (i = 0; i < 500; i++) {
               step_2844((struct _2844_context *)dev->dev);
               step_2844((struct _2844_context *)dev->dev);
               advance();
               step_count++;
               dev->bus_func(dev, tags, bus_out, &bus_in);
               if ((*tags & (CHAN_STA_IN)) == 0 && !sts) {
                   *tags &= ~(CHAN_SRV_OUT);
                   *tags |= (CHAN_SEL_OUT);
               }
               if ((*tags & (CHAN_OPR_IN|CHAN_ADR_IN)) ==
                            (CHAN_OPR_IN|CHAN_ADR_IN) &&
                    bus_in == 0x91) {
                   *tags |= (CHAN_CMD_OUT);
                    bus_out = (cmd & 0xff) | odd_parity[cmd & 0xff];
               }
               if ((*tags & (CHAN_ADR_IN|CHAN_CMD_OUT)) == CHAN_CMD_OUT) {
                  log_trace("Drop command out\n");
                  bus_out = 0x100;
                  *tags &= ~CHAN_CMD_OUT;
               }
               if ((*tags & (CHAN_STA_IN)) != 0 && bus_in == 0x20) {
                   *tags |= (CHAN_SRV_OUT);
                   sts = 1;
               }
               if ((*tags & (CHAN_STA_IN)) == 0 && sts) {
                   *tags &= ~(CHAN_SRV_OUT);
                   *tags |= (CHAN_SEL_OUT);
               }
           }
           break;
        }
        if ((*tags & (CHAN_STA_IN)) != 0) {
           log_trace("Status in %02x\n", bus_in);
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
           log_trace("Service out drop\n");
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

    bus_out = 0x100;
    log_trace("Read data\n");
    for (i = 0; i < 120000; i++) {
        step_disk();
        step_disk();
        step_count++;
        advance();
        if (sel)
           *tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;
        dev->bus_func(dev, tags, bus_out, &bus_in);
        if ((*tags & (CHAN_STA_IN)) != 0) {
           log_trace("Status in\n");
           status = bus_in;   /* Device end and channel end */
           *tags |= CHAN_SRV_OUT;
           if (cc)
               *tags |= CHAN_SUP_OUT;
           sta_in = 1;
        }
        if ((*tags & (CHAN_STA_IN|CHAN_SRV_IN|CHAN_SRV_OUT)) == (CHAN_SRV_OUT)) {
           log_trace("Service in drop\n");
           *tags &= ~(CHAN_SRV_OUT);
           if (sta_in) {
               *tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
               sel = 0;
               break;
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

    log_trace("Write data\n");
    bus_out = 0x100;
    for (i = 0; i < 50000; i++) {
        step_disk();
        step_disk();
        step_count++;
        advance();
        if (sel)
           *tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;
        dev->bus_func(dev, tags, bus_out, &bus_in);
        if ((*tags & CHAN_STA_IN) == CHAN_STA_IN) {
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
           if (sta_in && cc == 0) {
               log_trace("Drop select out write data\n");
               *tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
               sel = 0;
           }
           /* Check data chaining end */
           if (sta_in && cc == 1 && (status & 0x4) != 0) {
               break;
           }
        }
        if ((*tags & (CHAN_SRV_IN|CHAN_CMD_OUT)) == (CHAN_CMD_OUT)) {
           log_trace("Command in drop\n");
           bus_out = 0x100;
           *tags &= ~(CHAN_CMD_OUT);
        }
        if ((*tags & (CHAN_SRV_OUT|CHAN_CMD_OUT|CHAN_SRV_IN)) == (CHAN_SRV_IN)) {
           if (byte <= *num) {
               bus_out = data[byte];
               bus_out |= odd_parity[bus_out];
           }
           log_trace("Service in %03x %02x %02x\n", bus_in, data[byte], byte);
           byte++;
           if (byte > *num) {
               *tags |= CHAN_CMD_OUT;
           } else {
               *tags |= (CHAN_SRV_OUT);
           }
        }
        if ((*tags & (CHAN_OPR_IN)) == 0) {
           log_trace("Oper in drop\n");
           break;
        }
    }
    log_trace("Write data end\n");
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
    int         sel = 0;
    int         sta = 0;

    *tags &= ~(CHAN_SRV_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT);
    if ((*tags & (CHAN_OPR_IN)) != 0) {
        *tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;
        sel = 1;
    } else {
        sel = 0;
    }
    for (i = 0; i < 70000; i++) {
        step_disk();
        step_disk();
        step_count++;
        advance();
        dev->bus_func(dev, tags, bus_out, &bus_in);
        if ((*tags & (CHAN_OPR_IN)) == 0) {
           log_trace("Oper in drop\n");
           *tags &= ~(CHAN_SEL_OUT|CHAN_HLD_OUT);
           sel = 0;
           if (sta && (status & 0x4) != 0) {
               break;
           }
        }
        if ((*tags & (CHAN_REQ_IN)) != 0) {
           sel = 1;
        }
        if (sel)
           *tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;

        if ((*tags & (CHAN_ADR_IN)) != 0) { // && bus_in == 0x190) {
           log_trace("Address in %02x\n", bus_in);
           *tags |= CHAN_CMD_OUT;
           bus_out = 0x100;
        }

        if ((*tags & (CHAN_ADR_IN|CHAN_CMD_OUT)) == CHAN_CMD_OUT) {
           log_trace("Drop command out\n");
           bus_out = 0x100;
           *tags &= ~CHAN_CMD_OUT;
        }
        if ((*tags & (CHAN_STA_IN)) != 0) {
           if (cc)
               *tags |= CHAN_SUP_OUT;
           *tags |= CHAN_SRV_OUT;
           sta = 1;
           status = bus_in;
           log_trace("Status in %02x\n", status);
        }
        if ((*tags & (CHAN_SRV_IN|CHAN_STA_IN|CHAN_SRV_OUT)) == (CHAN_SRV_OUT)) {
           *tags &= ~CHAN_SRV_OUT;
           sel = 0;
//           i = 30000 - 50;
        }
    }
    return status;
}

CTEST_DATA(disk_test) {
    struct _device *dev;
};

CTEST_SETUP(disk_test) {
    struct _2844_context *ctx;
    disk = NULL;
    data->dev = model2844_init(NULL, 0x90);
    ctx = (struct _2844_context *)(data->dev->dev);
    ctx->disk[1] = (struct _dasd_t *)calloc(1, sizeof(struct _dasd_t));
    dasd_settype(ctx->disk[1], "2314");
    dasd_attach(ctx->disk[1], "test.ckd", 1);
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
       del_disk((void*)ctx);
    }
    if (data->dev) {
       del_chan(data->dev, data->dev->addr);
       free(data->dev);
    }
}

/* Make sure disk can reset and get to poll routine */
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

    tags = CHAN_OPR_OUT;
    bus_out = 0x100;
    for (i = 0; i < 200; i++) {
        step_disk();
        step_disk();
        step_count++;
        if (i == 30) {
           tags |= CHAN_ADR_OUT;
           bus_out = 0x91;
        }
        if (i == 31)
           sel = 1;
        if (sel)
           tags |= CHAN_SEL_OUT|CHAN_HLD_OUT;
        data->dev->bus_func(data->dev, &tags, bus_out, &bus_in);
        if ((tags & CHAN_ADR_IN) != 0) {
           log_trace("Got address in\n");
           ASSERT_EQUAL_X(0x91, bus_in);
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

    status = initial_select(data->dev, &tags, 0x3);
    ASSERT_EQUAL_X(0x10c, status);
}

/* Try to send Set file mask to controller */
CTEST2(disk_test, sense) {
    uint16_t    tags = 0;
    uint16_t    status;
    uint8_t     sense[6];
    int         num;

    status = initial_select(data->dev, &tags, 0x4);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = read_data(data->dev, &tags, &sense[0], &num, 0);
    printf("Sense %02x %d -> %02x %02x %02x %02x %02x %02x\n", status, num, sense[0], sense[1],
            sense[2], sense[3], sense[4], sense[5]);
    ASSERT_EQUAL_X(0x0, sense[0]);
    ASSERT_EQUAL_X(0x0, sense[1]);
    ASSERT_EQUAL_X(0x0, sense[2]);
    ASSERT_EQUAL_X(0x40, sense[3]);
    ASSERT_EQUAL_X(0x01, sense[4]);
    ASSERT_EQUAL_X(0x0, sense[5]);
    ASSERT_EQUAL_X(0x10c, status);
}

/* Try to send Set file mask to controller */
CTEST2(disk_test, setmask) {
    uint16_t    tags = 0;
    uint8_t     mask;
    uint8_t     sense[6];
    uint16_t    status;
    int         num;

    /* Test valid mask */
    mask = 0xc0;
    status = initial_select(data->dev, &tags, 0x1f);
    ASSERT_EQUAL_X(0x100, status);
    num = 1;
    status = write_data(data->dev, &tags, &mask, &num, 0);
    ASSERT_EQUAL_X(0x10c, status);
    /* Test invalid mask */
    log_trace("Set Mask %02x %d\n", status, num);
    mask = 0xf0;
    status = initial_select(data->dev, &tags, 0x1f);
    ASSERT_EQUAL_X(0x100, status);
    num = 1;
    status = write_data(data->dev, &tags, &mask, &num, 0);
    log_trace("Set Mask %02x %d\n", status, num);
    ASSERT_EQUAL_X(0x0e, status);
    /* Check sense data indicated command rejected */
    status = initial_select(data->dev, &tags, 0x4);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = read_data(data->dev, &tags, &sense[0], &num, 0);
    printf("Sense %02x %d -> %02x %02x %02x %02x %02x %02x\n", status, num, sense[0], sense[1],
            sense[2], sense[3], sense[4], sense[5]);
    ASSERT_EQUAL_X(0x80, sense[0]);
    ASSERT_EQUAL_X(0x00, sense[1]);
    ASSERT_EQUAL_X(0x00, sense[2]);
    ASSERT_EQUAL_X(0x40, sense[3]);
    ASSERT_EQUAL_X(0x01, sense[4]);
    ASSERT_EQUAL_X(0x00, sense[5]);
    ASSERT_EQUAL_X(0x10c, status);
}


/* Try to send Seek to controller */
CTEST2(disk_test, seek) {
    struct _2844_context *ctx = (struct _2844_context *)(data->dev->dev);
    static uint8_t  cmd[] = { 0, 0, 0, 0x10, 0, 5 };
    uint8_t     sense[6];
    uint16_t    tags = 0;
    uint16_t    status;
    int         num;

    status = initial_select(data->dev, &tags, 0x7);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = write_data(data->dev, &tags, &cmd[0], &num, 0);
    log_trace("Seek %02x %d\n", status, num);
    ASSERT_EQUAL_X(0x8, status);
    status = wait_dev(data->dev, &tags, 0);
    ASSERT_EQUAL_X(0x4, status);
    status = initial_select(data->dev, &tags, 0x4);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = read_data(data->dev, &tags, &sense[0], &num, 0);
    ASSERT_EQUAL(5, ctx->disk[1]->head);
    ASSERT_EQUAL(0x10, ctx->disk[1]->cyl);
}

/* Try to send Restore to controller */
CTEST2(disk_test, restore) {
    struct _2844_context *ctx = (struct _2844_context *)(data->dev->dev);
    uint8_t     sense[6];
    uint16_t    tags = 0;
    uint16_t    status;
    int         num;

    ctx->disk[1]->cyl = 10;
    ctx->disk[1]->head = 8;
    status = initial_select(data->dev, &tags, 0x13);
    ASSERT_EQUAL_X(0x100, status);
    log_trace("Restore %02x\n", status);
    ASSERT_EQUAL_X(0x100, status);
    status = wait_dev(data->dev, &tags, 0);
    log_trace("Wait done %02x\n", status);
    ASSERT_EQUAL_X(0x10c, status);
    log_trace("Wait2 done %02x\n", status);
    status = initial_select(data->dev, &tags, 0x4);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = read_data(data->dev, &tags, &sense[0], &num, 0);
    ASSERT_EQUAL(0, ctx->disk[1]->head);
    ASSERT_EQUAL(0, ctx->disk[1]->cyl);
    ASSERT_EQUAL_X(0x0, sense[0]);
    ASSERT_EQUAL_X(0x0, sense[1]);
    ASSERT_EQUAL_X(0x0, sense[2]);
    ASSERT_EQUAL_X(0x40, sense[3]);
    ASSERT_EQUAL_X(0x01, sense[4]);
    ASSERT_EQUAL_X(0x0, sense[5]);
    ASSERT_EQUAL_X(0x10c, status);
}

/* Try to read HA */
CTEST2(disk_test, readHA) {
    struct _2844_context *ctx = (struct _2844_context *)(data->dev->dev);
    static uint8_t  cmd[] = { 0, 0, 0, 0, 0, 0 };
    uint8_t     sense[6];
    uint8_t     ha[5];
    uint16_t    tags = 0;
    uint16_t    status;
    int         num;

    ctx->disk[1]->cyl = 0;
    ctx->disk[1]->head = 0;
    status = initial_select(data->dev, &tags, 0x7);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = write_data(data->dev, &tags, &cmd[0], &num, 1);
    log_trace("Seek %02x %d\n", status, num);
    ASSERT_EQUAL_X(0x4, status);
    log_trace("Seek complete\n");
    log_trace("Wait2 done %02x\n", status);
    status = initial_select(data->dev, &tags, 0x1a);
    ASSERT_EQUAL_X(0x100, status);
    num = 5;
    status = read_data(data->dev, &tags, &ha[0], &num, 0);
    printf("HA %02x %d -> %02x %02x %02x %02x %02x\n", status, num, ha[0], ha[1],
            ha[2], ha[3], ha[4]);
    ASSERT_EQUAL_X(0x10c, status);
    status = initial_select(data->dev, &tags, 0x4);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = read_data(data->dev, &tags, &sense[0], &num, 0);
    ASSERT_EQUAL(0, ctx->disk[1]->head);
    ASSERT_EQUAL(0, ctx->disk[1]->cyl);
    ASSERT_EQUAL_X(0x0, sense[0]);
    ASSERT_EQUAL_X(0x0, sense[1]);
    ASSERT_EQUAL_X(0x0, sense[2]);
    ASSERT_EQUAL_X(0x40, sense[3]);
    ASSERT_EQUAL_X(0x01, sense[4]);
    ASSERT_EQUAL_X(0x0, sense[5]);
    ASSERT_EQUAL_X(0x10c, status);
}

/* Try to read R0 */
CTEST2(disk_test, readR0) {
    struct _2844_context *ctx = (struct _2844_context *)(data->dev->dev);
    static uint8_t  cmd[] = { 0, 0, 0, 0, 0, 0 };
    uint8_t     sense[6];
    uint8_t     R0[16];
    uint16_t    tags = 0;
    uint16_t    status;
    int         num;
    int         i;

    ctx->disk[1]->cyl = 0;
    ctx->disk[1]->head = 0;
    status = initial_select(data->dev, &tags, 0x7);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = write_data(data->dev, &tags, &cmd[0], &num, 1);
    log_trace("Seek %02x %d\n", status, num);
    ASSERT_EQUAL_X(0x4, status);
    log_trace("Seek complete\n");
    log_trace("Wait2 done %02x\n", status);
    status = initial_select(data->dev, &tags, 0x16);
    ASSERT_EQUAL_X(0x100, status);
    num = 16;
    status = read_data(data->dev, &tags, &R0[0], &num, 0);
    printf("R0 %02x %d ->", status, num);
    for (i = 0; i < num; i++) {
        printf(" %02x", R0[i]);
    }
    printf("\n");
    ASSERT_EQUAL_X(0x10c, status);
    status = initial_select(data->dev, &tags, 0x4);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = read_data(data->dev, &tags, &sense[0], &num, 0);
    ASSERT_EQUAL(0, ctx->disk[1]->head);
    ASSERT_EQUAL(0, ctx->disk[1]->cyl);
    ASSERT_EQUAL_X(0x0, sense[0]);
    ASSERT_EQUAL_X(0x0, sense[1]);
    ASSERT_EQUAL_X(0x0, sense[2]);
    ASSERT_EQUAL_X(0x40, sense[3]);
    ASSERT_EQUAL_X(0x01, sense[4]);
    ASSERT_EQUAL_X(0x0, sense[5]);
    ASSERT_EQUAL_X(0x10c, status);
}

/* Try to send Set file mask to controller */
CTEST2(disk_test, read_ipl) {
    struct _2844_context *ctx = (struct _2844_context *)(data->dev->dev);
    uint8_t     ipl_rec[] = { 0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x0F,
                              0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x01,
                              0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    uint16_t    tags;
    uint16_t    status;
    uint8_t     sense[6];
    int         num;
    int         i;
    uint8_t     res[256];

    log_trace("Read IPL\n");
    tags = 0;
    ctx->disk[1]->cpos = 7000;
    status = initial_select(data->dev, &tags, 0x02);
    ASSERT_EQUAL_X(0x100, status);
    num = 24;
    status = read_data(data->dev, &tags, &res[0], &num, 0);
    if (status == 0x10c) {
        ASSERT_EQUAL_X(0x10c, status);
        for (i = 0; i < num; i++) {
            ASSERT_EQUAL_X(ipl_rec[i], res[i]);
            log_trace("Read %d: %02x\n", i, res[i]);
        }

    }
    status = initial_select(data->dev, &tags, 0x4);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = read_data(data->dev, &tags, &sense[0], &num, 0);
    printf("Sense %02x %d -> %02x %02x %02x %02x %02x %02x\n", status, num, sense[0], sense[1],
            sense[2], sense[3], sense[4], sense[5]);
    ASSERT_EQUAL_X(0x0, sense[0]);
    ASSERT_EQUAL_X(0x0, sense[1]);
    ASSERT_EQUAL_X(0x0, sense[2]);
    ASSERT_EQUAL_X(0x40, sense[3]);
    ASSERT_EQUAL_X(0x01, sense[4]);
    ASSERT_EQUAL_X(0x0, sense[5]);
    ASSERT_EQUAL_X(0x10c, status);
    print_track(data->dev, 1);
}

/* Try to write to controller */
CTEST2(disk_test, writeHA) {
    struct _2844_context *ctx = (struct _2844_context *)(data->dev->dev);
    static uint8_t  cmd[] = { 0, 0, 0, 10, 0, 4 };
    static uint8_t  mask[] = { 0xc0 };
    static uint8_t  mask1[] = { 0x00 };
    static uint8_t  wha[] = { 0, 1, 2, 3, 4};
    uint8_t         ha[5];
    uint8_t         R0[16];
    uint8_t         sense[6];
    uint16_t        tags;
    uint16_t        status;
    int             num;

    tags = 0;
    ctx->disk[1]->cpos = 7000;
    status = initial_select(data->dev, &tags, 0x7);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = write_data(data->dev, &tags, &cmd[0], &num, 1);
    log_trace("Seek %02x %d\n", status, num);
    ASSERT_EQUAL_X(0x4, status);
    log_trace("Seek complete\n");
    status = initial_select(data->dev, &tags, 0x1f);
    ASSERT_EQUAL_X(0x100, status);
    num = 1;
    status = write_data(data->dev, &tags, &mask[0], &num, 1);
    ASSERT_EQUAL_X(0x10c, status);
    log_trace("Start write HA\n");
    status = initial_select(data->dev, &tags, 0x19);
    if (status != 0x100) goto sense;
    num = 5;
    log_trace("Start write HA data\n");
    status = write_data(data->dev, &tags, &wha[0], &num, 1);
    print_track(data->dev, 1);
    if (status != 0x10c) goto sense;
    status = initial_select(data->dev, &tags, 0x3);
    log_trace("HA Done %x\n", status);
    ASSERT_EQUAL_X(0x100, status);
    status = wait_dev(data->dev, &tags, 0);
    ASSERT_EQUAL_X(0x10c, status);
sense:
    status = initial_select(data->dev, &tags, 0x4);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = read_data(data->dev, &tags, &sense[0], &num, 0);
    printf("Sense %02x %d -> %02x %02x %02x %02x %02x %02x\n", status, num, sense[0], sense[1],
            sense[2], sense[3], sense[4], sense[5]);
    print_track(data->dev, 1);
    ASSERT_EQUAL_X(0x0, sense[0]);
    ASSERT_EQUAL_X(0x0, sense[1]);
    ASSERT_EQUAL_X(0x0, sense[2]);
    ASSERT_EQUAL_X(0x40, sense[3]);
    ASSERT_EQUAL_X(0x01, sense[4]);
    ASSERT_EQUAL_X(0x0, sense[5]);
    ASSERT_EQUAL_X(0x10c, status);
    /* Try to read back in HA just written */
    ctx->disk[1]->cyl = 0;
    ctx->disk[1]->head = 0;
    status = initial_select(data->dev, &tags, 0x7);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = write_data(data->dev, &tags, &cmd[0], &num, 1);
    log_trace("Seek %02x %d\n", status, num);
    ASSERT_EQUAL_X(0x4, status);
    log_trace("Seek complete\n");
    log_trace("Wait2 done %02x\n", status);
    status = initial_select(data->dev, &tags, 0x1a);
    ASSERT_EQUAL_X(0x100, status);
    num = 5;
    status = read_data(data->dev, &tags, &ha[0], &num, 0);
    printf("HA %02x %d -> %02x %02x %02x %02x %02x\n", status, num, ha[0], ha[1],
            ha[2], ha[3], ha[4]);
    ASSERT_EQUAL_X(0x10c, status);
    status = initial_select(data->dev, &tags, 0x4);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = read_data(data->dev, &tags, &sense[0], &num, 0);
    printf("Sense1 %02x %d -> %02x %02x %02x %02x %02x %02x\n", status, num, sense[0], sense[1],
            sense[2], sense[3], sense[4], sense[5]);
    ASSERT_EQUAL(4, ctx->disk[1]->head);
    ASSERT_EQUAL(10, ctx->disk[1]->cyl);
    ASSERT_EQUAL_X(0x0, sense[0]);
    ASSERT_EQUAL_X(0x0, sense[1]);
    ASSERT_EQUAL_X(0x0, sense[2]);
    ASSERT_EQUAL_X(0x40, sense[3]);
    ASSERT_EQUAL_X(0x01, sense[4]);
    ASSERT_EQUAL_X(0x0, sense[5]);
    ASSERT_EQUAL_X(0x10c, status);
    ASSERT_EQUAL_X(0x0, ha[0]);
    ASSERT_EQUAL_X(0x1, ha[1]);
    ASSERT_EQUAL_X(0x2, ha[2]);
    ASSERT_EQUAL_X(0x3, ha[3]);
    ASSERT_EQUAL_X(0x4, ha[4]);
    /* Try to read R0, should fail */
    ctx->disk[1]->cyl = 0;
    ctx->disk[1]->head = 0;
    status = initial_select(data->dev, &tags, 0x7);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = write_data(data->dev, &tags, &cmd[0], &num, 1);
    log_trace("Seek %02x %d\n", status, num);
    ASSERT_EQUAL_X(0x4, status);
    log_trace("Seek complete\n");
    log_trace("Wait2 done %02x\n", status);
    status = initial_select(data->dev, &tags, 0x16);
    ASSERT_EQUAL_X(0x100, status);
    num = 16;
    status = read_data(data->dev, &tags, &R0[0], &num, 0);
    ASSERT_EQUAL_X(0x0e, status);
    status = initial_select(data->dev, &tags, 0x4);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = read_data(data->dev, &tags, &sense[0], &num, 0);
    printf("Sense %02x %d -> %02x %02x %02x %02x %02x %02x\n", status, num, sense[0], sense[1],
            sense[2], sense[3], sense[4], sense[5]);
    printf("Sense2 %02x %d -> %02x %02x %02x %02x %02x %02x\n", status, num, sense[0], sense[1],
            sense[2], sense[3], sense[4], sense[5]);
    ASSERT_EQUAL_X(0x0, sense[0]);
    ASSERT_EQUAL_X(0xa, sense[1]);
    ASSERT_EQUAL_X(0x0, sense[2]);
    ASSERT_EQUAL_X(0x40, sense[3]);
    ASSERT_EQUAL_X(0x01, sense[4]);
    ASSERT_EQUAL_X(0x0, sense[5]);
    ASSERT_EQUAL_X(0x10c, status);
    print_track(data->dev, 1);
    /* Try to write HA when mask does not permit it */
    tags = 0;
    ctx->disk[1]->cpos = 7000;
    status = initial_select(data->dev, &tags, 0x7);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = write_data(data->dev, &tags, &cmd[0], &num, 1);
    log_trace("Seek %02x %d\n", status, num);
    ASSERT_EQUAL_X(0x4, status);
    log_trace("Seek complete\n");
    status = initial_select(data->dev, &tags, 0x1f);
    ASSERT_EQUAL_X(0x100, status);
    num = 1;
    status = write_data(data->dev, &tags, &mask1[0], &num, 1);
    ASSERT_EQUAL_X(0x10c, status);
    log_trace("Start write fail HA\n");
    status = initial_select(data->dev, &tags, 0x19);
    printf("Start done %02x\n", status);
    if (status != 0x100) goto sense2;
    num = 5;
    log_trace("Start write HA data\n");
    status = write_data(data->dev, &tags, &wha[0], &num, 1);
    printf("Start done %02x\n", status);
    status = initial_select(data->dev, &tags, 0x3);
    log_trace("HA Done %x\n", status);
    ASSERT_EQUAL_X(0x100, status);
    status = wait_dev(data->dev, &tags, 0);
    ASSERT_EQUAL_X(0x10c, status);
sense2:
    status = initial_select(data->dev, &tags, 0x4);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = read_data(data->dev, &tags, &sense[0], &num, 0);
    printf("Sense3 %02x %d -> %02x %02x %02x %02x %02x %02x\n", status, num, sense[0], sense[1],
            sense[2], sense[3], sense[4], sense[5]);
    ASSERT_EQUAL_X(0x80, sense[0]);
    ASSERT_EQUAL_X(0x04, sense[1]);
    ASSERT_EQUAL_X(0x0, sense[2]);
    ASSERT_EQUAL_X(0x40, sense[3]);
    ASSERT_EQUAL_X(0x01, sense[4]);
    ASSERT_EQUAL_X(0x0, sense[5]);
    ASSERT_EQUAL_X(0x10c, status);
    print_track(data->dev, 1);
    return;
}

/* Try to write R0 record */
CTEST2(disk_test, writeR0) {
    struct _2844_context *ctx = (struct _2844_context *)(data->dev->dev);
    static uint8_t  cmd[] = { 0, 0, 0, 10, 0, 4 };
    static uint8_t  mask[] = { 0xc0 };
    static uint8_t  wr0[] = { 0, 10, 0, 4, 0, 0, 0, 8, 1, 2, 3, 4, 5, 6, 7, 8};
    uint8_t         wrk[100];
    uint8_t         sense[6];
    uint16_t        tags;
    uint16_t        status;
    int             num, i;

    tags = 0;
    ctx->disk[1]->cpos = 7000;
    status = initial_select(data->dev, &tags, 0x7);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = write_data(data->dev, &tags, &cmd[0], &num, 1);
    log_trace("Seek %02x %d\n", status, num);
    ASSERT_EQUAL_X(0x4, status);
    log_trace("Seek complete\n");
    /* Set file mask */
    status = initial_select(data->dev, &tags, 0x1f);
    ASSERT_EQUAL_X(0x100, status);
    num = 1;
    status = write_data(data->dev, &tags, &mask[0], &num, 1);
    ASSERT_EQUAL_X(0x10c, status);
    /* Search Home address */
    status = initial_select(data->dev, &tags, 0x39);
    ASSERT_EQUAL_X(0x100, status);
    num = 4;
    status = write_data(data->dev, &tags, &cmd[2], &num, 1);
    ASSERT_EQUAL_X(0x4c, status);
    /* Write R0 */
    status = initial_select(data->dev, &tags, 0x15);
    ASSERT_EQUAL_X(0x100, status);
    num = 16;
    log_trace("Start write R0\n");
    status = write_data(data->dev, &tags, &wr0[0], &num, 1);
    ASSERT_EQUAL_X(0x10c, status);
    status = initial_select(data->dev, &tags, 0x3);
    log_trace("HA Done %x\n", status);
    ASSERT_EQUAL_X(0x100, status);
    status = wait_dev(data->dev, &tags, 0);
    ASSERT_EQUAL_X(0x10c, status);
    print_track(data->dev, 1);
    /* Grab sense info and make sure all is correct. */
    status = initial_select(data->dev, &tags, 0x4);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = read_data(data->dev, &tags, &sense[0], &num, 0);
    printf("Sense %02x %d -> %02x %02x %02x %02x %02x %02x\n", status, num, sense[0], sense[1],
            sense[2], sense[3], sense[4], sense[5]);
    print_track(data->dev, 1);
    /* Try to read R0, should succeed */
    ctx->disk[1]->cyl = 0;
    ctx->disk[1]->head = 0;
    ctx->disk[1]->cpos = 7000;
    status = initial_select(data->dev, &tags, 0x7);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = write_data(data->dev, &tags, &cmd[0], &num, 1);
    log_trace("Seek %02x %d\n", status, num);
    ASSERT_EQUAL_X(0x4, status);
    log_trace("Seek complete\n");
    log_trace("Wait2 done %02x\n", status);
    status = initial_select(data->dev, &tags, 0x16);
    ASSERT_EQUAL_X(0x100, status);
    num = 16;
    status = read_data(data->dev, &tags, &wrk[0], &num, 0);
    ASSERT_EQUAL_X(0x10c, status);
    printf("R0 %02x %d ->", status, num);
    for (i = 0; i < num; i++) {
        printf(" %02x", wrk[i]);
        ASSERT_EQUAL_X(wr0[i], wrk[i]);
    }
    printf("\n");
    status = initial_select(data->dev, &tags, 0x4);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = read_data(data->dev, &tags, &sense[0], &num, 0);
    printf("Sense2 %02x %d -> %02x %02x %02x %02x %02x %02x\n", status, num, sense[0], sense[1],
            sense[2], sense[3], sense[4], sense[5]);
    ASSERT_EQUAL_X(0x0, sense[0]);
    ASSERT_EQUAL_X(0x0, sense[1]);
    ASSERT_EQUAL_X(0x0, sense[2]);
    ASSERT_EQUAL_X(0x40, sense[3]);
    ASSERT_EQUAL_X(0x01, sense[4]);
    ASSERT_EQUAL_X(0x0, sense[5]);
    ASSERT_EQUAL_X(0x10c, status);
    print_track(data->dev, 1);
    /* Try out of sequence write R0 */
    tags = 0;
    ctx->disk[1]->cpos = 7000;
    status = initial_select(data->dev, &tags, 0x7);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = write_data(data->dev, &tags, &cmd[0], &num, 1);
    log_trace("Seek %02x %d\n", status, num);
    ASSERT_EQUAL_X(0x4, status);
    log_trace("Seek complete\n");
    /* Set file mask */
    status = initial_select(data->dev, &tags, 0x1f);
    ASSERT_EQUAL_X(0x100, status);
    num = 1;
    status = write_data(data->dev, &tags, &mask[0], &num, 1);
    ASSERT_EQUAL_X(0x10c, status);
    /* Write R0 */
    status = initial_select(data->dev, &tags, 0x15);
    ASSERT_EQUAL_X(0x02, status);
    status = initial_select(data->dev, &tags, 0x4);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = read_data(data->dev, &tags, &sense[0], &num, 0);
    printf("Sensef %02x %d -> %02x %02x %02x %02x %02x %02x\n", status, num, sense[0], sense[1],
            sense[2], sense[3], sense[4], sense[5]);
    ASSERT_EQUAL_X(0x80, sense[0]);
    ASSERT_EQUAL_X(0x10, sense[1]);
    ASSERT_EQUAL_X(0x0, sense[2]);
    ASSERT_EQUAL_X(0x40, sense[3]);
    ASSERT_EQUAL_X(0x01, sense[4]);
    ASSERT_EQUAL_X(0x0, sense[5]);
    ASSERT_EQUAL_X(0x10c, status);
    print_track(data->dev, 1);
    return;
}


/* Try to write track */
CTEST2(disk_test, writeTrack) {
    struct _2844_context *ctx = (struct _2844_context *)(data->dev->dev);
    static uint8_t  cmd[] = { 0, 0, 0, 10, 0, 4 };
    static uint8_t  mask[] = { 0xc0 };
    static uint8_t  wha[] = { 0, 0, 10, 0, 4};
    static uint8_t  wr0[] = { 0, 10, 0, 4, 0, 0, 0, 8, 1, 2, 3, 4, 5, 6, 7, 8};
    uint8_t         wrk[512];
    uint8_t         sense[6];
    uint16_t        tags;
    uint16_t        bus_out = 0x100;
    uint16_t        bus_in;
    uint16_t        status;
    int             i, j;
    int             num;

    tags = 0;
    ctx->disk[1]->cpos = 7000;
    status = initial_select(data->dev, &tags, 0x7);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = write_data(data->dev, &tags, &cmd[0], &num, 1);
    log_trace("Seek %02x %d\n", status, num);
    ASSERT_EQUAL_X(0x4, status);
    log_trace("Seek complete\n");
    status = initial_select(data->dev, &tags, 0x1f);
    ASSERT_EQUAL_X(0x100, status);
    num = 1;
    status = write_data(data->dev, &tags, &mask[0], &num, 1);
    ASSERT_EQUAL_X(0x10c, status);
    log_trace("Start write HA\n");
    status = initial_select(data->dev, &tags, 0x19);
    if (status != 0x100) goto sense;
    num = 5;
    log_trace("Start write HA data\n");
    status = write_data(data->dev, &tags, &wha[0], &num, 1);
    print_track(data->dev, 1);
    if (status != 0x10c) goto sense;
    status = initial_select(data->dev, &tags, 0x15);
    if (status != 0x100) goto sense;
    num = 16;
    log_trace("Start write R0\n");
    status = write_data(data->dev, &tags, &wr0[0], &num, 1);
    ASSERT_EQUAL_X(0x10c, status);
    print_track(data->dev, 1);
    for (j = 1; j < 5; j++) {
        for (i = 0; i < 512; i++)
            wrk[i] = 0;
        wrk[1] = 10;
        wrk[3] = 4;
        wrk[4] = j;
        wrk[5] = 8;
        wrk[7] = 128;
        for (i = 0; i < 8; i++)
            wrk[8+i] = 0xf0 + i;
        for (i = 0; i < 128; i++)
            wrk[16+i] = i;
        status = initial_select(data->dev, &tags, 0x1d);
        if (status != 0x100) goto sense;
        num = 128 + 8 + 8;
        status = write_data(data->dev, &tags, &wrk[0], &num, 1);
        ASSERT_EQUAL_X(0x10c, status);
        print_track(data->dev, 1);
    }
    for (i = 0; i < 512; i++)
        wrk[i] = 0;
    wrk[1] = 10;
    wrk[3] = 4;
    wrk[4] = 5;
    wrk[5] = 8;
    wrk[7] = 128;
    for (i = 0; i < 8; i++)
        wrk[8+i] = 0xf0 + i;
    for (i = 0; i < 128; i++)
        wrk[16+i] = i;
    status = initial_select(data->dev, &tags, 0x1d);
    if (status != 0x100) goto sense;
    num = 128 + 8 + 8;
    status = write_data(data->dev, &tags, &wrk[0], &num, 1);
    ASSERT_EQUAL_X(0x10c, status);
    status = initial_select(data->dev, &tags, 0x3);
    log_trace("HA Done %x\n", status);
    ASSERT_EQUAL_X(0x100, status);
    status = wait_dev(data->dev, &tags, 0);
    ASSERT_EQUAL_X(0x10c, status);
    print_track(data->dev, 1);
sense:
    do {
       status = initial_select(data->dev, &tags, 0x4);
       if (status == 0x150 || status == 0x130) {
           tags = CHAN_OPR_OUT;
           for( i = 0; i < 50000; i++) {
               step_disk();
               step_disk();
               advance();
               step_count++;
               data->dev->bus_func(data->dev, &tags, bus_out, &bus_in);
               if ((tags & CHAN_REQ_IN) != 0) {
                  tags |= CHAN_SEL_OUT;
               log_trace("start Selout\n");
               }
               if ((tags & (CHAN_OPR_IN|CHAN_ADR_IN)) == (CHAN_OPR_IN|CHAN_ADR_IN)) {
                  bus_out = 0x100;
                  tags |= CHAN_CMD_OUT;
                 log_trace("Command out\n");
               }
               if ((tags & (CHAN_OPR_IN|CHAN_ADR_IN|CHAN_CMD_OUT)) == (CHAN_OPR_IN|CHAN_CMD_OUT)) {
                  bus_out = 0x100;
                  tags &= ~CHAN_CMD_OUT;
                 log_trace("Command out drop\n");
               }
               if ((tags & (CHAN_OPR_IN|CHAN_STA_IN|CHAN_SRV_OUT)) == (CHAN_OPR_IN|CHAN_STA_IN)) {
                  tags |= CHAN_SRV_OUT;
                  status = bus_in;
                  log_trace("Status accepted\n");
               }
               if ((tags & (CHAN_OPR_IN|CHAN_STA_IN|CHAN_SRV_OUT)) == (CHAN_OPR_IN|CHAN_SRV_OUT)) {
                  tags &= ~(CHAN_SRV_OUT|CHAN_SEL_OUT);
                  log_trace("Status done\n");
                  break;
               }
               if ((tags & CHAN_OPR_IN) != 0) {
                  tags |= CHAN_SEL_OUT;
               log_trace("hold Selout\n");
               }
           }
       }
    } while (status == 0x150 || status == 0x130);
    num = 6;
    status = read_data(data->dev, &tags, &sense[0], &num, 0);
    printf("Sense %02x %d -> %02x %02x %02x %02x %02x %02x\n", status, num, sense[0], sense[1],
            sense[2], sense[3], sense[4], sense[5]);
    print_track(data->dev, 1);
    return;
}

CTEST_DATA(disk_data) {
    struct _device *dev;
};

CTEST_SETUP(disk_data) {
    struct _2844_context *ctx;
    struct _dasd_t * dasd;
    int                 cyl;
    uint32_t            hd;
    int                 pos;
    int                 r;
    int                 key;

    disk = NULL;
    log_trace("Disk Data Setup\n");
    data->dev = model2844_init(NULL, 0x90);
    ctx = (struct _2844_context *)(data->dev->dev);
    ctx->disk[1] = (struct _dasd_t *)calloc(1, sizeof(struct _dasd_t));
    dasd = ctx->disk[1];
    dasd_settype(ctx->disk[1], "2314");
    dasd_attach(ctx->disk[1], "test.ckd", 1);
    (void)lseek(dasd->fd, sizeof(struct dasd_header), SEEK_SET);
    cyl = dasd->cyl = 10;
    dasd->head = 0;
    dasd->fpos = ((dasd->tsize * 20) * dasd->cyl) + sizeof(struct dasd_header);
    (void)lseek(dasd->fd, dasd->fpos, SEEK_SET);
    dasd->tstart = (dasd->tsize * dasd->head);
    pos = 0;
    memset(dasd->cbuf, 0, dasd->tsize);
    key = 0;
    for (hd = 0; hd < 20; hd++) {
        int cpos = pos;
        int   i, p;
        dasd->cbuf[pos++] = 0;            /* HA */
        dasd->cbuf[pos++] = (cyl >> 8);
        dasd->cbuf[pos++] = (cyl & 0xff);
        dasd->cbuf[pos++] = (hd >> 8);
        dasd->cbuf[pos++] = (hd & 0xff);
        dasd->cbuf[pos++] = (cyl >> 8);   /* R0 */
        dasd->cbuf[pos++] = (cyl & 0xff);
        dasd->cbuf[pos++] = (hd >> 8);
        dasd->cbuf[pos++] = (hd & 0xff);
        dasd->cbuf[pos++] = 0;              /* Rec */
        dasd->cbuf[pos++] = 0;              /* keylen */
        dasd->cbuf[pos++] = 0;              /* dlen */
        dasd->cbuf[pos++] = 8;              /*  */
        pos += 8;
        for (i = 1; i < 22; i++) {
            key++;
            dasd->cbuf[pos++] = (cyl >> 8);   /* R# */
            dasd->cbuf[pos++] = (cyl & 0xff);
            dasd->cbuf[pos++] = (hd >> 8);
            dasd->cbuf[pos++] = (hd & 0xff);
            dasd->cbuf[pos++] = i;            /* Rec */
            dasd->cbuf[pos++] = 4;              /* keylen */
            dasd->cbuf[pos++] = 1;              /* dlen */
            dasd->cbuf[pos++] = 4;              /*  */
            dasd->cbuf[pos++] = 0xf0 + (key / 1000);
            dasd->cbuf[pos++] = 0xf0 + ((key / 100) % 10);
            dasd->cbuf[pos++] = 0xf0 + ((key / 10) % 10);
            dasd->cbuf[pos++] = 0xf0 + ((key) % 10);
            dasd->cbuf[pos++] = 0xf0 + (key / 1000);
            dasd->cbuf[pos++] = 0xf0 + ((key / 100) % 10);
            dasd->cbuf[pos++] = 0xf0 + ((key / 10) % 10);
            dasd->cbuf[pos++] = 0xf0 + ((key) % 10);
            for (p = 0; p < 256; p++)
                dasd->cbuf[pos++] = p;
        }
        dasd->cbuf[pos++] = 0xff;           /* End record */
        dasd->cbuf[pos++] = 0xff;
        dasd->cbuf[pos++] = 0xff;
        dasd->cbuf[pos++] = 0xff;
        dasd->cbuf[pos++] = 0xff;
        dasd->cbuf[pos++] = 0xff;
        log_trace("Track len %d %d\n", dasd->tsize, pos);
        pos = cpos + dasd->tsize;
    }
    r = write(dasd->fd, dasd->cbuf, pos);
    if (r != pos) {
        log_error("Disk write on %s %d\n", dasd->file_name, r);
    }
    print_track(data->dev, 1);
    dasd->cyl = 0;
}

CTEST_TEARDOWN(disk_data) {
    struct _2844_context *ctx;
    int        i;
    log_trace("Cleanup\n");
    if (data->dev->dev) {
       ctx = (struct _2844_context *)(data->dev->dev);
//       ctx->cur_disk = NULL;
       for (i = 0; i < 8; i++) {
           if (ctx->disk[i] != NULL) {
               dasd_detach(ctx->disk[i]);
               free(ctx->disk[i]);
               ctx->disk[i] = NULL;
           }
       }
       free(data->dev->dev);
       del_disk((void*)ctx);
       data->dev->dev = NULL;
    }
    if (data->dev) {
       del_chan(data->dev, data->dev->addr);
       free(data->dev);
       data->dev = NULL;
    }
}

/* Try to read HA */
CTEST2(disk_data, readHA) {
    struct _2844_context *ctx = (struct _2844_context *)(data->dev->dev);
    static uint8_t  cmd[] = { 0, 0, 0, 10, 0, 4 };
    uint8_t     sense[6];
    uint8_t     ha[5];
    uint16_t    tags = 0;
    uint16_t    status;
    int         num;

    log_trace("Read HA test\n");
    ctx->disk[1]->cyl = 0;
    ctx->disk[1]->head = 0;
    ctx->disk[1]->cpos = 7000;
    status = initial_select(data->dev, &tags, 0x7);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = write_data(data->dev, &tags, &cmd[0], &num, 1);
    log_trace("Seek %02x %d\n", status, num);
    ASSERT_EQUAL_X(0x4, status);
    log_trace("Seek complete\n");
    log_trace("Wait2 done %02x\n", status);
    status = initial_select(data->dev, &tags, 0x1a);
    ASSERT_EQUAL_X(0x100, status);
    num = 5;
    status = read_data(data->dev, &tags, &ha[0], &num, 0);
    printf("HA %02x %d -> %02x %02x %02x %02x %02x\n", status, num, ha[0], ha[1],
            ha[2], ha[3], ha[4]);
    ASSERT_EQUAL_X(0x10c, status);
    status = initial_select(data->dev, &tags, 0x4);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = read_data(data->dev, &tags, &sense[0], &num, 0);
    printf("Sense %02x %d -> %02x %02x %02x %02x %02x %02x\n", status, num, sense[0], sense[1],
            sense[2], sense[3], sense[4], sense[5]);
    ASSERT_EQUAL(4, ctx->disk[1]->head);
    ASSERT_EQUAL(10, ctx->disk[1]->cyl);
    ASSERT_EQUAL_X(0x0, sense[0]);
    ASSERT_EQUAL_X(0x0, sense[1]);
    ASSERT_EQUAL_X(0x0, sense[2]);
    ASSERT_EQUAL_X(0x40, sense[3]);
    ASSERT_EQUAL_X(0x01, sense[4]);
    ASSERT_EQUAL_X(0x0, sense[5]);
    ASSERT_EQUAL_X(0x10c, status);
}

/* Try to read Track */
CTEST2(disk_data, readTrack) {
    struct _2844_context *ctx = (struct _2844_context *)(data->dev->dev);
    static uint8_t  cmd[] = { 0, 0, 0, 10, 0, 0 };
    uint8_t     sense[6];
    uint8_t     work[512];
    uint8_t     ha[5];
    uint16_t    tags = 0;
    uint16_t    status;
    int         num;
    int         i;
    int         rec1;

    log_trace("Read Track test\n");
    ctx->disk[1]->cyl = 0;
    ctx->disk[1]->head = 0;
    ctx->disk[1]->cpos = 7000;
    status = initial_select(data->dev, &tags, 0x7);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = write_data(data->dev, &tags, &cmd[0], &num, 1);
    log_trace("Seek %02x %d\n", status, num);
    ASSERT_EQUAL_X(0x4, status);
    log_trace("Seek complete\n");
    log_trace("Wait2 done %02x\n", status);
    status = initial_select(data->dev, &tags, 0x1a);
    ASSERT_EQUAL_X(0x100, status);
    num = 5;
    status = read_data(data->dev, &tags, &ha[0], &num, 1);
    printf("HA %02x %d -> %02x %02x %02x %02x %02x\n", status, num, ha[0], ha[1],
            ha[2], ha[3], ha[4]);
    ASSERT_EQUAL_X(0x10c, status);
    rec1 = 0;
    for (i = 0; i < 100; i++) {
        status = initial_select(data->dev, &tags, 0x1e);
        ASSERT_EQUAL_X(0x100, status);
        num = 256+8+8;
        status = read_data(data->dev, &tags, &work[0], &num, 1);
        printf("disk %02x %d -> %02x %02x %02x %02x %02x %02x %02x\n", status, num, work[7], work[8],
                work[9], work[10], work[11], work[12], work[13]);
        if (status != 0x10c)
            goto sense;
        if (work[4] == 01) {
            if (rec1)
                break;
            else
                rec1 = 1;
        }
    }
sense:
    status = initial_select(data->dev, &tags, 0x4);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = read_data(data->dev, &tags, &sense[0], &num, 0);
    printf("Sense %02x %d -> %02x %02x %02x %02x %02x %02x\n", status, num, sense[0], sense[1],
            sense[2], sense[3], sense[4], sense[5]);
    ASSERT_EQUAL(0, ctx->disk[1]->head);
    ASSERT_EQUAL(10, ctx->disk[1]->cyl);
    ASSERT_EQUAL_X(0x0, sense[0]);
    ASSERT_EQUAL_X(0x0, sense[1]);
    ASSERT_EQUAL_X(0x0, sense[2]);
    ASSERT_EQUAL_X(0x40, sense[3]);
    ASSERT_EQUAL_X(0x01, sense[4]);
    ASSERT_EQUAL_X(0x0, sense[5]);
    ASSERT_EQUAL_X(0x10c, status);
}

/* Try to read Cylinder */
CTEST2(disk_data, readCylinder) {
    struct _2844_context *ctx = (struct _2844_context *)(data->dev->dev);
    static uint8_t  cmd[] = { 0, 0, 0, 10, 0, 0 };
    uint8_t     sense[6];
    uint8_t     work[512];
    uint8_t     ha[5];
    uint16_t    tags = 0;
    uint16_t    status;
    int         num;
    int         i;

    log_trace("Read Track test\n");
    ctx->disk[1]->cyl = 0;
    ctx->disk[1]->head = 0;
    ctx->disk[1]->cpos = 3500;
    status = initial_select(data->dev, &tags, 0x7);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = write_data(data->dev, &tags, &cmd[0], &num, 1);
    log_trace("Seek %02x %d\n", status, num);
    ASSERT_EQUAL_X(0x4, status);
    log_trace("Seek complete\n");
    log_trace("Wait2 done %02x\n", status);
    status = initial_select(data->dev, &tags, 0x1a);
    ASSERT_EQUAL_X(0x100, status);
    num = 5;
    status = read_data(data->dev, &tags, &ha[0], &num, 1);
    printf("HA %02x %d -> %02x %02x %02x %02x %02x\n", status, num, ha[0], ha[1],
            ha[2], ha[3], ha[4]);
    ASSERT_EQUAL_X(0x10c, status);
    for (i = 0; i < 500; i++) {
        status = initial_select(data->dev, &tags, 0x9e);
        ASSERT_EQUAL_X(0x100, status);
        num = 256+8+8;
        status = read_data(data->dev, &tags, &work[0], &num, 1);
        printf("disk %02x %d h=%d -> %02x %02x %02x %02x %02x %02x %02x\n", status, num, 
                ctx->disk[1]->head, work[7], work[8], work[9], work[10], work[11], work[12],
                work[13]);
        if (status != 0x10c)
            goto sense;
    }
sense:
    status = initial_select(data->dev, &tags, 0x4);
    ASSERT_EQUAL_X(0x100, status);
    num = 6;
    status = read_data(data->dev, &tags, &sense[0], &num, 0);
    printf("Sense %02x %d -> %02x %02x %02x %02x %02x %02x\n", status, num, sense[0], sense[1],
            sense[2], sense[3], sense[4], sense[5]);
    ASSERT_EQUAL(19, ctx->disk[1]->head);
    ASSERT_EQUAL(10, ctx->disk[1]->cyl);
    ASSERT_EQUAL_X(0x0, sense[0]);
    ASSERT_EQUAL_X(0x20, sense[1]);
    ASSERT_EQUAL_X(0x0, sense[2]);
    ASSERT_EQUAL_X(0x40, sense[3]);
    ASSERT_EQUAL_X(0x01, sense[4]);
    ASSERT_EQUAL_X(0x0, sense[5]);
    ASSERT_EQUAL_X(0x10c, status);
}


