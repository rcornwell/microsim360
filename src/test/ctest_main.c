/*
 * microsim360 - Simulator test cases.
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

#define CTEST_MAIN
//#ifndef _WIN32
#define CTEST_SEGFAULT
//#endif
#define CTEST_NO_COLORS

#include "ctest.h"
#include "device.h"
#include "conf.h"
#include "logger.h"

extern void init_tests();

extern char *test_log_file;
extern char *test_log_level;

extern int       verbose;

int
main(int argc, const char *argv[])
{
    int          i;
    const char  *args[2];
    int          arg_cnt;
    int          result;
    char         buffer[1024];
    args[0] = argv[0];
    arg_cnt = 1;
    for (i = 1; i < argc; i++) {
       if (strcmp(argv[i], "-d") == 0) {
           log_init(test_log_file);
           sprintf(buffer, "loglevel %s\n", test_log_level);
           load_line(buffer);
       } else if (strcmp(argv[i], "-v") == 0) {
           verbose = 1;
       } else {
           arg_cnt = 2;
           args[1] = argv[i];
       }
    }

    init_tests();
    result = ctest_main(arg_cnt, args);
    return result;
}

