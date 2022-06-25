/*
 * microsim360 - IBM360 model 30 1052A interface.
 *
 * Copyright 2021, Richard Cornwell
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

#include <stdint.h>

#ifndef _MODEL1052_H_
#define _MODEL1052_H_
#include "device.h"

struct _device *model1052_init(void *render, uint16_t addr);
void *model1052_init_ctx(uint16_t port);
void  model1052_out(void *data, uint16_t out_char);
void  model1052_in(void *data, uint16_t *in_char);
void  model1052_func(void *data, uint16_t *tags_out, uint16_t tags_in, uint16_t *t_request);
void  model1052_done(void *data);
int   model1052_thrd(void *data);
void  model1052_dev(struct _device *unit, uint16_t *tags, uint16_t bus_out, uint16_t *bus_in);

#endif
