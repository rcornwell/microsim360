/*
 * microsim360 - Model 2030 microcode simulator.
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "logger.h"
#include "device.h"
#include "cpu.h"
#include "model2030.h"
#include "model1052.h"

struct _device *
model2030_init(void *render, uint16_t addr)
{
    INT_TMR = 1;   /* By default enable interval timer */
    return NULL;
}

/* Create a 2030 cpu system. */
int
model2030_create(struct _option *opt)
{
    extern  void *setup_fp2030(char *title);
    int     msize;
    uint16_t         port = 3270;
    struct _option   opts;

    if (title != NULL) {
        fprintf(stderr, "CPU already defined, can't support more then one\n");
        return 0;
    }
    title = "IBM360/30";
    setup_cpu = &setup_fp2030;
    step_cpu = &cycle_2030;

    while (get_option(&opts)) {
         int       v;

         if (strcmp(opts.opt, "PORT") == 0 && get_integer(&opts, &v)) {
             port = v;
         } else {
             fprintf(stderr, "Invalid option %s\n", opts.opt);
             return 0;
         }
    }

    if (opt->model != '\0') {
        msize = 2048 << (opt->model - 'A');
        if (msize > (64 * 1024)) {
            return 0;
        }
    } else {
        msize = 64 * 1024;
    }
    if ((M = (uint32_t *)calloc(msize, sizeof(uint32_t))) == NULL)
        return 0;
    mem_max = msize - 1;
    log_info("Model 30 configured %d %04x mem\n", msize, mem_max);
    cpu_2030.console = model1052_init_ctx(port);
    INT_TMR = 1;   /* By default enable interval timer */
    return 1;
}

