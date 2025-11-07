/*
 * microsim360 - Model 2844 disk controller test.
 *
 * Copyright 2025, Richard Cornwell
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
#include "model2844.h"

uint64_t   step_count = 0;

void
init_disk_tests()
{
    device_t *dev;
    struct _2844_context *ctx;
    int i;

    log_level = LOG_INFO|LOG_WARN|LOG_ERROR|LOG_TRACE|LOG_DEVICE|LOG_DISK|LOG_DMICRO|LOG_DREG;
    log_init("disk_debug.log");
    init_event();
    disk = NULL;
    dev = model2844_init(NULL, 0x90);
    ctx = (struct _2844_context *)(dev->dev);
    ctx->disk[1] = (struct _dasd_t *)calloc(1, sizeof(struct _dasd_t));
    dasd_settype(ctx->disk[1], "2314");
    dasd_attach(ctx->disk[1], "test.ckd", 1);
    dasd_format(ctx->disk[1], 1);

    ctx->WX = 0;
    for (i = 0; i < 20; i++) {
        step_2844(ctx);
        step_count++;
         if (ctx->WX == 0x5B6)
            break;
    }
}

void
print_bin(struct _device *dev, int unit)
{
    struct _2844_context *ctx = (struct _2844_context *)dev->dev;
    struct _dasd_t *dasd = ctx->disk[unit];
    int             pos;
    uint8_t         *rec;

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
    log_trace("Track: HA %02x %02x %02x %02x %02x\n", rec[0], rec[1], rec[2], rec[3], rec[4]);
    rec = &rec[5];
    for (i = 0; !end; i++) {
         klen = rec[5];
         dlen = (rec[6] << 8) | rec[7];
         if ((rec[0] & rec[1] & rec[2] & rec[3]) == 0xff) {
            log_trace("End\n");
            end = 1;
            klen = dlen = 0;
         } 
         if (klen == 0 && dlen == 0) {
            log_trace("EOF\n");
            end = 1;
         }
         log_trace("REC%d c=%02x%02x h=%02x%02x r=%02x k=%d d=%d\n", i, rec[0], rec[1], rec[2],
                 rec[3], rec[4], klen, dlen);
         rec += 8 + klen + dlen;
    }
}

void
test_advance()
{
    step_count++;
    step_disk();
    step_disk();
    advance();
}

CTEST_DATA(disk_test) {
    uint16_t       addr;
    struct _device *dev;
};

CTEST_SETUP(disk_test) {
     log_trace("Init test\n");
     data->addr = 0x91;
     data->dev = chan[0];
}

CTEST_TEARDOWN(disk_test) {
     log_trace("teardown test\n");
}


/* Try to send Test I/O to controller */
CTEST2(disk_test, test_io) {
    log_trace("TIO\n");
    ASSERT_EQUAL_X(0, test_io(data->addr));
}

/* Try to send Nop to controller */
CTEST2(disk_test, nop) {
     uint16_t status;

     log_trace("Nop\n");
     set_mem(0x40,  0xffffffff);         /* Set CSW to zero */
     set_mem(0x44,  0xffffffff);
     set_mem(0x48,  0x500);   /* Set CAW */
     set_mem(0x500, 0x03000600); /* Set channel words */
     set_mem(0x504, 0x00000001);
     set_mem(0x600, 0xffffffff);
     status = start_io(data->addr, 0x500, 1, 0);
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000001, get_mem(0x44));
     ASSERT_EQUAL_X(0xffFFFFFF, get_mem(0x600));
}

