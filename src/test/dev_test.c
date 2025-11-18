/*
 * microsim360 - Test test device.
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

#include <math.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "ctest.h"
#include "logger.h"
#include "device.h"
#include "test_chan.h"
#include "test_device.h"

uint64_t  step_count;

int       verbose = 0;

char     *test_log_file = "debug_device.log";
char     *test_log_level = "info warn error trace device";

void
init_tests()
{
}

void
test_advance()
{
    device_t  *dev;
    for (dev = chan[0]; dev != NULL; dev = dev->next) {
       test_step(dev);
   }
   step_count++;
}


CTEST_DATA(dev_test) {
    struct _device dev;
    struct _test_context test_ctx;
};

CTEST_SETUP(dev_test) {
    data->dev.bus_func = &test_dev;
    data->dev.dev = (void *)&data->test_ctx;
    data->dev.addr = 0xf;
    data->dev.next = NULL;
    data->test_ctx.burst_max = 256;
    data->test_ctx.cmd = 0;
    data->test_ctx.cmd_done = 0;
    data->test_ctx.data_rdy = 0;
    data->test_ctx.data_end = 0;
    data->test_ctx.status = 0;
    chan[0] = &data->dev;
    log_trace("Test IO Setup\n");
}

/* Test IO to valid device */
CTEST2(dev_test, tio) {
     log_trace("Test IO\n");
     ASSERT_EQUAL_X(0, test_io(data->dev.addr));
}

CTEST2(dev_test, read) {
     int      i;
     uint16_t status;

     log_trace("Read\n");
     for (i = 0; i < 0x10; i++) {
         data->test_ctx.buffer[i] = 0xf0 + i;
     }
     data->test_ctx.max_data = 0x10;
     data->test_ctx.burst = 1;
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0x00000010);
     set_mem(0x600, 0x55555555); /* Invalidate data */
     set_mem(0x604, 0x55555555);
     set_mem(0x608, 0x55555555);
     set_mem(0x60C, 0x55555555);

     status = start_io(data->dev.addr, 0x500, 0, 0);
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0xf0 + i, get_mem_b(0x600 + i));
     }
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
}

CTEST2(dev_test, write) {
     int i;
     uint16_t status;
     log_trace("Write\n");
     for (i = 0; i < 0x10; i++) {
         data->test_ctx.buffer[i] = 0x55;
     }
     data->test_ctx.max_data = 0x10;
     data->test_ctx.burst = 0;
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x01000600); /* Set channel words */
     set_mem(0x504, 0x00000010);
     set_mem(0x600, 0x0F1F2F3F); /* Data to send */
     set_mem(0x604, 0x4F5F6F7F);
     set_mem(0x608, 0x8F9FAFBF);
     set_mem(0x60C, 0xCFDFEFFF);

     status = start_io(data->dev.addr, 0x500, 0, 0);
     if (verbose) {
         for (i = 0; i < 0x10; i++) {
             printf(" %02x", data->test_ctx.buffer[i]);
         }
     }

     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x0f + (i << 4), data->test_ctx.buffer[i]);
     }
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
}


CTEST2(dev_test, sense1) {
     uint16_t status;
     log_trace("Sense\n");
     data->test_ctx.state = 0;
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x04000600); /* Set channel words */
     set_mem(0x504, 0x00000001);
     set_mem(0x600, 0xffffffff);
     status = start_io(data->dev.addr, 0x500, 0, 0);
     if (verbose) {
         printf("600=%08x  0x40=%08x %08x\n", get_mem(0x600),
                 get_mem(0x40), get_mem(0x44));
     }
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00FFFFFF, get_mem(0x600));
}

CTEST2(dev_test, read_back) {
     int      i;
     uint16_t status;
     log_trace("Read back\n");
     for (i = 0; i < 0x10; i++) {
         data->test_ctx.buffer[i] = 0x10 + (0x0f - i);
     }
     data->test_ctx.state = 0;
     data->test_ctx.max_data = 0x10;
     data->test_ctx.burst = 1;
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x0c00060f); /* Set channel words */
     set_mem(0x504, 0x00000010);
     set_mem(0x600, 0x55555555); /* Invalidate data */
     set_mem(0x604, 0x55555555);
     set_mem(0x608, 0x55555555);
     set_mem(0x60C, 0x55555555);
     set_mem(0x610, 0x55555555);

     status = start_io(data->dev.addr, 0x500, 0, 0);
     if (verbose) {
         printf(" 0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
         printf("0x600 = %08x %08x %08x %08x %08x\n", get_mem(0x600),
             get_mem(0x604), get_mem(0x608), get_mem(0x60c), get_mem(0x610));
     }
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x10 + i, get_mem_b(0x600 + i));
     }
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
}


