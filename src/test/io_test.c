/*
 * microsim360 - MPX IO instruction test cases.
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

CTEST_DATA(io_test) {
    struct _device dev;
    struct _test_context test_ctx;
};

CTEST_SETUP(io_test) {
    data->dev.bus_func = &test_dev;
    data->dev.dev = (void *)&data->test_ctx;
    data->dev.addr = 0xf;
    data->dev.next = NULL;
    data->test_ctx.addr = 0xf;
    chan[0] = &data->dev;
}

CTEST_TEARDOWN(io_test) {
}

/* Test Channel to valid channel */
CTEST2(io_test, tch) {
     init_cpu();
     log_trace("Test IO\n");
     data->test_ctx.state = 0;
     set_mem(0x400, 0x9f00000f);
     set_mem(0x404, 0x00000000);
     test_io_inst(0);
     ASSERT_EQUAL_X(CC0, CC_REG);
}

/* Test Channel to invalid channel */
CTEST2(io_test, tch2) {
     init_cpu();
     log_trace("Test IO\n");
     data->test_ctx.state = 0;
     set_mem(0x400, 0x9f00040f);
     set_mem(0x404, 0x00000000);
     test_io_inst(0);
     ASSERT_EQUAL_X(CC3, CC_REG);
}

/* Test IO to valid device */
CTEST2(io_test, tio) {
     init_cpu();
     log_trace("Test IO\n");
     data->test_ctx.state = 0;
     set_mem(0x400, 0x9d00000f);
     set_mem(0x404, 0x00000000);
     test_io_inst(0);
     ASSERT_EQUAL_X(CC0, CC_REG);
}

/* Test IO instruction to unassigned device */
CTEST2(io_test, tio2) {
     init_cpu();
     log_trace("Test IO2\n");
     data->test_ctx.state = 0;
     set_mem(0x400, 0x9d000010);
     set_mem(0x404, 0x00000000);
     test_io_inst(0);
     ASSERT_EQUAL_X(CC3, CC_REG);
}

CTEST2(io_test, sio_read_burst) {
     int i;
     init_cpu();
     for (i = 0; i < 0x10; i++) {
         data->test_ctx.buffer[i] = 0xf0 + i;
     }
     data->test_ctx.state = 0;
     data->test_ctx.max_data = 0x10;
     data->test_ctx.burst = 1;
     set_mask(0x00);
     set_mem(0x40, 0);         /* Set CSW to zero */
     set_mem(0x44, 0);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000420);
     set_mem(0x48, 0x500);   /* Set CAW */
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0x00000010);
     set_mem(0x600, 0x55555555); /* Invalidate data */
     set_mem(0x604, 0x55555555);
     set_mem(0x608, 0x55555555);
     set_mem(0x60C, 0x55555555);
     set_mem(0x400, 0x9c00000f); /* SIO 00f */
     set_mem(0x404, 0x82000410); /* LPSW 0410 */
     set_mem(0x420, 0x9d00000f);  /* TIO 00f */
     set_mem(0x424, 0x47000424);  /* BC  0,424 */
     set_mem(0x410, 0xff060000);  /* Wait PSW */
     set_mem(0x414, 0x14000408);

     test_io_inst(0);
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0xf0 + i, get_mem_b(0x600 + i));
     }
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0xff06000f, get_mem(0x38));
     ASSERT_EQUAL_X(0x94000408, get_mem(0x3C));
}

CTEST2(io_test, sio2_read) {
     int i;
     init_cpu();
     for (i = 0; i < 0x10; i++) {
         data->test_ctx.buffer[i] = 0xf0 + i;
     }
     data->test_ctx.state = 0;
     data->test_ctx.max_data = 0x10;
     data->test_ctx.burst = 0;
     set_mask(0x00);
     set_mem(0x40, 0);         /* Set CSW to zero */
     set_mem(0x44, 0);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000420);
     set_mem(0x48, 0x500);   /* Set CAW */
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0x00000010);
     set_mem(0x600, 0x55555555); /* Invalidate data */
     set_mem(0x604, 0x55555555);
     set_mem(0x608, 0x55555555);
     set_mem(0x60C, 0x55555555);
     set_mem(0x400, 0x9c00000f);  /* SIO 0f */
     set_mem(0x404, 0x82000410);  /* LPSW 410 */
     set_mem(0x420, 0x9d00000f);  /* TIO 0f */
     set_mem(0x424, 0x47000424);  /* BC 0,424 */
     set_mem(0x410, 0xff060000);  /* Wait state PSW */
     set_mem(0x414, 0x12000408);

     test_io_inst(0);
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0xf0 + i, get_mem_b(0x600 + i));
     }
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0xff06000f, get_mem(0x38));
     ASSERT_EQUAL_X(0x92000408, get_mem(0x3C));
}

CTEST2(io_test, sio3_write) {
     int i;
     init_cpu();
     for (i = 0; i < 0x10; i++) {
         data->test_ctx.buffer[i] = 0x55;
     }
     data->test_ctx.state = 0;
     data->test_ctx.max_data = 0x10;
     data->test_ctx.burst = 0;
     set_mask(0x00);
     set_mem(0x40, 0);         /* Set CSW to zero */
     set_mem(0x44, 0);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000420);
     set_mem(0x48, 0x500);   /* Set CAW */
     set_mem(0x500, 0x01000600); /* Set channel words */
     set_mem(0x504, 0x00000010);
     set_mem(0x600, 0x0F1F2F3F); /* Data to send */
     set_mem(0x604, 0x4F5F6F7F);
     set_mem(0x608, 0x8F9FAFBF);
     set_mem(0x60C, 0xCFDFEFFF);
     set_mem(0x400, 0x9c00000f);  /* SIO 0f */
     set_mem(0x404, 0x82000410);  /* LPSW 410 */
     set_mem(0x420, 0x9d00000f);  /* TIO 0f */
     set_mem(0x424, 0x47000424);  /* BC 0,424 */
     set_mem(0x410, 0xff060000);  /* Wait state PSW */
     set_mem(0x414, 0x12000408);

     test_io_inst2();
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x0f + (i << 4), data->test_ctx.buffer[i]);
     }
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0xff06000f, get_mem(0x38));
     ASSERT_EQUAL_X(0x92000408, get_mem(0x3C));
}

