/*
 * microsim360 - Check MPX and Selector channel together.
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

#include <math.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "ctest.h"
#include "xlat.h"
#include "logger.h"
#include "device.h"
#include "model_test.h"
#include "test_device.h"


CTEST_DATA(io_test2) {
    struct _device dev1;
    struct _test_context test_ctx1;
    struct _device dev2;
    struct _test_context test_ctx2;
};

CTEST_SETUP(io_test2) {
    data->dev1.bus_func = &test_dev;
    data->dev1.dev = (void *)&data->test_ctx1;
    data->dev1.addr = 0xf;
    data->dev1.next = &data->dev2;
    data->test_ctx1.burst_max = 256;
    data->dev2.bus_func = &test_dev;
    data->dev2.dev = (void *)&data->test_ctx2;
    data->dev2.addr = 0xe;
    data->dev2.next = NULL;
    data->test_ctx2.burst_max = 256;
    chan[0] = &data->dev1;
}

CTEST_TEARDOWN(io_test2) {
    chan[0] = NULL;
}

CTEST2(io_test2, read) {
     int i;
     int statusf = 0;
     int addrf = 0;
     int statuse = 0;
     int addre = 0;

     init_cpu();
     log_trace("Stacked\n");
     for (i = 0; i < 0x20; i++) {
         data->test_ctx1.buffer[i] = 0xe0 + i;
     }
     data->test_ctx1.max_data = 0x12;
     data->test_ctx1.burst = 0;

     for (i = 0; i < 0x20; i++) {
         data->test_ctx2.buffer[i] = 0xc0 + i;
     }
     data->test_ctx2.max_data = 0x10;
     data->test_ctx2.burst = 0;
     for (i = 0; i < 16; i++) {
         set_reg(i, 0);
     }
     set_mask(0x00);
     set_mem(0x40, 0);         /* Set CSW to zero */
     set_mem(0x44, 0);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000430);
     set_mem(0x48, 0x500);   /* Set CAW */
     set_mem(0x500, 0x12000600); /* Set channel words */
     set_mem(0x504, 0x00000012);
     set_mem(0x510, 0x12000700); /* Set channel words */
     set_mem(0x514, 0x00000010);
     set_mem(0x600, 0x55555555); /* Invalidate data */
     set_mem(0x604, 0x55555555);
     set_mem(0x608, 0x55555555);
     set_mem(0x60C, 0x55555555);
     set_mem(0x700, 0x55555555); /* Invalidate data */
     set_mem(0x704, 0x55555555);
     set_mem(0x708, 0x55555555);
     set_mem(0x70C, 0x55555555);
     set_mem(0x400, 0x41100500);  /* LA 1,500 */
     set_mem(0x404, 0x50100048);  /* ST 1,48 */
     set_mem(0x408, 0x9c00000f);  /* SIO 0f */
     set_mem(0x40c, 0x41100510);  /* LA 1,510 */
     set_mem(0x410, 0x50100048);  /* ST 1,48 */
     set_mem(0x414, 0x9c00000e);  /* SIO 0e */
     set_mem(0x418, 0x47700490);  /* BC  7,490 */
     set_mem(0x41c, 0x82000458);  /* LPSW 458 */
     set_mem(0x420, 0xffffffff);
     set_mem(0x430, 0x58400040);  /* L 4,40 */
     set_mem(0x434, 0x58500044);  /* L 5,44 */
     set_mem(0x438, 0x58600038);  /* L 6,38 */
     set_mem(0x43c, 0x41100460);  /* LA 1,460 */
     set_mem(0x440, 0x5010007c);  /* ST 1,7c  Adjust address */
     set_mem(0x444, 0x58100420);  /* L 1,420 */
     set_mem(0x448, 0x50100040);  /* ST 1,40 set CSW */
     set_mem(0x44c, 0x50100044);  /* ST 1,44 set CSW */
     set_mem(0x450, 0x82000458);  /* LPSW 458 */
     set_mem(0x458, 0xff060000);  /* Wait device 2 */
     set_mem(0x45c, 0x12000450);
     set_mem(0x460, 0x58800040);  /* L 8,40 */
     set_mem(0x464, 0x58900044);  /* L 9,44 */
     set_mem(0x468, 0x58a00038);  /* L 10,38 */
     set_mem(0x46c, 0x41100484);  /* LA 1,484 */
     set_mem(0x470, 0x5010007c);  /* ST 1,7c  Adjust address */
     set_mem(0x474, 0x58100420);  /* L 1,420 */
     set_mem(0x478, 0x50100040);  /* ST 1,40 set CSW */
     set_mem(0x47c, 0x50100044);  /* ST 1,44 set CSW */
     set_mem(0x480, 0x82000458);  /* LPSW 458 */
     set_mem(0x484, 0x58c00040);  /* L 12,40 */
     set_mem(0x488, 0x58d00044);  /* L 13,44 */
     set_mem(0x48c, 0x58e00038);  /* L 14,38 */
     set_mem(0x490, 0x41100000);  /* LA 1,0  */
     set_mem(0x494, 0x9d00000e);  /* TIO 0e */
     set_mem(0x498, 0x478004ac);  /* BC  8,4ac */
     set_mem(0x49c, 0x58000040);  /* L 0,40 */
     set_mem(0x4a0, 0x58100044);  /* L 1,44 */
     set_mem(0x4a4, 0x58200494);  /* L 2,494 */
     set_mem(0x4a8, 0x47f004c0);  /* BC  15,4c0 */
     set_mem(0x4ac, 0x9d00000f);  /* TIO 0f */
     set_mem(0x4b0, 0x478004c0);  /* BC  8,4c0 */
     set_mem(0x4b4, 0x58000040);  /* L 0,40 */
     set_mem(0x4b8, 0x58100044);  /* L 1,44 */
     set_mem(0x4bc, 0x582004ac);  /* L 2,4ac */
     set_mem(0x4c0, 0);           /*        */

     test_io_inst(0);
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0xe0 + i, get_mem_b(0x600 + i));
          ASSERT_EQUAL_X(0xc0 + i, get_mem_b(0x700 + i));
     }

     for (i = 0; i < 16; i+=4) {
         if ((get_reg(i+2) & 0xffff) == 0xe) {
             statuse |= get_reg(i+1);
             if ((get_reg(i+1) & 0x08000000) != 0) {
                 addre = get_reg(i);
             }
         }
         if ((get_reg(i+2) & 0xffff) == 0xf) {
             statusf |= get_reg(i+1);
             if ((get_reg(i+1) & 0x08000000) != 0) {
                 addrf = get_reg(i);
             }
         }
     }
     ASSERT_EQUAL_X(0x00000518, addre);
     ASSERT_EQUAL_X(0x0c000000, statuse);
     ASSERT_EQUAL_X(0x00000508, addrf);
     ASSERT_EQUAL_X(0x0c000000, statusf);
}