CTEST2(dev_test, sio6_nop) {
     uint16_t status;
     log_trace("Nop\n");
     set_mem(0x40,  0xffffffff);         /* Set CSW to zero */
     set_mem(0x44,  0xffffffff);
     set_mem(0x48,  0x500);   /* Set CAW */
     set_mem(0x500, 0x03000600); /* Set channel words */
     set_mem(0x504, 0x00000001);
     set_mem(0x600, 0xffffffff);
     status = start_io(data->dev.addr, 0x500, 0, 0);
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000001, get_mem(0x44));
     ASSERT_EQUAL_X(0xffFFFFFF, get_mem(0x600));
}

CTEST2(dev_test, sio6_ce_only) {
     uint16_t status;
     log_trace("CE TEST\n");
     set_mem(0x40,  0xffffffff);         /* Set CSW to zero */
     set_mem(0x44,  0xffffffff);
     set_mem(0x500, 0x13000600); /* Set channel words */
     set_mem(0x504, 0x00000001);
     set_mem(0x600, 0xffffffff);
     status = start_io(data->dev.addr, 0x500, 0, 0);
     ASSERT_EQUAL_X(SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x08000001, get_mem(0x44));
     set_mem(0x40,  0xffffffff);         /* Set CSW to zero */
     set_mem(0x44,  0xffffffff);
     status = wait_dev(data->dev.addr);
     ASSERT_EQUAL_X(SNS_DEVEND, status);
     ASSERT_EQUAL_X(0xffffffff, get_mem(0x40));
     ASSERT_EQUAL_X(0x0400ffff, get_mem(0x44));
}


CTEST2(dev_test, short_read) {
     int i;
     uint16_t status;
     for (i = 0; i < 0x20; i++) {
         data->test_ctx.buffer[i] = 0x10 + i;
     }
     data->test_ctx.max_data = 0x20;
     data->test_ctx.burst = 1;
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0x00000010);
     set_mem(0x600, 0x55555555); /* Invalidate data */
     set_mem(0x604, 0x55555555);
     set_mem(0x608, 0x55555555);
     set_mem(0x60C, 0x55555555);
     set_mem(0x700, 0x55555555); /* Invalidate data */
     set_mem(0x704, 0x55555555);
     set_mem(0x708, 0x55555555);
     set_mem(0x70C, 0x55555555);

     status = start_io(data->dev.addr, 0x500, 0, 0);
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x10 + i, get_mem_b(0x600 + i));
     }
     for (; i < 0x20; i++) {
          ASSERT_EQUAL_X(0x55, get_mem_b(0x700 + i - 0x10));
     }
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c400000, get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
}

CTEST2(dev_test, short_read_sli) {
     int i;
     uint16_t status;
     for (i = 0; i < 0x20; i++) {
         data->test_ctx.buffer[i] = 0x10 + i;
     }
     data->test_ctx.max_data = 0x20;
     data->test_ctx.burst = 1;
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0x20000010);
     set_mem(0x600, 0x55555555); /* Invalidate data */
     set_mem(0x604, 0x55555555);
     set_mem(0x608, 0x55555555);
     set_mem(0x60C, 0x55555555);
     set_mem(0x700, 0x55555555); /* Invalidate data */
     set_mem(0x704, 0x55555555);
     set_mem(0x708, 0x55555555);
     set_mem(0x70C, 0x55555555);

     status = start_io(data->dev.addr, 0x500, 0, 0);
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x10 + i, get_mem_b(0x600 + i));
     }
     for (; i < 0x20; i++) {
          ASSERT_EQUAL_X(0x55, get_mem_b(0x700 + i - 0x10));
     }
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
}


CTEST2(dev_test, short_write) {
     int i;
     uint16_t status;
     for (i = 0; i < 0x20; i++) {
         data->test_ctx.buffer[i] = 0x55;
     }
     data->test_ctx.max_data = 0x20;
     data->test_ctx.burst = 1;
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x01000600); /* Set channel words */
     set_mem(0x504, 0x00000010);
     set_mem(0x600, 0x0F1F2F3F); /* Data to send */
     set_mem(0x604, 0x4F5F6F7F);
     set_mem(0x608, 0x8F9FAFBF);
     set_mem(0x60C, 0xCFDFEFFF);

     status = start_io(data->dev.addr, 0x500, 0, 0);
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x0f + (i << 4), data->test_ctx.buffer[i]);
     }
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c400000, get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
}