CTEST2(io_test, sio4_write_burst) {
     int i;
     init_cpu();
     for (i = 0; i < 0x10; i++) {
         data->test_ctx.buffer[i] = 0x55;
     }
     data->test_ctx.state = 0;
     data->test_ctx.max_data = 0x10;
     data->test_ctx.burst = 1;
     set_mask(0x00);
     set_mem(0x40, 0);         /* Set CSW to zero */
     set_mem(0x44, 0);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000420);
     set_mem(0x48, 0x500);   /* Set CAW */
     set_mem(0x500, 0x01000600); /* Set channel words */
     set_mem(0x504, 0x00000010);
     set_mem(0x600, 0x0F1F2F3F); /* Data to send */
     set_mem(0x604, 0x4F5F6F7F);
     set_mem(0x608, 0x8F9FAFBF);
     set_mem(0x60C, 0xCFDFEFFF);
     set_mem(0x400, 0x9c00000f);  /* SIO 0f */
     set_mem(0x404, 0x82000410);  /* LPSW 410 */
     set_mem(0x420, 0x9d00000f);  /* TIO 0f */
     set_mem(0x424, 0x47000424);  /* BC 0,424 */
     set_mem(0x410, 0xff060000);  /* Wait state PSW */
     set_mem(0x414, 0x12000408);

     test_io_inst2();
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x0f + (i << 4), data->test_ctx.buffer[i]);
     }
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0xff06000f, get_mem(0x38));
     ASSERT_EQUAL_X(0x92000408, get_mem(0x3C));
}


CTEST2(io_test, sio5_sense) {
     init_cpu();
     data->test_ctx.state = 0;
     log_trace("Sense\n");
     set_mem(0x40, 0);         /* Set CSW to zero */
     set_mem(0x44, 0);
     set_mem(0x48, 0x500);   /* Set CAW */
     set_mem(0x500, 0x04000600); /* Set channel words */
     set_mem(0x504, 0x00000001);
     set_mem(0x600, 0xffffffff);
     set_mem(0x400, 0x9c00000f);  /* SIO 0xf */
     set_mem(0x404, 0x9d00000f);  /* TIO 0xf */
     set_mem(0x408, 0x47700404);  /* BC 7,404 */
     set_mem(0x40C, 0x00000000);  /* 0 */
     test_io_inst(0);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00FFFFFF, get_mem(0x600));
}

CTEST2(io_test, sio6_nop) {
     init_cpu();
     data->test_ctx.state = 0;
     set_mem(0x40,  0xffffffff);         /* Set CSW to zero */
     set_mem(0x44,  0xffffffff);
     set_mem(0x48,  0x500);   /* Set CAW */
     set_mem(0x500, 0x03000600); /* Set channel words */
     set_mem(0x504, 0x00000001);
     set_mem(0x600, 0xffffffff);
     set_mem(0x400, 0x9c00000f);  /* SIO 0xf */
     set_mem(0x404, 0x47800410);  /* BC 7,410 */
     set_mem(0x408, 0x9d00000f);  /* TIO 0xf */
     set_mem(0x40c, 0x47700408);  /* BC 7,404 */
     set_mem(0x410, 0x00000000);  /* 0 */
     test_io_inst(0);
     ASSERT_EQUAL_X(0xffffffff, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c00ffff, get_mem(0x44));
     ASSERT_EQUAL_X(0xffFFFFFF, get_mem(0x600));
}

CTEST2(io_test, short_read) {
     int i;
     init_cpu();
     for (i = 0; i < 0x20; i++) {
         data->test_ctx.buffer[i] = 0x10 + i;
     }
     data->test_ctx.max_data = 0x20;
     data->test_ctx.burst = 1;
     set_mask(0x00);
     set_mem(0x40, 0);         /* Set CSW to zero */
     set_mem(0x44, 0);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000420);
     set_mem(0x48, 0x500);   /* Set CAW */
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
     set_mem(0x400, 0x9c00000f); /* SIO 00f */
     set_mem(0x404, 0x82000410); /* LPSW 0410 */
     set_mem(0x420, 0x9d00000f);  /* TIO 00f */
     set_mem(0x424, 0x47000424);  /* BC  0,424 */
     set_mem(0x410, 0xff060000);  /* Wait PSW */
     set_mem(0x414, 0x14000408);

     test_io_inst2();
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x10 + i, get_mem_b(0x600 + i));
     }
     for (; i < 0x20; i++) {
          ASSERT_EQUAL_X(0x55, get_mem_b(0x700 + i - 0x10));
     }
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c400000, get_mem(0x44));
     ASSERT_EQUAL_X(0xff06000f, get_mem(0x38));
     ASSERT_EQUAL_X(0x94000408, get_mem(0x3C));
}

