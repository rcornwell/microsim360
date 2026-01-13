/*
 * microsim360 - Model 2415 tape controller test.
 *
 * Copyright 2025, Richard Cornwell
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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <sys/types.h>
#include <unistd.h>
#endif
#include "logger.h"
#include "device.h"
#include "test_chan.h"
#include "event.h"
#include "ctest.h"
#include "xlat.h"
#include "tape.h"
#include "model2415.h"

uint64_t   step_count = 0;
int        verbose = 0;
char       *test_log_file = "model2415_debug.log";
char       *test_log_level = "info warn error trace device tape";

/* 
 * Panel display functions
 */
void
model2415_draw(struct _device *unit, void *rend, int u)
{
}

void
model2415_init(struct _device *unit, void *rend)
{
}

/*
 * Popup device control panel.
 */

void *
model2415_control(struct _device *unit, int u, int x, int y)
{
    return NULL;
}

void
init_tests()
{
    struct _device *dev2415;
    struct _2415_context *ctx;
    init_event();
    /* Create devices for testing. */
    if ((dev2415 = calloc(1, sizeof(struct _device))) == NULL)
        return;
    if ((ctx = calloc(1, sizeof(struct _2415_context))) == NULL) {
        free(dev2415);
        return;
    }

    tape_init();

    dev2415->bus_func = &model2415_dev;
    dev2415->dev = (void *)ctx;
    dev2415->type_name = "2415";
    dev2415->n_units = 2;
    dev2415->addr = 0xC0;
    ctx->addr = 0xC0;
    ctx->chan = 0;
    ctx->state = STATE_IDLE;
    ctx->selected = 0;
    ctx->nunits = dev2415->n_units;
    ctx->track_7 = 1;
    add_chan(dev2415, dev2415->addr);
    //tape->tape[0] = (struct _tape_buffer *)calloc(1, sizeof(struct _tape_buffer));
    //tape->tape[0]->format = 0;
    //tape_attach(tape->tape[0], "tape1.tap", TYPE_TAP, 1, 1);
    //tape->tape[1] = (struct _tape_buffer *)calloc(1, sizeof(struct _tape_buffer));
    //tape->tape[1]->format = 0;
    //tape_attach(tape->tape[1], "tape1.tap", TYPE_TAP, 1, 1);
}

/* Write out a tape format block */
static void
write_block(FILE *f, uint32_t len, uint8_t *buffer, int type)
{
    uint32_t wlen = 0x7fffffff & len;
    uint8_t  xlen[4];

    switch(type) {
    case TYPE_TAP:
         wlen = 0x7fffffff & ((len + 1) & ~1);
         /* Fall through */
    case TYPE_E11:
         xlen[0] = (len) & 0xff;
         xlen[1] = (len >> 8) & 0xff;
         xlen[2] = (len >> 16) & 0xff;
         xlen[3] = (len >> 24) & 0xff;;
         fwrite(xlen, sizeof(uint8_t), 4, f);
         fwrite(buffer, sizeof(uint8_t), wlen, f);
         fwrite(xlen, sizeof(uint8_t), 4, f);
         break;
    case TYPE_P7B:
        /* Put IRG at end of record */
         buffer[0] |= IRG_MASK;
         fwrite(buffer, sizeof(uint8_t), wlen, f);
         break;
    }
}

/* Write a tape mark */
static void
write_mark(FILE *f, int type) {
    static uint8_t  mark[4] = { 0x0, 0x0, 0x0, 0x0 };
    switch (type) {
    case TYPE_TAP:
    case TYPE_E11:
         fwrite(&mark, sizeof(uint8_t), 4, f);
         break;
    case TYPE_P7B:
         fputc(BCD_TM|IRG_MASK, f);
         break;
   }
}

