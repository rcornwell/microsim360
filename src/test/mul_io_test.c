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
    chan[1] = NULL;
}

CTEST_TEARDOWN(io_test2) {
    chan[0] = NULL;
}

/* Test that two devices can interrupt at same time.
   One device should have it's status stacked. */
CTEST2(io_test2, stacked) {
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
     data->test_ctx1.max_data = 0x11;
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
     set_mem(0x40, 0xffffffff);         /* Set CSW to zero */
     set_mem(0x44, 0xffffffff);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000430);
     set_mem(0x48, 0x500);   /* Set CAW */
     set_mem(0x500, 0x12000600); /* Set channel words */
     set_mem(0x504, 0x00000011);
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
     set_mem(0x4b0, 0x478004d0);  /* BC  8,4d0 */
     set_mem(0x4b4, 0x58000040);  /* L 0,40 */
     set_mem(0x4b8, 0x58100044);  /* L 1,44 */
     set_mem(0x4bc, 0x582004ac);  /* L 2,4ac */
     set_mem(0x4c0, 0x9d00000e);  /* TIO 0e */
     set_mem(0x4c4, 0x477004c0);  /* BC  7,4c0 */
     set_mem(0x4c8, 0x9d00000f);  /* TIO 0f */
     set_mem(0x4cc, 0x477004c8);  /* BC  7,4c8 */
     set_mem(0x4d0, 0);           /*        */

     test_io_inst(0);
     if (verbose) {
         printf(" 0x38=%08x %08x", get_mem(0x38), get_mem(0x3c));
         printf(" 0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
         for (i = 0; i < 16; i++) {
             printf(" R0=%08x", get_reg(i));
         }
         printf("\n0x600 = %08x %08x %08x %08x %08x\n", get_mem(0x600),
                  get_mem(0x604), get_mem(0x608), get_mem(0x60c), get_mem(0x610));
         printf("0x700 = %08x %08x %08x %08x %08x\n", get_mem(0x700),
                  get_mem(0x704), get_mem(0x708), get_mem(0x70c), get_mem(0x710));
     }
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

/* Make sure halting device on one sub-channel does not
   stop device on another sub-channel */
CTEST2(io_test2, halt_io_mpx) {
     int i;

     init_cpu();
     log_trace("HIO MPX\n");
     for (i = 0; i < 0x20; i++) {
         data->test_ctx1.buffer[i] = 0xe0 + i;
     }
     data->test_ctx1.max_data = 0x30;
     data->test_ctx1.burst = 0;
     set_mask(0x00);
     set_mem(0x40, 0xffffffff);  /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000408);
     set_mem(0x48, 0x500);   /* Set CAW */
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0x88000001);
     set_mem(0x508, 0x00000601); /* Set channel words */
     set_mem(0x50c, 0x0000002f);
     for (i = 0x600; i < 0x700; i += 4)
         set_mem(i, 0x55555555); /* Invalidate data */
     set_mem(0x700, 0xffffffff);
     set_mem(0x400, 0x9c00000f); /* SIO 00f */
     set_mem(0x404, 0x82000440); /* LPSW 0440 */
     set_mem(0x408, 0x58000040); /* L 0, 040 */
     set_mem(0x40c, 0x58100044); /* L 1, 044 */
     set_mem(0x410, 0x58400448); /* L 4, 448 */
     set_mem(0x414, 0x50400040); /* ST 4,040 */
     set_mem(0x418, 0x50400044); /* ST 4,044 */
     set_mem(0x41c, 0x9e00000e); /* HIO 00e */
     set_mem(0x420, 0x05400700); /* BALR 4,0, BCR 0,0 */
     set_mem(0x424, 0x58200040); /* L 2, 040 */
     set_mem(0x428, 0x58300044); /* L 3, 044 */
     set_mem(0x42c, 0x9d00000f);  /* TIO 00f */
     set_mem(0x430, 0x4770042c);  /* BC  7,42c */
     set_mem(0x434, 0);
     set_mem(0x440, 0xff060000);  /* Wait PSW */
     set_mem(0x444, 0x14000408);
     set_mem(0x448, 0xffffffff);

     test_io_inst(0);
     if (verbose) {
         printf(" 0x38=%08x %08x", get_mem(0x38), get_mem(0x3c));
         printf(" 0x40=%08x %08x 700=%08x", get_mem(0x40), get_mem(0x44), get_mem(0x700));
         printf(" R0=%08x R1=%08x", get_reg(0), get_reg(1));
         printf(" R2=%08x R3=%08x", get_reg(2), get_reg(3));
         printf(" R4=%08x\n", get_reg(4));
         printf("0x600 = %08x %08x %08x %08x %08x\n", get_mem(0x600),
                  get_mem(0x604), get_mem(0x608), get_mem(0x60c), get_mem(0x610));
         printf("0x614 =  %08x %08x %08x %08x\n",get_mem(0x614), get_mem(0x618),
                  get_mem(0x61c), get_mem(0x620));
         printf("0x624 =  %08x %08x %08x %08x\n",get_mem(0x624), get_mem(0x628),
                  get_mem(0x62c), get_mem(0x630));
     }
     /* The result of a PCI can have a Address at different locations */
     ASSERT_EQUAL_X(0x00800000, get_reg(1) & 0xffbf0000);  /* Ignore Length error */
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44) & 0xffbf0000);  /* Ignore Length error */
     ASSERT_EQUAL_X(0xff06000f, get_mem(0x38));
     ASSERT_EQUAL_X(0x94000408, get_mem(0x3C));
     ASSERT_EQUAL_X(0xffffffff, get_mem(0x700));
}