CTEST2(dev_test, cda_read) {
     int i;
     uint16_t status;
     log_trace("CDA TEST\n");

     for (i = 0; i < 0x20; i++) {
         data->test_ctx.buffer[i] = 0x10 + i;
     }
     data->test_ctx.max_data = 0x20;
     data->test_ctx.burst = 0;
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0x80000010);
     set_mem(0x508, 0x02000700); /* Set channel words */
     set_mem(0x50c, 0x00000010);
     set_mem(0x600, 0x55555555); /* Invalidate data */
     set_mem(0x604, 0x55555555);
     set_mem(0x608, 0x55555555);
     set_mem(0x60C, 0x55555555);
     set_mem(0x700, 0x55555555); /* Invalidate data */
     set_mem(0x704, 0x55555555);
     set_mem(0x708, 0x55555555);
     set_mem(0x70C, 0x55555555);

     status = start_io(data->dev.addr, 0x500, 0, 0);
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x10 + i, get_mem_b(0x600 + i));
     }
     for (; i < 0x20; i++) {
          ASSERT_EQUAL_X(0x10 + i, get_mem_b(0x700 + i - 0x10));
     }
     ASSERT_EQUAL_X(0x00000510, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
}

CTEST2(dev_test, write_cda) {
     int i;
     uint16_t status;
     for (i = 0; i < 0x20; i++) {
         data->test_ctx.buffer[i] = 0x55;
     }
     data->test_ctx.max_data = 0x20;
     data->test_ctx.burst = 0;
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x01000600); /* Set channel words */
     set_mem(0x504, 0x80000010);
     set_mem(0x508, 0x00000700); /* Set channel words */
     set_mem(0x50c, 0x00000010);
     set_mem(0x600, 0x0F1F2F3F); /* Data to send */
     set_mem(0x604, 0x4F5F6F7F);
     set_mem(0x608, 0x8F9FAFBF);
     set_mem(0x60C, 0xCFDFEFFF);
     set_mem(0x700, 0x0C1C2C3C); /* Data to send */
     set_mem(0x704, 0x4C5C6C7C);
     set_mem(0x708, 0x8C9CACBC);
     set_mem(0x70C, 0xCCDCECFC);

     status = start_io(data->dev.addr, 0x500, 0, 0);
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x0f + (i << 4), data->test_ctx.buffer[i]);
     }
     for (; i < 0x20; i++) {
          ASSERT_EQUAL_X(0x0c + ((i - 0x10) << 4), data->test_ctx.buffer[i]);
     }
     ASSERT_EQUAL_X(0x00000510, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
}


CTEST2(dev_test, cda_read_skip) {
     int i;
     uint16_t status;
     for (i = 0; i < 0x10; i++) {
         data->test_ctx.buffer[i] = 0x10 + i;
     }
     data->test_ctx.max_data = 0x10;
     data->test_ctx.burst = 1;
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0x90000005);
     set_mem(0x508, 0x00000606); /* Set channel words */
     set_mem(0x50c, 0x0000000b);
     set_mem(0x600, 0x55555555); /* Invalidate data */
     set_mem(0x604, 0x55555555);
     set_mem(0x608, 0x55555555);
     set_mem(0x60C, 0x55555555);
     set_mem(0x700, 0x55555555); /* Invalidate data */
     set_mem(0x704, 0x55555555);
     set_mem(0x708, 0x55555555);
     set_mem(0x70C, 0x55555555);

     status = start_io(data->dev.addr, 0x500, 0, 0);
     for (i = 0; i < 6; i++) {
          ASSERT_EQUAL_X(0x55, get_mem_b(0x600 + i));
     }
     for (; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x10 + i, get_mem_b(0x600 + i + 1));
     }
     ASSERT_EQUAL_X(0x00000510, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
}