CTEST2(io_test, short_read_sli) {
     int i;
     init_cpu();
     for (i = 0; i < 0x20; i++) {
         data->test_ctx.buffer[i] = 0x10 + i;
     }
     data->test_ctx.state = 0;
     data->test_ctx.max_data = 0x20;
     data->test_ctx.burst = 1;
     set_mask(0x00);
     set_mem(0x40, 0);         /* Set CSW to zero */
     set_mem(0x44, 0);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000420);
     set_mem(0x48, 0x500);   /* Set CAW */
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
     set_mem(0x400, 0x9c00000f); /* SIO 00f */
     set_mem(0x404, 0x82000410); /* LPSW 0410 */
     set_mem(0x420, 0x9d00000f);  /* TIO 00f */
     set_mem(0x424, 0x47000424);  /* BC  0,424 */
     set_mem(0x410, 0xff060000);  /* Wait PSW */
     set_mem(0x414, 0x14000408);

     test_io_inst2();
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x10 + i, get_mem_b(0x600 + i));
     }
     for (; i < 0x20; i++) {
          ASSERT_EQUAL_X(0x55, get_mem_b(0x700 + i - 0x10));
     }
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0xff06000f, get_mem(0x38));
     ASSERT_EQUAL_X(0x94000408, get_mem(0x3C));
}

CTEST2(io_test, short_write_burst) {
     int i;
     init_cpu();
     for (i = 0; i < 0x20; i++) {
         data->test_ctx.buffer[i] = 0x55;
     }
     data->test_ctx.state = 0;
     data->test_ctx.max_data = 0x20;
     data->test_ctx.burst = 1;
     set_mask(0x00);
     set_mem(0x40, 0);         /* Set CSW to zero */
     set_mem(0x44, 0);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000420);
     set_mem(0x48, 0x500);   /* Set CAW */
     set_mem(0x500, 0x01000600); /* Set channel words */
     set_mem(0x504, 0x00000010);
     set_mem(0x600, 0x0F1F2F3F); /* Data to send */
     set_mem(0x604, 0x4F5F6F7F);
     set_mem(0x608, 0x8F9FAFBF);
     set_mem(0x60C, 0xCFDFEFFF);
     set_mem(0x400, 0x9c00000f);  /* SIO 0f */
     set_mem(0x404, 0x82000410);  /* LPSW 410 */
     set_mem(0x420, 0x9d00000f);  /* TIO 0f */
     set_mem(0x424, 0x47000424);  /* BC 0,424 */
     set_mem(0x410, 0xff060000);  /* Wait state PSW */
     set_mem(0x414, 0x12000408);

     test_io_inst2();
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x0f + (i << 4), data->test_ctx.buffer[i]);
     }
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c400000, get_mem(0x44));
     ASSERT_EQUAL_X(0xff06000f, get_mem(0x38));
     ASSERT_EQUAL_X(0x92000408, get_mem(0x3C));
}

CTEST2(io_test, cda_read) {
     int i;
     init_cpu();
     for (i = 0; i < 0x20; i++) {
         data->test_ctx.buffer[i] = 0x10 + i;
     }
     data->test_ctx.state = 0;
     data->test_ctx.max_data = 0x20;
     data->test_ctx.burst = 1;
     set_mask(0x00);
     set_mem(0x40, 0);         /* Set CSW to zero */
     set_mem(0x44, 0);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000420);
     set_mem(0x48, 0x500);   /* Set CAW */
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
     set_mem(0x400, 0x9c00000f); /* SIO 00f */
     set_mem(0x404, 0x82000410); /* LPSW 0410 */
     set_mem(0x420, 0x9d00000f);  /* TIO 00f */
     set_mem(0x424, 0x47000424);  /* BC  0,424 */
     set_mem(0x410, 0xff060000);  /* Wait PSW */
     set_mem(0x414, 0x14000408);

     test_io_inst2();
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x10 + i, get_mem_b(0x600 + i));
     }
     for (; i < 0x20; i++) {
          ASSERT_EQUAL_X(0x10 + i, get_mem_b(0x700 + i - 0x10));
     }
     ASSERT_EQUAL_X(0x00000510, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0xff06000f, get_mem(0x38));
     ASSERT_EQUAL_X(0x94000408, get_mem(0x3C));
}

CTEST2(io_test, cda_read2) {
     int i;
     init_cpu();
     for (i = 0; i < 0x20; i++) {
         data->test_ctx.buffer[i] = 0x10 + i;
     }
     data->test_ctx.state = 0;
     data->test_ctx.max_data = 0x20;
     data->test_ctx.burst = 0;
     set_mask(0x00);
     set_mem(0x40, 0);         /* Set CSW to zero */
     set_mem(0x44, 0);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000420);
     set_mem(0x48, 0x500);   /* Set CAW */
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
     set_mem(0x400, 0x9c00000f); /* SIO 00f */
     set_mem(0x404, 0x82000410); /* LPSW 0410 */
     set_mem(0x420, 0x9d00000f);  /* TIO 00f */
     set_mem(0x424, 0x47000424);  /* BC  0,424 */
     set_mem(0x410, 0xff060000);  /* Wait PSW */
     set_mem(0x414, 0x14000408);

     test_io_inst2();
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x10 + i, get_mem_b(0x600 + i));
     }
     for (; i < 0x20; i++) {
          ASSERT_EQUAL_X(0x10 + i, get_mem_b(0x700 + i - 0x10));
     }
     ASSERT_EQUAL_X(0x00000510, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0xff06000f, get_mem(0x38));
     ASSERT_EQUAL_X(0x94000408, get_mem(0x3C));
}

