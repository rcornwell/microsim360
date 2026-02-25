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
#include "xlat.h"

/* Write out a tape format block */
static void
write_block(FILE *f, uint32_t len, uint8_t *buffer, int type)
{
    uint32_t wlen = 0x0fffffff & len;
    uint8_t  xlen[4];

    switch(type) {
    case TYPE_TAP:
         wlen = 0x0fffffff & ((len + 1) & ~1);
         /* Fall through */
    case TYPE_E11:
         xlen[0] = (len) & 0xff;
         xlen[1] = (len >> 8) & 0xff;
         xlen[2] = (len >> 16) & 0xff;
         xlen[3] = (len >> 24) & 0x0f;
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

/* Write out a tape info block */
static void
write_block_spec(FILE *f, uint32_t len, uint8_t *buffer, int type, int rec)
{
    uint32_t wlen = 0x0fffffff & len;
    uint8_t  xlen[4];

    switch(type) {
    case TYPE_TAP:
         wlen = 0x0fffffff & ((len + 1) & ~1);
         /* Fall through */
    case TYPE_E11:
         xlen[0] = (len) & 0xff;
         xlen[1] = (len >> 8) & 0xff;
         xlen[2] = (len >> 16) & 0xff;
         xlen[3] = (len >> 24) & 0x0f;
         xlen[3] |= (rec << 4);
         fwrite(xlen, sizeof(uint8_t), 4, f);
         fwrite(buffer, sizeof(uint8_t), wlen, f);
         fwrite(xlen, sizeof(uint8_t), 4, f);
         break;
    case TYPE_P7B:  /* Not supported */
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

/* Convert buffer to BCD */
static int
convert_ascii_to_bcd(char *buffer)
{
     int     sz = strlen(buffer);
     int     i;

     for (i = 0; i < sz; i++) {
         uint8_t   ch = ebcdic_to_bcd[ascii_to_ebcdic[(int)buffer[i]]];
         ch |= 0100 ^ parity_table[ch];
         buffer[i] = ch;
     }
     return sz;
}
  
/* Convert buffer to ASCII */
static int
convert_bcd_to_ascii(uint8_t *buffer, int sz)
{
     int     i;

     for (i = 0; i < sz; i++) {
         uint8_t   ch = ebcdic_to_ascii[bcd_to_ebcdic[buffer[i] & 077]];
         buffer[i] = ch;
     }
     buffer[i] = '\0';
     return sz;
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

/* Create a tape file with a number of records and special records */
static int
create_tape_file_spec (const char *filename, int recs, int type)
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
       write_block_spec(f, strlen(buffer), (uint8_t *)buffer, type, (i | 1) & 7);
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
   create_tape_file_spec("tape4.tap", 100, TYPE_TAP);
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
    (void)remove("tape4.tap");
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
        if (r != TAPE_STATUS_OK)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec);
        i = 0;
        while ((r = tape_read_frame(tape_ctx, &buffer1[i])) == TAPE_STATUS_OK) {
           i++;
           ASSERT_TRUE(i < 128);
        }
        buffer1[i] = '\0';
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(strlen(buffer2), i);
        ASSERT_STR(buffer2, (char *)&buffer1[0]);
        ASSERT_EQUAL(0, tape_at_loadpt(tape_ctx));
        rec++;
    } while (r == TAPE_STATUS_EOB);
    ASSERT_EQUAL(TAPE_STATUS_MARK, r);
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
        if (r != TAPE_STATUS_OK)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec);
        i = 0;
        while (tape_read_frame(tape_ctx, &buffer1[i]) == TAPE_STATUS_OK) {
           i++;
           ASSERT_TRUE(i < 128);
        }
        buffer1[i] = '\0';
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(strlen(buffer2), i);
        ASSERT_STR(buffer2, (char *)&buffer1[0]);
        ASSERT_EQUAL(0, tape_at_loadpt(tape_ctx));
        rec++;
    } while (r == TAPE_STATUS_OK);
    ASSERT_EQUAL(TAPE_STATUS_MARK, r);
    ASSERT_EQUAL(100, rec);
    tape_detach(tape_ctx);
}

