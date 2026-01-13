/*
 * microsim360 - Model 1442 card reader controller test.
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
#include "card.h"
#include "xlat.h"
#include "model1442.h"

uint64_t   step_count = 0;
int        verbose = 0;
char       *test_log_file = "model1442_debug.log";
char       *test_log_level = "info warn error trace device card";

uint32_t   card_data[20];
/* Create a card file with number of cards. */
static
int create_card_file (const char *filename, int cards)
{
    FILE *f;
    char line[90];
    int i;
    int j;
    int w;

    f = fopen (filename, "w");
    if (f == NULL) {
        ASSERT_FAIL();
        return 0;
    }
    for (i=0; i<cards; i++) {
       sprintf (line, "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789  \n", i);
       fputs (line, f);
       w = 0;
       for (j=0; j < 80; j++) {
            w <<= 8;
            w |= ascii_to_ebcdic[(int)line[j]];
            if ((j & 0x3) == 3) {
                card_data[j >> 2] = w;
                w = 0;
            }
       }
    }
    fclose (f);
    return 1;
}

/* Create a card file with number of cards. */
static
int create_card_file2 (const char *filename, int cards)
{
    FILE *f;
    char line[90];
    int i;
    int j;
    int w;


    f = fopen (filename, "w");
    if (f == NULL) {
        ASSERT_FAIL();
        return 0;
    }
    for (i=0; i<cards; i++) {
       sprintf (line, "%05d                                     ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789  \n", i);
       fputs (line, f);
       w = 0;
       for (j=0; j < 80; j++) {
            w <<= 8;
            w |= ascii_to_ebcdic[(int)line[j]];
            if ((j & 0x3) == 3) {
                card_data[j >> 2] = w;
                w = 0;
            }
       }
    }
    fclose (f);
    return 1;
}

/* Create a card file with number of cards. */
static
int create_card_file3 (const char *filename, int cards)
{
    FILE *f;
    char line[90];
    uint16_t buffer[80];
    int i;
    int j;
    struct card_context *card_ctx = init_card_context();

    card_ctx->mode = MODE_BIN;
    f = fopen (filename, "w");
    if (f == NULL) {
        ASSERT_FAIL();
        return 0;
    }
    for (i=0; i<cards; i++) {
       sprintf (line, "%05d                                     ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789  ", i);
       for (j = 0; j < 80; j++) {
           buffer[j] = ascii_to_hol(line[j]);
       }
       for (j = 3; j < 12; j++) {
          buffer[j+10] = (1 << j) | 4;
       }
       stack_card(card_ctx, &buffer);
    }
    (void)save_deck(card_ctx, "file3.deck");
    (void)close_deck(card_ctx);
    return 1;
}

/* 
 * Panel display functions
 */
void
model1442_draw(struct _device *unit, void *rend, int u)
{
}

/*
 * Popup device control panel.
 */

void *
model1442_control(struct _device *unit, int u)
{
   return NULL;
}

void
model1442_init_graphics(struct _device *unit, void *rend)
{
}

int
model1442_create(struct _option *opt)
{
    return 1;
}

void
init_tests()
{
    init_event();
    (void)model1442_init(0xc);
}

void
test_advance()
{
    step_count++;
    advance();
}

CTEST_DATA(model1442_test) {
    uint16_t       addr;
    struct _device *dev;
};

CTEST_SETUP(model1442_test) {
     struct _1442_context *ctx;
     log_trace("Init test\n");
     data->dev = chan[0];
     ASSERT_NOT_NULL(data->dev);
     data->addr = data->dev->addr;
     ctx = (struct _1442_context *)(data->dev->dev);
     ASSERT_NOT_NULL(ctx);
     empty_cards(ctx->feed);
     empty_cards(ctx->stack[0]);
     empty_cards(ctx->stack[1]);
     ctx->pch_full = 0;
     ctx->rdr_full = 0;
}

CTEST_TEARDOWN(model1442_test) {
     log_trace("teardown test\n");
     (void)remove("file1.deck");
     (void)remove("file2.deck");
     (void)remove("file3.deck");
}

/* Try to send Test I/O to controller */
CTEST2(model1442_test, test_io) {
    log_trace("TIO\n");
    ASSERT_EQUAL_X(0, test_io(data->addr));
}