/* Create a tape file with a number of records */
static int
create_tape_file (const char *filename, int recs, int type)
{
    FILE *f;
    int i;
    int j;
    int sz;
    char   buffer[128];

    f = fopen (filename, "w");
    ASSERT_NOT_NULL(f);
    if (f == NULL)
       return 0;
    for (i=0; i<recs; i++) {
       sprintf (buffer,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", i);
       sz = strlen(buffer);
       for(j = 0; j < sz; j++) {
           buffer[j] = ascii_to_ebcdic[(int)buffer[j]];
       }
       write_block(f, sz, (uint8_t *)buffer, type);
    }
    write_mark(f, type);
    for (; i<recs+2; i++) {
       sprintf (buffer,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", i);
       sz = strlen(buffer);
       for(j = 0; j < sz; j++) {
           buffer[j] = ascii_to_ebcdic[(int)buffer[j]];
       }
       write_block(f, sz, (uint8_t *)buffer, type);
    }
    write_mark(f, type);
    write_mark(f, type);
    fclose (f);
    return 1;
}


void
test_advance()
{
    step_count++;
    advance();
}

CTEST_DATA(model2415_test) {
    uint16_t       addr;
    struct _device *dev;
};

CTEST_SETUP(model2415_test) {
     struct _2415_context *ctx;
     log_trace("Init test\n");
     data->dev = chan[0];
     ASSERT_NOT_NULL(data->dev);
        
     data->addr = data->dev->addr;
     ctx = (struct _2415_context *)(data->dev->dev);
     create_tape_file("tape1.tap", 3, TYPE_TAP);
     ctx->tape[0] = (struct _tape_buffer *)calloc(1, sizeof(struct _tape_buffer));
     ctx->tape[0]->format = WRITE_RING|DEN_1600|TRACK9;
     tape_attach(ctx->tape[0], "tape1.tap", TYPE_TAP, 1, 1);
}

CTEST_TEARDOWN(model2415_test) {
     struct _2415_context *ctx;
     log_trace("teardown test\n");
     ctx = (struct _2415_context *)(data->dev->dev);
     tape_detach(ctx->tape[0]);
     (void)remove("tape1.tap");
}

/* Try to send Test I/O to controller */
CTEST2(model2415_test, test_io) {
    log_trace("TIO\n");
    ASSERT_EQUAL_X(0, test_io(data->addr));
}

/* Try to send Nop to controller */
CTEST2(model2415_test, nop) {
     uint16_t status;

     log_trace("Nop\n");
     set_mem(0x40,  0xffffffff);         /* Set CSW to zero */
     set_mem(0x44,  0xffffffff);
     set_mem(0x48,  0x500);   /* Set CAW */
     set_mem(0x500, 0x03000600); /* Set channel words */
     set_mem(0x504, 0x00000001);
     set_mem(0x600, 0xffffffff);
     status = start_io(data->addr, 0x500, 1, 0);
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000001, get_mem(0x44));
     ASSERT_EQUAL_X(0xffFFFFFF, get_mem(0x600));
}

/* Try to issue sense command */
CTEST2(model2415_test, sense1) {
     uint16_t status;
     log_trace("Sense 1\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x04000700); /* Set channel words */
     set_mem(0x504, 0x00000006);
     set_mem(0x700, 0xffffffff);
     set_mem(0x704, 0xffffffff);
     status = start_io(data->addr, 0x500, 1, 0);
     if (verbose) {
        printf("700=%08x %08x 0x40=%08x %08x\n", get_mem(0x700),
                get_mem(0x704), get_mem(0x40), get_mem(0x44));
     }

     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00480300, get_mem(0x700));
     ASSERT_EQUAL_X(0x0000ffff, get_mem(0x704));
}

/* Try to issue sense command */
CTEST2(model2415_test, sense2) {
     uint16_t status;
     log_trace("Sense 2\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x04000700); /* Set channel words */
     set_mem(0x504, 0x00000006);
     set_mem(0x700, 0xffffffff);
     set_mem(0x704, 0xffffffff);
     status = start_io(data->addr + 1, 0x500, 1, 0);
     if (verbose) {
        printf("700=%08x %08x 0x40=%08x %08x\n", get_mem(0x700),
                get_mem(0x704), get_mem(0x40), get_mem(0x44));
     }

     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00000300, get_mem(0x700));
     ASSERT_EQUAL_X(0x0000ffff, get_mem(0x704));
}

/* Try to issue sense command, short record */
CTEST2(model2415_test, sense3) {
     uint16_t status;
     log_trace("Sense 1\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x04000700); /* Set channel words */
     set_mem(0x504, 0x00000002);
     set_mem(0x700, 0xffffffff);
     set_mem(0x704, 0xffffffff);
     status = start_io(data->addr, 0x500, 1, 0);
     if (verbose) {
        printf("700=%08x %08x 0x40=%08x %08x\n", get_mem(0x700),
                get_mem(0x704), get_mem(0x40), get_mem(0x44));
     }

     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c400000, get_mem(0x44));
     ASSERT_EQUAL_X(0x0048ffff, get_mem(0x700));
     ASSERT_EQUAL_X(0xffffffff, get_mem(0x704));
}

/* Try to issue sense command, load read */
CTEST2(model2415_test, sense4) {
     uint16_t status;
     log_trace("Sense 1\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x04000700); /* Set channel words */
     set_mem(0x504, 0x00000008);
     set_mem(0x700, 0xffffffff);
     set_mem(0x704, 0xffffffff);
     status = start_io(data->addr, 0x500, 1, 0);
     if (verbose) {
        printf("700=%08x %08x 0x40=%08x %08x\n", get_mem(0x700),
                get_mem(0x704), get_mem(0x40), get_mem(0x44));
     }

     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c400002, get_mem(0x44));
     ASSERT_EQUAL_X(0x00480300, get_mem(0x700));
     ASSERT_EQUAL_X(0x0000ffff, get_mem(0x704));
}

/* Try to read a record off tape */
CTEST2(model2415_test, read) {
     int     i;
     uint16_t status = 0;
     char     buffer[128];
     int     sz;
     
     log_trace("Read\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0x0000004e);
     set_mem(0x510, 0x04000700); /* Set channel words */
     set_mem(0x514, 0x00000006);
     for (i = 0; i < 0x60; i += 4) {
         set_mem(0x600 + i, 0xffffffff);
         set_mem(0x700 + i, 0xffffffff);
     }
     status = start_io(data->addr, 0x500, 1, 0);
     if (verbose) {
        printf("0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
        for(i = 0x600; i < 0x660; i+= 4) {
           printf("0x%03x=%08x ", i, get_mem(i));
        }
        printf("\n");

     }
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0C000000, get_mem(0x44));

       sprintf (buffer,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", 0);
       sz = strlen(buffer);
       for(i = 0; i < sz; i++) {
           buffer[i] = ascii_to_ebcdic[(int)buffer[i]];
       }
     for (i = 0; i < sz; i++) {
         ASSERT_EQUAL_X(buffer[i] & 0xff, get_mem_b(0x600+i));
     }

     /* Make sure sense is zero */
     status = start_io(data->addr, 0x510, 1, 0);
     if (verbose) {
        printf("700=%08x %08x 0x40=%08x %08x\n", get_mem(0x700), get_mem(0x704),
                get_mem(0x40), get_mem(0x44));
     }

     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00400300, get_mem(0x700));
     ASSERT_EQUAL_X(0x0000ffff, get_mem(0x704));
}

/* Try to read a record off tape, short */
CTEST2(model2415_test, read_short) {
     int     i;
     uint16_t status = 0;
     char     buffer[128];
     int     sz;
     
     log_trace("Read\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0x00000020);
     set_mem(0x510, 0x04000700); /* Set channel words */
     set_mem(0x514, 0x00000006);
     for (i = 0; i < 0x60; i += 4) {
         set_mem(0x600 + i, 0xffffffff);
         set_mem(0x700 + i, 0xffffffff);
     }
     status = start_io(data->addr, 0x500, 1, 0);
     if (verbose) {
        printf("0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
        for(i = 0x600; i < 0x660; i+= 4) {
           printf("0x%03x=%08x ", i, get_mem(i));
        }
        printf("\n");

     }
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0C400000, get_mem(0x44));

       sprintf (buffer,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", 0);
       sz = strlen(buffer);
       for(i = 0; i < sz; i++) {
           buffer[i] = ascii_to_ebcdic[(int)buffer[i]];
       }
     for (i = 0; i < 32; i++) {
         ASSERT_EQUAL_X(buffer[i] & 0xff, get_mem_b(0x600+i));
     }
     for (; i < sz; i++) {
         ASSERT_EQUAL_X(0xff, get_mem_b(0x600+i));
     }

     /* Make sure sense is zero */
     status = start_io(data->addr, 0x510, 1, 0);
     if (verbose) {
        printf("700=%08x %08x 0x40=%08x %08x\n", get_mem(0x700), get_mem(0x704),
                get_mem(0x40), get_mem(0x44));
     }

     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00400300, get_mem(0x700));
     ASSERT_EQUAL_X(0x0000ffff, get_mem(0x704));
}

/* Try to read a record off tape, short */
CTEST2(model2415_test, read_long) {
     int     i;
     uint16_t status = 0;
     char     buffer[128];
     int     sz;
     
     log_trace("Read\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0x00000060);
     set_mem(0x510, 0x04000700); /* Set channel words */
     set_mem(0x514, 0x00000006);
     for (i = 0; i < 0x60; i += 4) {
         set_mem(0x600 + i, 0xffffffff);
         set_mem(0x700 + i, 0xffffffff);
     }
     status = start_io(data->addr, 0x500, 1, 0);
     if (verbose) {
        printf("0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
        for(i = 0x600; i < 0x660; i+= 4) {
           printf("0x%03x=%08x ", i, get_mem(i));
        }
        printf("\n");

     }
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0C400012, get_mem(0x44));

       sprintf (buffer,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", 0);
       sz = strlen(buffer);
       for(i = 0; i < sz; i++) {
           buffer[i] = ascii_to_ebcdic[(int)buffer[i]];
       }
     for (i = 0; i < sz; i++) {
         ASSERT_EQUAL_X(buffer[i] & 0xff, get_mem_b(0x600+i));
     }

     /* Make sure sense is zero */
     status = start_io(data->addr, 0x510, 1, 0);
     if (verbose) {
        printf("700=%08x %08x 0x40=%08x %08x\n", get_mem(0x700), get_mem(0x704),
                get_mem(0x40), get_mem(0x44));
     }

     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00400300, get_mem(0x700));
     ASSERT_EQUAL_X(0x0000ffff, get_mem(0x704));
}


/* Try to read two records off tape */
CTEST2(model2415_test, read2) {
     int     i;
     uint16_t status = 0;
     char     buffer[128];
     int     sz;
     
     log_trace("Read\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0x5000004e);
     set_mem(0x508, 0x02000600); /* Set channel words */
     set_mem(0x50c, 0x0000004e);
     set_mem(0x510, 0x04000700); /* Set channel words */
     set_mem(0x514, 0x00000006);
     for (i = 0; i < 0x60; i += 4) {
         set_mem(0x600 + i, 0xffffffff);
         set_mem(0x700 + i, 0xffffffff);
     }
     status = start_io(data->addr, 0x500, 1, 0);
     if (verbose) {
        printf("0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
        for(i = 0x600; i < 0x660; i+= 4) {
           printf("0x%03x=%08x ", i, get_mem(i));
        }
        printf("\n");

     }
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000510, get_mem(0x40));
     ASSERT_EQUAL_X(0x0C000000, get_mem(0x44));

       sprintf (buffer,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", 1);
       sz = strlen(buffer);
       for(i = 0; i < sz; i++) {
           buffer[i] = ascii_to_ebcdic[(int)buffer[i]];
       }
     for (i = 0; i < sz; i++) {
         ASSERT_EQUAL_X(buffer[i] & 0xff, get_mem_b(0x600+i));
     }

     /* Make sure sense is zero */
     status = start_io(data->addr, 0x510, 1, 0);
     if (verbose) {
        printf("700=%08x %08x 0x40=%08x %08x\n", get_mem(0x700), get_mem(0x704),
                get_mem(0x40), get_mem(0x44));
     }

     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00400300, get_mem(0x700));
     ASSERT_EQUAL_X(0x0000ffff, get_mem(0x704));
}

/* Try to read three records off tape */
CTEST2(model2415_test, read_mark) {
     int     i;
     uint16_t status = 0;
     char     buffer[128];
     int     sz;
     
     log_trace("Read\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x02000600); /* Read 3 records */
     set_mem(0x504, 0x5000004e);
     set_mem(0x508, 0x02000600);
     set_mem(0x50c, 0x5000004e);
     set_mem(0x510, 0x02000600);
     set_mem(0x514, 0x4000004e);
     set_mem(0x518, 0x02000600);
     set_mem(0x51c, 0x0000004e);
     set_mem(0x520, 0x04000700); 
     set_mem(0x524, 0x00000006);
     for (i = 0; i < 0x60; i += 4) {
         set_mem(0x600 + i, 0xffffffff);
         set_mem(0x700 + i, 0xffffffff);
     }
     status = start_io(data->addr, 0x500, 1, 0);
     if (verbose) {
        printf("0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
        for(i = 0x600; i < 0x660; i+= 4) {
           printf("0x%03x=%08x ", i, get_mem(i));
        }
        printf("\n");

     }
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND|SNS_UNITEXP, status);
     ASSERT_EQUAL_X(0x00000520, get_mem(0x40));
     ASSERT_EQUAL_X(0x0d40004e, get_mem(0x44));

       sprintf (buffer,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", 2);
       sz = strlen(buffer);
       for(i = 0; i < sz; i++) {
           buffer[i] = ascii_to_ebcdic[(int)buffer[i]];
       }
     for (i = 0; i < sz; i++) {
         ASSERT_EQUAL_X(buffer[i] & 0xff, get_mem_b(0x600+i));
     }

     /* Make sure sense is zero */
     status = start_io(data->addr, 0x520, 1, 0);
     if (verbose) {
        printf("700=%08x %08x 0x40=%08x %08x\n", get_mem(0x700), get_mem(0x704),
                get_mem(0x40), get_mem(0x44));
     }

     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000528, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00400300, get_mem(0x700));
     ASSERT_EQUAL_X(0x0000ffff, get_mem(0x704));
}

/* Try to Skip record and read next */
CTEST2(model2415_test, read_fsr) {
     int     i;
     uint16_t status1 = 0;
//     uint16_t status2 = 0;
     uint32_t word40, word44;
     char     buffer[128];
     int     sz;
     
     log_trace("Read\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x37000600); /* Skip first record */
     set_mem(0x504, 0x40000001);
     set_mem(0x508, 0x02000600); /* Read in second */
     set_mem(0x50c, 0x0000004e);
     set_mem(0x510, 0x00000600);
     set_mem(0x514, 0x00000000);
     set_mem(0x518, 0x27000000);
     set_mem(0x51c, 0x00000001);
     set_mem(0x520, 0x04000700); 
     set_mem(0x524, 0x00000006);
     for (i = 0; i < 0x60; i += 4) {
         set_mem(0x600 + i, 0xffffffff);
         set_mem(0x700 + i, 0xffffffff);
     }
     status1 = start_io(data->addr, 0x500, 1, 0);
     word40 = get_mem(0x40);
     word44 = get_mem(0x44);
//     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
 //    set_mem(0x44, 0xffffffff);
//     if ((status1 & SNS_DEVEND) == 0) {
 //        status2 = wait_dev(data->addr);
  //   }
     if (verbose) {
        printf("word %08x %08x 0x40=%08x %08x\n", word40, word44,
                 get_mem(0x40), get_mem(0x44));
        for(i = 0x600; i < 0x660; i+= 4) {
           printf("0x%03x=%08x ", i, get_mem(i));
        }
        printf("\n");

     }
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status1);
//     ASSERT_EQUAL_X(SNS_DEVEND, status2);
     ASSERT_EQUAL_X(0x00000510, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));

       sprintf (buffer,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", 1);
       sz = strlen(buffer);
       for(i = 0; i < sz; i++) {
           buffer[i] = ascii_to_ebcdic[(int)buffer[i]];
       }
     for (i = 0; i < sz; i++) {
         ASSERT_EQUAL_X(buffer[i] & 0xff, get_mem_b(0x600+i));
     }

     /* Make sure sense is zero */
     status1 = start_io(data->addr, 0x520, 1, 0);
     if (verbose) {
        printf("700=%08x %08x 0x40=%08x %08x\n", get_mem(0x700), get_mem(0x704),
                get_mem(0x40), get_mem(0x44));
     }

     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status1);
     ASSERT_EQUAL_X(0x00000528, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00400300, get_mem(0x700));
     ASSERT_EQUAL_X(0x0000ffff, get_mem(0x704));
}

/* Try to Skip record and read next */
CTEST2(model2415_test, read_fsr2) {
     int     i;
     uint16_t status1 = 0;
     uint16_t status2 = 0;
     uint32_t word40, word44;
//     char     buffer[128];
     
     log_trace("Read\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x37000600); /* Skip first record */
     set_mem(0x504, 0x40000001);
     set_mem(0x508, 0x37000600); /* Skip second record */
     set_mem(0x50c, 0x40000001);
     set_mem(0x510, 0x37000600); /* Skip third record */
     set_mem(0x514, 0x40000001);
     set_mem(0x518, 0x37000600); /* Read in second */
     set_mem(0x51c, 0x00000001);
     set_mem(0x51c, 0x00000001);
     set_mem(0x520, 0x04000700); 
     set_mem(0x524, 0x00000006);
     for (i = 0; i < 0x60; i += 4) {
         set_mem(0x600 + i, 0xffffffff);
         set_mem(0x700 + i, 0xffffffff);
     }
     status1 = start_io(data->addr, 0x500, 1, 0);
     word40 = get_mem(0x40);
     word44 = get_mem(0x44);
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     if ((status1 & SNS_DEVEND) == 0) {
         status2 = wait_dev(data->addr);
     }
     if (verbose) {
        printf("word %08x %08x 0x40=%08x %08x\n", word40, word44,
                 get_mem(0x40), get_mem(0x44));

     }
     ASSERT_EQUAL_X(SNS_CHNEND, status1);
     ASSERT_EQUAL_X(SNS_DEVEND|SNS_UNITEXP, status2);
     ASSERT_EQUAL_X(0x00000520, word40);
     ASSERT_EQUAL_X(0x08000001, word44);
     ASSERT_EQUAL_X(0xffffffff, get_mem(0x40));
     ASSERT_EQUAL_X(0x0500ffff, get_mem(0x44));

     /* Make sure sense is zero */
     status1 = start_io(data->addr, 0x520, 1, 0);
     if (verbose) {
        printf("700=%08x %08x 0x40=%08x %08x\n", get_mem(0x700), get_mem(0x704),
                get_mem(0x40), get_mem(0x44));
     }

     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status1);
     ASSERT_EQUAL_X(0x00000528, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00400300, get_mem(0x700));
     ASSERT_EQUAL_X(0x0000ffff, get_mem(0x704));
}

/* Try to Skip file and read next */
CTEST2(model2415_test, read_fsf) {
     int     i;
     uint16_t status1 = 0;
     uint32_t word40, word44;
     char     buffer[128];
     int     sz;
     
     log_trace("Read\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x3f000600); /* Skip first record */
     set_mem(0x504, 0x40000001);
     set_mem(0x508, 0x02000600); /* Read in second */
     set_mem(0x50c, 0x0000004e);
     set_mem(0x510, 0x00000600);
     set_mem(0x514, 0x00000000);
     set_mem(0x518, 0x27000000);
     set_mem(0x51c, 0x00000001);
     set_mem(0x520, 0x04000700); 
     set_mem(0x524, 0x00000006);
     for (i = 0; i < 0x60; i += 4) {
         set_mem(0x600 + i, 0xffffffff);
         set_mem(0x700 + i, 0xffffffff);
     }
     status1 = start_io(data->addr, 0x500, 1, 0);
     word40 = get_mem(0x40);
     word44 = get_mem(0x44);
//     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
 //    set_mem(0x44, 0xffffffff);
//     if ((status1 & SNS_DEVEND) == 0) {
 //        status2 = wait_dev(data->addr);
  //   }
     if (verbose) {
        printf("word %08x %08x 0x40=%08x %08x\n", word40, word44,
                 get_mem(0x40), get_mem(0x44));
        for(i = 0x600; i < 0x660; i+= 4) {
           printf("0x%03x=%08x ", i, get_mem(i));
        }
        printf("\n");

     }
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status1);
//     ASSERT_EQUAL_X(SNS_DEVEND, status2);
     ASSERT_EQUAL_X(0x00000510, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));

       sprintf (buffer,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", 3);
       sz = strlen(buffer);
       for(i = 0; i < sz; i++) {
           buffer[i] = ascii_to_ebcdic[(int)buffer[i]];
       }
     for (i = 0; i < sz; i++) {
         ASSERT_EQUAL_X(buffer[i] & 0xff, get_mem_b(0x600+i));
     }

     /* Make sure sense is zero */
     status1 = start_io(data->addr, 0x520, 1, 0);
     if (verbose) {
        printf("700=%08x %08x 0x40=%08x %08x\n", get_mem(0x700), get_mem(0x704),
                get_mem(0x40), get_mem(0x44));
     }

     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status1);
     ASSERT_EQUAL_X(0x00000528, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00400300, get_mem(0x700));
     ASSERT_EQUAL_X(0x0000ffff, get_mem(0x704));
}

/* Try to Skip file and read back one record */
CTEST2(model2415_test, read_bsr) {
     int     i;
     uint16_t status1 = 0;
     uint16_t status2 = 0;
     uint32_t word40, word44;
     char     buffer[128];
     int     sz;
     
     log_trace("Read\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x3f000600); /* Skip file record */
     set_mem(0x504, 0x40000001);
     set_mem(0x508, 0x27000600); /* Back over file mark */
     set_mem(0x50c, 0x00000001);
     set_mem(0x510, 0x27000600); /* Back over record */
     set_mem(0x514, 0x40000001);
     set_mem(0x518, 0x02000600);
     set_mem(0x51c, 0x0000004e);
     set_mem(0x520, 0x04000700); 
     set_mem(0x524, 0x00000006);
     for (i = 0; i < 0x60; i += 4) {
         set_mem(0x600 + i, 0xffffffff);
         set_mem(0x700 + i, 0xffffffff);
     }
     status1 = start_io(data->addr, 0x500, 1, 0);
     word40 = get_mem(0x40);
     word44 = get_mem(0x44);
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     if ((status1 & SNS_DEVEND) == 0) {
         status2 = wait_dev(data->addr);
     }
     if (verbose) {
        printf("word %08x %08x 0x40=%08x %08x\n", word40, word44,
                 get_mem(0x40), get_mem(0x44));
        for(i = 0x600; i < 0x660; i+= 4) {
           printf("0x%03x=%08x ", i, get_mem(i));
        }
        printf("\n");

     }
     ASSERT_EQUAL_X(SNS_CHNEND, status1);
     ASSERT_EQUAL_X(SNS_DEVEND|SNS_UNITEXP, status2);
     ASSERT_EQUAL_X(0x00000510, word40);
     ASSERT_EQUAL_X(0x08000001, word44);
     ASSERT_EQUAL_X(0xffffffff, get_mem(0x40));
     ASSERT_EQUAL_X(0x0500ffff, get_mem(0x44));

     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     status1 = start_io(data->addr, 0x510, 1, 0);
     word40 = get_mem(0x40);
     word44 = get_mem(0x44);
//     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
 //    set_mem(0x44, 0xffffffff);
     if ((status1 & SNS_DEVEND) == 0) {
         status2 = wait_dev(data->addr);
     }
     if (verbose) {
        printf("word %08x %08x 0x40=%08x %08x\n", word40, word44,
                 get_mem(0x40), get_mem(0x44));
        for(i = 0x600; i < 0x660; i+= 4) {
           printf("0x%03x=%08x ", i, get_mem(i));
        }
        printf("\n");

     }
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status1);
//     ASSERT_EQUAL_X(SNS_DEVEND, status2);
     ASSERT_EQUAL_X(0x00000520, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));

       sprintf (buffer,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", 2);
       sz = strlen(buffer);
       for(i = 0; i < sz; i++) {
           buffer[i] = ascii_to_ebcdic[(int)buffer[i]];
       }
     for (i = 0; i < sz; i++) {
         ASSERT_EQUAL_X(buffer[i] & 0xff, get_mem_b(0x600+i));
     }

     /* Make sure sense is zero */
     status1 = start_io(data->addr, 0x520, 1, 0);
     if (verbose) {
        printf("700=%08x %08x 0x40=%08x %08x\n", get_mem(0x700), get_mem(0x704),
                get_mem(0x40), get_mem(0x44));
     }

     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status1);
     ASSERT_EQUAL_X(0x00000528, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00400300, get_mem(0x700));
     ASSERT_EQUAL_X(0x0000ffff, get_mem(0x704));
}

/* Try to Skip file and read back two records */
CTEST2(model2415_test, read_bsr2) {
     int     i;
     uint16_t status1 = 0;
     uint16_t status2 = 0;
     uint32_t word40, word44;
     char     buffer[128];
     int     sz;
     
     log_trace("Read\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x3f000600); /* Skip file record */
     set_mem(0x504, 0x40000001);
     set_mem(0x508, 0x27000600); /* Back over file mark */
     set_mem(0x50c, 0x00000001);
     set_mem(0x510, 0x27000600); /* Back over record */
     set_mem(0x514, 0x40000001);
     set_mem(0x518, 0x27000600); /* Back over record */
     set_mem(0x51c, 0x40000001);
     set_mem(0x520, 0x02000600);
     set_mem(0x524, 0x0000004e);
     set_mem(0x530, 0x04000700); 
     set_mem(0x534, 0x00000006);
     for (i = 0; i < 0x60; i += 4) {
         set_mem(0x600 + i, 0xffffffff);
         set_mem(0x700 + i, 0xffffffff);
     }
     status1 = start_io(data->addr, 0x500, 1, 0);
     word40 = get_mem(0x40);
     word44 = get_mem(0x44);
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     if ((status1 & SNS_DEVEND) == 0) {
         status2 = wait_dev(data->addr);
     }
     if (verbose) {
        printf("word %08x %08x 0x40=%08x %08x\n", word40, word44,
                 get_mem(0x40), get_mem(0x44));
        for(i = 0x600; i < 0x660; i+= 4) {
           printf("0x%03x=%08x ", i, get_mem(i));
        }
        printf("\n");

     }
     ASSERT_EQUAL_X(SNS_CHNEND, status1);
     ASSERT_EQUAL_X(SNS_DEVEND|SNS_UNITEXP, status2);
     ASSERT_EQUAL_X(0x00000510, word40);
     ASSERT_EQUAL_X(0x08000001, word44);
     ASSERT_EQUAL_X(0xffffffff, get_mem(0x40));
     ASSERT_EQUAL_X(0x0500ffff, get_mem(0x44));

     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     status1 = start_io(data->addr, 0x510, 1, 0);
     word40 = get_mem(0x40);
     word44 = get_mem(0x44);
//     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
 //    set_mem(0x44, 0xffffffff);
     if ((status1 & SNS_DEVEND) == 0) {
         status2 = wait_dev(data->addr);
     }
     if (verbose) {
        printf("word %08x %08x 0x40=%08x %08x\n", word40, word44,
                 get_mem(0x40), get_mem(0x44));
        for(i = 0x600; i < 0x660; i+= 4) {
           printf("0x%03x=%08x ", i, get_mem(i));
        }
        printf("\n");

     }
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status1);
//     ASSERT_EQUAL_X(SNS_DEVEND, status2);
     ASSERT_EQUAL_X(0x00000528, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));

       sprintf (buffer,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", 1);
       sz = strlen(buffer);
       for(i = 0; i < sz; i++) {
           buffer[i] = ascii_to_ebcdic[(int)buffer[i]];
       }
     for (i = 0; i < sz; i++) {
         ASSERT_EQUAL_X(buffer[i] & 0xff, get_mem_b(0x600+i));
     }

     /* Make sure sense is zero */
     status1 = start_io(data->addr, 0x530, 1, 0);
     if (verbose) {
        printf("700=%08x %08x 0x40=%08x %08x\n", get_mem(0x700), get_mem(0x704),
                get_mem(0x40), get_mem(0x44));
     }

     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status1);
     ASSERT_EQUAL_X(0x00000538, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00400300, get_mem(0x700));
     ASSERT_EQUAL_X(0x0000ffff, get_mem(0x704));
}

#if 0
/* Try to read a card, make sure it got into stacker. */
CTEST2(model2415_test, read_two) {
     struct _2415_context *ctx = (struct _2415_context *)(data->dev->dev);
     int     i;
     uint16_t status1 = 0;
     uint16_t status2 = 0;
     uint32_t word40;
     uint32_t word44;
     
     log_trace("Read feed\n");
     ctx->pch_full = 0;
     empty_cards(ctx->stack[0]);
     empty_cards(ctx->stack[1]);
     ASSERT_EQUAL(1, create_card_file ("file1.deck", 4));
     read_deck(ctx->feed, "file1.deck");
    printf("Size %d %d, %d %d\n", hopper_size(ctx->feed),
            stack_size(ctx->feed), hopper_size(ctx->stack[0]),
            stack_size(ctx->stack[0]));
     model2415_feed(ctx);         /* Load first card. */
    printf("Size %d %d, %d %d\n", hopper_size(ctx->feed),
            stack_size(ctx->feed), hopper_size(ctx->stack[0]),
            stack_size(ctx->stack[0]));
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0x40000050);
     set_mem(0x508, 0x02000700); /* Set channel words */
     set_mem(0x50c, 0x00000050);
     set_mem(0x510, 0x04000700); /* Set channel words */
     set_mem(0x514, 0x00000001);
     for (i = 0; i < 0x60; i += 4) {
         set_mem(0x600 + i, 0xffffffff);
         set_mem(0x700 + i, 0xffffffff);
     }
     status1 = start_io(data->addr, 0x500, 0, 0);
     word40 = get_mem(0x40);
     word44 = get_mem(0x44);
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     if ((status1 & SNS_DEVEND) == 0) {
         status2 = wait_dev(data->addr);
     }
     if (verbose) {
        printf("0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
        for(i = 0x600; i < 0x660; i+= 4) {
           printf("0x%03x=%08x ", i, get_mem(i));
        }
        printf("\n");
        for(i = 0x700; i < 0x760; i+= 4) {
           printf("0x%03x=%08x ", i, get_mem(i));
        }
        printf("\n");
    printf("Size %d %d, %d %d\n", hopper_size(ctx->feed),
            stack_size(ctx->feed), hopper_size(ctx->stack[0]),
            stack_size(ctx->stack[0]));
     }
     card_data[1] &= 0xF0ffffff;
     for (i = 0; i < 80; i+=4) {
         ASSERT_EQUAL_X(card_data[i>>2], get_mem(0x600+i));
     }
     card_data[1] |= 0x01000000;
     for (i = 0; i < 80; i+=4) {
         ASSERT_EQUAL_X(card_data[i>>2], get_mem(0x700+i));
     }
     ASSERT_EQUAL_X(SNS_CHNEND, status1);
     ASSERT_EQUAL_X(SNS_DEVEND, status2);
     ASSERT_EQUAL_X(0x00000510, word40);
     ASSERT_EQUAL_X(0x08000000, word44);
     ASSERT_EQUAL_X(0xffffffff, get_mem(0x40));
     ASSERT_EQUAL_X(0x0400ffff, get_mem(0x44));
     ASSERT_EQUAL_X(1, hopper_size(ctx->stack[0]));

     /* Make sure sense is zero */
     set_mem(0x700, 0xffffffff);
     status1 = start_io(data->addr, 0x510, 0, 0);
     if (verbose) {
        printf("700=%08x 0x40=%08x %08x\n", get_mem(0x700),
                 get_mem(0x40), get_mem(0x44));
     }

     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status1);
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00ffffff, get_mem(0x700));
}