CTEST2(tape_test, read_tap_spec) {
    uint8_t   buffer1[128];
    char      buffer2[128];
    int  rec;
    int  r;
    int  i;

    ASSERT_EQUAL(1, tape_attach(tape_ctx, "tape4.tap", TYPE_TAP, 0, 1));
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(0, tape_ring(tape_ctx));
    rec = 0;
    do {
        r = tape_read_forw(tape_ctx);
        if (r != TAPE_STATUS_OK)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec);
        i = 0;
        while (tape_read_frame(tape_ctx, &buffer1[i]) == TAPE_STATUS_OK) {
           i++;
           ASSERT_TRUE(i < 128);
        }
        buffer1[i] = '\0';
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(strlen(buffer2), i);
        ASSERT_STR(buffer2, (char *)&buffer1[0]);
        ASSERT_EQUAL(0, tape_at_loadpt(tape_ctx));
        rec++;
    } while (r == TAPE_STATUS_OK);
    ASSERT_EQUAL(TAPE_STATUS_MARK, r);
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
        if (r != TAPE_STATUS_OK)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec);
        i = 0;
        while (tape_read_frame(tape_ctx, &buffer1[i]) == TAPE_STATUS_OK) {
           i++;
           ASSERT_TRUE(i < 128);
        }
        buffer1[i] = '\0';
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(strlen(buffer2), i);
        ASSERT_STR(buffer2, (char *)&buffer1[0]);
        ASSERT_EQUAL(0, tape_at_loadpt(tape_ctx));
        rec++;
    } while (r == TAPE_STATUS_OK);
    ASSERT_EQUAL(TAPE_STATUS_MARK, r);
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
        if (r != TAPE_STATUS_OK)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec);
        i = 0;
        l = strlen(buffer2);
        for(i = 0; i < l; i++) {
           if (tape_write_frame(tape_ctx, buffer2[i]) != TAPE_STATUS_OK)
               break;
           sz++;
        }
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        rec++;
    } while (sz < 80*1024);
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_write_mark(tape_ctx));
    /* Rewind tape to start */
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_start_rewind(tape_ctx));
    while (!tape_at_loadpt(tape_ctx)) {
        tape_rewind_frames(tape_ctx, 10000);
    }
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    /* Attempt to read it in */
    rec_r = 0;
    sz_r = 0;
    do {
        r = tape_read_forw(tape_ctx);
        if (r != TAPE_STATUS_OK)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec_r);
        i = 0;
        while (tape_read_frame(tape_ctx, &buffer1[i]) == TAPE_STATUS_OK) {
           i++;
           sz_r++;
           ASSERT_TRUE(i < 128);
        }
        buffer1[i] = '\0';
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(strlen(buffer2), i);
        ASSERT_STR(buffer2, (char *)&buffer1[0]);
        rec_r++;
    } while (r == TAPE_STATUS_OK);
    ASSERT_EQUAL(TAPE_STATUS_MARK, r);
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
    ASSERT_EQUAL(rec, rec_r);
    ASSERT_EQUAL(sz, sz_r);
    /* Try and read tape backwards */

    /* Skip over the tape mark we just read */
    ASSERT_EQUAL(TAPE_STATUS_MARK, tape_read_back(tape_ctx));
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
    rec_r = rec;
    sz_r = 0;
    do {
        r = tape_read_back(tape_ctx);
        if (r != TAPE_STATUS_OK)
           break;
        rec_r--;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec_r);
        i = strlen(buffer2);
        buffer1[i] = '\0';
        while (i >=0 && tape_read_frame(tape_ctx, &buffer1[i-1]) == TAPE_STATUS_OK) {
           i--;
           ASSERT_TRUE(i >= 0);
           sz_r++;
        }
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(0, i);
        ASSERT_STR(buffer2, (char *)&buffer1[0]);
    } while (r == TAPE_STATUS_OK);
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

    ASSERT_EQUAL(1, tape_attach(tape_ctx, "tape2.tap", TYPE_TAP, 1, 1));
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(1, tape_ring(tape_ctx));
    /* Write a long enough tape to run over a couple buffers */
    rec = 0;
    sz = 0;
    do {
        r = tape_write_start(tape_ctx);
        if (r != TAPE_STATUS_OK)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec);
        i = 0;
        l = strlen(buffer2);
        for(i = 0; i < l; i++) {
           if (tape_write_frame(tape_ctx, buffer2[i]) != TAPE_STATUS_OK)
               break;
           sz++;
        }
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        rec++;
    } while (sz < 80*1024);
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_write_mark(tape_ctx));
    /* Rewind tape to start */
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_start_rewind(tape_ctx));
    while (!tape_at_loadpt(tape_ctx)) {
        tape_rewind_frames(tape_ctx, 10000);
    }
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    /* Attempt to read it in */
    rec_r = 0;
    do {
        r = tape_read_forw(tape_ctx);
        if (r != TAPE_STATUS_OK)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec_r);
        i = 0;
        while (tape_read_frame(tape_ctx, &buffer1[i]) == TAPE_STATUS_OK) {
           i++;
           ASSERT_TRUE(i < 128);
        }
        buffer1[i] = '\0';
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(strlen(buffer2), i);
        ASSERT_STR(buffer2, (char *)&buffer1[0]);
        rec_r++;
    } while (r == TAPE_STATUS_OK);
    ASSERT_EQUAL(TAPE_STATUS_MARK, r);
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
    ASSERT_EQUAL(rec, rec_r);
    /* Try and read tape backwards */
    /* Skip over the tape mark we just read */
    ASSERT_EQUAL(TAPE_STATUS_MARK, tape_read_back(tape_ctx));
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
    rec_r = rec;
    sz_r = 0;
    do {
        r = tape_read_back(tape_ctx);
        if (r != TAPE_STATUS_OK)
           break;
        rec_r--;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec_r);
        i = strlen(buffer2);
        buffer1[i] = '\0';
        while (tape_read_frame(tape_ctx, &buffer1[i-1]) == TAPE_STATUS_OK) {
           i--;
           ASSERT_TRUE(i >= 0);
           sz_r++;
        }
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(0, i);
        ASSERT_STR(buffer2, (char *)&buffer1[0]);
    } while (r == TAPE_STATUS_OK);
    ASSERT_EQUAL(0, rec_r);
    ASSERT_EQUAL(sz, sz_r);
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
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
    rec_r = 0;
    sz_r = 0;
    do {
        r = tape_write_start(tape_ctx);
        if (r != TAPE_STATUS_OK)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec_r);
        l = convert_ascii_to_bcd(buffer2);
        for(i = 0; i < l; i++) {
           if (tape_write_frame(tape_ctx, buffer2[i]) != TAPE_STATUS_OK)
               break;
           sz_r++;
        }
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        rec_r++;
    } while (sz_r < 80*1024);
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_write_mark(tape_ctx));
    /* Rewind tape to start */
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_start_rewind(tape_ctx));
    while (!tape_at_loadpt(tape_ctx)) {
        tape_rewind_frames(tape_ctx, 10000);
    }
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    rec = 0;
    sz = 0;
    do {
        r = tape_read_forw(tape_ctx);
        if (r != TAPE_STATUS_OK)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec);
        i = 0;
        while (tape_read_frame(tape_ctx, &buffer1[i]) == TAPE_STATUS_OK) {
           i++;
           sz++;
           ASSERT_TRUE(i < 128);
        }
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        l = strlen(buffer2);
        convert_bcd_to_ascii(buffer1, i);
        ASSERT_EQUAL(l, i);
        ASSERT_STR(buffer2, (char *)&buffer1[0]);
        rec++;
    } while (r == TAPE_STATUS_OK);
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
    ASSERT_EQUAL(TAPE_STATUS_MARK, r);
    ASSERT_EQUAL(rec_r, rec);
    ASSERT_EQUAL(sz_r, sz);
    /* Try and read tape backwards */
    /* Skip over the tape mark we just read */
    ASSERT_EQUAL(TAPE_STATUS_MARK, tape_read_back(tape_ctx));
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
    rec_r = rec;
    sz_r = 0;
    do {
        r = tape_read_back(tape_ctx);
        if (r != TAPE_STATUS_OK) {
           break;
        }
        rec_r--;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec_r);
        i = strlen(buffer2);
        buffer1[i] = '\0';
        while (tape_read_frame(tape_ctx, &buffer1[i-1]) == TAPE_STATUS_OK) {
           i--;
           ASSERT_TRUE(i >= 0);
           sz_r++;
        }
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        convert_bcd_to_ascii(buffer1, strlen(buffer2));
        ASSERT_EQUAL(0, i);
        ASSERT_STR(buffer2, (char *)&buffer1[0]);
    } while (r == TAPE_STATUS_OK);
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(0, rec_r);
    ASSERT_EQUAL(sz_r, sz);
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
        if (r != TAPE_STATUS_OK)
           break;
        for(i = 0; i < sz; i++) {
           d = (i & 0xff);
           if (tape_write_frame(tape_ctx, d) != TAPE_STATUS_OK)
               break;
        }
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        sz += 2000;
        rec++;
    } while (sz < 80*1024);
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_write_mark(tape_ctx));
    /* Rewind tape to start */
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_start_rewind(tape_ctx));
    while (!tape_at_loadpt(tape_ctx)) {
        tape_rewind_frames(tape_ctx, 10000);
    }
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    /* Make sure we can read the tape */
    rec_r = 0;
    sz_r = 4000;
    do {
        r = tape_read_forw(tape_ctx);
        if (r != TAPE_STATUS_OK)
           break;
        for(i = 0; i < sz_r; i++) {
           if (tape_read_frame(tape_ctx, &d) != TAPE_STATUS_OK)
               break;
           if (d != (i & 0xff))
               break;
        }
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(sz_r, i);
        sz_r += 2000;
        rec_r++;
    } while (r == TAPE_STATUS_OK);
    ASSERT_EQUAL(TAPE_STATUS_MARK, r);
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
        if (r != TAPE_STATUS_OK)
           break;
        for(i = 1; i < sz; i++) {
           d = (i & 0x7f);
           if (tape_write_frame(tape_ctx, d) != TAPE_STATUS_OK)
               break;
        }
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        sz += 2000;
        rec++;
    } while (sz < 80*1024);
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_write_mark(tape_ctx));
    /* Rewind tape to start */
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_start_rewind(tape_ctx));
    while (!tape_at_loadpt(tape_ctx)) {
        tape_rewind_frames(tape_ctx, 10000);
    }
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    /* Make sure we can read the tape */
    rec_r = 0;
    sz_r = 4000;
    do {
        r = tape_read_forw(tape_ctx);
        if (r != TAPE_STATUS_OK)
           break;
        for(i = 1; i < sz_r; i++) {
           if (tape_read_frame(tape_ctx, &d) != TAPE_STATUS_OK)
               break;
           if (d != (i & 0x7f))
               break;
        }
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(sz_r, i);
        sz_r += 2000;
        rec_r++;
    } while (r == TAPE_STATUS_OK);
    ASSERT_EQUAL(TAPE_STATUS_MARK, r);
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
        if (r != TAPE_STATUS_OK)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec);
        i = 0;
        l = strlen(buffer2);
        for(i = 0; i < l; i++) {
           if (tape_write_frame(tape_ctx, buffer2[i]) != TAPE_STATUS_OK)
               break;
           sz++;
        }
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        rec++;
        if ((rec % 10) == 0) {
            ASSERT_EQUAL(TAPE_STATUS_OK, tape_write_mark(tape_ctx));
        }
    } while (rec < 100);
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_write_mark(tape_ctx));
    /* Rewind tape to start */
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_start_rewind(tape_ctx));
    while (!tape_at_loadpt(tape_ctx)) {
        tape_rewind_frames(tape_ctx, 10000);
    }
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    /* Attempt to read it in */
    rec_r = 0;
    do {
        r = tape_read_forw(tape_ctx);
        /* Check for tape mark */
        if (r == TAPE_STATUS_MARK) {
            ASSERT_EQUAL(0, (rec_r) % 10);
            ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
            r = tape_read_forw(tape_ctx);
        }
        if (r != TAPE_STATUS_OK)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec_r);
        i = 0;
        while (tape_read_frame(tape_ctx, &buffer1[i]) == TAPE_STATUS_OK) {
           i++;
           ASSERT_TRUE(i < 128);
        }
        buffer1[i] = '\0';
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(strlen(buffer2), i);
        ASSERT_STR(buffer2, (char *)&buffer1[0]);
        rec_r++;
    } while (r == TAPE_STATUS_OK);
    ASSERT_EQUAL(TAPE_STATUS_MARK, r);
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
    ASSERT_EQUAL(TAPE_STATUS_EOT, tape_read_forw(tape_ctx));
    ASSERT_EQUAL(rec, rec_r);
    /* Try and read tape backwards */
    r = tape_read_back(tape_ctx);
    r = tape_read_frame(tape_ctx, &buffer1[0]);
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
    rec_r = rec;
    sz_r = 0;
    do {
        r = tape_read_back(tape_ctx);
        /* Check for tape mark */
        if (r == TAPE_STATUS_MARK) {
            ASSERT_EQUAL(0, (rec_r) % 10);
            ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
            r = tape_read_back(tape_ctx);
        }
        if (r != TAPE_STATUS_OK)
           break;
        rec_r--;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec_r);
        i = strlen(buffer2);
        buffer1[i] = '\0';
        while (tape_read_frame(tape_ctx, &buffer1[i-1]) == TAPE_STATUS_OK && i > 0) {
           i--;
           ASSERT_TRUE(i >= 0);
           sz_r++;
        }
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(0, i);
        ASSERT_STR(buffer2, (char *)&buffer1[0]);
    } while (r == TAPE_STATUS_OK);
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
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
    uint8_t   buffer3[128];
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
        if (r != TAPE_STATUS_OK)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec);
        l = convert_ascii_to_bcd(buffer2);
        for(i = 0; i < l; i++) {
           if (tape_write_frame(tape_ctx, buffer2[i]) != TAPE_STATUS_OK)
               break;
           sz++;
        }
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        rec++;
        if ((rec % 10) == 0) {
            ASSERT_EQUAL(TAPE_STATUS_OK, tape_write_mark(tape_ctx));
        }
    } while (rec < 100);
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_write_mark(tape_ctx));
    /* Rewind tape to start */
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_start_rewind(tape_ctx));
    while (!tape_at_loadpt(tape_ctx)) {
        tape_rewind_frames(tape_ctx, 10000);
    }
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    /* Attempt to read it in */
    rec_r = 0;
    do {
        r = tape_read_forw(tape_ctx);
        /* Check for tape mark */
        if (r == TAPE_STATUS_MARK) {
            ASSERT_EQUAL(0, (rec_r) % 10);
            ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
            r = tape_read_forw(tape_ctx);
        }
        if (r != TAPE_STATUS_OK)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec_r);
        i = 0;
        while (tape_read_frame(tape_ctx, &buffer1[i]) == TAPE_STATUS_OK) {
           i++;
           ASSERT_TRUE(i < 128);
        }
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        l = strlen(buffer2);
        convert_bcd_to_ascii(buffer1, i);
        buffer1[i] = '\0';
        ASSERT_EQUAL(strlen(buffer2), i);
        ASSERT_STR(buffer2, (char *)&buffer1[0]);
        rec_r++;
    } while (r == TAPE_STATUS_OK);
    ASSERT_EQUAL(TAPE_STATUS_MARK, r);
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
    ASSERT_EQUAL(TAPE_STATUS_EOT, tape_read_forw(tape_ctx));
    ASSERT_EQUAL(rec, rec_r);
    /* Try and read tape backwards */
    r = tape_read_back(tape_ctx);
    r = tape_read_frame(tape_ctx, &buffer1[0]);
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
    rec_r = rec;
    sz_r = 0;
    do {
        r = tape_read_back(tape_ctx);
        if (r == TAPE_STATUS_MARK) {
            ASSERT_EQUAL(0, (rec_r) % 10);
            ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
            r = tape_read_back(tape_ctx);
        }
        if (r != TAPE_STATUS_OK)
           break;
        rec_r--;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec_r);
        i = 0;
        while ((r = tape_read_frame(tape_ctx, &buffer3[i])) == TAPE_STATUS_OK) {
           i++;
           sz_r++;
        }
        for (l = 0; i >= 0;) {
            buffer1[l++] = buffer3[--i];
        }
        convert_bcd_to_ascii(buffer1, --l);
        i = strlen(buffer2);
        ASSERT_EQUAL(i, l);
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        ASSERT_STR(buffer2, (char *)&buffer1[0]);
    } while (r == TAPE_STATUS_EOB);
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(0, rec_r);
    ASSERT_EQUAL(sz_r, sz);
    tape_detach(tape_ctx);
}