/* Make sure halting running device on another sub-channel
   does not effect device on another sub-channel. */
CTEST2(io_test2, halt_io_mpx2) {
     int i;

     init_cpu();
     log_trace("HIO Mpx 2\n");
     for (i = 0; i < 0x20; i++) {
         data->test_ctx1.buffer[i] = 0xe0 + i;
     }
     data->test_ctx1.max_data = 0x30;
     data->test_ctx1.burst = 0;

     for (i = 0; i < 0x20; i++) {
         data->test_ctx2.buffer[i] = 0xc0 + i;
     }
     data->test_ctx2.max_data = 0x30;
     data->test_ctx2.burst = 0;
     for (i = 0; i < 16; i++) {
         set_reg(i, 0xffffffff);
     }
     set_mask(0x00);
     set_mem(0x40, 0xffffffff);         /* Set CSW to zero */
     set_mem(0x44, 0xffffffff);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000430);
     set_mem(0x48, 0x500);   /* Set CAW */
     set_mem(0x500, 0x12000600); /* Device 00f */
     set_mem(0x504, 0x00000030);
     set_mem(0x510, 0x12000700); /* Device 00e */
     set_mem(0x514, 0x08000030);
     set_mem(0x600, 0x55555555); /* Invalidate data */
     set_mem(0x604, 0x55555555);
     set_mem(0x608, 0x55555555);
     set_mem(0x60C, 0x55555555);
     set_mem(0x700, 0x55555555); /* Invalidate data */
     set_mem(0x704, 0x55555555);
     set_mem(0x708, 0x55555555);
     set_mem(0x70C, 0x55555555);
     set_mem(0x400, 0x41100500);  /* LA 1,500 */   /* Start first device */
     set_mem(0x404, 0x50100048);  /* ST 1,48 */
     set_mem(0x408, 0x9c00000f);  /* SIO 0f */
     set_mem(0x40c, 0x41100510);  /* LA 1,510 */   /* Start second device */
     set_mem(0x410, 0x50100048);  /* ST 1,48 */
     set_mem(0x414, 0x9c00000e);  /* SIO 0e */
     set_mem(0x418, 0x47700490);  /* BC  7,490 */
     set_mem(0x41c, 0x82000428);  /* LPSW 428 */   /* Wait for second device to */
     set_mem(0x420, 0xffffffff);                   /* send some data */
     set_mem(0x428, 0xff060000);  /* Wait device 2 */
     set_mem(0x42c, 0x12000450);
     set_mem(0x430, 0x58400040);  /* L 4,40 */     /* Save PCI status */
     set_mem(0x434, 0x58500044);  /* L 5,44 */
     set_mem(0x438, 0x58600038);  /* L 6,38 */
     set_mem(0x43c, 0x41100464);  /* LA 1,464 */   /* Set up for halt status */
     set_mem(0x440, 0x5010007c);  /* ST 1,7c  Adjust address */
     set_mem(0x444, 0x58100420);  /* L 1,420 */
     set_mem(0x448, 0x50100040);  /* ST 1,40 set CSW */
     set_mem(0x44c, 0x50100044);  /* ST 1,44 set CSW */
     set_mem(0x450, 0x50100038);  /* ST 1,38 set CSW */
     set_mem(0x454, 0x9e00000f);  /* HIO 00f */    /* Halt first device */
     set_mem(0x458, 0x82000428);  /* LPSW 428 */
     set_mem(0x464, 0x58800040);  /* L 8,40 */     /* Save next status */
     set_mem(0x468, 0x58900044);  /* L 9,44 */     /* Should be channel end 00f */
     set_mem(0x46c, 0x58a00038);  /* L 10,38 */
     set_mem(0x470, 0x4110048c);  /* LA 1,48c */
     set_mem(0x474, 0x5010007c);  /* ST 1,7c  Adjust address */
     set_mem(0x478, 0x58100420);  /* L 1,420 */
     set_mem(0x47c, 0x50100040);  /* ST 1,40 set CSW */
     set_mem(0x480, 0x50100044);  /* ST 1,44 set CSW */
     set_mem(0x484, 0x50100038);  /* ST 1,38 set CSW */
     set_mem(0x488, 0x82000428);  /* LPSW 428 */   /* Wait for second device to finish */
     set_mem(0x48c, 0x58c00040);  /* L 12,40 */
     set_mem(0x490, 0x58d00044);  /* L 13,44 */
     set_mem(0x494, 0x58e00038);  /* L 14,38 */
     set_mem(0x498, 0x58100420);  /* L 1,420 */
     set_mem(0x49c, 0x50100040);  /* ST 1,40 set CSW */
     set_mem(0x4a0, 0x50100044);  /* ST 1,44 set CSW */
     set_mem(0x4a4, 0x50100038);  /* ST 1,38 set CSW */
     set_mem(0x4a8, 0x9d00000f);  /* TIO 0f */    /* Make sure first is done */
     set_mem(0x4ac, 0x58000040);  /* L 0,40 */
     set_mem(0x4b0, 0x58100044);  /* L 1,44 */
     set_mem(0x4b4, 0x473004a8);  /* BC  3,4a8 */
     set_mem(0x4b8, 0x9d00000e);  /* TIO 0e */    /* Make sure second is done */
     set_mem(0x4bc, 0x478004dc);  /* BC  8,4dc */
     set_mem(0x4c0, 0x58200040);  /* L 2,40 */
     set_mem(0x4c4, 0x58300044);  /* L 3,44 */
     set_mem(0x4c8, 0x47f004b8);  /* BC  15,4b8 */
     set_mem(0x4d0, 0);           /*        */

     test_io_inst(0);
     if (verbose) {
         printf(" 0x38=%08x %08x", get_mem(0x38), get_mem(0x3c));
         printf(" 0x40=%08x %08x 700=%08x\n", get_mem(0x40), get_mem(0x44), get_mem(0x700));
         for (i = 0; i < 16; i++) {
             printf(" R%d=%08x", i, get_reg(i));
         }
         printf("\n0x600 = %08x %08x %08x %08x %08x\n", get_mem(0x600),
                 get_mem(0x604), get_mem(0x608), get_mem(0x60c), get_mem(0x610));
     }
     ASSERT_EQUAL_X(0x00000518, get_reg(4));
     ASSERT_EQUAL_X(0x00800000, get_reg(5) & 0xffbf0000);  /* Look at PCI */
     ASSERT_EQUAL_X(0xff06000e, get_reg(6));
     ASSERT_EQUAL_X(0x00000508, get_reg(8));
     ASSERT_EQUAL_X(0x08400000, get_reg(9) & 0xffff0000);  /* Length variable */
     ASSERT_EQUAL_X(0xff06000f, get_reg(10));
     ASSERT_EQUAL_X(0x00000000, get_reg(12));
     ASSERT_EQUAL_X(0x04000000, get_reg(13));
     ASSERT_EQUAL_X(0xff06000f, get_reg(14));
     ASSERT_EQUAL_X(0xffffffff, get_reg(1));
     ASSERT_EQUAL_X(0x04000000, get_reg(3));
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