CTEST_DATA(io_test3) {
    struct _device dev1;
    struct _test_context test_ctx1;
    struct _device dev2;
    struct _test_context test_ctx2;
};

CTEST_SETUP(io_test3) {
    data->dev1.bus_func = &test_dev;
    data->dev1.dev = (void *)&data->test_ctx1;
    data->dev1.addr = 0x00f;
    data->dev1.next = NULL;
    data->test_ctx1.burst_max = 256;
    data->dev2.bus_func = &test_dev;
    data->dev2.dev = (void *)&data->test_ctx2;
    data->dev2.addr = 0x10e;
    data->dev2.next = NULL;
    data->test_ctx2.burst_max = 256;
    chan[0] = &data->dev1;
    chan[1] = &data->dev2;
}

CTEST_TEARDOWN(io_test3) {
    chan[0] = NULL;
    chan[1] = NULL;
}

CTEST2(io_test3, read) {
     int i;
     init_cpu();
     log_trace("Multi I/O\n");
     for (i = 0; i < 0x20; i++) {
         data->test_ctx1.buffer[i] = 0xe0 + i;
     }
     data->test_ctx1.max_data = 0x12;
     data->test_ctx1.burst = 0;

     for (i = 0; i < 0x20; i++) {
         data->test_ctx2.buffer[i] = 0xc0 + i;
     }
     data->test_ctx2.max_data = 0x10;
     data->test_ctx2.burst = 0;
     set_mask(0x00);
     set_mem(0x40, 0);         /* Set CSW to zero */
     set_mem(0x44, 0);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000430);
     set_mem(0x48, 0x500);   /* Set CAW */
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0x00000012);
     set_mem(0x510, 0x02000700); /* Set channel words */
     set_mem(0x514, 0x00000010);
     set_mem(0x600, 0x55555555); /* Invalidate data */
     set_mem(0x604, 0x55555555);
     set_mem(0x608, 0x55555555);
     set_mem(0x60C, 0x55555555);
     set_mem(0x700, 0x55555555); /* Invalidate data */
     set_mem(0x704, 0x55555555);
     set_mem(0x708, 0x55555555);
     set_mem(0x70C, 0x55555555);
     set_mem(0x400, 0x41100500);  /* LA 1,500 */
     set_mem(0x404, 0x50100048);  /* ST 1,48 */
     set_mem(0x408, 0x9c00000f);  /* SIO 00f */
     set_mem(0x40c, 0x41100510);  /* LA 1,510 */
     set_mem(0x410, 0x50100048);  /* ST 1,48 */
     set_mem(0x414, 0x9c00010e);  /* SIO 10e */
     set_mem(0x418, 0x47700490);  /* BC  7,490 */
     set_mem(0x41c, 0x82000458);  /* LPSW 458 */
     set_mem(0x420, 0xffffffff);
     set_mem(0x430, 0x58400040);  /* L 4,40 */
     set_mem(0x434, 0x58500044);  /* L 5,44 */
     set_mem(0x438, 0x58600038);  /* L 6,38 */
     set_mem(0x43c, 0x41100460);  /* LA 1,460 */
     set_mem(0x440, 0x5010007c);  /* ST 1,7c  Adjust address */
     set_mem(0x444, 0x58100420);  /* L 1,420 */
     set_mem(0x448, 0x50100040);  /* ST 1,40 set CSW */
     set_mem(0x44c, 0x50100044);  /* ST 1,44 set CSW */
     set_mem(0x450, 0x82000458);  /* LPSW 458 */
     set_mem(0x458, 0xff060000);  /* Wait device 2 */
     set_mem(0x45c, 0x12000450);
     set_mem(0x460, 0x58800040);  /* L 8,40 */
     set_mem(0x464, 0x58900044);  /* L 9,44 */
     set_mem(0x468, 0x58a00038);  /* L 10,38 */
     set_mem(0x46c, 0x41100484);  /* LA 1,484 */
     set_mem(0x470, 0);

     test_io_inst(0);
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0xe0 + i, get_mem_b(0x600 + i));
          ASSERT_EQUAL_X(0xc0 + i, get_mem_b(0x700 + i));
     }
     ASSERT_EQUAL_X(0x00000518, get_reg(4));
     ASSERT_EQUAL_X(0x0c000000, get_reg(5));
     ASSERT_EQUAL_X(0xff06010e, get_reg(6));
     ASSERT_EQUAL_X(0x00000508, get_reg(8));
     ASSERT_EQUAL_X(0x0c000000, get_reg(9));
     ASSERT_EQUAL_X(0xff06000f, get_reg(10));
}