/* Make sure reading last card without EOF set, returns operator intervention */
CTEST2(model2415_test, read_inter) {
     struct _2415_context *ctx = (struct _2415_context *)(data->dev->dev);
     int     i;
     uint16_t status1 = 0;
     uint16_t status2 = 0;
     uint32_t word40;
     uint32_t word44;
     
     log_trace("Read intervent\n");
     ASSERT_EQUAL(1, create_card_file ("file1.deck", 2));
     read_deck(ctx->feed, "file1.deck");
     model2415_feed(ctx);         /* Load first card. */
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0x40000050);
     set_mem(0x508, 0x02000700); /* Set channel words */
     set_mem(0x50c, 0x00000050);
     for (i = 0; i < 0x60; i += 4) {
         set_mem(0x600 + i, 0xffffffff);
         set_mem(0x700 + i, 0xffffffff);
     }
     status1 = start_io(data->addr, 0x500, 0, 0);
     word40 = get_mem(0x40);
     word44 = get_mem(0x44);
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     if ((status1 & SNS_DEVEND) == 0) {
         status2 = wait_dev(data->addr);
     }
     if (verbose) {
        printf("first 40=%08x %08x ", word40, word44);
        printf("0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
        for(i = 0x600; i < 0x660; i+= 4) {
           printf("0x%03x=%08x ", i, get_mem(i));
        }
        printf("\n");
        for(i = 0x700; i < 0x760; i+= 4) {
           printf("0x%03x=%08x ", i, get_mem(i));
        }
        printf("\n");
     }
     card_data[1] &= 0xF0ffffff;
     for (i = 0; i < 80; i+=4) {
         ASSERT_EQUAL_X(card_data[i>>2], get_mem(0x600+i));
     }
     ASSERT_EQUAL_X(SNS_DEVEND|SNS_UNITCHK, status1);
     ASSERT_EQUAL_X(0x00000508, word40);
     ASSERT_EQUAL_X(0x06000000, word44);
     ASSERT_EQUAL_X(0, hopper_size(ctx->stack[0]));

     /* Make sure sense is zero */
     set_mem(0x700, 0xffffffff);
     status1 = start_io(data->addr, 0x510, 0, 0);
     if (verbose) {
        printf("700=%08x 0x40=%08x %08x\n", get_mem(0x700),
                get_mem(0x40), get_mem(0x44));
     }

     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status1);
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x40ffffff, get_mem(0x700));
}