/* Test I/O on MPX and Selector channel at same time */
CTEST2(io_test3, read_both) {
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
     set_mem(0x40, 0xffffffff);         /* Set CSW to zero */
     set_mem(0x44, 0xffffffff);
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


CTEST_DATA(io_test4) {
    struct _device dev1;
    struct _test_context test_ctx1;
    struct _device dev2;
    struct _test_context test_ctx2;
};

CTEST_SETUP(io_test4) {
    data->dev1.bus_func = &test_dev;
    data->dev1.dev = (void *)&data->test_ctx1;
    data->dev1.addr = 0xc0;
    data->dev1.next = &data->dev2;
    data->test_ctx1.burst_max = 256;
    data->dev2.bus_func = &test_dev;
    data->dev2.dev = (void *)&data->test_ctx2;
    data->dev2.addr = 0xc1;
    data->dev2.next = NULL;
    data->test_ctx2.burst_max = 256;
}

CTEST_TEARDOWN(io_test4) {
    chan[0] = NULL;
    chan[1] = NULL;
}

/* Test starting second device on same sub-channel. */
CTEST2(io_test4, read_mpx_sub) {
     int i;

     chan[0] = &data->dev1;
     init_cpu();
     log_trace("Subchannel\n");
     for (i = 0; i < 0x20; i++) {
         data->test_ctx1.buffer[i] = 0xe0 + i;
     }
     data->test_ctx1.max_data = 0x20;
     data->test_ctx1.burst = 0;

     for (i = 0; i < 0x20; i++) {
         data->test_ctx2.buffer[i] = 0xc0 + i;
     }
     data->test_ctx2.max_data = 0x20;
     data->test_ctx2.burst = 0;
     for (i = 0; i < 16; i++) {
         set_reg(i, 0);
     }
     set_mask(0x00);
     set_mem(0x40, 0xffffffff);  /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000408);
     set_mem(0x48, 0x500);   /* Set CAW */
     set_mem(0x500, 0x02000700); /* Set channel words */
     set_mem(0x504, 0x00000020);
     set_mem(0x510, 0x02000600); /* Set channel words */
     set_mem(0x514, 0x00000020);
     set_mem(0x600, 0xffffffff);
     set_mem(0x700, 0xffffffff);
     set_mem(0x400, 0x9c0000c0); /* SIO 0c0 */
     set_mem(0x404, 0x41200510);  /* LA 2,510 */
     set_mem(0x408, 0x50200048); /* ST 4,047 */
     set_mem(0x40c, 0x41200200); /* LA 2,0200 */
     set_mem(0x410, 0x46200410); /* BCT 2,410 */
     set_mem(0x414, 0x9c0000c1); /* SIO 0c1 */
     set_mem(0x418, 0x05100700); /* BALR 1,0, BCR 0,0 */
     set_mem(0x41c, 0x58200040); /* L 2, 040 */
     set_mem(0x420, 0x58300044); /* L 3, 044 */
     set_mem(0x424, 0x58400440); /* L 4, 440 */
     set_mem(0x428, 0x50400040); /* ST 4,040 */
     set_mem(0x42c, 0x50400044); /* ST 4,044 */
     set_mem(0x430, 0x9d0000c0);  /* TIO 0c0 */
     set_mem(0x434, 0x47700430);  /* BC  7,430 */
     set_mem(0x438, 0);
     set_mem(0x440, 0xffffffff);

     test_io_inst(0);
     if (verbose) {
         printf(" 0x40=%08x %08x", get_mem(0x40), get_mem(0x44));
         printf(" R0=%08x R1=%08x R2=%08x R3=%08x\n", get_reg(0),
                   get_reg(1), get_reg(2), get_reg(3));
         printf("0x700 = %08x %08x\n", get_mem(0x700), get_mem(0x704));
     }


     ASSERT_EQUAL_X(0x6000041a, get_reg(1));
     ASSERT_EQUAL_X(0xffffffff, get_reg(2));
     ASSERT_EQUAL_X(0xffffffff, get_reg(3));
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
}

