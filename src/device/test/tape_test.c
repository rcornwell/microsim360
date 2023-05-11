/*
 * microsim360 - Tape emulation test cases.
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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "ctest.h"
#include "logger.h"
#include "tape.h"
#include "card.h"

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
         xlen[0] = len & 0xff;
         xlen[1] = (len >> 8) & 0xff;
         xlen[2] = (len >> 16) & 0xff;
         xlen[3] = (len >> 24) & 0xff;
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

static struct _tape_buffer *tape_ctx;

/* Create a tape file with a number of records */
static int
create_tape_file (const char *filename, int recs, int type)
{
    FILE *f;
    int i;
    char   buffer[128];

    f = fopen (filename, "w");
    ASSERT_NOT_NULL(f);
    if (f == NULL)
       return 0;
    for (i=0; i<recs; i++) {
       sprintf (buffer,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", i);
       write_block(f, strlen(buffer), (uint8_t *)buffer, type);
    }
    write_mark(f, type);
    fclose (f);
    return 1;
}

CTEST_DATA(tape_test) {
   int dummy;
};

CTEST_SETUP(tape_test) {
   tape_init();
   tape_ctx = (struct _tape_buffer *)calloc(1, sizeof(struct _tape_buffer));
   ASSERT_NOT_NULL(tape_ctx);
   create_tape_file("tape1.p7b", 100, TYPE_P7B);
   create_tape_file("tape1.tap", 100, TYPE_TAP);
   create_tape_file("tape1.e11", 100, TYPE_E11);
}

CTEST_TEARDOWN(tape_test) {
    (void)remove("tape1.p7b");
    (void)remove("tape1.tap");
    (void)remove("tape1.e11");
    (void)remove("tape2.p7b");
    (void)remove("tape2.tap");
    (void)remove("tape2.e11");
    (void)remove("tape3.p7b");
    (void)remove("tape3.tap");
    (void)remove("tape3.e11");
    free(tape_ctx);
}

/* Check that we can attach to a tape */
CTEST2(tape_test, attach_test) {
    ASSERT_EQUAL(1, tape_attach(tape_ctx, "tape1.p7b", TYPE_P7B, 0, 1));
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    tape_detach(tape_ctx);
    ASSERT_EQUAL(1, tape_attach(tape_ctx, "tape1.tap", TYPE_TAP, 0, 1));
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    tape_detach(tape_ctx);
    ASSERT_EQUAL(1, tape_attach(tape_ctx, "tape1.e11", TYPE_E11, 0, 1));
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    tape_detach(tape_ctx);
    (void)remove("tape2.e11");
    ASSERT_EQUAL(0, tape_attach(tape_ctx, "tape2.e11", TYPE_E11, 0, 1));
}

CTEST2(tape_test, read_e11) {
    uint8_t   buffer1[128];
    char      buffer2[128];
    int  rec;
    int  r;
    int  i;

    ASSERT_EQUAL(1, tape_attach(tape_ctx, "tape1.e11", TYPE_E11, 0, 1));
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(0, tape_ring(tape_ctx));
    rec = 0;
    do {
        r = tape_read_forw(tape_ctx);
        if (r != 1)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec);
        i = 0;
        while (tape_read_frame(tape_ctx, &buffer1[i]) == 1) {
           i++;
        }
        buffer1[i] = '\0';
        ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(strlen(buffer2), i);
        ASSERT_STR(buffer2, (char *)&buffer1[0]);
        ASSERT_EQUAL(0, tape_at_loadpt(tape_ctx));
        rec++;
    } while (r == 1);
    ASSERT_EQUAL(2, r);
    ASSERT_EQUAL(100, rec);
    tape_detach(tape_ctx);
}

