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
 * 2540R 00C ctrl=000
 * 2540P 00D ctrl=000
 * 1403  00E ctrl=000 file="printout.txt"
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
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "logger.h"
#include "device.h"
#include "conf.h"

/*
 * Holds configutation file while reading.
 */
FILE      *config;

/*
 * Holds the current line from configuration file.
 */
char       line_buffer[1024];

/*
 * Pointer to where to grab next character from.
 */
char      *line_ptr;

/* Define header to look for */
DEV_LIST_SECTION struct _control model_list_start = {
        "list_start", 0, 0, NULL, NULL, DEV_LIST_MAGIC
    };


int
get_model(struct _option *opt)
{
    char           *p;
    int             len;
    char            temp1[20];
    char            temp2[20];
    int             flags = 0;
    /* Skip leading space */
    while(isspace(*line_ptr))
        line_ptr++;
    /* If comment or end of line, we got nothing */
    if (*line_ptr == '#' || *line_ptr == '\n' || *line_ptr == '\0')
        return 0;
    /* Grab either a word or a number */
    p = &opt->opt[0];
    len = 0;
    while (len < 20 && (isalnum(*line_ptr) || *line_ptr == '-' || *line_ptr == '/')) {
        /* Start of -#, save in temp1 */
        if (*line_ptr == '-' && (flags & 1) == 0) {
           flags |= 1;
           *p = '\0';
           p = &temp1[0];
           len = 0;
        /* Start of /#, save in temp2 */
        } else if (*line_ptr == '/' && (flags & 2) == 0) {
           flags |= 2;
           *p = '\0';
           p = &temp2[0];
           len = 0;
        /* Otherwise save it away */
        } else if (isalnum(*line_ptr)) {
           *p = toupper(*line_ptr);
            p++;
            len++;
        } else {
            fprintf(stderr, "Invalid character in option\n");
            free(opt);
            return 0;
        }
        line_ptr++;
    }
    *p = '\0';
    /* Check if primary option starts with numeric */
    if (isdigit(opt->opt[0])) {
        /* If so scan to end to see if letter is last character */
        for (p = &opt->opt[0]; isdigit(*p) && *p != '\0'; p++);
        /* Check if we are before last character and isalpha */
        if (*p != '\0' && isalpha(*p) && p[1] == '\0') {
           /* If so, copy character to model, and trim string */
           opt->model = *p;
           *p = '\0';
        }
    }

    /* Next check if there was a dash option followed by number */
    if (flags & 1) {
        int     i = 0;
        for (p = &temp1[0]; isdigit(*p); p++) {
            i = (i * 10) + (*p - '0');
        }
        opt->dash_num = i;
    }

    /* Next check if there was a slash option followed by number */
    if (flags & 2) {
        int     i = 0;
        for (p = &temp2[0]; isdigit(*p); p++) {
            i = (i * 10) + (*p - '0');
        }
        opt->slash_num = i;
    }
    opt->flags = flags;
    return 1;
}

int
get_addr(struct _option *opt)
{
    char           *p;
    int             len;
    int             i;
    char            temp1[20];
    /* Skip leading space */
    while(isspace(*line_ptr))
        line_ptr++;
    /* If comment or end of line, we got nothing */
    if (*line_ptr == '#' || *line_ptr == '\n' || *line_ptr == '\0')
        return 0;
    p = &temp1[0];
    len = 0;
    while (len < 20 && isxdigit(*line_ptr)) {
       *p = toupper(*line_ptr);
       p++;
       line_ptr++;
    }
    *p = '\0';
    if (temp1[0] == '\0')
        return 0;
    i = 0;
    p = &temp1[0];
    while (*p != '\0') {
        i <<= 4;
        if (isalpha(*p)) {
            i += (*p - 'A' + 10);
        } else {
            i += (*p - '0');
        }
        p++;
    }
    opt->addr = i;
    return 1;
}

int
get_string(struct _option *opt)
{
    char           *p;
    int             len;
    int             quote=0;

    /* Skip leading space */
    while(isspace(*line_ptr))
        line_ptr++;
    p = &opt->string[0];
    len = 0;
    /* If comment or end of line, we got nothing */
    if (*line_ptr == '#' || *line_ptr == '\n' || *line_ptr == '\0')
        return 0;
    /* Check if it starts with a " */
    if (*line_ptr == '"') {
        quote = 1;
        line_ptr++;
    }
    while (len < 1024) {
       if (*line_ptr == '"' && quote) {
           line_ptr++;
           if (*line_ptr == '"') {
               *p = '"';
               line_ptr++;
               len++;
           } else if (isspace(*line_ptr) || *line_ptr == '\n' || *line_ptr == '\0') {
               *p = '\0';
               return 1;
           }
       } else {
           if (quote == 0 && (isspace(*line_ptr) || *line_ptr == '\n' || *line_ptr == '\0')) {
               *p = '\0';
               return 1;
           }
           *p = *line_ptr++;
           if (quote == 0 && isalpha(*p))
               *p = toupper(*p);
           p++;
           len++;
       }
    }
    *p = '\0';
    return 1;
}