CTEST2(io_test, write_cda) {
     int i;
     init_cpu();
     for (i = 0; i < 0x20; i++) {
         data->test_ctx.buffer[i] = 0x55;
     }
     data->test_ctx.state = 0;
     data->test_ctx.max_data = 0x20;
     data->test_ctx.burst = 0;
     set_mask(0x00);
     set_mem(0x40, 0);         /* Set CSW to zero */
     set_mem(0x44, 0);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000420);
     set_mem(0x48, 0x500);   /* Set CAW */
     set_mem(0x500, 0x01000600); /* Set channel words */
     set_mem(0x504, 0x80000010);
     set_mem(0x508, 0x01000700); /* Set channel words */
     set_mem(0x50c, 0x00000010);
     set_mem(0x600, 0x0F1F2F3F); /* Data to send */
     set_mem(0x604, 0x4F5F6F7F);
     set_mem(0x608, 0x8F9FAFBF);
     set_mem(0x60C, 0xCFDFEFFF);
     set_mem(0x700, 0x0C1C2C3C); /* Data to send */
     set_mem(0x704, 0x4C5C6C7C);
     set_mem(0x708, 0x8C9CACBC);
     set_mem(0x70C, 0xCCDCECFC);
     set_mem(0x400, 0x9c00000f);  /* SIO 0f */
     set_mem(0x404, 0x82000410);  /* LPSW 410 */
     set_mem(0x420, 0x9d00000f);  /* TIO 0f */
     set_mem(0x424, 0x47000424);  /* BC 0,424 */
     set_mem(0x410, 0xff060000);  /* Wait state PSW */
     set_mem(0x414, 0x12000408);

     test_io_inst2();
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x0f + (i << 4), data->test_ctx.buffer[i]);
     }
     for (; i < 0x20; i++) {
          ASSERT_EQUAL_X(0x0c + ((i - 0x10) << 4), data->test_ctx.buffer[i]);
     }
     ASSERT_EQUAL_X(0x00000510, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0xff06000f, get_mem(0x38));
     ASSERT_EQUAL_X(0x92000408, get_mem(0x3C));
}

CTEST2(io_test, cda_read_skip) {
     int i;
     init_cpu();
     for (i = 0; i < 0x10; i++) {
         data->test_ctx.buffer[i] = 0x10 + i;
     }
     data->test_ctx.state = 0;
     data->test_ctx.max_data = 0x10;
     data->test_ctx.burst = 1;
     set_mask(0x00);
     set_mem(0x40, 0);         /* Set CSW to zero */
     set_mem(0x44, 0);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000420);
     set_mem(0x48, 0x500);   /* Set CAW */
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0x90000005);
     set_mem(0x508, 0x02000606); /* Set channel words */
     set_mem(0x50c, 0x0000000b);
     set_mem(0x600, 0x55555555); /* Invalidate data */
     set_mem(0x604, 0x55555555);
     set_mem(0x608, 0x55555555);
     set_mem(0x60C, 0x55555555);
     set_mem(0x700, 0x55555555); /* Invalidate data */
     set_mem(0x704, 0x55555555);
     set_mem(0x708, 0x55555555);
     set_mem(0x70C, 0x55555555);
     set_mem(0x400, 0x9c00000f); /* SIO 00f */
     set_mem(0x404, 0x82000410); /* LPSW 0410 */
     set_mem(0x420, 0x9d00000f);  /* TIO 00f */
     set_mem(0x424, 0x47000424);  /* BC  0,424 */
     set_mem(0x410, 0xff060000);  /* Wait PSW */
     set_mem(0x414, 0x14000408);

     test_io_inst2();
     for (i = 0; i < 6; i++) {
          ASSERT_EQUAL_X(0x55, get_mem_b(0x600 + i));
     }
     for (; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x10 + i, get_mem_b(0x600 + i + 1));
     }
     ASSERT_EQUAL_X(0x00000510, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0xff06000f, get_mem(0x38));
     ASSERT_EQUAL_X(0x94000408, get_mem(0x3C));
}

CTEST2(io_test, read_back) {
     int i;
     init_cpu();
     for (i = 0; i < 0x10; i++) {
         data->test_ctx.buffer[i] = 0x10 + (0x0f - i);
     }
     data->test_ctx.state = 0;
     data->test_ctx.max_data = 0x10;
     data->test_ctx.burst = 1;
     set_mask(0x00);
     set_mem(0x40, 0);         /* Set CSW to zero */
     set_mem(0x44, 0);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000420);
     set_mem(0x48, 0x500);   /* Set CAW */
     set_mem(0x500, 0x0c00060f); /* Set channel words */
     set_mem(0x504, 0x00000010);
     set_mem(0x600, 0x55555555); /* Invalidate data */
     set_mem(0x604, 0x55555555);
     set_mem(0x608, 0x55555555);
     set_mem(0x60C, 0x55555555);
     set_mem(0x610, 0x55555555);
     set_mem(0x400, 0x9c00000f); /* SIO 00f */
     set_mem(0x404, 0x82000410); /* LPSW 0410 */
     set_mem(0x420, 0x9d00000f);  /* TIO 00f */
     set_mem(0x424, 0x47000424);  /* BC  0,424 */
     set_mem(0x410, 0xff060000);  /* Wait PSW */
     set_mem(0x414, 0x14000408);

     test_io_inst2();
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x10 + i, get_mem_b(0x600 + i));
     }
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0xff06000f, get_mem(0x38));
     ASSERT_EQUAL_X(0x94000408, get_mem(0x3C));
}