CTEST2(tape_test, read_tap) {
    uint8_t   buffer1[128];
    char      buffer2[128];
    int  rec;
    int  r;
    int  i;

    ASSERT_EQUAL(1, tape_attach(tape_ctx, "tape1.tap", TYPE_TAP, 0, 1));
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(0, tape_ring(tape_ctx));
    rec = 0;
    do {
        r = tape_read_forw(tape_ctx);
        if (r != 1)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec);
        i = 0;
        while (tape_read_frame(tape_ctx, &buffer1[i]) == 1) {
           i++;
        }
        buffer1[i] = '\0';
        ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(strlen(buffer2), i);
        ASSERT_STR(buffer2, (char *)&buffer1[0]);
        ASSERT_EQUAL(0, tape_at_loadpt(tape_ctx));
        rec++;
    } while (r == 1);
    ASSERT_EQUAL(2, r);
    ASSERT_EQUAL(100, rec);
    tape_detach(tape_ctx);
}

CTEST2(tape_test, read_p7b) {
    uint8_t   buffer1[128];
    char      buffer2[128];
    int  rec;
    int  r;
    int  i;

    ASSERT_EQUAL(1, tape_attach(tape_ctx, "tape1.p7b", TYPE_P7B, 0, 1));
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(0, tape_ring(tape_ctx));
    rec = 0;
    do {
        r = tape_read_forw(tape_ctx);
        if (r != 1)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec);
        i = 0;
        while (tape_read_frame(tape_ctx, &buffer1[i]) == 1) {
           i++;
        }
        buffer1[i] = '\0';
        ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(strlen(buffer2), i);
        ASSERT_STR(buffer2, (char *)&buffer1[0]);
        ASSERT_EQUAL(0, tape_at_loadpt(tape_ctx));
        rec++;
    } while (r == 1);
    ASSERT_EQUAL(2, r);
    ASSERT_EQUAL(100, rec);
    tape_detach(tape_ctx);
}

/* Test buffering during reading and writing */
CTEST2(tape_test, write_e11_long) {
    uint8_t   buffer1[128];
    char      buffer2[128];
    int  rec;
    int  rec_r;
    int  r;
    int  i;
    int  l;
    int  sz_r;
    int  sz;

    ASSERT_EQUAL(1, tape_attach(tape_ctx, "tape2.e11", TYPE_E11, 1, 1));
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(1, tape_ring(tape_ctx));
    rec = 0;
    sz = 0;
    /* Write a long enough tape to run over a couple buffers */
    do {
        r = tape_write_start(tape_ctx);
        if (r != 1)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec);
        i = 0;
        l = strlen(buffer2);
        for(i = 0; i < l; i++) {
           if (tape_write_frame(tape_ctx, buffer2[i]) != 1)
               break;
           sz++;
        }
        ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
        rec++;
    } while (sz < 80*1024);
    ASSERT_EQUAL(1, tape_write_mark(tape_ctx));
    /* Rewind tape to start */
    ASSERT_EQUAL(1, tape_start_rewind(tape_ctx));
    while (!tape_at_loadpt(tape_ctx)) {
        tape_rewind_frames(tape_ctx, 10000);
    }
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    /* Attempt to read it in */
    rec_r = 0;
    sz_r = 0;
    do {
        r = tape_read_forw(tape_ctx);
        if (r != 1)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec_r);
        i = 0;
        while (tape_read_frame(tape_ctx, &buffer1[i]) == 1) {
           i++;
           sz_r++;
        }
        buffer1[i] = '\0';
        ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(strlen(buffer2), i);
        ASSERT_STR(buffer2, (char *)&buffer1[0]);
        rec_r++;
    } while (r == 1);
    ASSERT_EQUAL(2, r);
    ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
    ASSERT_EQUAL(rec, rec_r);
    ASSERT_EQUAL(sz, sz_r);
    /* Try and read tape backwards */

    /* Skip over the tape mark we just read */
    ASSERT_EQUAL(2, tape_read_back(tape_ctx));
    ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
    rec_r = rec;
    sz_r = 0;
    do {
        r = tape_read_back(tape_ctx);
        if (r != 1)
           break;
        rec_r--;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec_r);
        i = strlen(buffer2);
        buffer1[i] = '\0';
        while (i >=0 && tape_read_frame(tape_ctx, &buffer1[i-1]) == 1) {
           i--;
           sz_r++;
        }
        ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(0, i);
        ASSERT_STR(buffer2, (char *)&buffer1[0]);
    } while (r == 1);
    ASSERT_EQUAL(0, rec_r);
    ASSERT_EQUAL(sz, sz_r);
    tape_detach(tape_ctx);
}