int
get_option(struct _option *opt)
{
    char           *p;
    int             len;

    opt->opt[0] = '\0';
    opt->string[0] = '\0';
    opt->flags = 0;
    /* Skip leading space */
    while(isspace(*line_ptr))
        line_ptr++;
    /* If comment or end of line, we got nothing */
    if (*line_ptr == '#' || *line_ptr == '\n' || *line_ptr == '\0')
        return 0;
    /* Grab either a word or a number */
    p = &opt->opt[0];
    len = 0;
    while (len < 20 && (isalnum(*line_ptr) || *line_ptr == '-')) {
       *p = toupper(*line_ptr);
       p++;
       len++;
       line_ptr++;
    }
    *p = '\0';
    /* Check if we have equals sign */
    if (*line_ptr == '=') {
        line_ptr++;
        if (get_string(opt)) {
            opt->flags = 1;
        } else {
            return 0;
        }
    }
    return 1;
}

int
get_integer(struct _option *opt, int *value)
{
    char           *p;

    *value = 0;
    if (opt->string[0] == '\0') {
       fprintf(stderr, "Option %s requires a number\n", opt->opt);
       return 0;
    }

    for (p = &opt->string[0]; *p != 0; p++) {
        if (isdigit(*p)) {
            *value = (*value * 10) + (*p - '0');
        } else {
            fprintf(stderr, "Option %s requires a number (%s)\n", opt->opt, opt->string);
            return 0;
        }
    }
    return 1;
}

int
get_index(struct _option *opt, char *list[])
{
    int        i;

    if (opt->string[0] == '\0') {
       fprintf(stderr, "Option %s requires a value\n", opt->opt);
       return 0;
    }

    for (i = 0; list[i] != NULL; i++) {
        if (strcmp(opt->string, list[i]) == 0)
            return i;
    }
    fprintf(stderr, "Option %s not valid (%s)\n", opt->opt, opt->string);
    return -1;
}

/*
 * Find first device list entry.
 */
struct _control *
dev_list()
{
    struct _control *list_begin = &model_list_start;
    while (1) {
        struct _control *t = list_begin;
        t--;
        if (t->magic != DEV_LIST_MAGIC)
            break;
        list_begin--;
    }
    return list_begin;
}

int
load_config(char *name)
{
    struct _option opt;
    struct _control *devlist;
    int            fnd;

    if ((config = fopen(name, "r")) == NULL) {
        fprintf(stderr, "Unable to open configuration file: %s\n", name);
        return 0;
    }

    while (fgets(line_buffer, sizeof(line_buffer), config) != NULL) {
       line_ptr = line_buffer;
/*       fprintf(stderr, "line=%s", line_buffer); */
       fnd = 0;
       if (get_model(&opt)) {
           for (devlist = dev_list(); devlist->magic == DEV_LIST_MAGIC && fnd == 0; devlist++) {
               if (strcmp(opt.opt, devlist->name) == 0) {
                   fnd = 1;
                   switch(devlist->type) {
                   case HEAD_TYPE:
                             break;
                   case DEV_TYPE:
                   case CTRL_TYPE:
                   case UNIT_TYPE:
                             if (get_addr(&opt) == 0) {
                                fprintf(stderr, "Missing address on %s\n", opt.opt);
                                fclose(config);
                                return 0;
                             }
                             /* Fall through */
                   case CPU_TYPE:
                             if ((devlist->create)(&opt) == 0) {
                                fprintf(stderr, "Unable to create device %s\n", opt.opt);
                                fclose(config);
                                return 0;
                             }
                             break;
                   case LOG_TYPE:
                             if ((devlist->create)(&opt) == 0) {
                                fprintf(stderr, "Unable to set log %s\n", opt.opt);
                                fclose(config);
                                return 0;
                             }
                             break;
                   default:
                             fprintf(stderr, "Unknown type %d\n", devlist->type);
                             break;
                   }
               }
           }
           if (fnd == 0) {
                fprintf(stderr, "Unknown device %s\n", opt.opt);
           }
       }
    }
    fclose(config);
    return 1;
}

int
load_line(char *line)
{
    struct _option opt;
    struct _control *devlist;
    int            fnd;

    line_ptr = line;
/*    fprintf(stderr, "line=%s", line_buffer); */
    fnd = 0;
    if (get_model(&opt)) {
        for (devlist = dev_list(); devlist->magic == DEV_LIST_MAGIC; devlist++) {
            if (strcmp(opt.opt, devlist->name) == 0) {
                fnd = 1;
                switch(devlist->type) {
                case HEAD_TYPE:
                          break;
                case DEV_TYPE:
                case CTRL_TYPE:
                case UNIT_TYPE:
                          if (get_addr(&opt) == 0) {
                             fprintf(stderr, "Missing address on %s\n", opt.opt);
                             fclose(config);
                             return 0;
                          }
                          /* Fall through */
                case CPU_TYPE:
                          if ((devlist->create)(&opt) == 0) {
                             fprintf(stderr, "Unable to create device %s\n", opt.opt);
                             fclose(config);
                             return 0;
                          }
                          break;
                case LOG_TYPE:
                          if ((devlist->create)(&opt) == 0) {
                             fprintf(stderr, "Unable to set log %s\n", opt.opt);
                             fclose(config);
                             return 0;
                          }
                          break;
                default:
                          fprintf(stderr, "Unknown type %d\n", devlist->type);
                          break;
                }
            }
        }
        if (fnd == 0) {
             fprintf(stderr, "Unknown device %s\n", opt.opt);
        }
    }
    return 1;
}