CTEST2(io_test, cmd_chain) {
     int i;
     init_cpu();
     for (i = 0; i < 0x10; i++) {
         data->test_ctx.buffer[i] = 0x55;
     }
     data->test_ctx.state = 0;
     data->test_ctx.max_data = 0x10;
     data->test_ctx.burst = 0;
     set_mask(0x00);
     log_trace("CMD CHAIN\n");
     set_mem(0x40, 0);         /* Set CSW to zero */
     set_mem(0x44, 0);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000420);
     set_mem(0x48, 0x500);   /* Set CAW */
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
     set_mem(0x400, 0x9c00000f);  /* SIO 0f */
     set_mem(0x404, 0x82000410);  /* LPSW 410 */
     set_mem(0x420, 0x9d00000f);  /* TIO 0f */
     set_mem(0x424, 0x47000424);  /* BC 0,424 */
     set_mem(0x410, 0xff060000);  /* Wait state PSW */
     set_mem(0x414, 0x12000408);

     test_io_inst2();
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x0f + (i << 4), data->test_ctx.buffer[i]);
     }
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0xff06000f, get_mem(0x38));
     ASSERT_EQUAL_X(0x92000408, get_mem(0x3C));
}

CTEST2(io_test, tic_error) {
     init_cpu();
     data->test_ctx.state = 0;
     data->test_ctx.max_data = 0x10;
     data->test_ctx.burst = 1;
     set_mask(0x00);
     set_mem(0x40, 0);         /* Set CSW to zero */
     set_mem(0x44, 0);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000420);
     set_mem(0x48, 0x500);   /* Set CAW */
     set_mem(0x500, 0x08000520); /* Set channel words */
     set_mem(0x504, 0x40000010);
     set_mem(0x520, 0x04000702); /* Set channel words */
     set_mem(0x524, 0x40000001);
     set_mem(0x700, 0xffffffff);
     set_mem(0x600, 0x0F1F2F3F); /* Data to send */
     set_mem(0x604, 0x4F5F6F7F);
     set_mem(0x608, 0x8F9FAFBF);
     set_mem(0x60C, 0xCFDFEFFF);
     set_mem(0x400, 0x9c00000f);  /* SIO 0f */
     set_mem(0x404, 0x47300400);  /* BC 3,404 */
     set_mem(0x408, 0x47800420);  /* BC 8,420 */
     set_mem(0x40c, 0x9d00000f);  /* TIO 0f */
     set_mem(0x410, 0x00000000);
     set_mem(0x420, 0x00000000);  /* stop */

     test_io_inst2();
     ASSERT_EQUAL_X(0x00000000, get_mem(0x40));
     ASSERT_EQUAL_X(0x00200000, get_mem(0x44));
}

CTEST2(io_test, tic_tic) {
     int i;
     init_cpu();
     for (i = 0; i < 0x10; i++) {
         data->test_ctx.buffer[i] = 0x55;
     }
     data->test_ctx.max_data = 0x10;
     data->test_ctx.burst = 0;
     set_mask(0x00);
     log_trace("TIC TIC\n");
     set_mem(0x40, 0);         /* Set CSW to zero */
     set_mem(0x44, 0);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000420);
     set_mem(0x48, 0x500);   /* Set CAW */
     set_mem(0x500, 0x01000600); /* Set channel words */
     set_mem(0x504, 0x40000010);
     set_mem(0x508, 0x08000518); /* TIC to 520 */
     set_mem(0x510, 0x04000701); /* Set channel words */
     set_mem(0x514, 0x00000001);
     set_mem(0x518, 0x08000510); /* TIC to 510 */
     set_mem(0x700, 0xffffffff);
     set_mem(0x600, 0x0F1F2F3F); /* Data to send */
     set_mem(0x604, 0x4F5F6F7F);
     set_mem(0x608, 0x8F9FAFBF);
     set_mem(0x60C, 0xCFDFEFFF);
     set_mem(0x400, 0x9c00000f);  /* SIO 0f */
     set_mem(0x404, 0x82000410);  /* LPSW 410 */
     set_mem(0x420, 0x9d00000f);  /* TIO 0f */
     set_mem(0x424, 0x47000424);  /* BC 0,424 */
     set_mem(0x410, 0xff060000);  /* Wait state PSW */
     set_mem(0x414, 0x12000408);

     test_io_inst2();
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x0f + (i << 4), data->test_ctx.buffer[i]);
     }
     ASSERT_EQUAL_X(0x00000520, get_mem(0x40));
     /* On the model 30 the high count byte indicates the error.
        The low byte is meaningless. */
     ASSERT_TRUE((0x00200000 & get_mem(0x44)) != 0);
     ASSERT_EQUAL_X(0xff06000f, get_mem(0x38));
     ASSERT_EQUAL_X(0x92000408, get_mem(0x3C));
}

CTEST2(io_test, tic_test) {
     int i;
     init_cpu();
     for (i = 0; i < 0x10; i++) {
         data->test_ctx.buffer[i] = 0x55;
     }
     data->test_ctx.state = 0;
     data->test_ctx.max_data = 0x10;
     data->test_ctx.burst = 0;
     set_mask(0x00);
     log_trace("TIC TEST\n");
     set_mem(0x40, 0);         /* Set CSW to zero */
     set_mem(0x44, 0);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000420);
     set_mem(0x48, 0x500);   /* Set CAW */
     set_mem(0x500, 0x01000600); /* Set channel words */
     set_mem(0x504, 0x40000010);
     set_mem(0x508, 0x08000520);
     set_mem(0x520, 0x03000701); /* Set channel words */
     set_mem(0x524, 0x40000001);
     set_mem(0x528, 0x04000701); /* Set channel words */
     set_mem(0x52c, 0x00000001);
     set_mem(0x700, 0xffffffff);
     set_mem(0x600, 0x0F1F2F3F); /* Data to send */
     set_mem(0x604, 0x4F5F6F7F);
     set_mem(0x608, 0x8F9FAFBF);
     set_mem(0x60C, 0xCFDFEFFF);
     set_mem(0x400, 0x9c00000f);  /* SIO 0f */
     set_mem(0x404, 0x82000410);  /* LPSW 410 */
     set_mem(0x420, 0x9d00000f);  /* TIO 0f */
     set_mem(0x424, 0x47000424);  /* BC 0,424 */
     set_mem(0x410, 0xff060000);  /* Wait state PSW */
     set_mem(0x414, 0x12000408);

     test_io_inst2();
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x0f + (i << 4), data->test_ctx.buffer[i]);
     }
     ASSERT_EQUAL_X(0x00000530, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0xff06000f, get_mem(0x38));
     ASSERT_EQUAL_X(0x92000408, get_mem(0x3C));
}