/* Write a long record and re-read it */
CTEST2(tape_test, write_tap_long_rec) {
    uint8_t   buffer1[128];
    char      buffer2[128];
    int  rec;
    int  rec_r;
    int  r;
    int  i;
    int  l;
    int  sz_r;
    int  sz;
    int  cnt;

    ASSERT_EQUAL(1, tape_attach(tape_ctx, "tape2.tap", TYPE_TAP, 1, 1));
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(1, tape_ring(tape_ctx));
    /* Write a long enough tape to run over a couple buffers */
    cnt = 0;
    do {
        r = tape_write_start(tape_ctx);
        if (r != TAPE_STATUS_OK)
           break;
        rec = 0;
        sz = 0;
        do {
            sprintf (buffer2,
              "%02d%03d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789  ", cnt, rec);
            i = 0;
            l = strlen(buffer2);
            for(i = 0; i < l; i++) {
               if (tape_write_frame(tape_ctx, buffer2[i]) != TAPE_STATUS_OK)
                   break;
               sz++;
            }
            rec++;
        } while (rec < 100);
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
    } while (cnt++ < 10);

    ASSERT_EQUAL(TAPE_STATUS_OK, tape_write_mark(tape_ctx));
    /* Rewind tape to start */
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_start_rewind(tape_ctx));
    while (!tape_at_loadpt(tape_ctx)) {
        tape_rewind_frames(tape_ctx, 10000);
    }
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    /* Attempt to read it in */
    cnt = 0;
    do {
        r = tape_read_forw(tape_ctx);
        if (r != TAPE_STATUS_OK)
           break;
        rec_r = 0;
        sz_r = 0;
        do {
            for (i = 0; i < 80; i++) {
                r = tape_read_frame(tape_ctx, &buffer1[i]);
                if (r == TAPE_STATUS_EOB) {
                    break;
                }
                ASSERT_EQUAL(r, TAPE_STATUS_OK);
                sz_r++;
            }
            if (r == TAPE_STATUS_EOB) {
                break;
            }
            buffer1[i] = '\0';
            sprintf (buffer2,
              "%02d%03d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789  ", cnt, rec_r);
            ASSERT_STR(buffer2, (char *)&buffer1[0]);
            rec_r++;
        } while (r == TAPE_STATUS_OK);
        ASSERT_EQUAL(TAPE_STATUS_EOB, r);
        ASSERT_EQUAL(sz, sz_r);
        ASSERT_EQUAL(rec, rec_r);
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(0, tape_parity_error(tape_ctx));
        cnt++;
    } while (cnt < 20);
    ASSERT_EQUAL(TAPE_STATUS_MARK, r);

    /* Try and read tape backwards */
    /* Skip over the tape mark we just read */
    ASSERT_EQUAL(TAPE_STATUS_MARK, tape_read_back(tape_ctx));
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
    rec_r = rec;
    sz_r = 0;
    do {
        r = tape_read_back(tape_ctx);
        if (r != TAPE_STATUS_OK)
           break;
        rec_r = 99;
        sz_r = 0;
        cnt--;
        do {
            sprintf (buffer2,
              "%02d%03d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789  ", cnt, rec_r);
            i = strlen(buffer2);
            buffer1[i] = '\0';
            while (i > 0 && r == TAPE_STATUS_OK) {
                  sz_r++;
                  r = tape_read_frame(tape_ctx, &buffer1[--i]);
            }
            if (r != TAPE_STATUS_EOB) {
                ASSERT_EQUAL(0, i);
                ASSERT_STR(buffer2, (char *)&buffer1[0]);
                rec_r--;
            }
        } while (r != TAPE_STATUS_EOB);
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(sz+1, sz_r);
        ASSERT_EQUAL(0, tape_parity_error(tape_ctx));
    } while (!tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(0, cnt);
    tape_detach(tape_ctx);
}