/* Make sure reading last card EOF set, returns unit exception */
CTEST2(model2415_test, read_eof) {
     struct _2415_context *ctx = (struct _2415_context *)(data->dev->dev);
     int     i;
     uint16_t status1 = 0;
     uint16_t status2 = 0;
     uint32_t word40;
     uint32_t word44;
     
     log_trace("Read eof\n");
     ASSERT_EQUAL(1, create_card_file ("file1.deck", 2));
     read_deck(ctx->feed, "file1.deck");
     ctx->eof_flag = 1;
     model2415_feed(ctx);         /* Load first card. */
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0x40000050);
     set_mem(0x508, 0x02000700); /* Set channel words */
     set_mem(0x50c, 0x00000050);
     for (i = 0; i < 0x60; i += 4) {
         set_mem(0x600 + i, 0xffffffff);
         set_mem(0x700 + i, 0xffffffff);
     }
     status1 = start_io(data->addr, 0x500, 0, 0);
     word40 = get_mem(0x40);
     word44 = get_mem(0x44);
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     if ((status1 & SNS_DEVEND) == 0) {
         status2 = wait_dev(data->addr);
     }
     if (verbose) {
        printf("first 40=%08x %08x ", word40, word44);
        printf("0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
        for(i = 0x600; i < 0x660; i+= 4) {
           printf("0x%03x=%08x ", i, get_mem(i));
        }
        printf("\n");
        for(i = 0x700; i < 0x760; i+= 4) {
           printf("0x%03x=%08x ", i, get_mem(i));
        }
        printf("\n");
     }
     card_data[1] &= 0xF0ffffff;
     for (i = 0; i < 80; i+=4) {
         ASSERT_EQUAL_X(card_data[i>>2], get_mem(0x600+i));
     }
     ASSERT_EQUAL_X(SNS_CHNEND, status1);
     ASSERT_EQUAL_X(SNS_DEVEND|SNS_UNITEXP, status2);
     ASSERT_EQUAL_X(0x00000510, word40);
     ASSERT_EQUAL_X(0x08000000, word44);
     ASSERT_EQUAL_X(0xffffffff, get_mem(0x40));
     ASSERT_EQUAL_X(0x0500ffff, get_mem(0x44));
     ASSERT_EQUAL_X(0, hopper_size(ctx->stack[0]));

     /* Make sure sense is zero */
     set_mem(0x700, 0xffffffff);
     status1 = start_io(data->addr, 0x510, 0, 0);
     if (verbose) {
        printf("700=%08x 0x40=%08x %08x\n", get_mem(0x700),
                get_mem(0x40), get_mem(0x44));
     }

     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status1);
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x40ffffff, get_mem(0x700));
}