/* Test buffering during reading and writing */
CTEST2(tape_test, write_tap_long) {
    uint8_t   buffer1[128];
    char      buffer2[128];
    int  rec;
    int  rec_r;
    int  r;
    int  i;
    int  l;
    int  sz_r;
    int  sz;

    ASSERT_EQUAL(1, tape_attach(tape_ctx, "tape2.tap", TYPE_E11, 1, 1));
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(1, tape_ring(tape_ctx));
    /* Write a long enough tape to run over a couple buffers */
    rec = 0;
    sz = 0;
    do {
        r = tape_write_start(tape_ctx);
        if (r != 1)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec);
        i = 0;
        l = strlen(buffer2);
        for(i = 0; i < l; i++) {
           if (tape_write_frame(tape_ctx, buffer2[i]) != 1)
               break;
           sz++;
        }
        ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
        rec++;
    } while (sz < 80*1024);
    ASSERT_EQUAL(1, tape_write_mark(tape_ctx));
    /* Rewind tape to start */
    ASSERT_EQUAL(1, tape_start_rewind(tape_ctx));
    while (!tape_at_loadpt(tape_ctx)) {
        tape_rewind_frames(tape_ctx, 10000);
    }
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    /* Attempt to read it in */
    rec_r = 0;
    do {
        r = tape_read_forw(tape_ctx);
        if (r != 1)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec_r);
        i = 0;
        while (tape_read_frame(tape_ctx, &buffer1[i]) == 1) {
           i++;
        }
        buffer1[i] = '\0';
        ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(strlen(buffer2), i);
        ASSERT_STR(buffer2, (char *)&buffer1[0]);
        rec_r++;
    } while (r == 1);
    ASSERT_EQUAL(2, r);
    ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
    ASSERT_EQUAL(rec, rec_r);
    /* Try and read tape backwards */
    /* Skip over the tape mark we just read */
    ASSERT_EQUAL(2, tape_read_back(tape_ctx));
    ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
    rec_r = rec;
    sz_r = 0;
    do {
        r = tape_read_back(tape_ctx);
        if (r != 1)
           break;
        rec_r--;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec_r);
        i = strlen(buffer2);
        buffer1[i] = '\0';
        while (tape_read_frame(tape_ctx, &buffer1[i-1]) == 1) {
           i--;
           sz_r++;
        }
        ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(0, i);
        ASSERT_STR(buffer2, (char *)&buffer1[0]);
    } while (r == 1);
    ASSERT_EQUAL(0, rec_r);
    ASSERT_EQUAL(sz, sz_r);
    tape_detach(tape_ctx);
}

