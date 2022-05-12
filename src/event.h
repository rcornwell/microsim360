/*
 * microsim360 - Event scheduler
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

#ifndef _EVENT_H_
#define _EVENT_H_

#include "device.h"

typedef void (*_callback)(struct _device *dev, void *arg, int iarg);

struct _event {
    struct _event    *next;
    struct _event    *prev;

    int               time;       /* Number of cyles to event */
    _callback         func;       /* Function to call when timeout */
    struct _device   *dev;        /* Device event registered to */
    void             *arg;        /* Pointer to argument */
    int               iarg;       /* Integer argument */
};

/* Add an event */
int add_event(struct _device *dev, _callback func, int time, void *arg, int iarg);

/* Cancel event for device having callback func */
void cancel_event(struct _device *dev, _callback func);

/* Advance time by one clock cycle */
void advance();

/* Initialize event system */
void init_event();

#endif