/* Try to read a card, make sure it got into stacker 2. */
CTEST2(model2415_test, read_stack2) {
     struct _2415_context *ctx = (struct _2415_context *)(data->dev->dev);
     int     i;
     uint16_t status1 = 0;
     uint16_t status2 = 0;
     uint32_t word40;
     uint32_t word44;
     
     log_trace("Read feed\n");
     ctx->pch_full = 0;
     empty_cards(ctx->stack[0]);
     empty_cards(ctx->stack[1]);
     ASSERT_EQUAL(1, create_card_file ("file1.deck", 4));
     read_deck(ctx->feed, "file1.deck");
    printf("Size %d %d, %d %d\n", hopper_size(ctx->feed),
            stack_size(ctx->feed), hopper_size(ctx->stack[0]),
            stack_size(ctx->stack[0]));
     model2415_feed(ctx);         /* Load first card. */
    printf("Size %d %d, %d %d\n", hopper_size(ctx->feed),
            stack_size(ctx->feed), hopper_size(ctx->stack[0]),
            stack_size(ctx->stack[0]));
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x22000600); /* Set channel words */
     set_mem(0x504, 0x40000050);
     set_mem(0x508, 0x22000700); /* Set channel words */
     set_mem(0x50c, 0x00000050);
     set_mem(0x510, 0x04000700); /* Set channel words */
     set_mem(0x514, 0x00000001);
     for (i = 0; i < 0x60; i += 4) {
         set_mem(0x600 + i, 0xffffffff);
         set_mem(0x700 + i, 0xffffffff);
     }
     status1 = start_io(data->addr, 0x500, 0, 0);
     word40 = get_mem(0x40);
     word44 = get_mem(0x44);
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     if ((status1 & SNS_DEVEND) == 0) {
         status2 = wait_dev(data->addr);
     }
     if (verbose) {
        printf("0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
        for(i = 0x600; i < 0x660; i+= 4) {
           printf("0x%03x=%08x ", i, get_mem(i));
        }
        printf("\n");
        for(i = 0x700; i < 0x760; i+= 4) {
           printf("0x%03x=%08x ", i, get_mem(i));
        }
        printf("\n");
    printf("Size %d %d, %d %d\n", hopper_size(ctx->feed),
            stack_size(ctx->feed), hopper_size(ctx->stack[0]),
            stack_size(ctx->stack[0]));
     }
     card_data[1] &= 0xF0ffffff;
     for (i = 0; i < 80; i+=4) {
         ASSERT_EQUAL_X(card_data[i>>2], get_mem(0x600+i));
     }
     card_data[1] |= 0x01000000;
     for (i = 0; i < 80; i+=4) {
         ASSERT_EQUAL_X(card_data[i>>2], get_mem(0x700+i));
     }
     ASSERT_EQUAL_X(SNS_CHNEND, status1);
     ASSERT_EQUAL_X(SNS_DEVEND, status2);
     ASSERT_EQUAL_X(0x00000510, word40);
     ASSERT_EQUAL_X(0x08000000, word44);
     ASSERT_EQUAL_X(0xffffffff, get_mem(0x40));
     ASSERT_EQUAL_X(0x0400ffff, get_mem(0x44));
     ASSERT_EQUAL_X(1, hopper_size(ctx->stack[1]));

     /* Make sure sense is zero */
     set_mem(0x700, 0xffffffff);
     status1 = start_io(data->addr, 0x510, 0, 0);
     if (verbose) {
        printf("700=%08x 0x40=%08x %08x\n", get_mem(0x700),
                 get_mem(0x40), get_mem(0x44));
     }

     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status1);
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00ffffff, get_mem(0x700));
}