/* Test starting second device after first device ends
   channel use. */
CTEST2(io_test4, read_sel_two) {
     int i;

     chan[0] = NULL;
     chan[1] = &data->dev1;
     init_cpu();
     log_trace("Select Two\n");
     for (i = 0; i < 0x20; i++) {
         data->test_ctx1.buffer[i] = 0xe0 + i;
     }
     data->test_ctx1.max_data = 0x10;
     data->test_ctx1.burst = 0;

     for (i = 0; i < 0x20; i++) {
         data->test_ctx2.buffer[i] = 0xc0 + i;
     }
     data->test_ctx2.max_data = 0x20;
     data->test_ctx2.burst = 0;
     for (i = 0; i < 16; i++) {
         set_reg(i, 0);
     }
     set_mask(0x00);
     set_mem(0x40, 0xffffffff);  /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000430);
     set_mem(0x48, 0x500);   /* Set CAW */
     set_mem(0x500, 0x12000700); /* Set channel words */
     set_mem(0x504, 0x00000010);
     set_mem(0x510, 0x12000600); /* Set channel words */
     set_mem(0x514, 0x00000020);
     set_mem(0x600, 0xffffffff);
     set_mem(0x700, 0xffffffff);
     set_mem(0x400, 0x9c0001c0); /* SIO 1c0 */
     set_mem(0x404, 0x82000420);  /* LPSW 420 */ /* Wait device 1 */
     set_mem(0x420, 0xff060000);  /* Wait device */
     set_mem(0x424, 0xffffffff);
     set_mem(0x430, 0x58000040); /* L 0, 040 */   /* Save status */
     set_mem(0x434, 0x58100044); /* L 1, 044 */
     set_mem(0x438, 0x58200038); /* L 2, 038 */
     set_mem(0x43c, 0x58500424); /* L 5, 424 */
     set_mem(0x440, 0x41500510);  /* LA 5,510 */  /* Start second device */
     set_mem(0x444, 0x50500048); /* ST 5,048 */
     set_mem(0x448, 0x9c0001c1); /* SIO 1c1 */
     set_mem(0x44c, 0x47700448);  /* BC  7,448 */
     set_mem(0x450, 0x58500424); /* L 5, 424 */
     set_mem(0x454, 0x50500040); /* ST 5,040 */
     set_mem(0x458, 0x50500044); /* ST 5,044 */
     set_mem(0x45c, 0x50500038); /* ST 5,038 */
     set_mem(0x460, 0x41500470);  /* LA 5,470 */
     set_mem(0x464, 0x5050007c); /* ST 5,07c */
     set_mem(0x468, 0x82000420);  /* LPSW 420 */  /* Wait for second device */
     set_mem(0x470, 0x58600040); /* L 6, 040 */
     set_mem(0x474, 0x58700044); /* L 7, 044 */
     set_mem(0x478, 0x58800038); /* L 8, 038 */
     set_mem(0x47c, 0x58500424); /* L 5, 424 */
     set_mem(0x480, 0x50500040); /* ST 5,040 */
     set_mem(0x484, 0x50500044); /* ST 5,044 */
     set_mem(0x488, 0x50500038); /* ST 5,038 */
     set_mem(0x48c, 0x415004a0);  /* LA 5,4a0 */
     set_mem(0x490, 0x5050007c); /* ST 5,07c */
     set_mem(0x494, 0x82000420);  /* LPSW 420 */  /* Wait for a to finish device */
     set_mem(0x4a0, 0x58a00040); /* L 10, 040 */
     set_mem(0x4a4, 0x58b00044); /* L 11, 044 */
     set_mem(0x4a8, 0x58c00038); /* L 12, 038 */
     set_mem(0x4ac, 0x58500424); /* L 5, 424 */
     set_mem(0x4b0, 0x50500040); /* ST 5,040 */
     set_mem(0x4b4, 0x50500044); /* ST 5,044 */
     set_mem(0x4b8, 0x50500038); /* ST 5,038 */
     set_mem(0x4bc, 0x415004f0);  /* LA 5,4f0 */
     set_mem(0x4c0, 0x5050007c); /* ST 5,07c */
     set_mem(0x4c4, 0x82000420);  /* LPSW 420 */  /* Wait for a to finish device */
     set_mem(0x4f0, 0);

     test_io_inst(0);
     if (verbose) {
         for (i = 0; i < 16; i++) {
             printf(" R%d=%08x", i, get_reg(i));
         }
         printf("\n0x40=%08x %08x 38=%08x ", get_mem(0x40), get_mem(0x44), get_mem(0x38));
         printf("0x600 = %08x %08x ", get_mem(0x600), get_mem(0x604));
         printf("0x700 = %08x %08x\n", get_mem(0x700), get_mem(0x704));
     }

     ASSERT_EQUAL_X(0x00000508, get_reg(0));
     ASSERT_EQUAL_X(0x08000000, get_reg(1));
     ASSERT_EQUAL_X(0xff0601c0, get_reg(2));
     ASSERT_EQUAL_X(0x00000518, get_reg(6));
     ASSERT_EQUAL_X(0x08000000, get_reg(7));
     ASSERT_EQUAL_X(0xff0601c1, get_reg(8));
     ASSERT_EQUAL_X(0x00000000, get_reg(10));
     ASSERT_EQUAL_X(0x04000000, get_reg(11));
     ASSERT_EQUAL_X(0xff0601c0, get_reg(12));
     ASSERT_EQUAL_X(0x00000000, get_mem(0x40));
     ASSERT_EQUAL_X(0x04000000, get_mem(0x44));
     ASSERT_EQUAL_X(0xff0601c1, get_mem(0x38));
}