/* Test Writing Erase gap at begining of tape */
CTEST2(tape_test, write_tap_erg) {

    ASSERT_EQUAL(1, tape_attach(tape_ctx, "tape2.tap", TYPE_TAP, 1, 1));
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(1, tape_ring(tape_ctx));
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_erase_gap(tape_ctx));
    ASSERT_EQUAL(0, tape_at_loadpt(tape_ctx));
    tape_detach(tape_ctx);
}

/* Write a long record and erase first record re-read it */
CTEST2(tape_test, write_tap_long_erg) {
    uint8_t   buffer1[128];
    char      buffer2[128];
    int  rec;
    int  rec_r;
    int  r;
    int  i;
    int  l;
    int  sz_r;
    int  sz;
    int  cnt;
    int  irg;

    log_tape("Test write ERG\n");
    ASSERT_EQUAL(1, tape_attach(tape_ctx, "tape2.tap", TYPE_TAP, 1, 1));
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(1, tape_ring(tape_ctx));
    /* Write a long enough tape to run over a couple buffers */
    for (cnt = 0; cnt < 10; cnt++) {
        r = tape_write_start(tape_ctx);
        if (r != TAPE_STATUS_OK)
           break;
        rec = 0;
        sz = 0;
        for (rec = 0; rec < 100; rec++) {
            sprintf (buffer2,
              "%02d%03d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789  ", cnt, rec);
            l = strlen(buffer2);
            for(i = 0; i < l; i++) {
               if (tape_write_frame(tape_ctx, buffer2[i]) != TAPE_STATUS_OK)
                   break;
               sz++;
            }
        }
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
    }

    ASSERT_EQUAL(TAPE_STATUS_OK, tape_write_mark(tape_ctx));
    /* Rewind tape to start */
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_start_rewind(tape_ctx));
    while (!tape_at_loadpt(tape_ctx)) {
        tape_rewind_frames(tape_ctx, 10000);
    }
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_erase_gap(tape_ctx));
    /* Rewind tape to start */
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_start_rewind(tape_ctx));
    while (!tape_at_loadpt(tape_ctx)) {
        tape_rewind_frames(tape_ctx, 10000);
    }
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    /* Attempt to read it in */
    for (cnt = 0; cnt < 20; cnt++) {
        irg = 0;
        r = tape_read_forw(tape_ctx);
        if (r == TAPE_STATUS_IRG) {
           irg = 1;
        } else if (r != TAPE_STATUS_OK) {
           break;
        }
        rec_r = 0;
        sz_r = 0;
        do {
            for (i = 0; i < 80; i++) {
                /* Skip over IRG */
                r = tape_read_frame(tape_ctx, &buffer1[i]);
                if (r == TAPE_STATUS_IRG) {
                   irg = 1;
                   i--;
                   continue;
                }
                if (r == TAPE_STATUS_EOB) {
                    break;
                }
                ASSERT_EQUAL(r, TAPE_STATUS_OK);
                sz_r++;
            }
            if (r == TAPE_STATUS_EOB) {
                break;
            }
            buffer1[i] = '\0';
            if (!irg) {
                if (cnt == 0) {
                    cnt = ((buffer1[0] - '0') * 10) + (buffer1[1] - '0');
                }
                sprintf (buffer2,
                  "%02d%03d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789  ", cnt, rec_r);
                ASSERT_STR(buffer2, (char *)&buffer1[0]);
                rec_r++;
            }
        } while (r == TAPE_STATUS_OK);
        ASSERT_EQUAL(TAPE_STATUS_EOB, r);
        if (irg) {
            ASSERT_EQUAL(1, tape_parity_error(tape_ctx));
            ASSERT_EQUAL(TAPE_STATUS_PARITY, tape_finish_rec(tape_ctx));
        } else {
            ASSERT_EQUAL(sz, sz_r);
            ASSERT_EQUAL(rec, rec_r);
            ASSERT_EQUAL(0, tape_parity_error(tape_ctx));
            ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        }
    }
    ASSERT_EQUAL(TAPE_STATUS_MARK, r);
    ASSERT_EQUAL(10, cnt);

    /* Try and read tape backwards */
    /* Skip over the tape mark we just read */
    ASSERT_EQUAL(TAPE_STATUS_MARK, tape_read_back(tape_ctx));
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
    rec_r = rec;
    sz_r = 0;
    do {
        r = tape_read_back(tape_ctx);
        if (r == TAPE_STATUS_IRG) {
           irg = 1;
        } else if (r != TAPE_STATUS_OK) {
           break;
        }
        rec_r = 99;
        sz_r = 0;
        cnt--;
        do {
            sprintf (buffer2,
                  "%02d%03d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789  ", cnt, rec_r);
            i = strlen(buffer2) + 1;
            buffer1[--i] = '\0';
            while (i > 0) {
                /* Skip over IRG */
                r = tape_read_frame(tape_ctx, &buffer1[i-1]);
                if (r == TAPE_STATUS_IRG) {
                   irg = 1;
                   continue;
                }
                if (r == TAPE_STATUS_EOB) {
                    break;
                }
                i--;
                sz_r++;
                ASSERT_EQUAL(r, TAPE_STATUS_OK);
            }
            if (r == TAPE_STATUS_OK && !irg) {
                ASSERT_EQUAL(0, i);
                ASSERT_STR(buffer2, (char *)&buffer1[i]);
                rec_r--;
            }
        } while (r != TAPE_STATUS_EOB);
        r = tape_finish_rec(tape_ctx);
        if (r == TAPE_STATUS_OK) {
           ASSERT_EQUAL(sz, sz_r);
        }
    } while (r == TAPE_STATUS_OK && !tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(TAPE_STATUS_PARITY, r);
    ASSERT_EQUAL(1, tape_parity_error(tape_ctx));
    ASSERT_EQUAL(0, cnt);
    tape_detach(tape_ctx);
}

