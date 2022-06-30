/*
 * microsim360 - Options structure
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

#ifndef _OPTION_H_
#define _OPTION_H_

#include <limits.h>
#include <stdint.h>

/* Options to create function */
struct _option {
    char      opt[20];
    int       flags;
    char      model;
    int       dash_num;
    int       slash_num;
    uint16_t  addr;
    char      string[PATH_MAX];
};

#define HEAD_TYPE    0
#define CPU_TYPE     1
#define DEVICE_TYPE  2
#define CTRL_TYPE    3
#define UNIT_TYPE    4
#define LOG_TYPE     5

#define CHAR_OPT     1
#define NUM_MOD      2
#define NUM_OPT      4

#define DEV_LIST_MAGIC   0xdeadbeef

#if defined(_MSC_VER)
#pragma section(".devlist", read, shared)
#define DEV_LIST_SECTION __declspec(allocate(".devlist")) __declspec(align(1))
#elif defined(__APPLE__)
#define DEV_LIST_SECTION __attribute__ ((used, section ("__DATA, .devlist"), aligned(1)))
#else
#define DEV_LIST_SECTION __attribute__ ((used, section (".devlist"), aligned(1)))
#endif


#define DEV_LIST_STRUCT(mod, type, opts) \
    DEV_LIST_SECTION static struct _control model_##mod = { \
        #mod, type, opts, model##mod##_create, NULL, DEV_LIST_MAGIC, \
    }

#define STRINGIFY(x) #x

#define LOG_OPT_STRUCT(opt) \
    DEV_LIST_SECTION static struct _control log_##opt = { \
        STRINGIFY(LOG##opt), LOG_TYPE, 0, log##opt##_create, NULL, DEV_LIST_MAGIC, \
    }


/* Example
 * 2030E/1    # Specifies a Model 30 with 1 selector channel.
 * 1050 port=3200
 * 2821  000
 * 2540R 00C
 * 2540P 00D
 * 1403  00E file="printout.txt"
 * 2415  0c0 7-track
 * 2400  0c0 file="systap.tap"
 * 2400  0c1 file="sys001.tap" new ring
 * 2400  0c2 file="sys002.tap" new ring
 * 2400  0c3 file="sys003.tap" new ring
 * 2400  0c4 file="sys004.tap" new ring
 * 2400  0c5 7-track
 * 2841  190
 * 2311  190 file="system.ckd"
 * 2311  191 file="data.ckd" new label=111111
 *
 */

int get_model(struct _option *opt);

int get_addr(struct _option *opt);

int get_string(struct _option *opt);

int get_option(struct _option *opt);

int get_integer(struct _option *opt, int *value);

int get_index(struct _option *opt, char *list[]);

int load_config(char *name);

int load_line(char *line);

#endif