CTEST2(io_test, sms_test) {
     int i;
     init_cpu();
     for (i = 0; i < 0x10; i++) {
         data->test_ctx.buffer[i] = 0x55;
     }
     data->test_ctx.state = 0;
     data->test_ctx.max_data = 0x10;
     data->test_ctx.sms = 1;
     data->test_ctx.burst = 0;
     log_trace("SMS TEST\n");
     set_mask(0x00);
     set_mem(0x40, 0);         /* Set CSW to zero */
     set_mem(0x44, 0);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000420);
     set_mem(0x48, 0x500);   /* Set CAW */
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
     set_mem(0x400, 0x9c00000f);  /* SIO 0f */
     set_mem(0x404, 0x82000410);  /* LPSW 410 */
     set_mem(0x420, 0x9d00000f);  /* TIO 0f */
     set_mem(0x424, 0x47000424);  /* BC 0,424 */
     set_mem(0x410, 0xff060000);  /* Wait state PSW */
     set_mem(0x414, 0x12000408);

     test_io_inst2();
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x0f + (i << 4), data->test_ctx.buffer[i]);
     }
     ASSERT_EQUAL_X(0x00000548, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0xffffff00, get_mem(0x700));
     ASSERT_EQUAL_X(0xff06000f, get_mem(0x38));
     ASSERT_EQUAL_X(0x92000408, get_mem(0x3C));
}

CTEST2(io_test, pci_test) {
     int i;
     init_cpu();
     for (i = 0; i < 0x40; i++) {
         data->test_ctx.buffer[i] = 0x10 + i;
     }
     data->test_ctx.state = 0;
     data->test_ctx.max_data = 0x40;
     data->test_ctx.burst = 0;
     set_mask(0x00);
     log_trace("PCI TEST\n");
     set_mem(0x40, 0);         /* Set CSW to zero */
     set_mem(0x44, 0);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000408);
     set_mem(0x48, 0x500);   /* Set CAW */
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0x80000005);
     set_mem(0x508, 0x00000605); /* Set channel words */
     set_mem(0x50c, 0x8800000b);
     set_mem(0x510, 0x00000610); /* Set channel words */
     set_mem(0x514, 0x20000020);
     set_mem(0x600, 0x55555555); /* Invalidate data */
     set_mem(0x604, 0x55555555);
     set_mem(0x608, 0x55555555);
     set_mem(0x60C, 0x55555555);
     set_mem(0x610, 0x55555555);
     set_mem(0x614, 0x55555555);
     set_mem(0x618, 0x55555555);
     set_mem(0x61C, 0x55555555);
     set_mem(0x620, 0x55555555);
     set_mem(0x400, 0x9c00000f); /* SIO 00f */
     set_mem(0x404, 0x82000430); /* LPSW 0430 */
     set_mem(0x408, 0x58000040); /* L 0, 040 */
     set_mem(0x40c, 0x58100044); /* L 1, 044 */
     set_mem(0x410, 0x41200440); /* LA 2,440 */
     set_mem(0x414, 0x5020007c); /* ST 2,04c */
     set_mem(0x418, 0x82000438); /* LPSW 0438 */
     set_mem(0x440, 0x9d00000f);  /* TIO 00f */
     set_mem(0x444, 0x47700440);  /* BC  7,420 */
     set_mem(0x448, 0);
     set_mem(0x430, 0xff060000);  /* Wait PSW */
     set_mem(0x434, 0x14000408);
     set_mem(0x438, 0xff060000);  /* Wait PSW */
     set_mem(0x43c, 0x14000420);

     test_io_inst(0);
     for (i = 0; i < 0x20; i++) {
          ASSERT_EQUAL_X(0x10 + i, get_mem_b(0x600 + i));
     }
     /* The result of a PCI can have a Address at different locations */
     ASSERT_TRUE((0x00000510 == get_reg(0)) | (0x00000518 == get_reg(0)));
     ASSERT_EQUAL_X(0x00800000, get_reg(1) & 0xffff0000);
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0xff06000f, get_mem(0x38));
     ASSERT_EQUAL_X(0x94000420, get_mem(0x3C));
}

/* Test halt io on idle device */
CTEST2(io_test, halt_io) {
     int i;
     init_cpu();
     for (i = 0; i < 0x40; i++) {
         data->test_ctx.buffer[i] = 0x10 + i;
     }
     data->test_ctx.state = 0;
     data->test_ctx.max_data = 0x40;
     data->test_ctx.burst = 0;
     set_mask(0x00);
     set_mem(0x40, 0);         /* Set CSW to zero */
     set_mem(0x44, 0);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000408);
     set_mem(0x400, 0x9d00000f);  /* TIO 00f */
     set_mem(0x404, 0x47700400);  /* BC  7,420 */
     set_mem(0x408, 0x9e00000f); /* HIO 00f */
     set_mem(0x40c, 0);

     test_io_inst(0);
     ASSERT_EQUAL_X(CC1, CC_REG);
}


