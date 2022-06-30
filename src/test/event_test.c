/*
 * microsim360 - Event system test cases.
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

#include "ctest.h"
#include "device.h"
#include "event.h"

uint64_t   step_count;

int        a_time;
int        b_time;
int        c_time;
int        d_time;

int        a_data;
int        b_data;
int        c_data;
int        d_data;

/* Callbacks, save step count in routine time and
   set argument to iarg.
*/
static void
a_callback(struct _device *unit, void *arg, int iarg)
{
    if (arg != NULL)
       *((int *)arg) = iarg;
    a_time = step_count;
}

static void
b_callback(struct _device *unit, void *arg, int iarg)
{
    if (arg != NULL)
        *((int *)arg) = iarg;
    b_time = step_count;
}

static void
c_callback(struct _device *unit, void *arg, int iarg)
{
    if (arg != NULL)
        *((int *)arg) = iarg;
    c_time = step_count;
    add_event(unit, &a_callback, iarg, arg, iarg);
}

static void
d_callback(struct _device *unit, void *arg, int iarg)
{
    if (arg != NULL)
        *((int *)arg) = iarg;
    d_time = step_count;
}

void
init_test()
{
    step_count = 0;
    a_time = 0;
    a_data = 0;
    b_time = 0;
    b_data = 0;
    c_time = 0;
    c_data = 0;
    d_time = 0;
    d_data = 0;
}

/* Schedule event and make sure we get callback */
CTEST(event, test1) {
    struct _device  dev;

    init_test();
    add_event(&dev, &a_callback, 10, (void *)&a_data, 1);
    while (step_count < 20) {
        step_count++;
        advance();
    };
    ASSERT_EQUAL(10, a_time);
    ASSERT_EQUAL(1, a_data);
}

/* Schedule 2 events and make sure they both fire */
CTEST(event, test2) {
    struct _device  dev;

    init_test();
    add_event(&dev, &a_callback, 10, (void *)&a_data, 1);
    add_event(&dev, &b_callback, 20, (void *)&b_data, 2);
    while (step_count < 30) {
        step_count++;
        advance();
    };
    ASSERT_EQUAL(10, a_time);
    ASSERT_EQUAL(1, a_data);
    ASSERT_EQUAL(20, b_time);
    ASSERT_EQUAL(2, b_data);
}

/* Schedule 2 events for same time, make sure they fire at correct time */
CTEST(event, test3) {
    struct _device  dev;

    init_test();
    add_event(&dev, &a_callback, 20, (void *)&a_data, 1);
    add_event(&dev, &b_callback, 20, (void *)&b_data, 2);
    while (step_count < 30) {
        step_count++;
        advance();
    };
    ASSERT_EQUAL(20, a_time);
    ASSERT_EQUAL(1, a_data);
    ASSERT_EQUAL(20, b_time);
    ASSERT_EQUAL(2, b_data);
}

/* Add in an event at callback time */
CTEST(event, test4) {
    struct _device  dev;

    init_test();
    add_event(&dev, &c_callback, 10, (void *)&a_data, 5);
    add_event(&dev, &b_callback, 20, (void *)&b_data, 2);
    while (step_count < 30) {
        step_count++;
        advance();
    };
    ASSERT_EQUAL(15, a_time);
    ASSERT_EQUAL(5, a_data);
    ASSERT_EQUAL(20, b_time);
    ASSERT_EQUAL(2, b_data);
    ASSERT_EQUAL(10, c_time);
    ASSERT_EQUAL(0, c_data);
}

/* Schedule 3 events, last one before first, make sure all are correct */
CTEST(event, test5) {
    struct _device  dev;

    init_test();
    add_event(&dev, &a_callback, 20, (void *)&a_data, 1);
    add_event(&dev, &b_callback, 20, (void *)&b_data, 2);
    add_event(&dev, &d_callback, 25, (void *)&d_data, 3);
    while (step_count < 30) {
        step_count++;
        advance();
    };
    ASSERT_EQUAL(20, a_time);
    ASSERT_EQUAL(1, a_data);
    ASSERT_EQUAL(20, b_time);
    ASSERT_EQUAL(2, b_data);
    ASSERT_EQUAL(25, d_time);
    ASSERT_EQUAL(3, d_data);
}
CTEST(event, test6) {
    struct _device  dev;

    init_test();
    add_event(&dev, &a_callback, 10, (void *)&a_data, 5);
    add_event(&dev, &b_callback, 20, (void *)&b_data, 2);
    while (step_count < 30) {
        step_count++;
        advance();
        if (a_data == 5) {
           cancel_event(&dev, &b_callback);
        }
    };
    ASSERT_EQUAL(10, a_time);
    ASSERT_EQUAL(5, a_data);
    ASSERT_EQUAL(0, b_time);
    ASSERT_EQUAL(0, b_data);
}

CTEST(event, test7) {
    struct _device  dev;

    init_test();
    add_event(&dev, &a_callback, 10, (void *)&a_data, 5);
    add_event(&dev, &b_callback, 20, (void *)&b_data, 2);
    add_event(&dev, &d_callback, 30, (void *)&d_data, 3);
    while (step_count < 30) {
        step_count++;
        advance();
        if (a_data == 5) {
           cancel_event(&dev, &b_callback);
        }
    };
    ASSERT_EQUAL(10, a_time);
    ASSERT_EQUAL(5, a_data);
    ASSERT_EQUAL(0, b_time);
    ASSERT_EQUAL(0, b_data);
    ASSERT_EQUAL(30, d_time);
    ASSERT_EQUAL(3, d_data);
}