/* Test writing erase gap over start of tape */
CTEST2(tape_test, write_tap_short_erg) {
    uint8_t   buffer1[128];
    char      buffer2[128];
    int  rec;
    int  rec_r;
    int  r;
    int  i;
    int  l;
    int  irg;
    int  first;

    log_tape("Test write ERG\n");
    ASSERT_EQUAL(1, tape_attach(tape_ctx, "tape2.tap", TYPE_TAP, 1, 1));
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(1, tape_ring(tape_ctx));
    /* Every 10 records write a tape mark, then put 2 on end */
    rec = 0;
    do {
        r = tape_write_start(tape_ctx);
        if (r != TAPE_STATUS_OK)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789  ", rec);
        l = strlen(buffer2);
        for(i = 0; i < l; i++) {
           if (tape_write_frame(tape_ctx, buffer2[i]) != TAPE_STATUS_OK)
               break;
        }
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        rec++;
        if ((rec % 10) == 0) {
            ASSERT_EQUAL(TAPE_STATUS_OK, tape_write_mark(tape_ctx));
        }
    } while (rec < 1000);
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_write_mark(tape_ctx));
    /* Rewind tape to start */
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_start_rewind(tape_ctx));
    while (!tape_at_loadpt(tape_ctx)) {
        tape_rewind_frames(tape_ctx, 10000);
    }
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_erase_gap(tape_ctx));
    /* Rewind tape to start */
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_start_rewind(tape_ctx));
    while (!tape_at_loadpt(tape_ctx)) {
        tape_rewind_frames(tape_ctx, 10000);
    }
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    /* Attempt to read it in */
    rec_r = 0;
    do {
        irg = 0;
        r = tape_read_forw(tape_ctx);
        /* Check for tape mark */
        if (r == TAPE_STATUS_MARK) {
            ASSERT_EQUAL(0, (rec_r) % 10);
            ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
            r = tape_read_forw(tape_ctx);
        } else if (r == TAPE_STATUS_IRG) {
            irg = 1;
        } else if (r != TAPE_STATUS_OK)
           break;
        for (i = 0; i < 120; i++) {
            /* Skip over IRG */
            r = tape_read_frame(tape_ctx, &buffer1[i]);
            if (r == TAPE_STATUS_IRG) {
                irg = 1;
                i--;
                continue;
            }
            if (r == TAPE_STATUS_EOB || r == TAPE_STATUS_MARK) {
                break;
            }
            ASSERT_EQUAL(TAPE_STATUS_OK, r);
        }
        buffer1[i] = '\0';
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        if (r == TAPE_STATUS_EOB && !irg) {
            if (rec_r == 0) {
               rec_r = atoi((char *)buffer1);
               first = rec_r;
            }
           sprintf (buffer2,
              "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789  ", rec_r);
            ASSERT_EQUAL(strlen(buffer2), i);
            ASSERT_STR(buffer2, (char *)&buffer1[0]);
            rec_r++;
        }
    } while (r != TAPE_STATUS_MARK || r == TAPE_STATUS_EOB);
    ASSERT_EQUAL(TAPE_STATUS_MARK, r);
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
    ASSERT_EQUAL(TAPE_STATUS_EOT, tape_read_forw(tape_ctx));
    ASSERT_EQUAL(rec, rec_r);
    /* Try and read tape backwards */
    r = tape_read_back(tape_ctx);
    r = tape_read_frame(tape_ctx, &buffer1[0]);
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
    rec_r = rec;
    do {
        r = tape_read_back(tape_ctx);
        /* Check for tape mark */
        if (r == TAPE_STATUS_MARK) {
            ASSERT_EQUAL(0, (rec_r) % 10);
            ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
            r = tape_read_back(tape_ctx);
        }
        if (r != TAPE_STATUS_OK)
           break;
        rec_r--;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789  ", rec_r);
        i = 120;
        buffer1[--i] = '\0';
        while (i > 0) {
            /* Skip over IRG */
            while ((r = tape_read_frame(tape_ctx, &buffer1[i-1])) == TAPE_STATUS_IRG);
            if (r == TAPE_STATUS_EOB) {
                break;
            }
            i--;
            ASSERT_EQUAL(r, TAPE_STATUS_OK);
        }
        if (tape_finish_rec(tape_ctx) == TAPE_STATUS_OK) {
            ASSERT_STR(buffer2, (char *)&buffer1[i]);
        }
    } while (r == TAPE_STATUS_EOB && !tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(first, rec_r + 1);
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    tape_detach(tape_ctx);
}