/* Try to feed card then read next card. */
CTEST2(model2415_test, read_feed) {
     int     i;
     uint16_t status1 = 0;
     uint16_t status2 = 0;
     uint32_t word40;
     uint32_t word44;
     
     log_trace("Read feed\n");
     ctx->pch_full = 0;
     empty_cards(ctx->stack[0]);
     empty_cards(ctx->stack[1]);
     ASSERT_EQUAL(1, create_card_file ("file1.deck", 4));
     read_deck(ctx->feed, "file1.deck");
    printf("Size %d %d, %d %d\n", hopper_size(ctx->feed),
            stack_size(ctx->feed), hopper_size(ctx->stack[0]),
            stack_size(ctx->stack[0]));
     model2415_feed(ctx);         /* Load first card. */
    printf("Size %d %d, %d %d\n", hopper_size(ctx->feed),
            stack_size(ctx->feed), hopper_size(ctx->stack[0]),
            stack_size(ctx->stack[0]));
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x83000000); /* Set channel words */
     set_mem(0x504, 0x60000050);
     set_mem(0x508, 0x02000700); /* Set channel words */
     set_mem(0x50c, 0x00000050);
     set_mem(0x510, 0x04000700); /* Set channel words */
     set_mem(0x514, 0x00000001);
     for (i = 0; i < 0x60; i += 4) {
         set_mem(0x600 + i, 0xffffffff);
         set_mem(0x700 + i, 0xffffffff);
     }
     status1 = start_io(data->addr, 0x500, 0, 0);
     word40 = get_mem(0x40);
     word44 = get_mem(0x44);
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     if ((status1 & SNS_DEVEND) == 0) {
         status2 = wait_dev(data->addr);
     }
     if (verbose) {
        printf("0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
        for(i = 0x600; i < 0x660; i+= 4) {
           printf("0x%03x=%08x ", i, get_mem(i));
        }
        printf("\n");
        for(i = 0x700; i < 0x760; i+= 4) {
           printf("0x%03x=%08x ", i, get_mem(i));
        }
        printf("\n");
        printf("Size %d %d, %d %d\n", hopper_size(ctx->feed),
            stack_size(ctx->feed), hopper_size(ctx->stack[0]),
            stack_size(ctx->stack[0]));
     }
     card_data[1] &= 0xF0ffffff;
     card_data[1] |= 0x01000000;
     for (i = 0; i < 80; i+=4) {
         ASSERT_EQUAL_X(card_data[i>>2], get_mem(0x700+i));
     }
     ASSERT_EQUAL_X(SNS_CHNEND, status1);
     ASSERT_EQUAL_X(SNS_DEVEND, status2);
     ASSERT_EQUAL_X(0x00000510, word40);
     ASSERT_EQUAL_X(0x08000000, word44);
     ASSERT_EQUAL_X(0xffffffff, get_mem(0x40));
     ASSERT_EQUAL_X(0x0400ffff, get_mem(0x44));
     ASSERT_EQUAL_X(1, hopper_size(ctx->stack[0]));

     /* Make sure sense is zero */
     set_mem(0x700, 0xffffffff);
     status1 = start_io(data->addr, 0x510, 0, 0);
     if (verbose) {
        printf("700=%08x 0x40=%08x %08x\n", get_mem(0x700),
                 get_mem(0x40), get_mem(0x44));
     }

     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status1);
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00ffffff, get_mem(0x700));
}