/* Test starting same device after device ends
   channel use. */
CTEST2(io_test4, read_sel_same) {
     int i;

     chan[0] = NULL;
     chan[1] = &data->dev1;
     init_cpu();
     log_trace("Select one\n");
     for (i = 0; i < 0x40; i++) {
         data->test_ctx1.buffer[i] = 0xe0 + i;
     }
     data->test_ctx1.max_data = 0x40;
     data->test_ctx1.burst = 0;

     for (i = 0; i < 16; i++) {
         set_reg(i, 0);
     }
     set_mask(0x00);
     set_mem(0x40, 0xffffffff);  /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000408);
     set_mem(0x48, 0x500);   /* Set CAW */
     set_mem(0x500, 0x12000700); /* Set channel words */
     set_mem(0x504, 0x00000040);
     set_mem(0x600, 0xffffffff);
     set_mem(0x700, 0xffffffff);
     set_mem(0x400, 0x9c0001c0); /* SIO 1c0 */
     set_mem(0x404, 0x41200510);  /* LA 2,510 */
     set_mem(0x408, 0x50200048); /* ST 4,047 */
     set_mem(0x40c, 0x41200100); /* LA 2,0100 */
     set_mem(0x410, 0x46200410); /* BCT 2,410 */
     set_mem(0x414, 0x9c0001c0); /* SIO 1c0 */   /* Return busy status */
     set_mem(0x418, 0x05100700); /* BALR 1,0, BCR 0,0 */
     set_mem(0x41c, 0x58200040); /* L 2, 040 */
     set_mem(0x420, 0x58300044); /* L 3, 044 */
     set_mem(0x424, 0x58400460); /* L 4, 460 */
     set_mem(0x428, 0x50400040); /* ST 4,040 */
     set_mem(0x42c, 0x50400044); /* ST 4,044 */
     set_mem(0x430, 0x9d0001c0); /* TIO 1c0 */
     set_mem(0x434, 0x47300430); /* BC  3,430 */
     set_mem(0x438, 0x58500040); /* L 5, 040 */
     set_mem(0x43c, 0x58600044); /* L 6, 044 */
     set_mem(0x440, 0x58400460); /* L 4, 460 */
     set_mem(0x444, 0x50400040); /* ST 4,040 */
     set_mem(0x448, 0x50400044); /* ST 4,044 */
     set_mem(0x44c, 0x9d0001c0);  /* TIO 1c0 */
     set_mem(0x450, 0x4770044c);  /* BC  7,44c */
     set_mem(0x454, 0);
     set_mem(0x460, 0xffffffff);

     test_io_inst(0);
     if (verbose) {
         printf(" 0x40=%08x %08x", get_mem(0x40), get_mem(0x44));
         printf(" R0=%08x R1=%08x R2=%08x R3=%08x\n", get_reg(0),
                get_reg(1), get_reg(2), get_reg(3));
         printf(" R5=%08x R6=%08x\n", get_reg(5), get_reg(6));
         printf("0x700 = %08x %08x\n", get_mem(0x700), get_mem(0x704));
     }

     ASSERT_EQUAL_X(0x6000041a, get_reg(1));
     ASSERT_EQUAL_X(0xffffffff, get_reg(2));
     ASSERT_EQUAL_X(0xffffffff, get_reg(3));
     ASSERT_EQUAL_X(0x00000508, get_reg(5));
     ASSERT_EQUAL_X(0x08000000, get_reg(6));
     ASSERT_EQUAL_X(0x00000000, get_mem(0x40));
     ASSERT_EQUAL_X(0x04000000, get_mem(0x44));
}