/* Test writing erase gap between two */
CTEST2(tape_test, write_tap_write_gap) {
    uint8_t   buffer1[128];
    char      buffer2[128];
    int  rec;
    int  rec_r;
    int  r;
    int  i;
    int  l;
    int  sz_r;
    int  sz;

    log_tape("Test write GAP\n");
    ASSERT_EQUAL(1, tape_attach(tape_ctx, "tape2.tap", TYPE_TAP, 1, 1));
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(1, tape_ring(tape_ctx));
    /* Every 10 records write a tape mark, then put 2 on end */
    rec = 0;
    sz = 0;
    do {
        r = tape_write_start(tape_ctx);
        if (r != TAPE_STATUS_OK)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec);
        l = strlen(buffer2);
        for(i = 0; i < l; i++) {
           if (tape_write_frame(tape_ctx, buffer2[i]) != TAPE_STATUS_OK)
               break;
           sz++;
        }
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_erase_gap(tape_ctx));
        rec++;
    } while (rec < 20);
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_write_mark(tape_ctx));
    /* Rewind tape to start */
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_start_rewind(tape_ctx));
    while (!tape_at_loadpt(tape_ctx)) {
        tape_rewind_frames(tape_ctx, 10000);
    }
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    /* Attempt to read it in */
    rec_r = 0;
    do {
        r = tape_read_forw(tape_ctx);
        if (r != TAPE_STATUS_OK && r != TAPE_STATUS_IRG)
           break;
        i = 0;
        for (i = 0; i < 120; i++) {
            /* Skip over IRG */
            while ((r = tape_read_frame(tape_ctx, &buffer1[i])) == TAPE_STATUS_IRG);
            if (r == TAPE_STATUS_EOB || r == TAPE_STATUS_MARK) {
                break;
            }
            ASSERT_EQUAL(r, TAPE_STATUS_OK);
        }
        if (r != TAPE_STATUS_EOB) {
            break;
        }
        buffer1[i] = '\0';
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec_r);
        r = tape_finish_rec(tape_ctx);
        if (r == TAPE_STATUS_OK) {
            ASSERT_EQUAL(TAPE_STATUS_OK, r);
            ASSERT_EQUAL(strlen(buffer2), i);
            ASSERT_STR(buffer2, (char *)&buffer1[0]);
        }
        rec_r++;
    } while (r == TAPE_STATUS_OK);
    ASSERT_EQUAL(TAPE_STATUS_MARK, r);
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
//    ASSERT_EQUAL(TAPE_STATUS_EOT, tape_read_forw(tape_ctx));
    ASSERT_EQUAL(rec, rec_r);
    /* Try and read tape backwards */
    r = tape_read_back(tape_ctx);
    r = tape_read_frame(tape_ctx, &buffer1[0]);
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
    rec_r = rec;
    sz_r = 0;
    do {
        r = tape_read_back(tape_ctx);
        /* Check for tape mark */
        if (r == TAPE_STATUS_MARK) {
            ASSERT_EQUAL(0, (rec_r) % 10);
            ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
            r = tape_read_back(tape_ctx);
        }
        if (r != TAPE_STATUS_OK)
           break;
        rec_r--;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec_r);
        i = 120;
        buffer1[--i] = '\0';
        while (i > 0) {
            /* Skip over IRG */
            r = tape_read_frame(tape_ctx, &buffer1[i-1]);
            if (r == TAPE_STATUS_IRG) {
               continue;
            }
            if (r == TAPE_STATUS_EOB) {
                break;
            }
            i--;
            sz_r++;
            ASSERT_EQUAL(r, TAPE_STATUS_OK);
        }
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        ASSERT_STR(buffer2, (char *)&buffer1[i]);
    } while (r == TAPE_STATUS_EOB && !tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(0, rec_r);
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    tape_detach(tape_ctx);
}

/* Test Writing Erase gap at begining of tape */
CTEST2(tape_test, write_p7b_erg) {

    ASSERT_EQUAL(1, tape_attach(tape_ctx, "tape2.p7b", TYPE_P7B, 1, 1));
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(1, tape_ring(tape_ctx));
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_erase_gap(tape_ctx));
    ASSERT_EQUAL(0, tape_at_loadpt(tape_ctx));
    tape_detach(tape_ctx);
}