CTEST2(io_test, halt_io2) {
     int i;
     init_cpu();
     for (i = 0; i < 0x80; i++) {
         data->test_ctx.buffer[i] = (0x10 + i) & 0xff;
     }
     log_trace("HIO 2\n");
     data->test_ctx.state = 0;
     data->test_ctx.max_data = 0x80;
     data->test_ctx.burst = 0;
     set_mask(0x00);
     set_mem(0x40, 0);         /* Set CSW to zero */
     set_mem(0x44, 0);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000408);
     set_mem(0x48, 0x500);   /* Set CAW */
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0xc8000001);
     set_mem(0x508, 0x00000601); /* Set channel words */
     set_mem(0x50c, 0x8000003f);
     set_mem(0x510, 0x00000640); /* Set channel words */
     set_mem(0x514, 0x00000040);
     set_mem(0x518, 0x04000700); /* Set channel words */
     set_mem(0x51c, 0x00000001);
     for (i = 0x600; i < 0x700; i += 4)
         set_mem(i, 0x55555555); /* Invalidate data */
     set_mem(0x700, 0xffffffff);
     set_mem(0x400, 0x9c00000f); /* SIO 00f */
     set_mem(0x404, 0x82000430); /* LPSW 0430 */
     set_mem(0x408, 0x58000040); /* L 0, 040 */
     set_mem(0x40c, 0x58100044); /* L 1, 044 */
     set_mem(0x410, 0x9e00000f); /* HIO 00f */
     set_mem(0x414, 0x9d00000f);  /* TIO 00f */
     set_mem(0x418, 0x47700414);  /* BC  7,420 */
     set_mem(0x448, 0);
     set_mem(0x430, 0xff060000);  /* Wait PSW */
     set_mem(0x434, 0x14000408);
     set_mem(0x438, 0xff060000);  /* Wait PSW */
     set_mem(0x43c, 0x14000440);

     test_io_inst(0);
     /* The result of a PCI can have a Address at different locations */
     ASSERT_EQUAL_X(0x00800000, get_reg(1) & 0xffbf0000);  /* Ignore Length error */
     ASSERT_EQUAL_X(0x00000510, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44) & 0xffbf0000);  /* Ignore Length error */
     ASSERT_EQUAL_X(0xff06000f, get_mem(0x38));
     ASSERT_EQUAL_X(0x94000408, get_mem(0x3C));
     ASSERT_EQUAL_X(0xffffffff, get_mem(0x700));
}

CTEST2(io_test, tio_busy) {
     int i;
     init_cpu();
     for (i = 0; i < 0x80; i++) {
         data->test_ctx.buffer[i] = 0x10 + i;
     }
     data->test_ctx.state = 0;
     data->test_ctx.max_data = 0x80;
     data->test_ctx.burst = 0;
     log_trace("TIO Busy\n");
     set_mask(0x00);
     set_mem(0x40, 0);         /* Set CSW to zero */
     set_mem(0x44, 0);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000408);
     set_mem(0x48, 0x500);   /* Set CAW */
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0xc8000001);
     set_mem(0x508, 0x00000601); /* Set channel words */
     set_mem(0x50c, 0x0000007f);
     set_mem(0x510, 0x04000700); /* Set channel words */
     set_mem(0x514, 0x00000001);
     for (i = 0x600; i < 0x700; i += 4)
         set_mem(i, 0x55555555); /* Invalidate data */
     set_mem(0x700, 0xffffffff);
     set_mem(0x400, 0x9c00000f); /* SIO 00f */
     set_mem(0x404, 0x82000430); /* LPSW 0430 */
     set_mem(0x408, 0x9d00000f); /* TIO  00f */
     set_mem(0x40c, 0x05109d00); /* BALR 1,0, TIO 00f */
     set_mem(0x410, 0x000f0771); /* 00f, BCR 7,1 */
     set_mem(0x414, 0);
     set_mem(0x430, 0xff060000);  /* Wait PSW */
     set_mem(0x434, 0x14000408);
     set_mem(0x438, 0xff060000);  /* Wait PSW */
     set_mem(0x43c, 0x14000440);

     test_io_inst(0);
     /* The result of a PCI can have a Address at different locations */
     ASSERT_EQUAL_X(0x6000040e, get_reg(1));  /* CC2 and Loop address */
     ASSERT_EQUAL_X(0x00000510, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0xff06000f, get_mem(0x38));
     ASSERT_EQUAL_X(0x94000408, get_mem(0x3C));
     ASSERT_EQUAL_X(0xffffffff, get_mem(0x700));
}

CTEST2(io_test, read_prot) {
     int i;
     init_cpu();
     for (i = 0; i < 0x10; i++) {
         data->test_ctx.buffer[i] = 0x55;
     }
     data->test_ctx.state = 0;
     data->test_ctx.max_data = 0x10;
     data->test_ctx.burst = 1;
     set_mem_key(0x4000, 3);
     set_mask(0x00);
     set_mem(0x40, 0);         /* Set CSW to zero */
     set_mem(0x44, 0);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000420);
     set_mem(0x48,  0x20000500);   /* Set CAW */
     set_mem(0x500, 0x01004000); /* Set channel words */
     set_mem(0x504, 0x00000010);
     set_mem(0x4000, 0x0F1F2F3F); /* Data to send */
     set_mem(0x4004, 0x4F5F6F7F);
     set_mem(0x4008, 0x8F9FAFBF);
     set_mem(0x400C, 0xCFDFEFFF);
     set_mem(0x400, 0x9c00000f);  /* SIO 0f */
     set_mem(0x404, 0x82000410);  /* LPSW 410 */
     set_mem(0x420, 0x9d00000f);  /* TIO 0f */
     set_mem(0x424, 0x47000424);  /* BC 0,424 */
     set_mem(0x410, 0xff060000);  /* Wait state PSW */
     set_mem(0x414, 0x12000408);

     test_io_inst2();
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x0f + (i << 4), data->test_ctx.buffer[i]);
     }
     ASSERT_EQUAL_X(0x20000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0xff06000f, get_mem(0x38));
     ASSERT_EQUAL_X(0x92000408, get_mem(0x3C));
}