/* Test starting same device after first device ends
   channel use. */
CTEST2(io_test4, read_sel_tio) {
     int i;

     chan[0] = NULL;
     chan[1] = &data->dev1;
     init_cpu();
     log_trace("Select TIO\n");
     for (i = 0; i < 0x20; i++) {
         data->test_ctx1.buffer[i] = 0xe0 + i;
     }
     data->test_ctx1.max_data = 0x4;
     data->test_ctx1.burst = 0;

     for (i = 0; i < 0x20; i++) {
         data->test_ctx2.buffer[i] = 0xc0 + i;
     }
     data->test_ctx2.max_data = 0x4;
     data->test_ctx2.burst = 0;
     for (i = 0; i < 16; i++) {
         set_reg(i, 0);
     }
     set_mask(0x00);
     set_mem(0x40, 0xffffffff);  /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000408);
     set_mem(0x48, 0x500);   /* Set CAW */
     set_mem(0x500, 0x12000700); /* Set channel words */
     set_mem(0x504, 0x00000004);
     set_mem(0x510, 0x12000600); /* Set channel words */
     set_mem(0x514, 0x00000004);
     set_mem(0x600, 0xffffffff);
     set_mem(0x700, 0xffffffff);
     set_mem(0x400, 0x9c0001c0); /* SIO 1c0 */
     set_mem(0x404, 0x41200510);  /* LA 2,510 */
     set_mem(0x408, 0x50200048); /* ST 4,047 */
     set_mem(0x40c, 0x41200400); /* LA 2,0400 */
     set_mem(0x410, 0x46200410); /* BCT 2,410 */
     set_mem(0x414, 0x9d0001c0); /* TIO 1c0 */   /* return status */
     set_mem(0x418, 0x05100700); /* BALR 1,0, BCR 0,0 */
     set_mem(0x41c, 0x58200040); /* L 2, 040 */
     set_mem(0x420, 0x58300044); /* L 3, 044 */
     set_mem(0x424, 0x58400440); /* L 4, 440 */
     set_mem(0x428, 0x50400040); /* ST 4,040 */
     set_mem(0x42c, 0x50400044); /* ST 4,044 */
     set_mem(0x430, 0x9d0001c0);  /* TIO 1c0 */
     set_mem(0x434, 0x47700430);  /* BC  7,430 */
     set_mem(0x438, 0);
     set_mem(0x440, 0xffffffff);

     test_io_inst(0);
     if (verbose) {
         printf(" 0x40=%08x %08x", get_mem(0x40), get_mem(0x44));
         printf(" R0=%08x R1=%08x R2=%08x R3=%08x\n", get_reg(0),
                get_reg(1), get_reg(2), get_reg(3));
         printf("0x700 = %08x %08x\n", get_mem(0x700), get_mem(0x704));
     }

     ASSERT_EQUAL_X(0x5000041a, get_reg(1));
     ASSERT_EQUAL_X(0x00000508, get_reg(2));
     ASSERT_EQUAL_X(0x08000000, get_reg(3));
     ASSERT_EQUAL_X(0x00000000, get_mem(0x40));
     ASSERT_EQUAL_X(0x04000000, get_mem(0x44));
}