/* Try to send Nop to controller */
CTEST2(model1442_test, nop) {
     uint16_t status;

     log_trace("Nop\n");
     set_mem(0x40,  0xffffffff);         /* Set CSW to zero */
     set_mem(0x44,  0xffffffff);
     set_mem(0x48,  0x500);   /* Set CAW */
     set_mem(0x500, 0x03000600); /* Set channel words */
     set_mem(0x504, 0x00000001);
     set_mem(0x600, 0xffffffff);
     status = start_io(data->addr, 0x500, 0, 0);
     ASSERT_EQUAL_X(SNS_CHNEND|SNS_DEVEND, status);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000001, get_mem(0x44));
     ASSERT_EQUAL_X(0xffFFFFFF, get_mem(0x600));
}

/* Try to issue sense command */
CTEST2(model1442_test, sense) {
     struct _1442_context *ctx = (struct _1442_context *)(data->dev->dev);
     uint16_t status;
     log_trace("Sense\n");
     set_mem(0x40, 0xffffffff);   /* Set CSW to all ones */
     set_mem(0x44, 0xffffffff);
     set_mem(0x500, 0x04000700); /* Set channel words */
     set_mem(0x504, 0x00000001);
     set_mem(0x700, 0xffffffff);
     status = start_io(data->addr, 0x500, 0, 0);
     if (verbose) {
        printf("700=%08x 0x40=%08x %08x\n", get_mem(0x700),
                get_mem(0x40), get_mem(0x44));
     }

     ASSERT_EQUAL_X(SNS_DEVEND|SNS_CHNEND, status);
     ASSERT_EQUAL_X(0x00000508, get_mem(0x40));
     ASSERT_EQUAL_X(0x0c000000, get_mem(0x44));
     ASSERT_EQUAL_X(0x00ffffff, get_mem(0x700));
     ASSERT_EQUAL_X(0, hopper_size(ctx->stack[0]));
     ASSERT_EQUAL_X(0, hopper_size(ctx->stack[1]));
}