/* Try to punch a test card */
CTEST2(model2415_test, punch_card) {
     struct _2415_context *ctx = (struct _2415_context *)(data->dev->dev);
     int     i;
     uint16_t status1 = 0;
     uint16_t status2 = 0;
     uint32_t word40;
     uint32_t word44;
     char     buffer[82];
     FILE     *f;
     
     log_trace("Punch card\n");
     ctx->pch_full = 0;
     blank_deck(ctx->feed, 10);
     empty_cards(ctx->stack[0]);
     empty_cards(ctx->stack[1]);

//     read_deck(ctx->feed, "file1.deck");
     model2415_feed(ctx);         /* Load first card. */
    printf("Size %d %d, %d %d\n", hopper_size(ctx->feed),
            stack_size(ctx->feed), hopper_size(ctx->stack[0]),
            stack_size(ctx->stack[0]));
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x83000000); /* Set channel words */
     set_mem(0x504, 0x60000050);
     set_mem(0x508, 0x81000600); /* Set channel words */
     set_mem(0x50c, 0x20000050);
     set_mem(0x510, 0x04000700); /* Set channel words */
     set_mem(0x514, 0x00000001);
     set_mem(0x600, 0xf0f0f0f0);
     set_mem(0x604, 0xf040c1c2);
     set_mem(0x608, 0xc3c4c5c6);
     set_mem(0x60c, 0xc7c8c9d1);
     set_mem(0x610, 0xd2d3d4d5);
     set_mem(0x614, 0xd6d7d8d9);
     set_mem(0x618, 0xe2e3e4e5);
     set_mem(0x61c, 0xe6e7e8e9);
     set_mem(0x620, 0xf0f1f2f3);
     set_mem(0x624, 0xf4f5f6f7);
     set_mem(0x628, 0xf8f9c1c2);
     set_mem(0x62c, 0xc3c4c5c6);
     set_mem(0x630, 0xc7c8c9d1);
     set_mem(0x634, 0xd2d3d4d5);
     set_mem(0x638, 0xd6d7d8d9);
     set_mem(0x63c, 0xe2e3e4e5);
     set_mem(0x640, 0xe6e7e8e9);
     set_mem(0x644, 0xf0f1f2f3);
     set_mem(0x648, 0xf4f5f6f7);
     set_mem(0x64c, 0xf8f94040);
     status1 = start_io(data->addr, 0x500, 0, 0);
     word40 = get_mem(0x40);
     word44 = get_mem(0x44);
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     if ((status1 & SNS_DEVEND) == 0) {
         status2 = wait_dev(data->addr);
     }
     if (verbose) {
        printf("0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
        printf("\n");
        printf("Size %d %d, %d %d\n", hopper_size(ctx->feed),
            stack_size(ctx->feed), hopper_size(ctx->stack[0]),
            stack_size(ctx->stack[0]));
     }

     ASSERT_EQUAL_X(SNS_CHNEND, status1);
     ASSERT_EQUAL_X(SNS_DEVEND, status2);
     ASSERT_EQUAL_X(0x00000510, word40);
     ASSERT_EQUAL_X(0x08000000, word44);
     ASSERT_EQUAL_X(0xffffffff, get_mem(0x40));
     ASSERT_EQUAL_X(0x0400ffff, get_mem(0x44));
     ASSERT_EQUAL_X(1, hopper_size(ctx->stack[0]));

     /* Make sure sense is zero */
     set_mem(0x700, 0xffffffff);
     status1 = start_io(data->addr, 0x510, 0, 0);
     if (verbose) {
        printf("700=%08x 0x40=%08x %08x\n", get_mem(0x700),
                 get_mem(0x40), get_mem(0x44));
     }

     ASSERT_EQUAL(0, save_deck(ctx->stack[0], "file2.deck"));
     close_deck(ctx->stack[0]);

    if ((f = fopen("file2.deck", "r")) == NULL)
        ASSERT_FAIL();
    i = 0;
    while(fgets(buffer, sizeof(buffer), f) != NULL) {
     if (verbose) {
         puts(buffer);
     }
         sprintf(buffer, "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\n", i);
         ASSERT_STR(buffer, buffer);
         i++;
    }
    fclose(f);


     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status1);
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00ffffff, get_mem(0x700));
}

/* Try to punch a test card in two writes */
CTEST2(model2415_test, punch_card2) {
     struct _2415_context *ctx = (struct _2415_context *)(data->dev->dev);
     int     i;
     uint16_t status1 = 0;
     uint16_t status2 = 0;
     uint32_t word40;
     uint32_t word44;
     char     buffer[81];
     FILE     *f;
     
     log_trace("Punch card 2\n");
     ctx->pch_full = 0;
     blank_deck(ctx->feed, 10);
     empty_cards(ctx->stack[0]);
     empty_cards(ctx->stack[1]);

     model2415_feed(ctx);         /* Load first card. */
    printf("Size %d %d, %d %d\n", hopper_size(ctx->feed),
            stack_size(ctx->feed), hopper_size(ctx->stack[0]),
            stack_size(ctx->stack[0]));
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x02000710); /* Set channel words */
     set_mem(0x504, 0x60000050);
     set_mem(0x508, 0x01000600); /* Set channel words */
     set_mem(0x50c, 0x60000020);
     set_mem(0x510, 0x81000620); /* Set channel words */
     set_mem(0x514, 0x20000030);
     set_mem(0x520, 0x04000700); /* Set channel words */
     set_mem(0x524, 0x00000001);
     set_mem(0x600, 0xf0f0f0f0);
     set_mem(0x604, 0xf040c1c2);
     set_mem(0x608, 0xc3c4c5c6);
     set_mem(0x60c, 0xc7c8c9d1);
     set_mem(0x610, 0xd2d3d4d5);
     set_mem(0x614, 0xd6d7d8d9);
     set_mem(0x618, 0xe2e3e4e5);
     set_mem(0x61c, 0xe6e7e8e9);
     set_mem(0x620, 0xf0f1f2f3);
     set_mem(0x624, 0xf4f5f6f7);
     set_mem(0x628, 0xf8f9c1c2);
     set_mem(0x62c, 0xc3c4c5c6);
     set_mem(0x630, 0xc7c8c9d1);
     set_mem(0x634, 0xd2d3d4d5);
     set_mem(0x638, 0xd6d7d8d9);
     set_mem(0x63c, 0xe2e3e4e5);
     set_mem(0x640, 0xe6e7e8e9);
     set_mem(0x644, 0xf0f1f2f3);
     set_mem(0x648, 0xf4f5f6f7);
     set_mem(0x64c, 0xf8f94040);
     status1 = start_io(data->addr, 0x500, 0, 0);
     word40 = get_mem(0x40);
     word44 = get_mem(0x44);
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     if ((status1 & SNS_DEVEND) == 0) {
         status2 = wait_dev(data->addr);
     }
     if (verbose) {
        printf("0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
        printf("\n");
        printf("Size %d %d, %d %d\n", hopper_size(ctx->feed),
            stack_size(ctx->feed), hopper_size(ctx->stack[0]),
            stack_size(ctx->stack[0]));
     }

     ASSERT_EQUAL_X(SNS_CHNEND, status1);
     ASSERT_EQUAL_X(SNS_DEVEND, status2);
     ASSERT_EQUAL_X(0x00000518, word40);
     ASSERT_EQUAL_X(0x08000000, word44);
     ASSERT_EQUAL_X(0xffffffff, get_mem(0x40));
     ASSERT_EQUAL_X(0x0400ffff, get_mem(0x44));
     ASSERT_EQUAL_X(1, hopper_size(ctx->stack[0]));

     /* Make sure sense is zero */
     set_mem(0x700, 0xffffffff);
     status1 = start_io(data->addr, 0x520, 0, 0);
     if (verbose) {
        printf("700=%08x 0x40=%08x %08x\n", get_mem(0x700),
                 get_mem(0x40), get_mem(0x44));
     }

     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status1);
     ASSERT_EQUAL_X(0x00000528, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00ffffff, get_mem(0x700));

     ASSERT_EQUAL(0, save_deck(ctx->stack[0], "file2.deck"));
     close_deck(ctx->stack[0]);


    if ((f = fopen("file2.deck", "r")) == NULL)
        ASSERT_FAIL();
    i = 0;
    while(fgets(buffer, sizeof(buffer), f) != NULL) {
     if (verbose) {
         puts(buffer);
     }
         sprintf(buffer, "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\n", i);
         ASSERT_STR(buffer, buffer);
         i++;
    }
    fclose(f);
}