CTEST2(dev_test, read_ce) {
     int i;
     uint16_t status;
     for (i = 0; i < 0x10; i++) {
         data->test_ctx.buffer[i] = 0xf0 + i;
     }
     log_trace("read ce\n");
     data->test_ctx.max_data = 0x10;
     data->test_ctx.burst = 1;
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x12000600); /* Set channel words */
     set_mem(0x504, 0x00000010);
     set_mem(0x600, 0x55555555); /* Invalidate data */
     set_mem(0x604, 0x55555555);
     set_mem(0x608, 0x55555555);
     set_mem(0x60C, 0x55555555);
     set_mem(0x610, 0x55555555);

     status = start_io(data->dev.addr, 0x500, 0, 0);
     if (verbose) {
         printf("\n 0x40=%08x %08x", get_mem(0x40), get_mem(0x44));
         printf(" 0x38=%08x %08x", get_mem(0x38), get_mem(0x3c));
         printf(" 0x600 = %08x %08x %08x %08x %08x\n", get_mem(0x600),
             get_mem(0x604), get_mem(0x608), get_mem(0x60c), get_mem(0x610));
     }
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0xf0 + i, get_mem_b(0x600 + i));
     }
     ASSERT_EQUAL_X(SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x08000000, get_mem(0x44));
     set_mem(0x40,  0xffffffff);         /* Set CSW to zero */
     set_mem(0x44,  0xffffffff);
     status = wait_dev(data->dev.addr);
     ASSERT_EQUAL_X(SNS_DEVEND, status);
     ASSERT_EQUAL_X(0xffffffff, get_mem(0x40));
     ASSERT_EQUAL_X(0x0400ffff, get_mem(0x44));
}

CTEST2(dev_test, cmd_chain) {
     int i;
     uint16_t status;
     for (i = 0; i < 0x10; i++) {
         data->test_ctx.buffer[i] = 0x55;
     }
     data->test_ctx.max_data = 0x10;
     data->test_ctx.burst = 0;
     log_trace("CMD CHAIN\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x01000600); /* Set channel words */
     set_mem(0x504, 0x40000010);
     set_mem(0x508, 0x03000701); /* Set channel words */
     set_mem(0x50c, 0x40000001);
     set_mem(0x510, 0x04000701); /* Set channel words */
     set_mem(0x514, 0x00000001);
     set_mem(0x700, 0xffffffff);
     set_mem(0x600, 0x0F1F2F3F); /* Data to send */
     set_mem(0x604, 0x4F5F6F7F);
     set_mem(0x608, 0x8F9FAFBF);
     set_mem(0x60C, 0xCFDFEFFF);

     status = start_io(data->dev.addr, 0x500, 0, 0);
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x0f + (i << 4), data->test_ctx.buffer[i]);
     }
     ASSERT_EQUAL_X(0xFF00FFFF, get_mem(0x700));
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
}

/* Test command chain with short record, suppressing */
CTEST2(dev_test, cmd_chain_sli) {
     int i;
     uint16_t status;
     for (i = 0; i < 0x20; i++) {
         data->test_ctx.buffer[i] = 0x10 + i;
     }
     data->test_ctx.max_data = 0x20;
     data->test_ctx.burst = 0;
     log_trace("CMD CHAIN SLI\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0x60000010);
     set_mem(0x508, 0x02000700); /* Set channel words */
     set_mem(0x50c, 0x00000020);
     /* Invalidate data */
     for(i = 0; i <= 0x20; i += 4) {
        set_mem(0x600 + i, 0x55555555);
        set_mem(0x700 + i, 0x55555555);
     }

     status = start_io(data->dev.addr, 0x500, 0, 0);
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x10 + i, get_mem_b(0x600 + i));
     }
     for (; i < 0x20; i++) {
          ASSERT_EQUAL_X(0x55, get_mem_b(0x600 + i));
     }
     for (i = 0; i < 0x20; i++) {
          ASSERT_EQUAL_X(0x10 + i, get_mem_b(0x700 + i));
     }
     ASSERT_EQUAL_X(0x00000510, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
}

/* Test command chain with short record */
CTEST2(dev_test, cmd_chain_short) {
     int i;
     uint16_t status;
     for (i = 0; i < 0x20; i++) {
         data->test_ctx.buffer[i] = 0x10 + i;
     }
     data->test_ctx.max_data = 0x20;
     data->test_ctx.burst = 0;
     log_trace("CMD CHAIN SLI\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0x40000010);
     set_mem(0x508, 0x02000700); /* Set channel words */
     set_mem(0x50c, 0x00000020);
     /* Invalidate data */
     for(i = 0; i <= 0x20; i += 4) {
        set_mem(0x600 + i, 0x55555555);
        set_mem(0x700 + i, 0x55555555);
     }

     status = start_io(data->dev.addr, 0x500, 0, 0);
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x10 + i, get_mem_b(0x600 + i));
     }
     for (; i < 0x20; i++) {
          ASSERT_EQUAL_X(0x55, get_mem_b(0x600 + i));
     }
     for (i = 0; i < 0x20; i++) {
          ASSERT_EQUAL_X(0x55, get_mem_b(0x700 + i));
     }
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c400000, get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
}