/* Test buffering during reading and writing */
CTEST2(tape_test, write_p7b_long) {
    uint8_t   buffer1[128];
    char      buffer2[128];
    int  rec;
    int  rec_r;
    int  r;
    int  i;
    int  l;
    int  sz_r;
    int  sz;

    ASSERT_EQUAL(1, tape_attach(tape_ctx, "tape2.p7b", TYPE_P7B, 1, 1));
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(1, tape_ring(tape_ctx));
    rec = 0;
    sz = 0;
    do {
        r = tape_write_start(tape_ctx);
        if (r != 1)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec);
        i = 0;
        l = strlen(buffer2);
        for(i = 0; i < l; i++) {
           if (tape_write_frame(tape_ctx, buffer2[i]) != 1)
               break;
           sz++;
        }
        ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
        rec++;
    } while (sz < 80*1024);
    ASSERT_EQUAL(1, tape_write_mark(tape_ctx));
    /* Rewind tape to start */
    ASSERT_EQUAL(1, tape_start_rewind(tape_ctx));
    while (!tape_at_loadpt(tape_ctx)) {
        tape_rewind_frames(tape_ctx, 10000);
    }
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    rec_r = 0;
    do {
        r = tape_read_forw(tape_ctx);
        if (r != 1)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec_r);
        i = 0;
        while (tape_read_frame(tape_ctx, &buffer1[i]) == 1) {
           i++;
        }
        buffer1[i] = '\0';
        ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(strlen(buffer2), i);
        ASSERT_STR(buffer2, (char *)&buffer1[0]);
        rec_r++;
    } while (r == 1);
    ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
    ASSERT_EQUAL(2, r);
    ASSERT_EQUAL(rec, rec_r);
    /* Try and read tape backwards */
    /* Skip over the tape mark we just read */
    ASSERT_EQUAL(2, tape_read_back(tape_ctx));
    ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
    rec_r = rec;
    sz_r = 0;
    do {
        r = tape_read_back(tape_ctx);
        if (r != 1)
           break;
        rec_r--;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec_r);
        i = strlen(buffer2);
        buffer1[i] = '\0';
        while ((r = tape_read_frame(tape_ctx, &buffer1[i-1])) == 1) {
           i--;
           sz_r++;
        }
        ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(0, i);
        ASSERT_STR(buffer2, (char *)&buffer1[0]);
        sz_r = 0;
    } while (r == 1);
    tape_detach(tape_ctx);
}


/* Write a series of record in increasing size */
CTEST2(tape_test, write_long_rec) {
    uint8_t   d;
    int  rec;
    int  rec_r;
    int  r;
    int  i;
    int  sz_r;
    int  sz;

    ASSERT_EQUAL(1, tape_attach(tape_ctx, "tape3.tap", TYPE_E11, 1, 1));
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(1, tape_ring(tape_ctx));
    rec = 0;
    sz = 4000;
    do {
        r = tape_write_start(tape_ctx);
        if (r != 1)
           break;
        for(i = 0; i < sz; i++) {
           d = (i & 0xff);
           if (tape_write_frame(tape_ctx, d) != 1)
               break;
        }
        ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
        sz += 2000;
        rec++;
    } while (sz < 80*1024);
    ASSERT_EQUAL(1, tape_write_mark(tape_ctx));
    /* Rewind tape to start */
    ASSERT_EQUAL(1, tape_start_rewind(tape_ctx));
    while (!tape_at_loadpt(tape_ctx)) {
        tape_rewind_frames(tape_ctx, 10000);
    }
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    /* Make sure we can read the tape */
    rec_r = 0;
    sz_r = 4000;
    do {
        r = tape_read_forw(tape_ctx);
        if (r != 1)
           break;
        for(i = 0; i < sz_r; i++) {
           if (tape_read_frame(tape_ctx, &d) != 1)
               break;
           if (d != (i & 0xff))
               break;
        }
        ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(sz_r, i);
        sz_r += 2000;
        rec_r++;
    } while (r == 1);
    ASSERT_EQUAL(2, r);
    ASSERT_EQUAL(rec, rec_r);
    ASSERT_EQUAL(sz, sz_r);
    tape_detach(tape_ctx);
}