/* Write a long record and erase first record re-read it */
CTEST2(tape_test, write_p7b_long_erg) {
    uint8_t   buffer1[128];
    char      buffer2[128];
    int  rec;
    int  rec_r;
    int  r;
    int  i;
    int  l;
    int  sz_r;
    int  sz;
    int  cnt;
    int  irg;

    log_tape("Test write ERG\n");
    ASSERT_EQUAL(1, tape_attach(tape_ctx, "tape2.p7b", TYPE_P7B, 1, 1));
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(1, tape_ring(tape_ctx));
    /* Write a long enough tape to run over a couple buffers */
    for (cnt = 0; cnt < 10; cnt++) {
        r = tape_write_start(tape_ctx);
        if (r != TAPE_STATUS_OK)
           break;
        rec = 0;
        sz = 0;
        for (rec = 0; rec < 100; rec++) {
            sprintf (buffer2,
              "%02d%03d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789  ", cnt, rec);
            l = convert_ascii_to_bcd(buffer2);
            for(i = 0; i < l; i++) {
               if (tape_write_frame(tape_ctx, buffer2[i]) != TAPE_STATUS_OK)
                   break;
               sz++;
            }
        }
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
    }

    ASSERT_EQUAL(TAPE_STATUS_OK, tape_write_mark(tape_ctx));
    /* Rewind tape to start */
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_start_rewind(tape_ctx));
    while (!tape_at_loadpt(tape_ctx)) {
        tape_rewind_frames(tape_ctx, 10000);
    }
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_erase_gap(tape_ctx));
    /* Rewind tape to start */
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_start_rewind(tape_ctx));
    while (!tape_at_loadpt(tape_ctx)) {
        tape_rewind_frames(tape_ctx, 10000);
    }
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    /* Attempt to read it in */
    for (cnt = 0; cnt < 20; cnt++) {
        irg = 0;
        r = tape_read_forw(tape_ctx);
        if (r == TAPE_STATUS_IRG) {
           irg = 1;
        } else if (r != TAPE_STATUS_OK) {
           break;
        }
        rec_r = 0;
        sz_r = 0;
        do {
            for (i = 0; i < 80; i++) {
                /* Skip over IRG */
                r = tape_read_frame(tape_ctx, &buffer1[i]);
                if (r == TAPE_STATUS_IRG) {
                   irg = 1;
                   i--;
                   continue;
                }
                if (r == TAPE_STATUS_EOB) {
                    break;
                }
                ASSERT_EQUAL(r, TAPE_STATUS_OK);
                sz_r++;
            }
            if (r == TAPE_STATUS_EOB) {
                break;
            }
            convert_bcd_to_ascii(buffer1, i);
            if (!irg) {
                if (cnt == 0) {
                    cnt = ((buffer1[0] - '0') * 10) + (buffer1[1] - '0');
                }
                sprintf (buffer2,
                  "%02d%03d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789  ", cnt, rec_r);
                ASSERT_STR(buffer2, (char *)&buffer1[0]);
                rec_r++;
            }
        } while (r == TAPE_STATUS_OK);
        ASSERT_EQUAL(TAPE_STATUS_EOB, r);
        if (irg) {
            ASSERT_EQUAL(1, tape_parity_error(tape_ctx));
            ASSERT_EQUAL(TAPE_STATUS_PARITY, tape_finish_rec(tape_ctx));
        } else {
            ASSERT_EQUAL(sz, sz_r);
            ASSERT_EQUAL(rec, rec_r);
            ASSERT_EQUAL(0, tape_parity_error(tape_ctx));
            ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        }
    }
    ASSERT_EQUAL(TAPE_STATUS_MARK, r);
    ASSERT_EQUAL(10, cnt);

    /* Try and read tape backwards */
    /* Skip over the tape mark we just read */
    ASSERT_EQUAL(TAPE_STATUS_MARK, tape_read_back(tape_ctx));
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
    rec_r = rec;
    sz_r = 0;
    do {
        r = tape_read_back(tape_ctx);
        if (r == TAPE_STATUS_IRG) {
           irg = 1;
        } else if (r != TAPE_STATUS_OK) {
           break;
        }
        rec_r = 99;
        sz_r = 0;
        cnt--;
        do {
            sprintf (buffer2,
              "%02d%03d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789  ", cnt, rec_r);
            i = strlen(buffer2) + 1;
            buffer1[--i] = '\0';
            while (i > 0) {
                /* Skip over IRG */
                r = tape_read_frame(tape_ctx, &buffer1[i-1]);
                if (r == TAPE_STATUS_IRG) {
                   irg = 1;
                   continue;
                }
                if (r == TAPE_STATUS_EOB) {
                    break;
                }
                i--;
                sz_r++;
                ASSERT_EQUAL(r, TAPE_STATUS_OK);
            }
            convert_bcd_to_ascii(buffer1, strlen(buffer2));
            if (r == TAPE_STATUS_OK && !irg) {
                ASSERT_EQUAL(0, i);
                ASSERT_STR(buffer2, (char *)&buffer1[i]);
                rec_r--;
            }
        } while (r != TAPE_STATUS_EOB);
        r = tape_finish_rec(tape_ctx);
        if (r == TAPE_STATUS_OK) {
           ASSERT_EQUAL(sz, sz_r);
        }
    } while (r == TAPE_STATUS_OK && !tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(TAPE_STATUS_PARITY, r);
    ASSERT_EQUAL(1, tape_parity_error(tape_ctx));
    ASSERT_EQUAL(0, cnt);
    tape_detach(tape_ctx);
}

