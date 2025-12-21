/*
 * microsim360 - Configuration file parser
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

/*
 * CPU size A     2k
 *          B     4k
 *          C     8k
 *          D    16k
 *          E    32k
 *          F    64k
 *          G   128k
 *          H   256k
 *          I   512k
 *          J  1024k
 *          K  2048k
 *          L  4096K
 */

/* Configuration file format:
 *
 * logfile "string"
 * log option [[,] option]
 * cpunumber memsize
 * 1050 port=# (default 3270).
 * controller [address] option=opt
 * unit       address   option=opt file="name" label=value
 * # rest of line is comment.
 *
 * option[=valu]e can be seperated with , or just blanks.
 * label=value will create a standard label volume with value.
 * file="name" will call attach after processing options.
 * new means that a new file should be created.
 *
 */

/* Example
 * 2030E/1    # Specifies a Model 30 with 1 selector channel.
 * 1050 port=3200
 * 2821  000
 * 2540R 00C
 * 2540P 00D
 * 1403  00E file="printout.txt"
 * 2415  0c0 7track
 * 2400  0c0 file="systap.tap"
 * 2400  0c1 file="sys001.tap" ring
 * 2400  0c2 file="sys002.tap" ring
 * 2400  0c3 file="sys003.tap" ring
 * 2400  0c4 file="sys004.tap" ring
 * 2400  0c5 7track
 * 2841  190
 * 2311  190 file="system.ckd"
 * 2311  191 file="data.ckd" new label=111111
 *
 */

#include "config.h"
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "logger.h"
#include "device.h"
#include "widgets.h"
#include "conf.h"
#ifdef _WIN32
#include "getopt.h"
#endif

int
main(int argc, char *argv[])
{
    int    c;
    char  *conf_file = NULL;
    char  *log_file = NULL;
    struct _device   *dev;
    int               scr_wid, scr_hi;
    int               i;

    opterr = 0;

    while((c = getopt(argc, argv, "l:f:")) != -1) {
       switch (c) {
       case 'l':
            log_file = optarg;
            break;
       case 'f':
            conf_file = optarg;
            break;
       case '?':
            if (optopt == 'f')
                fprintf(stderr, "Option -%c requires a file name.\n", optopt);
            else if (isprint (optopt))
                fprintf(stderr, "Unknown option '-%c'.\n", optopt);
            else
                fprintf(stderr, "Unknown option character '\\x%x'.\n", optopt);
            exit(1);
       default:
            abort();
       }
    }

    if (log_file != NULL) {
       log_init(log_file);
       log_level = LOG_INFO|LOG_WARN|LOG_ERROR;
    }
    if (conf_file != NULL) {
       if (load_config(conf_file) == 0) {
          fprintf(stderr, "error in configuration: %s\n", conf_file);
          exit(1);
       }
    }
    for (i = 0; i < 6; i++) {
        for (dev = chan[i]; dev != NULL; dev = dev->next) {
             log_info("Device %03x %s\n", dev->addr, dev->type_name);
        }
    }
    layout_periph(&scr_wid, &scr_hi);
    if (title != NULL) {
        SDL_Setup(title, scr_wid, scr_hi);
        run_sim();
    }
}