/* Write a series of record in increasing size */
CTEST2(tape_test, write_long_rec_p7b) {
    uint8_t   d;
    int  rec;
    int  rec_r;
    int  r;
    int  i;
    int  sz_r;
    int  sz;

    ASSERT_EQUAL(1, tape_attach(tape_ctx, "tape3.p7b", TYPE_P7B, 1, 1));
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(1, tape_ring(tape_ctx));
    rec = 0;
    sz = 4000;
    do {
        r = tape_write_start(tape_ctx);
        if (r != 1)
           break;
        for(i = 0; i < sz; i++) {
           d = (i & 0x7f);
           if (tape_write_frame(tape_ctx, d) != 1)
               break;
        }
        ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
        sz += 2000;
        rec++;
    } while (sz < 80*1024);
    ASSERT_EQUAL(1, tape_write_mark(tape_ctx));
    /* Rewind tape to start */
    ASSERT_EQUAL(1, tape_start_rewind(tape_ctx));
    while (!tape_at_loadpt(tape_ctx)) {
        tape_rewind_frames(tape_ctx, 10000);
    }
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    /* Make sure we can read the tape */
    rec_r = 0;
    sz_r = 4000;
    do {
        r = tape_read_forw(tape_ctx);
        if (r != 1)
           break;
        for(i = 0; i < sz_r; i++) {
           if (tape_read_frame(tape_ctx, &d) != 1)
               break;
           if (d != (i & 0x7f))
               break;
        }
        ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(sz_r, i);
        sz_r += 2000;
        rec_r++;
    } while (r == 1);
    ASSERT_EQUAL(2, r);
    ASSERT_EQUAL(rec, rec_r);
    ASSERT_EQUAL(sz, sz_r);
    tape_detach(tape_ctx);
}

/* Write a tape mark ever 10 records, verify read correct forward and
   backwards.
*/
CTEST2(tape_test, write_mark) {
    uint8_t   buffer1[128];
    char      buffer2[128];
    int  rec;
    int  rec_r;
    int  r;
    int  i;
    int  l;
    int  sz_r;
    int  sz;

    ASSERT_EQUAL(1, tape_attach(tape_ctx, "tape2.tap", TYPE_E11, 1, 1));
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(1, tape_ring(tape_ctx));
    /* Every 10 records write a tape mark, then put 2 on end */
    rec = 0;
    sz = 0;
    do {
        r = tape_write_start(tape_ctx);
        if (r != 1)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec);
        i = 0;
        l = strlen(buffer2);
        for(i = 0; i < l; i++) {
           if (tape_write_frame(tape_ctx, buffer2[i]) != 1)
               break;
           sz++;
        }
        ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
        rec++;
        if ((rec % 10) == 0) {
            ASSERT_EQUAL(1, tape_write_mark(tape_ctx));
        }
    } while (rec < 100);
    ASSERT_EQUAL(1, tape_write_mark(tape_ctx));
    /* Rewind tape to start */
    ASSERT_EQUAL(1, tape_start_rewind(tape_ctx));
    while (!tape_at_loadpt(tape_ctx)) {
        tape_rewind_frames(tape_ctx, 10000);
    }
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    /* Attempt to read it in */
    rec_r = 0;
    do {
        r = tape_read_forw(tape_ctx);
        /* Check for tape mark */
        if (r == 2) {
            ASSERT_EQUAL(0, (rec_r) % 10);
            ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
            r = tape_read_forw(tape_ctx);
        }
        if (r != 1)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec_r);
        i = 0;
        while (tape_read_frame(tape_ctx, &buffer1[i]) == 1) {
           i++;
        }
        buffer1[i] = '\0';
        ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(strlen(buffer2), i);
        ASSERT_STR(buffer2, (char *)&buffer1[0]);
        rec_r++;
    } while (r == 1);
    ASSERT_EQUAL(2, r);
    ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
    ASSERT_EQUAL(0, tape_read_forw(tape_ctx));
    ASSERT_EQUAL(rec, rec_r);
    /* Try and read tape backwards */
    r = tape_read_back(tape_ctx);
    r = tape_read_frame(tape_ctx, &buffer1[0]);
    ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
    rec_r = rec;
    sz_r = 0;
    do {
        r = tape_read_back(tape_ctx);
        /* Check for tape mark */
        if (r == 2) {
            ASSERT_EQUAL(0, (rec_r) % 10);
            ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
            r = tape_read_back(tape_ctx);
        }
        if (r != 1)
           break;
        rec_r--;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec_r);
        i = strlen(buffer2);
        buffer1[i] = '\0';
        while (tape_read_frame(tape_ctx, &buffer1[i-1]) == 1 && i > 0) {
           i--;
           sz_r++;
        }
        ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(0, i);
        ASSERT_STR(buffer2, (char *)&buffer1[0]);
    } while (r == 1);
    ASSERT_EQUAL(0, rec_r);
    ASSERT_EQUAL(sz, sz_r);
    tape_detach(tape_ctx);
}