/* Test writing erase gap over start of tape */
CTEST2(tape_test, write_p7b_short_erg) {
    uint8_t   buffer1[128];
    char      buffer2[128];
    int  rec;
    int  rec_r;
    int  r;
    int  i;
    int  l;
    int  irg;
    int  first;

    log_tape("Test write ERG\n");
    ASSERT_EQUAL(1, tape_attach(tape_ctx, "tape2.p7b", TYPE_P7B, 1, 1));
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(1, tape_ring(tape_ctx));
    /* Every 10 records write a tape mark, then put 2 on end */
    rec = 0;
    do {
        r = tape_write_start(tape_ctx);
        if (r != TAPE_STATUS_OK)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789  ", rec);
        l = convert_ascii_to_bcd(buffer2);
        for(i = 0; i < l; i++) {
           if (tape_write_frame(tape_ctx, buffer2[i]) != TAPE_STATUS_OK)
               break;
        }
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        rec++;
        if ((rec % 10) == 0) {
            ASSERT_EQUAL(TAPE_STATUS_OK, tape_write_mark(tape_ctx));
        }
    } while (rec < 1000);
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_write_mark(tape_ctx));
    /* Rewind tape to start */
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_start_rewind(tape_ctx));
    while (!tape_at_loadpt(tape_ctx)) {
        tape_rewind_frames(tape_ctx, 10000);
    }
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_erase_gap(tape_ctx));
    /* Rewind tape to start */
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_start_rewind(tape_ctx));
    while (!tape_at_loadpt(tape_ctx)) {
        tape_rewind_frames(tape_ctx, 10000);
    }
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    /* Attempt to read it in */
    rec_r = 0;
    do {
        irg = 0;
        r = tape_read_forw(tape_ctx);
        /* Check for tape mark */
        if (r == TAPE_STATUS_MARK) {
            ASSERT_EQUAL(0, (rec_r) % 10);
            ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
            r = tape_read_forw(tape_ctx);
        } else if (r == TAPE_STATUS_IRG) {
            irg = 1;
        } else if (r != TAPE_STATUS_OK)
           break;
        for (i = 0; i < 120; i++) {
            /* Skip over IRG */
            r = tape_read_frame(tape_ctx, &buffer1[i]);
            if (r == TAPE_STATUS_IRG) {
                irg = 1;
                i--;
                continue;
            }
            if (r == TAPE_STATUS_EOB || r == TAPE_STATUS_MARK) {
                break;
            }
            ASSERT_EQUAL(TAPE_STATUS_OK, r);
        }
        convert_bcd_to_ascii(buffer1, i);
        if (r == TAPE_STATUS_EOB && !irg) {
            if (rec_r == 0) {
               rec_r = atoi((char *)buffer1);
               first = rec_r;
            }
            sprintf (buffer2,
               "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789  ", rec_r);
            ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
            ASSERT_EQUAL(strlen(buffer2), i);
            ASSERT_STR(buffer2, (char *)&buffer1[0]);
            rec_r++;
        } else {
            tape_finish_rec(tape_ctx);
        }
    } while (r != TAPE_STATUS_MARK || r == TAPE_STATUS_EOB);
    ASSERT_EQUAL(TAPE_STATUS_MARK, r);
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
    ASSERT_EQUAL(TAPE_STATUS_EOT, tape_read_forw(tape_ctx));
    ASSERT_EQUAL(rec, rec_r);
    /* Try and read tape backwards */
    r = tape_read_back(tape_ctx);
    r = tape_read_frame(tape_ctx, &buffer1[0]);
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
    rec_r = rec;
    do {
        irg = 0;
        r = tape_read_back(tape_ctx);
        /* Check for tape mark */
        if (r == TAPE_STATUS_MARK) {
            ASSERT_EQUAL(0, (rec_r) % 10);
            ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
            r = tape_read_back(tape_ctx);
        }
        if (r != TAPE_STATUS_OK)
           break;
        i = 120;
        buffer1[--i] = '\0';
        while (i > 0) {
            /* Skip over IRG */
            while ((r = tape_read_frame(tape_ctx, &buffer1[i-1])) == TAPE_STATUS_IRG) {
                 irg = 1;
            }
            if (r == TAPE_STATUS_EOB) {
                break;
            }
            i--;
            ASSERT_EQUAL(r, TAPE_STATUS_OK);
        }
        convert_bcd_to_ascii(&buffer1[i], strlen(buffer2));
        if (tape_finish_rec(tape_ctx) == TAPE_STATUS_OK && !irg) {
            rec_r--;
            sprintf (buffer2,
               "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789  ", rec_r);
            ASSERT_STR(buffer2, (char *)&buffer1[i]);
        }
    } while (r == TAPE_STATUS_EOB && !tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(first, rec_r);
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    tape_detach(tape_ctx);
}

/* Test writing erase gap between two */
CTEST2(tape_test, write_p7b_write_gap) {
    uint8_t   buffer1[128];
    char      buffer2[128];
    int  rec;
    int  rec_r;
    int  r;
    int  i;
    int  l;
    int  sz_r;
    int  sz;

    log_tape("Test write GAP\n");
    ASSERT_EQUAL(1, tape_attach(tape_ctx, "tape2.p7b", TYPE_P7B, 1, 1));
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(1, tape_ring(tape_ctx));
    /* Every 10 records write a tape mark, then put 2 on end */
    rec = 0;
    sz = 0;
    do {
        r = tape_write_start(tape_ctx);
        if (r != TAPE_STATUS_OK)
           break;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec);
        l = convert_ascii_to_bcd(buffer2);
        for(i = 0; i < l; i++) {
           if (tape_write_frame(tape_ctx, buffer2[i]) != TAPE_STATUS_OK)
               break;
           sz++;
        }
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_erase_gap(tape_ctx));
        rec++;
    } while (rec < 20);
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_write_mark(tape_ctx));
    /* Rewind tape to start */
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_start_rewind(tape_ctx));
    while (!tape_at_loadpt(tape_ctx)) {
        tape_rewind_frames(tape_ctx, 10000);
    }
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    /* Attempt to read it in */
    rec_r = 0;
    do {
        r = tape_read_forw(tape_ctx);
        if (r != TAPE_STATUS_OK && r != TAPE_STATUS_IRG)
           break;
        for (i = 0; i < 120; i++) {
            /* Skip over IRG */
            while ((r = tape_read_frame(tape_ctx, &buffer1[i])) == TAPE_STATUS_IRG);
            if (r == TAPE_STATUS_EOB || r == TAPE_STATUS_MARK) {
                break;
            }
            ASSERT_EQUAL(r, TAPE_STATUS_OK);
        }
        if (r != TAPE_STATUS_EOB) {
            break;
        }
        convert_bcd_to_ascii(buffer1, i);
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec_r);
        r = tape_finish_rec(tape_ctx);
        if (r == TAPE_STATUS_OK) {
            ASSERT_EQUAL(TAPE_STATUS_OK, r);
            ASSERT_EQUAL(strlen(buffer2), i);
            ASSERT_STR(buffer2, (char *)&buffer1[0]);
        }
        rec_r++;
    } while (r == TAPE_STATUS_OK);
    ASSERT_EQUAL(TAPE_STATUS_MARK, r);
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
//    ASSERT_EQUAL(TAPE_STATUS_EOT, tape_read_forw(tape_ctx));
    ASSERT_EQUAL(rec, rec_r);
    /* Try and read tape backwards */
    r = tape_read_back(tape_ctx);
    r = tape_read_frame(tape_ctx, &buffer1[0]);
    ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
    rec_r = rec;
    sz_r = 0;
    do {
        r = tape_read_back(tape_ctx);
        /* Check for tape mark */
        if (r == TAPE_STATUS_MARK) {
            ASSERT_EQUAL(0, (rec_r) % 10);
            ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
            r = tape_read_back(tape_ctx);
        }
        if (r != TAPE_STATUS_OK)
           break;
        rec_r--;
        sprintf (buffer2,
          "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", rec_r);
        i = 120;
        buffer1[--i] = '\0';
        while (i > 0) {
            /* Skip over IRG */
            r = tape_read_frame(tape_ctx, &buffer1[i-1]);
            if (r == TAPE_STATUS_IRG) {
               continue;
            }
            if (r == TAPE_STATUS_EOB) {
                break;
            }
            i--;
            sz_r++;
            ASSERT_EQUAL(r, TAPE_STATUS_OK);
        }
        convert_bcd_to_ascii(&buffer1[i], strlen(buffer2));
        ASSERT_EQUAL(TAPE_STATUS_OK, tape_finish_rec(tape_ctx));
        ASSERT_STR(buffer2, (char *)&buffer1[i]);
    } while (r == TAPE_STATUS_EOB && !tape_at_loadpt(tape_ctx));
    ASSERT_EQUAL(0, rec_r);
    ASSERT_EQUAL(1, tape_at_loadpt(tape_ctx));
    tape_detach(tape_ctx);
}