/* Try to send Set file mask to controller */
CTEST2(disk_test, sense) {
     uint16_t status;
     log_trace("Sense\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x04000600); /* Set channel words */
     set_mem(0x504, 0x00000006);
     set_mem(0x600, 0xffffffff);
     set_mem(0x604, 0xffffffff);
     status = start_io(data->addr, 0x500, 1, 0);
     printf("600=%08x 604=%08x 0x40=%08x %08x\n", get_mem(0x600), get_mem(0x604), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00000040, get_mem(0x600));
     ASSERT_EQUAL_X(0x0100ffff, get_mem(0x604));
}

/* Try to send Set file mask to controller */
CTEST2(disk_test, setmask) {
     uint16_t status;
    
     /* Test valid mask */
     log_trace("Set Mask\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x1f000600); /* Set channel words */
     set_mem(0x504, 0x00000001);
     set_mem(0x510, 0x04000600); /* Set channel words */
     set_mem(0x514, 0x00000006);
     set_mem(0x600, 0xc0000000);
     set_mem(0x604, 0xffffffff);
     status = start_io(data->addr, 0x500, 1, 0);
     printf("600=%08x 604=%08x 0x40=%08x %08x\n", get_mem(0x600), get_mem(0x604), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     /* Test Invalid mask */
     set_mem(0x600, 0xf0000000);
     status = start_io(data->addr, 0x500, 1, 0);
     printf("600=%08x 604=%08x 0x40=%08x %08x\n", get_mem(0x600), get_mem(0x604), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK, status);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0e000000, get_mem(0x44));
     set_mem(0x600, 0xffffffff);
     set_mem(0x604, 0xffffffff);
     status = start_io(data->addr, 0x510, 1, 0);
     printf("600=%08x 604=%08x 0x40=%08x %08x\n", get_mem(0x600), get_mem(0x604), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x80000040, get_mem(0x600));
     ASSERT_EQUAL_X(0x0100ffff, get_mem(0x604));
}


/* Try to send Seek to controller */
CTEST2(disk_test, seek) {
     struct _2844_context *ctx = (struct _2844_context *)(data->dev->dev);
     uint16_t status;

     log_trace("Seek\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x07000600); /* Set channel words */
     set_mem(0x504, 0x00000006);
     set_mem(0x510, 0x04000610); /* Set channel words */
     set_mem(0x514, 0x00000006);
     set_mem(0x600, 0x00000010);
     set_mem(0x604, 0x0005ffff);
     set_mem(0x610, 0xffffffff);
     set_mem(0x614, 0xffffffff);

     status = start_io(data->addr, 0x500, 1, 0);
     printf("600=%08x 604=%08x 0x40=%08x %08x\n", get_mem(0x600), get_mem(0x604), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x08000000, get_mem(0x44));
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     status = wait_dev(data->addr);
     ASSERT_EQUAL_X(SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x0400ffff, get_mem(0x44));
     ASSERT_EQUAL(5, ctx->disk[1]->head);
     ASSERT_EQUAL(0x10, ctx->disk[1]->cyl);

     status = start_io(data->addr, 0x510, 1, 0);
     printf("610=%08x 614=%08x 0x40=%08x %08x\n", get_mem(0x610), get_mem(0x614), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00000040, get_mem(0x610));
     ASSERT_EQUAL_X(0x0100ffff, get_mem(0x614));
}

/* Try to send Restore to controller */
CTEST2(disk_test, restore) {
     struct _2844_context *ctx = (struct _2844_context *)(data->dev->dev);
     uint16_t status;

     log_trace("Restore\n");
     ctx->disk[1]->cyl = 10;
     ctx->disk[1]->head = 8;
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x13000600); /* Set channel words */
     set_mem(0x504, 0x20000001);
     set_mem(0x510, 0x04000610); /* Set channel words */
     set_mem(0x514, 0x00000006);
     set_mem(0x600, 0x00000010);
     set_mem(0x604, 0x0005ffff);
     set_mem(0x610, 0xffffffff);
     set_mem(0x614, 0xffffffff);

     status = start_io(data->addr, 0x500, 1, 0);
     printf("600=%08x 604=%08x 0x40=%08x %08x\n", get_mem(0x600), get_mem(0x604), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000001, get_mem(0x44));
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);

     status = start_io(data->addr, 0x510, 1, 0);
     printf("610=%08x 614=%08x 0x40=%08x %08x\n", get_mem(0x610), get_mem(0x614), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00000040, get_mem(0x610));
     ASSERT_EQUAL_X(0x0100ffff, get_mem(0x614));
     ASSERT_EQUAL(0, ctx->disk[1]->head);
     ASSERT_EQUAL(0, ctx->disk[1]->cyl);
}

/* Try to read HA */
CTEST2(disk_test, readHA) {
     uint16_t status;

     log_trace("ReadHA\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x07000600); /* Set channel words */
     set_mem(0x504, 0x40000006);
     set_mem(0x508, 0x1a000610);
     set_mem(0x50c, 0x00000005);
     set_mem(0x510, 0x04000620); /* Set channel words */
     set_mem(0x514, 0x00000006);
     set_mem(0x600, 0x00000010);
     set_mem(0x604, 0x0005ffff);
     set_mem(0x610, 0xffffffff);
     set_mem(0x614, 0xffffffff);
     set_mem(0x620, 0xffffffff);
     set_mem(0x624, 0xffffffff);

     status = start_io(data->addr, 0x500, 1, 0);
     printf("610=%08x 614=%08x 0x40=%08x %08x\n", get_mem(0x610), get_mem(0x614), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000510, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00001000, get_mem(0x610));
     ASSERT_EQUAL_X(0x05ffffff, get_mem(0x614));
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);

     status = start_io(data->addr, 0x510, 1, 0);
     printf("620=%08x 624=%08x 0x40=%08x %08x\n", get_mem(0x620), get_mem(0x624), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00000040, get_mem(0x620));
     ASSERT_EQUAL_X(0x0100ffff, get_mem(0x624));
}

/* Try to read R0 */
CTEST2(disk_test, readR0) {
     uint16_t status;

     log_trace("Read R0\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x07000600); /* Set channel words */
     set_mem(0x504, 0x40000006);
     set_mem(0x508, 0x16000610);
     set_mem(0x50c, 0x00000010);
     set_mem(0x510, 0x04000620); /* Set channel words */
     set_mem(0x514, 0x00000006);
     set_mem(0x600, 0x00000010);
     set_mem(0x604, 0x0005ffff);
     set_mem(0x610, 0xffffffff);
     set_mem(0x614, 0xffffffff);
     set_mem(0x620, 0xffffffff);
     set_mem(0x624, 0xffffffff);

     status = start_io(data->addr, 0x500, 1, 0);
     printf("610=%08x 614=%08x ", get_mem(0x610), get_mem(0x614));
     printf("618=%08x 61c=%08x ", get_mem(0x618), get_mem(0x61c));
     printf("620=%08x 624=%08x 0x40=%08x %08x\n", get_mem(0x620), get_mem(0x624), get_mem(0x40), get_mem(0x44));
     printf("620=%08x 624=%08x 0x40=%08x %08x\n", get_mem(0x620), get_mem(0x624), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000510, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00100005, get_mem(0x610));
     ASSERT_EQUAL_X(0x00000008, get_mem(0x614));
     ASSERT_EQUAL_X(0x00000000, get_mem(0x618));
     ASSERT_EQUAL_X(0x00000000, get_mem(0x61c));
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     status = start_io(data->addr, 0x510, 1, 0);
     printf("620=%08x 624=%08x 0x40=%08x %08x\n", get_mem(0x620), get_mem(0x624), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00000040, get_mem(0x620));
     ASSERT_EQUAL_X(0x0100ffff, get_mem(0x624));
}

/* Read IPL record */
CTEST2(disk_test, read_ipl) {
     uint8_t     ipl_rec[] = { 0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x0F,
                              0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x01,
                              0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
     uint16_t status;
     int i;

     log_trace("Read IPL\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0x00000018);
     set_mem(0x510, 0x04000620); /* Set channel words */
     set_mem(0x514, 0x00000006);
     set_mem(0x600, 0xffffffff);
     set_mem(0x604, 0xffffffff);
     set_mem(0x608, 0xffffffff);
     set_mem(0x60c, 0xffffffff);
     set_mem(0x610, 0xffffffff);
     set_mem(0x614, 0xffffffff);
     set_mem(0x620, 0xffffffff);
     set_mem(0x624, 0xffffffff);

     status = start_io(data->addr, 0x500, 1, 0);
     printf("600=%08x 604=%08x ", get_mem(0x600), get_mem(0x604));
     printf("608=%08x 60c=%08x ", get_mem(0x608), get_mem(0x60c));
     printf("610=%08x 614=%08x 0x40=%08x %08x\n", get_mem(0x610), get_mem(0x614), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     for (i = 0; i < sizeof(ipl_rec); i++) {
        ASSERT_EQUAL_X(ipl_rec[i], get_mem_b(0x600+i));
        log_trace("Read %d: %02x\n", i, get_mem_b(0x600+i));
     }
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);

     status = start_io(data->addr, 0x510, 1, 0);
     printf("620=%08x 624=%08x 0x40=%08x %08x\n", get_mem(0x620), get_mem(0x624), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00000040, get_mem(0x620));
     ASSERT_EQUAL_X(0x0100ffff, get_mem(0x624));
}

/* Try to write to controller */
CTEST2(disk_test, writeHA) {
     static uint8_t  wha[] =  { 0, 1, 2,  3, 4};
     static uint8_t  orig[] = { 0, 0, 0x10, 0, 0x4};
     uint16_t status;
     int i;

     log_trace("Write HA\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x07000600); /* Set channel words */
     set_mem(0x504, 0x40000006);
     set_mem(0x508, 0x1f000608);
     set_mem(0x50c, 0x40000001);
     set_mem(0x510, 0x19000610); /* Set channel words */
     set_mem(0x514, 0x00000005);
     set_mem(0x520, 0x04000620); /* set channel words */
     set_mem(0x524, 0x00000006);
     set_mem(0x600, 0x00000010);
     set_mem(0x604, 0x0004ffff);
     set_mem(0x608, 0xc0ffffff);
     set_mem(0x60c, 0xffffffff);
     set_mem(0x610, 0xffffffff);
     set_mem(0x614, 0xffffffff);
     set_mem(0x620, 0xffffffff);
     set_mem(0x624, 0xffffffff);

     /* Set desired HA */
     for (i = 0; i < sizeof(wha); i++) {
          set_mem_b(0x610 + i, wha[i]);
     }
     status = start_io(data->addr, 0x500, 1, 0);
     printf("600=%08x 604=%08x ", get_mem(0x600), get_mem(0x604));
     printf("608=%08x 60c=%08x ", get_mem(0x608), get_mem(0x60c));
     printf("610=%08x 614=%08x 0x40=%08x %08x\n", get_mem(0x610), get_mem(0x614), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);

     status = start_io(data->addr, 0x520, 1, 0);
     if (status == 0x250) {  /* Controller Busy, wait for ready */
         status = wait_dev(data->addr & 0xf0);
         status = start_io(data->addr, 0x520, 1, 0);
     }
     printf("620=%08x 624=%08x 0x40=%08x %08x\n", get_mem(0x620), get_mem(0x624), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000528, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00000040, get_mem(0x620));
     ASSERT_EQUAL_X(0x0100ffff, get_mem(0x624));

     /* Read back in HA to verify */
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x07000600); /* Set channel words */
     set_mem(0x504, 0x40000006);
     set_mem(0x508, 0x1a000620);
     set_mem(0x50c, 0x00000005);
     set_mem(0x620, 0xffffffff);
     set_mem(0x624, 0xffffffff);

     status = start_io(data->addr, 0x500, 1, 0);
     printf("620=%08x 624=%08x 0x40=%08x %08x\n", get_mem(0x620), get_mem(0x624), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000510, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);

     /* Compare with original */
     for (i = 0; i < sizeof(wha); i++) {
        ASSERT_EQUAL_X(wha[i], get_mem_b(0x620+i));
        log_trace("Read %d: %02x\n", i, get_mem_b(0x620+i));
     }

     status = start_io(data->addr, 0x520, 1, 0);
     printf("620=%08x 624=%08x 0x40=%08x %08x\n", get_mem(0x620), get_mem(0x624), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000528, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00000040, get_mem(0x620));
     ASSERT_EQUAL_X(0x0100ffff, get_mem(0x624));


     log_trace("Restore HA\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x07000600); /* Set channel words */
     set_mem(0x504, 0x40000006);
     set_mem(0x508, 0x1f000608);
     set_mem(0x50c, 0x40000001);
     set_mem(0x510, 0x19000610); /* Set channel words */
     set_mem(0x514, 0x00000005);

     /* Set desired HA */
     for (i = 0; i < sizeof(orig); i++) {
          set_mem_b(0x610 + i, orig[i]);
     }
     status = start_io(data->addr, 0x500, 1, 0);
     printf("600=%08x 604=%08x ", get_mem(0x600), get_mem(0x604));
     printf("608=%08x 60c=%08x ", get_mem(0x608), get_mem(0x60c));
     printf("610=%08x 614=%08x 0x40=%08x %08x\n", get_mem(0x610), get_mem(0x614), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     status = start_io(data->addr, 0x520, 1, 0);
     if (status == 0x250) {  /* Controller Busy, wait for ready */
         status = wait_dev(data->addr & 0xf0);
         status = start_io(data->addr, 0x520, 1, 0);
     }
     printf("620=%08x 624=%08x 0x40=%08x %08x\n", get_mem(0x620), get_mem(0x624), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000528, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00000040, get_mem(0x620));
     ASSERT_EQUAL_X(0x0100ffff, get_mem(0x624));
}

/* Try to write R0 record */
CTEST2(disk_test, writeR0) {
     static uint8_t  ha[] = { 0, 0, 0x10, 0, 0x4};
                             /*    Cyl          head     rec   key     data len */
     static uint8_t  wr0[] = {  0,      10,    0,    4,    0,    0,    0, 8, 1, 2, 3, 4, 5, 6, 7, 8};
     static uint8_t  orig[] = { 0x00, 0x10, 0x00, 0x05, 0x00, 0x00,  0x00, 0x08,
                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
     uint16_t status;
     int i;

     log_trace("Write R0\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x07000600); /* Set channel words */
     set_mem(0x504, 0x40000006);
     set_mem(0x508, 0x1f000608);
     set_mem(0x50c, 0x40000001);
     set_mem(0x510, 0x15000620); /* Set channel words */
     set_mem(0x514, 0x00000005);
     set_mem(0x530, 0x04000630); /* set channel words */
     set_mem(0x534, 0x00000006);
     set_mem(0x600, 0x00000010);
     set_mem(0x604, 0x0004ffff);
     set_mem(0x608, 0xc0ffffff);
     set_mem(0x60c, 0xffffffff);
     set_mem(0x610, 0xffffffff);
     set_mem(0x614, 0xffffffff);
     set_mem(0x630, 0xffffffff);
     set_mem(0x634, 0xffffffff);

     /* Set desired R0 */
     for (i = 0; i < sizeof(wr0); i++) {
          set_mem_b(0x620 + i, wr0[i]);
     }
     set_mem(0x514, sizeof(wr0));

     /* Set desired HA */
     for (i = 0; i < sizeof(ha); i++) {
          set_mem_b(0x610 + i, ha[i]);
     }

     /* Should result in sequence error */
     status = start_io(data->addr, 0x500, 1, 0);
     printf("600=%08x 604=%08x ", get_mem(0x600), get_mem(0x604));
     printf("608=%08x 60c=%08x ", get_mem(0x608), get_mem(0x60c));
     printf("610=%08x 614=%08x 0x40=%08x %08x\n", get_mem(0x610), get_mem(0x614), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_UNITCHK, status);
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x02000010, get_mem(0x44));
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);

     status = start_io(data->addr, 0x530, 1, 0);
     if (status == 0x250) {  /* Controller Busy, wait for ready */
         status = wait_dev(data->addr & 0xf0);
         status = start_io(data->addr, 0x530, 1, 0);
     }
     printf("630=%08x 634=%08x 0x40=%08x %08x\n", get_mem(0x630), get_mem(0x634), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000538, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x80100040, get_mem(0x630));
     ASSERT_EQUAL_X(0x0100ffff, get_mem(0x634));

     set_mem(0x500, 0x07000600); /* Set channel words */
     set_mem(0x504, 0x40000006);
     set_mem(0x508, 0x1f000608);
     set_mem(0x50c, 0x40000001);
     set_mem(0x510, 0x19000610); /* Set channel words */
     set_mem(0x514, 0x40000005);
     set_mem(0x518, 0x15000620); /* Set channel words */
     set_mem(0x51c, 0x00000000);

     set_mem(0x51c, sizeof(wr0));

     /* Try again to write it correctly */
     status = start_io(data->addr, 0x500, 1, 0);
     printf("600=%08x 604=%08x ", get_mem(0x600), get_mem(0x604));
     printf("608=%08x 60c=%08x ", get_mem(0x608), get_mem(0x60c));
     printf("610=%08x 614=%08x 0x40=%08x %08x\n", get_mem(0x610), get_mem(0x614), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000520, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);

     status = start_io(data->addr, 0x530, 1, 0);
     if (status == 0x250) {  /* Controller Busy, wait for ready */
         status = wait_dev(data->addr & 0xf0);
         status = start_io(data->addr, 0x530, 1, 0);
     }
     printf("630=%08x 634=%08x 0x40=%08x %08x\n", get_mem(0x630), get_mem(0x634), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000538, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00000040, get_mem(0x630));
     ASSERT_EQUAL_X(0x0100ffff, get_mem(0x634));

     /* Write back correct one */
     for (i = 0; i < sizeof(orig); i++) {
          set_mem_b(0x620 + i, orig[i]);
     }
     set_mem(0x51c, sizeof(orig));

     status = start_io(data->addr, 0x500, 1, 0);
     printf("600=%08x 604=%08x ", get_mem(0x600), get_mem(0x604));
     printf("608=%08x 60c=%08x ", get_mem(0x608), get_mem(0x60c));
     printf("610=%08x 614=%08x 0x40=%08x %08x\n", get_mem(0x610), get_mem(0x614), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000520, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);

     status = start_io(data->addr, 0x530, 1, 0);
     if (status == 0x250) {  /* Controller Busy, wait for ready */
         status = wait_dev(data->addr & 0xf0);
         status = start_io(data->addr, 0x530, 1, 0);
     }
     printf("630=%08x 634=%08x 0x40=%08x %08x\n", get_mem(0x630), get_mem(0x634), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000538, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00000040, get_mem(0x630));
     ASSERT_EQUAL_X(0x0100ffff, get_mem(0x634));
}

/* Try to write track with 1 record */
CTEST2(disk_test, write_track_one) {
     static uint8_t  ha[] = { 0, 0, 0x10, 0, 0x5};
                              /*    Cyl          head     rec   key     data len */
     static uint8_t  wr0[] = { 0x00, 0x10, 0x00, 0x05, 0x00, 0x00,  0x00, 0x08,
                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
     static uint8_t  hdr[] = { 0x00, 0x10, 0x00, 0x05, 0x01, 0x08, 0x00, 0x20,
                               0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7};
     uint16_t status;
     int sz;
     int i;

     log_trace("Write record\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);

     set_mem(0x500, 0x07000600); /* Seek */
     set_mem(0x504, 0x40000006);
     set_mem(0x508, 0x1f000608); /* Set file mask */
     set_mem(0x50c, 0x40000001);
     set_mem(0x510, 0x19000610); /* Write Header */
     set_mem(0x514, 0x40000005);
     set_mem(0x518, 0x15000620); /* Write R0 */
     set_mem(0x51c, 0x40000000);
     set_mem(0x520, 0x1D000640); /* Write Record */
     set_mem(0x524, 0x00000000);
     set_mem(0x600, 0x00000010);
     set_mem(0x604, 0x0005ffff);
     set_mem(0x608, 0xc0ffffff);

     /* Set desired HA */
     for (i = 0; i < sizeof(ha); i++) {
          set_mem_b(0x610 + i, ha[i]);
     }

     /* Set desired R0 */
     for (i = 0; i < sizeof(wr0); i++) {
          set_mem_b(0x620 + i, wr0[i]);
     }
     set_mem(0x51c, 0x40000000 + sizeof(wr0));

     /* Make record */
     for (i = 0; i < sizeof(hdr); i++) {
          set_mem_b(0x640 + i, hdr[i]);
     }
     sz = hdr[7] + sizeof(hdr);;
     for (; i < (sz); i++) {
          set_mem_b(0x640 + i, i);
     }
     set_mem(0x524, sz);

     /* Try again to write it correctly */
     status = start_io(data->addr, 0x500, 1, 0);
     printf("600=%08x 604=%08x ", get_mem(0x600), get_mem(0x604));
     printf("608=%08x 60c=%08x ", get_mem(0x608), get_mem(0x60c));
     printf("610=%08x 614=%08x 0x40=%08x %08x\n", get_mem(0x610), get_mem(0x614), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000528, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);

     status = start_io(data->addr, 0x530, 1, 0);
     if (status == 0x250) {  /* Controller Busy, wait for ready */
         status = wait_dev(data->addr & 0xf0);
         status = start_io(data->addr, 0x530, 1, 0);
     }
     printf("630=%08x 634=%08x 0x40=%08x %08x\n", get_mem(0x630), get_mem(0x634), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000538, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00000040, get_mem(0x630));
     ASSERT_EQUAL_X(0x0100ffff, get_mem(0x634));
}

/* Search for a Home address */
CTEST2(disk_test, search_ha) {
     uint16_t status;

     set_mem(0x500, 0x07000600); /* Seek */
     set_mem(0x504, 0x40000006);
     set_mem(0x508, 0x1f000608); /* Set file mask */
     set_mem(0x50c, 0x40000001);
     set_mem(0x510, 0x39000610); /* Header to look for */
     set_mem(0x514, 0x00000004);
     set_mem(0x518, 0x08000510);
     set_mem(0x51c, 0x00000000);
     set_mem(0x530, 0x04000630); /* set channel words */
     set_mem(0x534, 0x00000006);
     set_mem(0x600, 0x00000010);
     set_mem(0x604, 0x0006ffff);
     set_mem(0x608, 0xc0ffffff);
     set_mem(0x60c, 0xffffffff);
     set_mem(0x610, 0x00100006);
     set_mem(0x614, 0xffffffff);
     set_mem(0x630, 0xffffffff);
     set_mem(0x634, 0xffffffff);

     /* Try to find it. */
     status = start_io(data->addr, 0x500, 1, 0);
     printf("0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_SMS|SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x4c000000, get_mem(0x44));
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);

     status = start_io(data->addr, 0x530, 1, 0);
     if (status == 0x250) {  /* Controller Busy, wait for ready */
         status = wait_dev(data->addr & 0xf0);
         status = start_io(data->addr, 0x530, 1, 0);
     }
     printf("630=%08x 634=%08x 0x40=%08x %08x\n", get_mem(0x630), get_mem(0x634), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000538, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00000040, get_mem(0x630));
     ASSERT_EQUAL_X(0x0100ffff, get_mem(0x634));

     /* Set incorrect HA */
     set_mem(0x610, 0x00110004);
     status = start_io(data->addr, 0x500, 1, 0);
     printf("0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);

     status = start_io(data->addr, 0x530, 1, 0);
     if (status == 0x250) {  /* Controller Busy, wait for ready */
         status = wait_dev(data->addr & 0xf0);
         status = start_io(data->addr, 0x530, 1, 0);
     }
     printf("630=%08x 634=%08x 0x40=%08x %08x\n", get_mem(0x630), get_mem(0x634), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000538, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00000040, get_mem(0x630));
     ASSERT_EQUAL_X(0x0100ffff, get_mem(0x634));
}

/* Search for a Home address */
CTEST2(disk_test, search_ha_mt) {
     uint16_t status;

     set_mem(0x500, 0x07000600); /* Seek */
     set_mem(0x504, 0x40000006);
     set_mem(0x508, 0x1f000608); /* Set file mask */
     set_mem(0x50c, 0x40000001);
     set_mem(0x510, 0xb9000610); /* Header to look for */
     set_mem(0x514, 0x40000004);
     set_mem(0x518, 0x08000510);
     set_mem(0x51c, 0x00000000);
     set_mem(0x520, 0x03000610); /* Terminate chain */
     set_mem(0x524, 0x00000001);
     set_mem(0x530, 0x04000630); /* set channel words */
     set_mem(0x534, 0x00000006);
     set_mem(0x600, 0x00000010);
     set_mem(0x604, 0x0000ffff);
     set_mem(0x608, 0xc0ffffff);
     set_mem(0x60c, 0xffffffff);
     set_mem(0x610, 0x00100006);
     set_mem(0x614, 0xffffffff);
     set_mem(0x630, 0xffffffff);
     set_mem(0x634, 0xffffffff);

     /* Try to find it. */
     status = start_io(data->addr, 0x500, 1, 0);
     printf("0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000528, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000001, get_mem(0x44));
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);

     status = start_io(data->addr, 0x530, 1, 0);
     if (status == 0x250) {  /* Controller Busy, wait for ready */
         status = wait_dev(data->addr & 0xf0);
         status = start_io(data->addr, 0x530, 1, 0);
     }
     printf("630=%08x 634=%08x 0x40=%08x %08x\n", get_mem(0x630), get_mem(0x634), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000538, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00000040, get_mem(0x630));
     ASSERT_EQUAL_X(0x0100ffff, get_mem(0x634));

     /* Set incorrect HA */
     set_mem(0x610, 0x00110004);
     status = start_io(data->addr, 0x500, 1, 0);
     printf("0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND|SNS_UNITCHK, status);
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0e400004, get_mem(0x44));
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);

     status = start_io(data->addr, 0x530, 1, 0);
     if (status == 0x250) {  /* Controller Busy, wait for ready */
         status = wait_dev(data->addr & 0xf0);
         status = start_io(data->addr, 0x530, 1, 0);
     }
     printf("630=%08x 634=%08x 0x40=%08x %08x\n", get_mem(0x630), get_mem(0x634), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000538, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00200040, get_mem(0x630));
     ASSERT_EQUAL_X(0x0100ffff, get_mem(0x634));
}

/* Read Count key and data */
CTEST2(disk_test, read_ckd) {
                              /*    Cyl          head     rec   key     data len */
     static uint8_t  hdr[] = { 0x00, 0x10, 0x00, 0x05, 0x01, 0x08, 0x00, 0x20,
                               0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7};
     uint16_t status;
     int i;

     log_trace("Read record\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);

     set_mem(0x500, 0x07000600); /* Seek */
     set_mem(0x504, 0x40000006);
     set_mem(0x508, 0x1f000608); /* Set file mask */
     set_mem(0x50c, 0x40000001);
     set_mem(0x510, 0x39000610); /* Header to look for */
     set_mem(0x514, 0x40000004);
     set_mem(0x518, 0x08000510);
     set_mem(0x51c, 0x00000000);
     set_mem(0x520, 0x1e000640); /* Terminate chain */
     set_mem(0x524, 0x00000000);
     set_mem(0x530, 0x04000700); /* set channel words */
     set_mem(0x534, 0x00000006);
     set_mem(0x600, 0x00000010);
     set_mem(0x604, 0x0005ffff);
     set_mem(0x608, 0xc0ffffff);
     set_mem(0x60c, 0xffffffff);
     set_mem(0x610, 0x00100005);
     set_mem(0x614, 0xffffffff);
     set_mem(0x620, 0x00000010);
     set_mem(0x624, 0x0005ffff);
     set_mem(0x628, 0xc0ffffff);
     set_mem(0x62c, 0xffffffff);
     set_mem(0x700, 0xffffffff);
     set_mem(0x704, 0xffffffff);
     for(i = 0x640; i < 0x700; i += 4) {
         set_mem(i, 0xffffffff);
     }

     set_mem(0x524, sizeof(hdr) + 0x20);
     /* Try to find it. */
     status = start_io(data->addr, 0x500, 1, 0);
     for (i = 0; i < 0x30; i+= 4) {
         printf("%04x=%08x ", i + 0x640, get_mem(0x640 + i));
     }
     printf("0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000528, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);

     status = start_io(data->addr, 0x530, 1, 0);
     if (status == 0x250) {  /* Controller Busy, wait for ready */
         status = wait_dev(data->addr & 0xf0);
         status = start_io(data->addr, 0x530, 1, 0);
     }
     printf("700=%08x 704=%08x 0x40=%08x %08x\n", get_mem(0x700), get_mem(0x704), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000538, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00000040, get_mem(0x700));
     ASSERT_EQUAL_X(0x0100ffff, get_mem(0x704));

     /* Compare with original */
     for (i = 0; i < sizeof(hdr); i++) {
        ASSERT_EQUAL_X(hdr[i], get_mem_b(0x640+i));
        log_trace("Read %d: %02x\n", i, get_mem_b(0x640+i));
     }
     for (; i < (sizeof(hdr) + 0x20); i++) {
        ASSERT_EQUAL_X(i, get_mem_b(0x640+i));
        log_trace("Read %d: %02x\n", i, get_mem_b(0x640+i));
     }

}

/* Read key and data */
CTEST2(disk_test, read_kd) {
                              /*    Cyl          head     rec   key     data len */
     static uint8_t  hdr[] = { 0x00, 0x10, 0x00, 0x05, 0x01, 0x08, 0x00, 0x20,
                               0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7};
     uint16_t status;
     int i;

     log_trace("Read record\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);

     set_mem(0x500, 0x07000600); /* Seek */
     set_mem(0x504, 0x40000006);
     set_mem(0x508, 0x1f000608); /* Set file mask */
     set_mem(0x50c, 0x40000001);
     set_mem(0x510, 0x39000610); /* Header to look for */
     set_mem(0x514, 0x40000004);
     set_mem(0x518, 0x08000510);
     set_mem(0x51c, 0x00000000);
     set_mem(0x520, 0x0e000640); /* Terminate chain */
     set_mem(0x524, 0x00000000);
     set_mem(0x530, 0x04000700); /* set channel words */
     set_mem(0x534, 0x00000006);
     set_mem(0x600, 0x00000010);
     set_mem(0x604, 0x0005ffff);
     set_mem(0x608, 0xc0ffffff);
     set_mem(0x60c, 0xffffffff);
     set_mem(0x610, 0x00100005);
     set_mem(0x614, 0xffffffff);
     set_mem(0x620, 0x00000010);
     set_mem(0x624, 0x0005ffff);
     set_mem(0x628, 0xc0ffffff);
     set_mem(0x62c, 0xffffffff);
     set_mem(0x700, 0xffffffff);
     set_mem(0x704, 0xffffffff);
     for(i = 0x640; i < 0x700; i += 4) {
         set_mem(i, 0xffffffff);
     }

     set_mem(0x524, sizeof(hdr) + 0x20 - 8);
     /* Try to find it. */
     status = start_io(data->addr, 0x500, 1, 0);
     for (i = 0; i < 0x30; i+= 4) {
         printf("%04x=%08x ", i + 0x640, get_mem(0x640 + i));
     }
     printf("0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000528, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);

     status = start_io(data->addr, 0x530, 1, 0);
     if (status == 0x250) {  /* Controller Busy, wait for ready */
         status = wait_dev(data->addr & 0xf0);
         status = start_io(data->addr, 0x530, 1, 0);
     }
     printf("700=%08x 704=%08x 0x40=%08x %08x\n", get_mem(0x700), get_mem(0x704), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000538, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00000040, get_mem(0x700));
     ASSERT_EQUAL_X(0x0100ffff, get_mem(0x704));

     /* Compare with original */
     for (i = 8; i < sizeof(hdr); i++) {
        ASSERT_EQUAL_X(hdr[i], get_mem_b(0x638+i));
        log_trace("Read %d: %02x\n", i, get_mem_b(0x638+i));
     }
     for (; i < (sizeof(hdr) + 0x20); i++) {
        ASSERT_EQUAL_X(i, get_mem_b(0x638+i));
        log_trace("Read %d: %02x\n", i, get_mem_b(0x638+i));
     }

}

/* Read data */
CTEST2(disk_test, read_d) {
                              /*    Cyl          head     rec   key     data len */
     static uint8_t  hdr[] = { 0x00, 0x10, 0x00, 0x05, 0x01, 0x08, 0x00, 0x20,
                               0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7};
     uint16_t status;
     int sz;
     int i;

     log_trace("Read record\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);

     set_mem(0x500, 0x07000600); /* Seek */
     set_mem(0x504, 0x40000006);
     set_mem(0x508, 0x1f000608); /* Set file mask */
     set_mem(0x50c, 0x40000001);
     set_mem(0x510, 0x39000610); /* Header to look for */
     set_mem(0x514, 0x40000004);
     set_mem(0x518, 0x08000510);
     set_mem(0x51c, 0x00000000);
     set_mem(0x520, 0x06000640); /* Terminate chain */
     set_mem(0x524, 0x00000000);
     set_mem(0x530, 0x04000700); /* set channel words */
     set_mem(0x534, 0x00000006);
     set_mem(0x600, 0x00000010);
     set_mem(0x604, 0x0005ffff);
     set_mem(0x608, 0xc0ffffff);
     set_mem(0x60c, 0xffffffff);
     set_mem(0x610, 0x00100005);
     set_mem(0x614, 0xffffffff);
     set_mem(0x620, 0x00000010);
     set_mem(0x624, 0x0005ffff);
     set_mem(0x628, 0xc0ffffff);
     set_mem(0x62c, 0xffffffff);
     set_mem(0x700, 0xffffffff);
     set_mem(0x704, 0xffffffff);
     for(i = 0x640; i < 0x700; i += 4) {
         set_mem(i, 0xffffffff);
     }

     set_mem(0x524, 0x20);
     /* Try to find it. */
     status = start_io(data->addr, 0x500, 1, 0);
     for (i = 0; i < 0x30; i+= 4) {
         printf("%04x=%08x ", i + 0x640, get_mem(0x640 + i));
     }
     printf("0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000528, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);

     status = start_io(data->addr, 0x530, 1, 0);
     if (status == 0x250) {  /* Controller Busy, wait for ready */
         status = wait_dev(data->addr & 0xf0);
         status = start_io(data->addr, 0x530, 1, 0);
     }
     printf("700=%08x 704=%08x 0x40=%08x %08x\n", get_mem(0x700), get_mem(0x704), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000538, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00000040, get_mem(0x700));
     ASSERT_EQUAL_X(0x0100ffff, get_mem(0x704));

     /* Compare with original */
     for (i = 0; i < 0x20; i++) {
        ASSERT_EQUAL_X(i + 0x10, get_mem_b(0x640+i));
        log_trace("Read %d: %02x\n", i-sizeof(hdr), get_mem_b(0x640+i));
     }
     /* Compare with original */
     sz = get_mem_b(0x647) + sizeof(hdr);
     for (i = sizeof(hdr); i < (sizeof(hdr) + 0x20); i++) {
        ASSERT_EQUAL_X(i, get_mem_b(0x630+i));
        log_trace("Read %d: %02x\n", i, get_mem_b(0x630+i));
     }
}

/* Write data */
CTEST2(disk_test, write_d) {
                              /*    Cyl          head     rec   key     data len */
     static uint8_t  hdr[] = { 0x00, 0x10, 0x00, 0x05, 0x01, 0x08, 0x00, 0x20,
                               0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7};
     uint16_t status;
     int i;

     log_trace("Write record\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);

     set_mem(0x500, 0x07000600); /* Seek */
     set_mem(0x504, 0x40000006);
     set_mem(0x508, 0x1f000608); /* Set file mask */
     set_mem(0x50c, 0x40000001);
     set_mem(0x510, 0x3100062c); /* Header to look for */
     set_mem(0x514, 0x40000005);
     set_mem(0x518, 0x08000510);
     set_mem(0x51c, 0x00000000);
     set_mem(0x520, 0x05000640); /* Terminate chain */
     set_mem(0x524, 0x00000020);
     set_mem(0x530, 0x04000700); /* set channel words */
     set_mem(0x534, 0x00000006);
     set_mem(0x600, 0x00000010);
     set_mem(0x604, 0x0005ffff);
     set_mem(0x608, 0xc0ffffff);
     set_mem(0x60c, 0xffffffff);
     set_mem(0x610, 0x00100005);
     set_mem(0x614, 0xffffffff);
     set_mem(0x620, 0x00000010);
     set_mem(0x624, 0x0005ffff);
     set_mem(0x628, 0xc0ffffff);
     set_mem(0x62c, 0x00100005);
     set_mem(0x630, 0x01ffffff);
     set_mem(0x700, 0xffffffff);
     set_mem(0x704, 0xffffffff);

     for (i = 0; i < 0x20; i++) {
          set_mem_b(0x640 + i, i);
     }
     /* Try to find it. */
     status = start_io(data->addr, 0x500, 1, 0);
     printf("0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000528, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);

     status = start_io(data->addr, 0x530, 1, 0);
     if (status == 0x250) {  /* Controller Busy, wait for ready */
         status = wait_dev(data->addr & 0xf0);
         status = start_io(data->addr, 0x530, 1, 0);
     }
     printf("700=%08x 704=%08x 0x40=%08x %08x\n", get_mem(0x700), get_mem(0x704), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000538, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00000040, get_mem(0x700));
     ASSERT_EQUAL_X(0x0100ffff, get_mem(0x704));

     log_trace("Read record\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);

     set_mem(0x500, 0x07000600); /* Seek */
     set_mem(0x504, 0x40000006);
     set_mem(0x508, 0x1f000608); /* Set file mask */
     set_mem(0x50c, 0x40000001);
     set_mem(0x510, 0x39000610); /* Header to look for */
     set_mem(0x514, 0x40000004);
     set_mem(0x518, 0x08000510);
     set_mem(0x51c, 0x00000000);
     set_mem(0x520, 0x1e000640); /* Terminate chain */
     set_mem(0x524, 0x00000000);
     set_mem(0x530, 0x04000700); /* set channel words */
     set_mem(0x534, 0x00000006);
     set_mem(0x600, 0x00000010);
     set_mem(0x604, 0x0005ffff);
     set_mem(0x608, 0xc0ffffff);
     set_mem(0x60c, 0xffffffff);
     set_mem(0x610, 0x00100005);
     set_mem(0x614, 0xffffffff);
     set_mem(0x620, 0x00000010);
     set_mem(0x624, 0x0005ffff);
     set_mem(0x628, 0xc0ffffff);
     set_mem(0x62c, 0xffffffff);
     set_mem(0x700, 0xffffffff);
     set_mem(0x704, 0xffffffff);
     for(i = 0x640; i < 0x700; i += 4) {
         set_mem(i, 0xffffffff);
     }

     set_mem(0x524, sizeof(hdr) + 0x20);
     /* Try to find it. */
     status = start_io(data->addr, 0x500, 1, 0);
     for (i = 0; i < 0x30; i+= 4) {
         printf("%04x=%08x ", i + 0x640, get_mem(0x640 + i));
     }
     printf("0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000528, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);

     status = start_io(data->addr, 0x530, 1, 0);
     if (status == 0x250) {  /* Controller Busy, wait for ready */
         status = wait_dev(data->addr & 0xf0);
         status = start_io(data->addr, 0x530, 1, 0);
     }
     printf("700=%08x 704=%08x 0x40=%08x %08x\n", get_mem(0x700), get_mem(0x704), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000538, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00000040, get_mem(0x700));
     ASSERT_EQUAL_X(0x0100ffff, get_mem(0x704));

     /* Compare with original */
     for (; i < 0x20; i++) {
        ASSERT_EQUAL_X(i, get_mem_b(0x640+i));
        log_trace("Read %d: %02x\n", i-0x20, get_mem_b(0x640+i));
     }
}

/* Write key + data */
CTEST2(disk_test, write_kd) {
                              /*    Cyl          head     rec   key     data len */
     static uint8_t  hdr[] = { 0x00, 0x10, 0x00, 0x05, 0x01, 0x08, 0x00, 0x20,
                               0xf7, 0xf6, 0xf5, 0xf4, 0xf3, 0xf2, 0xf1, 0xf0};
     uint16_t status;
     int i;

     log_trace("Write record\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);

     set_mem(0x500, 0x07000600); /* Seek */
     set_mem(0x504, 0x40000006);
     set_mem(0x508, 0x1f000608); /* Set file mask */
     set_mem(0x50c, 0x40000001);
     set_mem(0x510, 0x3100062c); /* Header to look for */
     set_mem(0x514, 0x40000005);
     set_mem(0x518, 0x08000510);
     set_mem(0x51c, 0x00000000);
     set_mem(0x520, 0x0d000648); /* Terminate chain */
     set_mem(0x524, 0x00000028);
     set_mem(0x530, 0x04000700); /* set channel words */
     set_mem(0x534, 0x00000006);
     set_mem(0x600, 0x00000010);
     set_mem(0x604, 0x0005ffff);
     set_mem(0x608, 0xc0ffffff);
     set_mem(0x60c, 0xffffffff);
     set_mem(0x610, 0x00100005);
     set_mem(0x614, 0xffffffff);
     set_mem(0x620, 0x00000010);
     set_mem(0x624, 0x0005ffff);
     set_mem(0x628, 0xc0ffffff);
     set_mem(0x62c, 0x00100005);
     set_mem(0x630, 0x01ffffff);
     set_mem(0x700, 0xffffffff);
     set_mem(0x704, 0xffffffff);

     for (i = 8; i < sizeof(hdr); i++) {
          set_mem_b(0x640 + i, hdr[i]);
     }

     for (; i < sizeof(hdr) + 0x20; i++) {
          set_mem_b(0x640 + i, 0xf0 ^ i);
     }
     /* Try to find it. */
     status = start_io(data->addr, 0x500, 1, 0);
     printf("0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000528, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);

     status = start_io(data->addr, 0x530, 1, 0);
     if (status == 0x250) {  /* Controller Busy, wait for ready */
         status = wait_dev(data->addr & 0xf0);
         status = start_io(data->addr, 0x530, 1, 0);
     }
     printf("700=%08x 704=%08x 0x40=%08x %08x\n", get_mem(0x700), get_mem(0x704), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000538, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00000040, get_mem(0x700));
     ASSERT_EQUAL_X(0x0100ffff, get_mem(0x704));

     log_trace("Read record\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);

     set_mem(0x500, 0x07000600); /* Seek */
     set_mem(0x504, 0x40000006);
     set_mem(0x508, 0x1f000608); /* Set file mask */
     set_mem(0x50c, 0x40000001);
     set_mem(0x510, 0x39000610); /* Header to look for */
     set_mem(0x514, 0x40000004);
     set_mem(0x518, 0x08000510);
     set_mem(0x51c, 0x00000000);
     set_mem(0x520, 0x1e000640); /* Terminate chain */
     set_mem(0x524, 0x00000000);
     set_mem(0x530, 0x04000700); /* set channel words */
     set_mem(0x534, 0x00000006);
     set_mem(0x600, 0x00000010);
     set_mem(0x604, 0x0005ffff);
     set_mem(0x608, 0xc0ffffff);
     set_mem(0x60c, 0xffffffff);
     set_mem(0x610, 0x00100005);
     set_mem(0x614, 0xffffffff);
     set_mem(0x620, 0x00000010);
     set_mem(0x624, 0x0005ffff);
     set_mem(0x628, 0xc0ffffff);
     set_mem(0x62c, 0xffffffff);
     set_mem(0x700, 0xffffffff);
     set_mem(0x704, 0xffffffff);
     for(i = 0x640; i < 0x700; i += 4) {
         set_mem(i, 0xffffffff);
     }

     set_mem(0x524, sizeof(hdr) + 0x20);
     /* Try to find it. */
     status = start_io(data->addr, 0x500, 1, 0);
     for (i = 0; i < 0x30; i+= 4) {
         printf("%04x=%08x ", i + 0x640, get_mem(0x640 + i));
     }
     printf("0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000528, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);

     status = start_io(data->addr, 0x530, 1, 0);
     if (status == 0x250) {  /* Controller Busy, wait for ready */
         status = wait_dev(data->addr & 0xf0);
         status = start_io(data->addr, 0x530, 1, 0);
     }
     printf("700=%08x 704=%08x 0x40=%08x %08x\n", get_mem(0x700), get_mem(0x704), get_mem(0x40), get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000538, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00000040, get_mem(0x700));
     ASSERT_EQUAL_X(0x0100ffff, get_mem(0x704));

     /* Compare with original */
     for (i = 0; i < sizeof(hdr); i++) {
        ASSERT_EQUAL_X(hdr[i], get_mem_b(0x640+i));
        log_trace("Read %d: %02x\n", i, get_mem_b(0x640+i));
     }
     for (; i < sizeof(hdr) + 0x20; i++) {
        ASSERT_EQUAL_X(0xf0 ^ i, get_mem_b(0x640+i));
        log_trace("Read %d: %02x\n", i-0x20, get_mem_b(0x640+i));
     }
}
#if 0
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

#endif