/* Write a tape mark ever 10 records, verify read correct forward and
   backwards.
*/
CTEST2(tape_test, write_mark_p7b) {
    uint8_t   buffer1[128];
    char      buffer2[128];
    int  rec;
    int  rec_r;
    int  r;
    int  i;
    int  l;
    int  sz_r;
    int  sz;

    ASSERT_EQUAL(1, tape_attach(tape_ctx, "tape2.p7b", TYPE_P7B, 1, 1));
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(1, tape_ring(tape_ctx));
    /* Every 10 records write a tape mark, then put 2 on end */
    rec = 0;
    sz = 0;
    do {
        r = tape_write_start(tape_ctx);
        if (r != 1)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec);
        i = 0;
        l = strlen(buffer2);
        for(i = 0; i < l; i++) {
           if (tape_write_frame(tape_ctx, buffer2[i]) != 1)
               break;
           sz++;
        }
        ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
        rec++;
        if ((rec % 10) == 0) {
            ASSERT_EQUAL(1, tape_write_mark(tape_ctx));
        }
    } while (rec < 100);
    ASSERT_EQUAL(1, tape_write_mark(tape_ctx));
    /* Rewind tape to start */
    ASSERT_EQUAL(1, tape_start_rewind(tape_ctx));
    while (!tape_at_loadpt(tape_ctx)) {
        tape_rewind_frames(tape_ctx, 10000);
    }
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    /* Attempt to read it in */
    rec_r = 0;
    do {
        r = tape_read_forw(tape_ctx);
        /* Check for tape mark */
        if (r == 2) {
            ASSERT_EQUAL(0, (rec_r) % 10);
            ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
            r = tape_read_forw(tape_ctx);
        }
        if (r != 1)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec_r);
        i = 0;
        while (tape_read_frame(tape_ctx, &buffer1[i]) == 1) {
           i++;
        }
        buffer1[i] = '\0';
        ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(strlen(buffer2), i);
        ASSERT_STR(buffer2, (char *)&buffer1[0]);
        rec_r++;
    } while (r == 1);
    ASSERT_EQUAL(2, r);
    ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
    ASSERT_EQUAL(0, tape_read_forw(tape_ctx));
    ASSERT_EQUAL(rec, rec_r);
    /* Try and read tape backwards */
    r = tape_read_back(tape_ctx);
    r = tape_read_frame(tape_ctx, &buffer1[0]);
    ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
    rec_r = rec;
    sz_r = 0;
    do {
        r = tape_read_back(tape_ctx);
        /* Check for tape mark */
        if (r == 2) {
            ASSERT_EQUAL(0, (rec_r) % 10);
            ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
            r = tape_read_back(tape_ctx);
        }
        if (r != 1)
           break;
        rec_r--;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec_r);
        i = strlen(buffer2);
        buffer1[i] = '\0';
        while (tape_read_frame(tape_ctx, &buffer1[i-1]) == 1) {
           i--;
           sz_r++;
        }
        ASSERT_EQUAL(1, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(0, i);
        ASSERT_STR(buffer2, (char *)&buffer1[0]);
    } while (r == 1);
    ASSERT_EQUAL(0, rec_r);
    ASSERT_EQUAL(sz, sz_r);
    tape_detach(tape_ctx);
}