/* Test nop instruction with chaining */
CTEST2(dev_test, nop_cc) {
     uint16_t status;
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x03000600); /* Set channel words */
     set_mem(0x504, 0x40000001);
     set_mem(0x508, 0x03000600); /* Set channel words */
     set_mem(0x50c, 0x00000001);

     status = start_io(data->dev.addr, 0x500, 0, 0);
     ASSERT_EQUAL_X(0x00000510, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000001, get_mem(0x44));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
}

/* Test Command and CDA chaining */
CTEST2(dev_test, cda_cc) {
     int i;
     uint16_t status;
     for (i = 0; i < 0x20; i++) {
         data->test_ctx.buffer[i] = 0x10 + i;
     }
     log_trace("CC CD chain\n");
     data->test_ctx.max_data = 0x20;
     data->test_ctx.burst = 0;
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0x80000010);
     set_mem(0x508, 0x00000700); /* Set channel words */
     set_mem(0x50c, 0x40000010);
     set_mem(0x510, 0x04000800); /* Set channel words */
     set_mem(0x514, 0x00000001);
     set_mem(0x600, 0x55555555); /* Invalidate data */
     set_mem(0x604, 0x55555555);
     set_mem(0x608, 0x55555555);
     set_mem(0x60C, 0x55555555);
     set_mem(0x700, 0x55555555); /* Invalidate data */
     set_mem(0x704, 0x55555555);
     set_mem(0x708, 0x55555555);
     set_mem(0x70C, 0x55555555);
     set_mem(0x800, 0x55555555); /* Invalidate data */

     status = start_io(data->dev.addr, 0x500, 0, 0);
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x10 + i, get_mem_b(0x600 + i));
     }
     for (; i < 0x20; i++) {
          ASSERT_EQUAL_X(0x10 + i, get_mem_b(0x700 + i - 0x10));
     }
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00555555, get_mem(0x800));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
}


CTEST2(dev_test, tic_test) {
     int i;
     uint16_t status;
     for (i = 0; i < 0x10; i++) {
         data->test_ctx.buffer[i] = 0x55;
     }
     data->test_ctx.max_data = 0x10;
     data->test_ctx.burst = 0;
     log_trace("TIC TEST\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x01000600); /* Set channel words */
     set_mem(0x504, 0x40000010);
     set_mem(0x508, 0x08000520);
     set_mem(0x50c, 0x00000000);
     set_mem(0x510, 0x04000702); /* Set channel words */
     set_mem(0x514, 0x00000001);
     set_mem(0x520, 0x03000701); /* Set channel words */
     set_mem(0x524, 0x40000001);
     set_mem(0x528, 0x04000701); /* Set channel words */
     set_mem(0x52c, 0x00000001);
     set_mem(0x600, 0x0F1F2F3F); /* Data to send */
     set_mem(0x604, 0x4F5F6F7F);
     set_mem(0x608, 0x8F9FAFBF);
     set_mem(0x60C, 0xCFDFEFFF);
     set_mem(0x700, 0xffffffff);

     status = start_io(data->dev.addr, 0x500, 0, 0);
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x0f + (i << 4), data->test_ctx.buffer[i]);
     }
     ASSERT_EQUAL_X(0x00000530, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0xff00ffff, get_mem(0x700));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
}

CTEST2(dev_test, sms_test) {
     int i;
     uint16_t status;
     for (i = 0; i < 0x10; i++) {
         data->test_ctx.buffer[i] = 0x55;
     }
     data->test_ctx.max_data = 0x10;
     data->test_ctx.sms = 1;
     data->test_ctx.burst = 0;
     log_trace("SMS TEST\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x01000600); /* Set channel words */
     set_mem(0x504, 0x40000010);
     set_mem(0x508, 0x08000520);
     set_mem(0x50c, 0x00000000);
     set_mem(0x510, 0x08000540);
     set_mem(0x514, 0x00000000);
     set_mem(0x520, 0x03000701); /* Set channel words */
     set_mem(0x524, 0x40000001);
     set_mem(0x528, 0x04000701); /* Set channel words */
     set_mem(0x52c, 0x00000001);
     set_mem(0x540, 0x04000703); /* Set channel words */
     set_mem(0x544, 0x00000001);
     set_mem(0x700, 0xffffffff);
     set_mem(0x600, 0x0F1F2F3F); /* Data to send */
     set_mem(0x604, 0x4F5F6F7F);
     set_mem(0x608, 0x8F9FAFBF);
     set_mem(0x60C, 0xCFDFEFFF);

     status = start_io(data->dev.addr, 0x500, 0, 0);
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x0f + (i << 4), data->test_ctx.buffer[i]);
     }
     ASSERT_EQUAL_X(0x00000548, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0xffffff00, get_mem(0x700));
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
}