CTEST2(io_test, write_prot) {
     int i;
     init_cpu();
     for (i = 0; i < 0x10; i++) {
         data->test_ctx.buffer[i] = 0xf0 + i;
     }
     data->test_ctx.state = 0;
     data->test_ctx.max_data = 0x10;
     data->test_ctx.burst = 1;
     set_mem_key(0x4000, 3);
     log_trace("Prot\n");
     set_mask(0x00);
     set_mem(0x40, 0);         /* Set CSW to zero */
     set_mem(0x44, 0);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000420);
     set_mem(0x48,  0x20000500);   /* Set CAW */
     set_mem(0x500, 0x01004000); /* Set channel words */
     set_mem(0x504, 0x00000010);
     set_mem(0x4000, 0x55555555); /* Invalidate data */
     set_mem(0x4004, 0x55555555);
     set_mem(0x4008, 0x55555555);
     set_mem(0x400C, 0x55555555);
     set_mem(0x400, 0x9c00000f); /* SIO 00f */
     set_mem(0x404, 0x82000410); /* LPSW 0410 */
     set_mem(0x420, 0x9d00000f);  /* TIO 00f */
     set_mem(0x424, 0x47000424);  /* BC  0,424 */
     set_mem(0x410, 0xff060000);  /* Wait PSW */
     set_mem(0x414, 0x14000408);

     test_io_inst2();
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x55, get_mem_b(0x4000 + i));
     }
     ASSERT_EQUAL_X(0x20000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0xff06000f, get_mem(0x38));
     ASSERT_EQUAL_X(0x94000408, get_mem(0x3C));
}

CTEST2(io_test, read_prot2) {
     int i;
     init_cpu();
     for (i = 0; i < 0x10; i++) {
         data->test_ctx.buffer[i] = 0x55;
     }
     data->test_ctx.state = 0;
     data->test_ctx.max_data = 0x10;
     data->test_ctx.burst = 1;
     set_mem_key(0x4000, 3);
     set_mask(0x00);
     set_mem(0x40, 0);         /* Set CSW to zero */
     set_mem(0x44, 0);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000420);
     set_mem(0x48,  0x30000500);   /* Set CAW */
     set_mem(0x500, 0x01004000); /* Set channel words */
     set_mem(0x504, 0x00000010);
     set_mem(0x4000, 0x0F1F2F3F); /* Data to send */
     set_mem(0x4004, 0x4F5F6F7F);
     set_mem(0x4008, 0x8F9FAFBF);
     set_mem(0x400C, 0xCFDFEFFF);
     set_mem(0x400, 0x9c00000f);  /* SIO 0f */
     set_mem(0x404, 0x82000410);  /* LPSW 410 */
     set_mem(0x420, 0x9d00000f);  /* TIO 0f */
     set_mem(0x424, 0x47000424);  /* BC 0,424 */
     set_mem(0x410, 0xff060000);  /* Wait state PSW */
     set_mem(0x414, 0x12000408);

     test_io_inst2();
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0x0f + (i << 4), data->test_ctx.buffer[i]);
     }
     ASSERT_EQUAL_X(0x30000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0xff06000f, get_mem(0x38));
     ASSERT_EQUAL_X(0x92000408, get_mem(0x3C));
}

CTEST2(io_test, write_prot2) {
     int i;
     init_cpu();
     for (i = 0; i < 0x10; i++) {
         data->test_ctx.buffer[i] = 0xf0 + i;
     }
     data->test_ctx.state = 0;
     data->test_ctx.max_data = 0x10;
     data->test_ctx.burst = 1;
     set_mem_key(0x4000, 3);
     log_trace("Prot\n");
     set_mask(0x00);
     set_mem(0x40, 0);         /* Set CSW to zero */
     set_mem(0x44, 0);
     set_mem(0x78, 0x00000000);
     set_mem(0x7c, 0x00000420);
     set_mem(0x48,  0x30000500);   /* Set CAW */
     set_mem(0x500, 0x02004000); /* Set channel words */
     set_mem(0x504, 0x00000010);
     set_mem(0x4000, 0x55555555); /* Invalidate data */
     set_mem(0x4004, 0x55555555);
     set_mem(0x4008, 0x55555555);
     set_mem(0x400C, 0x55555555);
     set_mem(0x400, 0x9c00000f); /* SIO 00f */
     set_mem(0x404, 0x82000410); /* LPSW 0410 */
     set_mem(0x420, 0x9d00000f);  /* TIO 00f */
     set_mem(0x424, 0x47000424);  /* BC  0,424 */
     set_mem(0x410, 0xff060000);  /* Wait PSW */
     set_mem(0x414, 0x14000408);

     test_io_inst2();
     for (i = 0; i < 0x10; i++) {
          ASSERT_EQUAL_X(0xf0 + i, get_mem_b(0x4000 + i));
     }
     ASSERT_EQUAL_X(0x30000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0xff06000f, get_mem(0x38));
     ASSERT_EQUAL_X(0x94000408, get_mem(0x3C));
}

