/*
 * microsim360 - Model 2030 CPU instruction test cases.
 *
 * Copyright 2022, Richard Cornwell
 *                 Original test cases by Ken Shirriff
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
#include "model2050.h"
#include "model_test.h"
#include "test_device.h"

struct _device *chan[6];

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
    add_chan(&data->dev, 0xf);
}

CTEST_TEARDOWN(io_test) {
}

CTEST2(io_test, tio) {
     init_cpu();
     set_mem(0x400, 0x9d00000f);
     test_inst(0);
     ASSERT_EQUAL(CC0, CC_REG);
}

CTEST2(io_test, sio) {
     init_cpu();
     set_mem(0x48, 0x500);   /* Set CAW */
     set_mem(0x500, 0x04000600); /* Set channel words */
     set_mem(0x504, 0x00000001);
     set_mem(0x600, 0xffffffff);
     set_mem(0x400, 0x9c00000f);
     set_mem(0x404, 0x9d00000f);
     test_inst2(0);
     printf("CC = %x 600=%08x  0x40=%08x %08x\n", CC_REG, get_mem(0x600), get_mem(0x40), get_mem(0x44));
}