/* Test halting device on same subchannel. */
CTEST2(io_test4, halt_io_mpx) {
     int i;
     chan[0] = &data->dev1;
     chan[1] = NULL;
     init_cpu();
     log_trace("HIO MPX\n");
     for (i = 0; i < 0x20; i++) {
         data->test_ctx1.buffer[i] = 0xe0 + i;
     }
     data->test_ctx1.max_data = 0x30;
     data->test_ctx1.burst = 0;
     set_mask(0x00);
     set_mem(0x40, 0xffffffff);  /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000408);
     set_mem(0x48, 0x500);   /* Set CAW */
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0x88000001);
     set_mem(0x508, 0x00000601); /* Set channel words */
     set_mem(0x50c, 0x0000002f);
     for (i = 0x600; i < 0x700; i += 4)
         set_mem(i, 0x55555555); /* Invalidate data */
     set_mem(0x700, 0xffffffff);
     set_mem(0x400, 0x9c0000c0); /* SIO 0c0 */
     set_mem(0x404, 0x82000440); /* LPSW 0440 */
     set_mem(0x408, 0x58000040); /* L 0, 040 */
     set_mem(0x40c, 0x58100044); /* L 1, 044 */
     set_mem(0x410, 0x58400448); /* L 4, 448 */
     set_mem(0x414, 0x50400040); /* ST 4,040 */
     set_mem(0x418, 0x50400044); /* ST 4,044 */
     set_mem(0x41c, 0x9e0000c1); /* HIO 0c1 */
     set_mem(0x420, 0x05400700); /* BALR 4,0, BCR 0,0 */
     set_mem(0x424, 0x58200040); /* L 2, 040 */
     set_mem(0x428, 0x58300044); /* L 3, 044 */
     set_mem(0x42c, 0x9d0000c0);  /* TIO 0c0 */
     set_mem(0x430, 0x4770042c);  /* BC  7,42c */
     set_mem(0x434, 0);
     set_mem(0x440, 0xff060000);  /* Wait PSW */
     set_mem(0x444, 0x14000408);
     set_mem(0x448, 0xffffffff);

     test_io_inst(0);
     if (verbose) {
         printf(" 0x38=%08x %08x", get_mem(0x38), get_mem(0x3c));
         printf(" 0x40=%08x %08x 700=%08x", get_mem(0x40), get_mem(0x44), get_mem(0x700));
         printf(" R0=%08x R1=%08x", get_reg(0), get_reg(1));
         printf(" R2=%08x R3=%08x", get_reg(2), get_reg(3));
         printf(" R4=%08x\n", get_reg(4));
         printf("0x600 = %08x %08x %08x %08x %08x\n", get_mem(0x600), get_mem(0x604), get_mem(0x608),
                 get_mem(0x60c), get_mem(0x610));
         printf("0x614 =  %08x %08x %08x %08x\n",get_mem(0x614), get_mem(0x618),
                 get_mem(0x61c), get_mem(0x620));
         printf("0x624 =  %08x %08x %08x %08x\n",get_mem(0x624), get_mem(0x628),
                 get_mem(0x62c), get_mem(0x630));
     }
     /* The result of a PCI can have a Address at different locations */
     ASSERT_EQUAL_X(0x00800000, get_reg(1) & 0xffbf0000);
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44) & 0xffbf0000);  /* Ignore Length error */
     ASSERT_EQUAL_X(0xff0600c0, get_mem(0x38));
     ASSERT_EQUAL_X(0x94000408, get_mem(0x3C));
}