/* Try to over punch a test card in writes */
CTEST2(model2415_test, punch_over) {
     struct _2415_context *ctx = (struct _2415_context *)(data->dev->dev);
     int     i;
     uint16_t status1 = 0;
     uint16_t status2 = 0;
     uint32_t word40;
     uint32_t word44;
     char     buffer[82];
     FILE     *f;
     
     log_trace("Punch over\n");
     ctx->pch_full = 0;
     empty_cards(ctx->stack[0]);
     empty_cards(ctx->stack[1]);
     ASSERT_EQUAL(1, create_card_file2 ("file1.deck", 4));
     read_deck(ctx->feed, "file1.deck");
    printf("Size %d %d, %d %d\n", hopper_size(ctx->feed),
            stack_size(ctx->feed), hopper_size(ctx->stack[0]),
            stack_size(ctx->stack[0]));
     model2415_feed(ctx);         /* Load first card. */
     model2415_feed(ctx);         /* Move to punch station */
    printf("Size %d %d, %d %d\n", hopper_size(ctx->feed),
            stack_size(ctx->feed), hopper_size(ctx->stack[0]),
            stack_size(ctx->stack[0]));
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x81000600); /* Set channel words */
     set_mem(0x504, 0x2000002A);
     set_mem(0x520, 0x04000700); /* Set channel words */
     set_mem(0x524, 0x00000001);
     set_mem(0x600, 0x40404040);
     set_mem(0x604, 0x4040c1c2);
     set_mem(0x608, 0xc3c4c5c6);
     set_mem(0x60c, 0xc7c8c9d1);
     set_mem(0x610, 0xd2d3d4d5);
     set_mem(0x614, 0xd6d7d8d9);
     set_mem(0x618, 0xe2e3e4e5);
     set_mem(0x61c, 0xe6e7e8e9);
     set_mem(0x620, 0xf0f1f2f3);
     set_mem(0x624, 0xf4f5f6f7);
     set_mem(0x628, 0xf8f94040);
     status1 = start_io(data->addr, 0x500, 0, 0);
     word40 = get_mem(0x40);
     word44 = get_mem(0x44);
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     if ((status1 & SNS_DEVEND) == 0) {
         status2 = wait_dev(data->addr);
     }
     if (verbose) {
        printf("0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
        printf("\n");
        printf("Size %d %d, %d %d\n", hopper_size(ctx->feed),
            stack_size(ctx->feed), hopper_size(ctx->stack[0]),
            stack_size(ctx->stack[0]));
     }

     ASSERT_EQUAL_X(SNS_CHNEND, status1);
     ASSERT_EQUAL_X(SNS_DEVEND, status2);
     ASSERT_EQUAL_X(0x00000508, word40);
     ASSERT_EQUAL_X(0x08000000, word44);
     ASSERT_EQUAL_X(0xffffffff, get_mem(0x40));
     ASSERT_EQUAL_X(0x0400ffff, get_mem(0x44));
     ASSERT_EQUAL_X(1, hopper_size(ctx->stack[0]));

     /* Make sure sense is zero */
     set_mem(0x700, 0xffffffff);
     status1 = start_io(data->addr, 0x520, 0, 0);
     if (verbose) {
        printf("700=%08x 0x40=%08x %08x\n", get_mem(0x700),
                 get_mem(0x40), get_mem(0x44));
     }

     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status1);
     ASSERT_EQUAL_X(0x00000528, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00ffffff, get_mem(0x700));


     ASSERT_EQUAL(0, save_deck(ctx->stack[0], "file2.deck"));
     close_deck(ctx->stack[0]);


    if ((f = fopen("file2.deck", "r")) == NULL)
        ASSERT_FAIL();
    i = 0;
    while(fgets(buffer, sizeof(buffer), f) != NULL) {
     if (verbose) {
         puts(buffer);
     }
         sprintf(buffer, "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\n", i);
         ASSERT_STR(buffer, buffer);
         i++;
    }
    fclose(f);
}

/* Try to read invalid punch card */
CTEST2(model2415_test, read_invalid) {
     struct _2415_context *ctx = (struct _2415_context *)(data->dev->dev);
     int     i;
     uint16_t status1 = 0;
     uint16_t status2 = 0;
     uint32_t word40;
     uint32_t word44;
     
     log_trace("Read invalid\n");
     ctx->pch_full = 0;
     empty_cards(ctx->stack[0]);
     empty_cards(ctx->stack[1]);
     ASSERT_EQUAL(1, create_card_file3 ("file3.deck", 3));
     ctx->feed->mode = MODE_BIN;
     read_deck(ctx->feed, "file3.deck");
     model2415_feed(ctx);         /* Load first card. */
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x02000600); /* Set channel words */
     set_mem(0x504, 0x00000050);
     set_mem(0x510, 0x04000700); /* Set channel words */
     set_mem(0x514, 0x00000001);
     for (i = 0; i < 0x60; i += 4) {
         set_mem(0x600 + i, 0xffffffff);
         set_mem(0x700 + i, 0xffffffff);
     }
     status1 = start_io(data->addr, 0x500, 0, 0);
     word40 = get_mem(0x40);
     word44 = get_mem(0x44);
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     if ((status1 & SNS_DEVEND) == 0) {
         status2 = wait_dev(data->addr);
     }
     if (verbose) {
        printf("0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
        for(i = 0x600; i < 0x660; i+= 4) {
           printf("0x%03x=%08x ", i, get_mem(i));
        }
        printf("\n");
    printf("Size %d %d, %d %d\n", hopper_size(ctx->feed),
            stack_size(ctx->feed), hopper_size(ctx->stack[0]),
            stack_size(ctx->stack[0]));

     }
     ASSERT_EQUAL_X(SNS_CHNEND, status1);
     ASSERT_EQUAL_X(SNS_DEVEND|SNS_UNITCHK, status2);
     ASSERT_EQUAL_X(0x00000508, word40);
     ASSERT_EQUAL_X(0x08000000, word44);
     ASSERT_EQUAL_X(0xffffffff, get_mem(0x40));
     ASSERT_EQUAL_X(0x0600ffff, get_mem(0x44));
     card_data[1] &= 0xF0ffffff;

     /* Make sure sense is zero */
     status1 = start_io(data->addr, 0x510, 0, 0);
     if (verbose) {
        printf("700=%08x 0x40=%08x %08x\n", get_mem(0x700),
                get_mem(0x40), get_mem(0x44));
     }

     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status1);
     ASSERT_EQUAL_X(0x00000518, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x08ffffff, get_mem(0x700));
}

/* Read 10 cards and verify data correct. */
CTEST2(model2415_test, read_ten) {
     struct _2415_context *ctx = (struct _2415_context *)(data->dev->dev);
     int      i;
     int      card;
     uint16_t status1 = 0;
     uint16_t status2 = 0;
     uint32_t word40;
     uint32_t word44;
     
     log_trace("Read ten\n");
     ctx->feed->mode = MODE_AUTO;
     ASSERT_EQUAL(1, create_card_file ("file1.deck", 11));
     read_deck(ctx->feed, "file1.deck");
     ctx->eof_flag = 1;
     model2415_feed(ctx);         /* Load first card. */
     for (card = 0; card < 10; card++) {
         set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
         set_mem(0x44, 0xffffffff);
         set_mem(0x500, 0x02000600); /* Set channel words */
         set_mem(0x504, 0x00000050);
         for (i = 0; i < 0x60; i += 4) {
             set_mem(0x600 + i, 0xffffffff);
         }
         status1 = start_io(data->addr, 0x500, 0, 0);
         word40 = get_mem(0x40);
         word44 = get_mem(0x44);
         set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
         set_mem(0x44, 0xffffffff);
         if ((status1 & SNS_DEVEND) == 0) {
             status2 = wait_dev(data->addr);
         }
         if (verbose) {
            printf("0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
            for(i = 0x600; i < 0x660; i+= 4) {
               printf("0x%03x=%08x ", i, get_mem(i));
            }
            printf("\n");
         }
         card_data[0] &= 0xFFfffff0;
         card_data[1] &= 0xF0ffffff;
         card_data[1] |= card << 24;
         for (i = 0; i < 80; i+=4) {
             ASSERT_EQUAL_X(card_data[i>>2], get_mem(0x600+i));
         }
         ASSERT_EQUAL_X(SNS_CHNEND, status1);
         ASSERT_EQUAL_X(SNS_DEVEND, status2);
         ASSERT_EQUAL_X(0x00000508, word40);
         ASSERT_EQUAL_X(0x08000000, word44);
         ASSERT_EQUAL_X(0xffffffff, get_mem(0x40));
         ASSERT_EQUAL_X(0x0400ffff, get_mem(0x44));
    }

    /* Read last card. */
         set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
         set_mem(0x44, 0xffffffff);
         for (i = 0; i < 0x60; i += 4) {
             set_mem(0x600 + i, 0xffffffff);
         }
         status1 = start_io(data->addr, 0x500, 0, 0);
         word40 = get_mem(0x40);
         word44 = get_mem(0x44);
         set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
         set_mem(0x44, 0xffffffff);
         if ((status1 & SNS_DEVEND) == 0) {
             status2 = wait_dev(data->addr);
         }
         if (verbose) {
            printf("0x40=%08x %08x\n", get_mem(0x40), get_mem(0x44));
            for(i = 0x600; i < 0x660; i+= 4) {
               printf("0x%03x=%08x ", i, get_mem(i));
            }
            printf("\n");
         }
         card_data[0] &= 0xFFfffff0;
         card_data[1] &= 0xF0ffffff;
         card_data[0] |= 1;
         for (i = 0; i < 80; i+=4) {
             ASSERT_EQUAL_X(card_data[i>>2], get_mem(0x600+i));
         }
         ASSERT_EQUAL_X(SNS_CHNEND, status1);
         ASSERT_EQUAL_X(SNS_DEVEND|SNS_UNITEXP, status2);
         ASSERT_EQUAL_X(0x00000508, word40);
         ASSERT_EQUAL_X(0x08000000, word44);
         ASSERT_EQUAL_X(0xffffffff, get_mem(0x40));
         ASSERT_EQUAL_X(0x0500ffff, get_mem(0x44));
}

#endif

