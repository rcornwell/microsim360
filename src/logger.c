/*
 * microsim360 - Logger routines.
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

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include "logger.h"

int log_level;
extern uint64_t     step_count;

FILE *log_file = NULL;

struct _log_type {
     int           mask;
     char         *name;
} log_type[] = {
     { LOG_TRACE,  "TRACE" },
     { LOG_INFO,  "INFO" },
     { LOG_WARN,  "WARN" },
     { LOG_ITRACE,  "ITRACE" },
     { LOG_MICRO,  "MICRO" },
     { LOG_REG,  "REG" },
     { LOG_MPXCHN,  "MPXCHN" },
     { LOG_SELCHN,  "SELCHN" },
     { LOG_DEVICE,  "DEVICE" },
     { LOG_CONSOLE, "CONSOLE" },
     { LOG_TAPE,    "TAPE" },
     { LOG_DISK,    "DISK" },
     { LOG_CARD,    "CARD" },
     { 0, NULL},
};


void
log_init(char *filename)
{
    log_file = fopen(filename, "w");
    if (log_file == NULL) {
        fprintf(stderr, "Unable to open log file %s\n", filename);
        exit(1);
    }
}

static int last_level = 0;

void 
log_print_s(int level, char *file, int line, const char *fmt, ...)
{
    va_list        ap;
    int            i;

    if (log_file == NULL)
        return;
    if (last_level != 0) {
       fprintf(log_file, "\n");
    }
#ifdef LOG_FILE
    fprintf(log_file, "%ld:[%s:%d] ", step_count, file, line);
#else
    fprintf(log_file, "%ld: ", step_count);
#endif
    for (i = 0; log_type[i].mask != 0; i++) {
        if (log_type[i].mask == level) {
            fprintf(log_file, "%s ", log_type[i].name);
            break;
        }
    }
    last_level = level;
    va_start(ap, fmt);
    vfprintf(log_file, fmt, ap);
    va_end(ap);
}
void 
log_print_c(int level, const char *fmt, ...)
{
    va_list        ap;
    int            i;

    if (log_file == NULL)
        return;
    if (last_level == 0) {
#ifdef LOG_FILE
        fprintf(log_file, "%ld:[%s:%d] ", step_count, file, line);
#else
        fprintf(log_file, "%ld: ", step_count);
#endif
        for (i = 0; log_type[i].mask != 0; i++) {
            if (log_type[i].mask == level) {
                fprintf(log_file, "%s ", log_type[i].name);
                break;
            }
        }
        last_level = level;
    }
    va_start(ap, fmt);
    vfprintf(log_file, fmt, ap);
    va_end(ap);
}


void 
log_print(int level, char *file, int line, const char *fmt, ...)
{
    va_list        ap;
    int            i;

    if (log_file == NULL)
        return;
    if (last_level != 0) {
       fprintf(log_file, "\n");
       last_level = 0;
    }
#ifdef LOG_FILE
        fprintf(log_file, "%ld:[%s:%d] ", step_count, file, line);
#else
        fprintf(log_file, "%ld: ", step_count);
#endif
    for (i = 0; log_type[i].mask != 0; i++) {
        if (log_type[i].mask == level) {
            fprintf(log_file, "%s ", log_type[i].name);
            break;
        }
    }
    va_start(ap, fmt);
    vfprintf(log_file, fmt, ap);
    va_end(ap);
    fflush(log_file);
}