/* Try to read a card */
CTEST2(model1442_test, read) {
     struct _1442_context *ctx = (struct _1442_context *)(data->dev->dev);
     int     i;
     uint16_t status1 = 0;
     uint16_t status2 = 0;
     uint32_t word40;
     uint32_t word44;
     
     log_trace("Read\n");
     ctx->pch_full = 0;
     empty_cards(ctx->stack[0]);
     empty_cards(ctx->stack[1]);
     ASSERT_EQUAL(1, create_card_file ("file1.deck", 3));
     read_deck(ctx->feed, "file1.deck");
     model1442_feed(ctx);         /* Load first card. */
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
     ASSERT_EQUAL_X(SNS_DEVEND, status2);
     ASSERT_EQUAL_X(0x00000508, word40);
     ASSERT_EQUAL_X(0x08000000, word44);
     ASSERT_EQUAL_X(0xffffffff, get_mem(0x40));
     ASSERT_EQUAL_X(0x0400ffff, get_mem(0x44));
     card_data[1] &= 0xF0ffffff;
     for (i = 0; i < 80; i+=4) {
         ASSERT_EQUAL_X(card_data[i>>2], get_mem(0x600+i));
     }
     ASSERT_EQUAL_X(0, stack_size(ctx->stack[0]));

     /* Make sure sense is zero */
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

/* Try to read a card, make sure it got into stacker. */
CTEST2(model1442_test, read_two) {
     struct _1442_context *ctx = (struct _1442_context *)(data->dev->dev);
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
     if (verbose) {
         printf("Size %d %d, %d %d\n", hopper_size(ctx->feed),
            stack_size(ctx->feed), hopper_size(ctx->stack[0]),
            stack_size(ctx->stack[0]));
     }
     model1442_feed(ctx);         /* Load first card. */
     if (verbose) {
         printf("Size %d %d, %d %d\n", hopper_size(ctx->feed),
            stack_size(ctx->feed), hopper_size(ctx->stack[0]),
            stack_size(ctx->stack[0]));
     }
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
CTEST2(model1442_test, read_inter) {
     struct _1442_context *ctx = (struct _1442_context *)(data->dev->dev);
     int     i;
     uint16_t status1 = 0;
     uint32_t word40;
     uint32_t word44;
     
     log_trace("Read intervent\n");
     ASSERT_EQUAL(1, create_card_file ("file1.deck", 2));
     read_deck(ctx->feed, "file1.deck");
     model1442_feed(ctx);         /* Load first card. */
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
CTEST2(model1442_test, read_eof) {
     struct _1442_context *ctx = (struct _1442_context *)(data->dev->dev);
     int     i;
     uint16_t status1 = 0;
     uint16_t status2 = 0;
     uint32_t word40;
     uint32_t word44;
     
     log_trace("Read eof\n");
     ASSERT_EQUAL(1, create_card_file ("file1.deck", 2));
     read_deck(ctx->feed, "file1.deck");
     ctx->eof_flag = 1;
     model1442_feed(ctx);         /* Load first card. */
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
CTEST2(model1442_test, read_stack2) {
     struct _1442_context *ctx = (struct _1442_context *)(data->dev->dev);
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
     if (verbose) {
         printf("Size %d %d, %d %d\n", hopper_size(ctx->feed),
            stack_size(ctx->feed), hopper_size(ctx->stack[0]),
            stack_size(ctx->stack[0]));
     }
     model1442_feed(ctx);         /* Load first card. */
     if (verbose) {
         printf("Size %d %d, %d %d\n", hopper_size(ctx->feed),
            stack_size(ctx->feed), hopper_size(ctx->stack[0]),
            stack_size(ctx->stack[0]));
     }
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
CTEST2(model1442_test, read_feed) {
     struct _1442_context *ctx = (struct _1442_context *)(data->dev->dev);
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
     if (verbose) {
         printf("Size %d %d, %d %d\n", hopper_size(ctx->feed),
            stack_size(ctx->feed), hopper_size(ctx->stack[0]),
            stack_size(ctx->stack[0]));
     }
     model1442_feed(ctx);         /* Load first card. */
     if (verbose) {
         printf("Size %d %d, %d %d\n", hopper_size(ctx->feed),
            stack_size(ctx->feed), hopper_size(ctx->stack[0]),
            stack_size(ctx->stack[0]));
     }
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
CTEST2(model1442_test, punch_card) {
     struct _1442_context *ctx = (struct _1442_context *)(data->dev->dev);
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

     model1442_feed(ctx);         /* Load first card. */
     if (verbose) {
         printf("Size %d %d, %d %d\n", hopper_size(ctx->feed),
            stack_size(ctx->feed), hopper_size(ctx->stack[0]),
            stack_size(ctx->stack[0]));
     }
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
CTEST2(model1442_test, punch_card2) {
     struct _1442_context *ctx = (struct _1442_context *)(data->dev->dev);
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

     model1442_feed(ctx);         /* Load first card. */
     if (verbose) {
         printf("Size %d %d, %d %d\n", hopper_size(ctx->feed),
            stack_size(ctx->feed), hopper_size(ctx->stack[0]),
            stack_size(ctx->stack[0]));
     }
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
CTEST2(model1442_test, punch_over) {
     struct _1442_context *ctx = (struct _1442_context *)(data->dev->dev);
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
     if (verbose) {
         printf("Size %d %d, %d %d\n", hopper_size(ctx->feed),
            stack_size(ctx->feed), hopper_size(ctx->stack[0]),
            stack_size(ctx->stack[0]));
     }
     model1442_feed(ctx);         /* Load first card. */
     model1442_feed(ctx);         /* Move to punch station */
     if (verbose) {
         printf("Size %d %d, %d %d\n", hopper_size(ctx->feed),
            stack_size(ctx->feed), hopper_size(ctx->stack[0]),
            stack_size(ctx->stack[0]));
     }
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
CTEST2(model1442_test, read_invalid) {
     struct _1442_context *ctx = (struct _1442_context *)(data->dev->dev);
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
     model1442_feed(ctx);         /* Load first card. */
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
CTEST2(model1442_test, read_ten) {
     struct _1442_context *ctx = (struct _1442_context *)(data->dev->dev);
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
     model1442_feed(ctx);         /* Load first card. */
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