/* Test Halt of I/O during transfer */
CTEST2(dev_test, halt_io) {
     int i;
     uint16_t status;
     for (i = 0; i < 0x80; i++) {
         data->test_ctx.buffer[i] = (0x10 + i) & 0xff;
     }
     log_trace("HIO\n");
     data->test_ctx.max_data = 0x10;
     data->test_ctx.burst = 0;
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0x80000005);
     set_mem(0x508, 0x00000620); /* Set channel words */
     set_mem(0x50c, 0x40000005);
     set_mem(0x510, 0x04000700); /* Set channel words */
     set_mem(0x514, 0x00000001);
     for (i = 0x600; i < 0x700; i += 4)
         set_mem(i, 0x55555555); /* Invalidate data */
     set_mem(0x700, 0xffffffff);

     status = start_io(data->dev.addr, 0x500, 0, 1);
     if (verbose) {
         printf(" 0x40=%08x %08x 700=%08x", get_mem(0x40), get_mem(0x44),
                  get_mem(0x700));
         printf("0x600 = %08x %08x %08x %08x %08x\n", get_mem(0x600),
                 get_mem(0x604), get_mem(0x608),
                 get_mem(0x60c), get_mem(0x610));
         printf("0x614 =  %08x %08x %08x %08x\n",get_mem(0x614), get_mem(0x618),
                 get_mem(0x61c), get_mem(0x620));
         printf("0x624 =  %08x %08x %08x %08x\n",get_mem(0x624), get_mem(0x628),
                 get_mem(0x62c), get_mem(0x630));
     }
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44) & 0xffbf0000);  /* Ignore Length error */
     ASSERT_EQUAL_X(0xffffffff, get_mem(0x700));
}

/* Test Halt of I/O after transfer */
CTEST2(dev_test, halt_io_2) {
     int i;
     uint16_t status;
     for (i = 0; i < 0x80; i++) {
         data->test_ctx.buffer[i] = (0x10 + i) & 0xff;
     }
     log_trace("HIO 2\n");
     data->test_ctx.max_data = 0x10;
     data->test_ctx.burst = 0;
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x12000600); /* Set channel words */
     set_mem(0x504, 0x40000010);
     set_mem(0x508, 0x02000620); /* Set channel words */
     set_mem(0x50c, 0x80000010);
     set_mem(0x510, 0x00000640); /* Set channel words */
     set_mem(0x514, 0x4000002F);
     set_mem(0x518, 0x04000700); /* Set channel words */
     set_mem(0x51c, 0x00000001);
     for (i = 0x600; i < 0x700; i += 4)
         set_mem(i, 0x55555555); /* Invalidate data */
     set_mem(0x700, 0xffffffff);

     status = start_io(data->dev.addr, 0x500, 0, 1);
     if (verbose) {
         printf(" 0x40=%08x %08x 700=%08x", get_mem(0x40), get_mem(0x44),
             get_mem(0x700));
         printf("0x600 = %08x %08x %08x %08x %08x\n", get_mem(0x600),
             get_mem(0x604), get_mem(0x608), get_mem(0x60c), get_mem(0x610));
         printf("0x614 =  %08x %08x %08x %08x\n",get_mem(0x614), get_mem(0x618),
             get_mem(0x61c), get_mem(0x620));
         printf("0x624 =  %08x %08x %08x %08x\n",get_mem(0x624), get_mem(0x628),
             get_mem(0x62c), get_mem(0x630));
     }
     ASSERT_EQUAL_X(SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x08000000, get_mem(0x44));
     ASSERT_EQUAL_X(0xffffffff, get_mem(0x700));
     set_mem(0x40,  0xffffffff);         /* Set CSW to zero */
     set_mem(0x44,  0xffffffff);
     status = wait_dev(data->dev.addr);
     ASSERT_EQUAL_X(SNS_DEVEND, status);
     ASSERT_EQUAL_X(0xffffffff, get_mem(0x40));
     ASSERT_EQUAL_X(0x0400ffff, get_mem(0x44));
}