/* Test halting non running device while other device working. */
CTEST2(io_test4, halt_io_sel) {
     int i;
     chan[0] = NULL;
     chan[1] = &data->dev1;
     init_cpu();
     log_trace("HIO 2\n");
     for (i = 0; i < 0x20; i++) {
         data->test_ctx1.buffer[i] = 0xe0 + i;
     }
     data->test_ctx1.max_data = 0x30;
     data->test_ctx1.burst = 0;
     set_mask(0x00);
     set_mem(0x40, 0xffffffff);  /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000408);
     set_mem(0x48, 0x500);   /* Set CAW */
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0x88000001);
     set_mem(0x508, 0x00000601); /* Set channel words */
     set_mem(0x50c, 0x0000002f);
     for (i = 0x600; i < 0x700; i += 4)
         set_mem(i, 0x55555555); /* Invalidate data */
     set_mem(0x700, 0xffffffff);
     set_mem(0x400, 0x9c0001c0); /* SIO 1c0 */
     set_mem(0x404, 0x82000440); /* LPSW 0440 */
     set_mem(0x408, 0x58000040); /* L 0, 040 */
     set_mem(0x40c, 0x58100044); /* L 1, 044 */
     set_mem(0x410, 0x58400448); /* L 4, 448 */
     set_mem(0x414, 0x50400040); /* ST 4,040 */
     set_mem(0x418, 0x50400044); /* ST 4,044 */
     set_mem(0x41c, 0x9e0001c1); /* HIO 1c1 */
     set_mem(0x420, 0x05400700); /* BALR 4,0, BCR 0,0 */
     set_mem(0x424, 0x58200040); /* L 2, 040 */
     set_mem(0x428, 0x58300044); /* L 3, 044 */
     set_mem(0x42c, 0x9d0001c0);  /* TIO 1c0 */
     set_mem(0x430, 0x4770042c);  /* BC  7,42c */
     set_mem(0x434, 0);
     set_mem(0x440, 0xff060000);  /* Wait PSW */
     set_mem(0x444, 0x14000408);
     set_mem(0x448, 0xffffffff);

     test_io_inst(0);
     if (verbose) {
         printf(" 0x38=%08x %08x", get_mem(0x38), get_mem(0x3c));
         printf(" 0x40=%08x %08x 700=%08x", get_mem(0x40), get_mem(0x44), get_mem(0x700));
         printf(" R0=%08x R1=%08x", get_reg(0), get_reg(1));
         printf(" R2=%08x R3=%08x", get_reg(2), get_reg(3));
         printf(" R4=%08x\n", get_reg(4));
         printf("0x600 = %08x %08x %08x %08x %08x\n", get_mem(0x600), get_mem(0x604), get_mem(0x608),
                 get_mem(0x60c), get_mem(0x610));
         printf("0x614 =  %08x %08x %08x %08x\n",get_mem(0x614), get_mem(0x618),
                 get_mem(0x61c), get_mem(0x620));
         printf("0x624 =  %08x %08x %08x %08x\n",get_mem(0x624), get_mem(0x628),
                 get_mem(0x62c), get_mem(0x630));
     }
     /* The result of a PCI can have a Address at different locations */
     ASSERT_EQUAL_X(0x00800000, get_reg(1) & 0xffbf0000);  /* Ignore Length error */
     ASSERT_EQUAL_X(0x60000422, get_reg(4));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44) & 0xffbf0000);  /* Ignore Length error */
     ASSERT_EQUAL_X(0xff0601c0, get_mem(0x38));
     ASSERT_EQUAL_X(0x94000408, get_mem(0x3C));
}
