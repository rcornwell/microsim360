/*
 * microsim360 - Card emulation test cases.
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
#include "card.h"

static struct card_context *card_ctx;

uint64_t step_count = 0;

/* Create a card file with number of cards. */
static
int create_card_file (const char *filename, int cards)
{
    FILE *f;
    int i;

    f = fopen (filename, "w");
    if (f == NULL) {
        ASSERT_FAIL();
        return 0;
    }
    for (i=0; i<cards; i++)
       fprintf (f, "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\n", i);
    fclose (f);
    return 1;
}

CTEST_DATA(card_test) {
};

CTEST_SETUP(card_test) {
   ASSERT_EQUAL(1, create_card_file ("file1.deck", 10));
   ASSERT_EQUAL(1, create_card_file ("file2.deck", 20));
   ASSERT_EQUAL(1, create_card_file ("file3.deck", 30));
   ASSERT_EQUAL(1, create_card_file ("file4.deck", 40));
   card_ctx = init_card_context();
}

CTEST_TEARDOWN(card_test) {
    (void)remove("file1.deck");
    (void)remove("file2.deck");
    (void)remove("file3.deck");
    (void)remove("file4.deck");
    free(card_ctx);
}

/* Check that we can read a deck */
CTEST2(card_test, read_decks) {
    ASSERT_EQUAL(1, read_deck(card_ctx, "file1.deck"));
    ASSERT_EQUAL(10, hopper_size(card_ctx));
}

/* Check that emoty_cards will remove all cards in hopper */
CTEST2(card_test, empty_decks) {
    ASSERT_EQUAL(1, read_deck(card_ctx, "file1.deck"));
    ASSERT_EQUAL(10, hopper_size(card_ctx));
    empty_cards(card_ctx);
    ASSERT_EQUAL(0, hopper_size(card_ctx));
}

/* Check that we can stack cards into hopper */
CTEST2(card_test, stacking_decks) {
    ASSERT_EQUAL(1, read_deck(card_ctx, "file1.deck"));
    ASSERT_EQUAL(10, hopper_size(card_ctx));
    ASSERT_EQUAL(1, read_deck(card_ctx, "file2.deck"));
    ASSERT_EQUAL(30, hopper_size(card_ctx));
    ASSERT_EQUAL(1, read_deck(card_ctx, "file3.deck"));
    ASSERT_EQUAL(60, hopper_size(card_ctx));
    ASSERT_EQUAL(1, read_deck(card_ctx, "file4.deck"));
    ASSERT_EQUAL(100, hopper_size(card_ctx));
}

/* Load 10 cards into hopper, then read them */
CTEST2(card_test, reading_cards) {
    uint16_t card_image[80];
    int i;
    ASSERT_EQUAL(1, read_deck(card_ctx, "file1.deck"));
    ASSERT_EQUAL(10, hopper_size(card_ctx));
    for (i = 0; i < 10; i++) {
        ASSERT_EQUAL(1, read_card(card_ctx, &card_image));
    }
    ASSERT_EQUAL(0, hopper_size(card_ctx));
}

/* Verify that cards match hollerith values */
CTEST2(card_test, checking_translation) {
    uint16_t card_image[80];
    char     buffer[81];
    char     buffer2[81];
    int i, j;
    ASSERT_EQUAL(1, read_deck(card_ctx, "file1.deck"));
    ASSERT_EQUAL(10, hopper_size(card_ctx));
    for (i = 0; i < 10; i++) {
        sprintf(buffer, "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789  ", i);
        ASSERT_EQUAL(1, read_card(card_ctx, &card_image));
        for (j = 0; j < 80; j++) {
            buffer2[j] = hol_to_ascii(card_image[j]);
        }
        buffer2[80] = '\0';
        ASSERT_STR(buffer, buffer2);
    }
    ASSERT_EQUAL(0, hopper_size(card_ctx));
}

/* Test that blank cards creates requested number of blank cards */
CTEST2(card_test, blank_deck) {
    uint16_t card_image[80];
    char     buffer[81];
    char     buffer2[81];
    int i, j;
    blank_deck(card_ctx, 10);
    memset(buffer, ' ', 80);
    buffer[80] = '\0';
    ASSERT_EQUAL(10, hopper_size(card_ctx));
    for (i = 0; i < 10; i++) {
        ASSERT_EQUAL(1, read_card(card_ctx, &card_image));
        for (j = 0; j < 80; j++) {
            buffer2[j] = hol_to_ascii(card_image[j]);
        }
        buffer2[80] = '\0';
        ASSERT_STR(buffer, buffer2);
    }
    ASSERT_EQUAL(0, hopper_size(card_ctx));
}

/* Test punch of a blank deck */
CTEST2(card_test, punch_deck) {
    uint16_t card_image[80];
    char     buffer[81];
    int i;
    memset(buffer, ' ', 80);
    buffer[80] = '\0';
    for (i = 0; i < 10; i++) {
        ASSERT_EQUAL(0, stack_card(card_ctx, &card_image));
    }
    ASSERT_EQUAL(10, stack_size(card_ctx));
    empty_cards(card_ctx);
}

/* Test that save_deck will create correct output */
CTEST2(card_test, save_deck) {
    FILE     *f;
    char     buffer[81];
    int i;
    blank_deck(card_ctx, 10);
    ASSERT_EQUAL(10, stack_size(card_ctx));
    ASSERT_EQUAL(0, save_deck(card_ctx, "file2.deck"));
    if ((f = fopen("file2.deck", "r")) == NULL)
        ASSERT_FAIL();
    i = 0;
    while(fgets(buffer, sizeof(buffer), f) != NULL) {
         i++;
         ASSERT_STR(" \n", buffer);
    }
    fclose(f);
    ASSERT_EQUAL(10, i);
    remove("file2.deck");
}

/* Create a test deck and verify that saved file match */
CTEST2(card_test, save_deck2) {
    FILE     *f;
    uint16_t card_image[80];
    char     buffer[81];
    char     buffer2[81];
    int i, j;
    for (i = 0; i < 10; i++) {
        sprintf(buffer, "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789  ", i);
        for (j = 0; j < 80; j++) {
            card_image[j] = ascii_to_hol(buffer[j]);
        }
        stack_card(card_ctx, &card_image);
    }
    ASSERT_EQUAL(0, card_ctx->hopper_pos);
    ASSERT_EQUAL(10, stack_size(card_ctx));
    ASSERT_EQUAL(10, hopper_size(card_ctx));
    ASSERT_EQUAL(0, save_deck(card_ctx, "file3.deck"));
    ASSERT_EQUAL(0, hopper_size(card_ctx));
    if ((f = fopen("file3.deck", "r")) == NULL)
        ASSERT_FAIL();
    i = 0;
    while(fgets(buffer, sizeof(buffer), f) != NULL) {
         sprintf(buffer2, "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\n", i);
         ASSERT_STR(buffer2, buffer);
         i++;
    }
    fclose(f);
    ASSERT_EQUAL(10, i);
    remove("file3.deck");
}

static uint8_t ebcdic_string[80] = {
 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0x40, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xd1,
 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6,
 0xc7, 0xc8, 0xc9, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xe2, 0xe3, 0xe4, 0xe5,
 0xe6, 0xe7, 0xe8, 0xe9, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0x40, 0x40
};

/* Now try with ebcdic values */
CTEST2(card_test, ebcdic_test) {
    FILE     *f;
    char     buffer[80];
    char     buffer2[80];
    int i, j;

    if ((f = fopen("file1.deck", "w")) == NULL)
        ASSERT_FAIL();
    for (i = 0; i < 10; i++) {
        fprintf(f, "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\n", i);
    }
    fclose(f);
    ASSERT_EQUAL(1, read_deck(card_ctx, "file1.deck"));
    ASSERT_EQUAL(10, hopper_size(card_ctx));
    card_ctx->hopper_cards = hopper_size(card_ctx);
    card_ctx->hopper_pos = 0;
    card_ctx->mode = MODE_EBCDIC;
    ASSERT_EQUAL(0, save_deck(card_ctx, "file2.deck"));
    if ((f = fopen("file2.deck", "r")) == NULL)
        ASSERT_FAIL();
    i = 0;
    memcpy(buffer2, ebcdic_string, sizeof(buffer2));
    while(fread(buffer, 1, sizeof(buffer), f) > 0) {
         buffer2[4] = 0xf0 + i;
         for (j = 0; j < 80; j++) {
            if (buffer[j] != buffer2[j]) {
                break;
            }
         }
         ASSERT_EQUAL(80, j);
         i++;
    }
    fclose(f);
    ASSERT_EQUAL(10, i);
    ASSERT_EQUAL(0, hopper_size(card_ctx));
}

/* Try to read in a Ebcdic deck */
CTEST2(card_test, ebcdic_read) {
    FILE     *f;
    uint16_t card_image[80];
    char     buffer[81];
    char     buffer2[81];
    int i, j;

    if ((f = fopen("file1.deck", "w")) == NULL)
        ASSERT_FAIL();
    memcpy(buffer, ebcdic_string, sizeof(buffer));
    for (i = 0; i < 10; i++) {
        buffer[4] = 0xf0 + i;
        fwrite(buffer, 1, 80, f);
    }
    fclose(f);
    card_ctx->mode = MODE_EBCDIC;
    ASSERT_EQUAL(1, read_deck(card_ctx, "file1.deck"));
    for (i = 0; hopper_size(card_ctx) != 0; i++) {
        sprintf(buffer, "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789  ", i);
        ASSERT_EQUAL(1, read_card(card_ctx, &card_image));
        for (j = 0; j < 80; j++) {
            buffer2[j] = hol_to_ascii(card_image[j]);
        }
        buffer2[80] = '\0';
        ASSERT_STR(buffer, buffer2);
    }
    ASSERT_EQUAL(10, i);
}

/* Test creating a binary deck, and make sure it is output 
   correctly and that it can be re-read
*/
CTEST2(card_test, binary_deck) {
    FILE        *f;
    uint16_t    card_image[80];
    char        buffer[500];
    char        buffer2[500];
    int         i, j;

    for (j = 0; j < 10; j++) {
       strcpy(buffer2, "~raw");
       for (i = 0; i < 80; i++) {
            card_image[i] = i;
            sprintf(buffer, "%04o", i);
            strcat(buffer2, buffer);
       }
       stack_card(card_ctx, &card_image);
       strcat(buffer2, "\n");
    }
    memset(card_image, 0, sizeof(card_image));
    card_image[0] = 07; /* Stack EOR card */
    stack_card(card_ctx, &card_image);
    card_image[0] = 015; /* Stack EOF card */
    stack_card(card_ctx, &card_image);
    card_image[0] = 017; /* Stack EOI card */
    stack_card(card_ctx, &card_image);
    /* Now stack some ascii text... should auto handle it */
    for (i = 13; i < 23; i++) {
        sprintf(buffer, "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789  ", i);
        for (j = 0; j < 80; j++) {
            card_image[j] = ascii_to_hol(buffer[j]);
        }
        stack_card(card_ctx, &card_image);
    }
    ASSERT_EQUAL(0, save_deck(card_ctx, "file5.deck"));
    if ((f = fopen("file5.deck", "r")) == NULL)
        ASSERT_FAIL();
    i = 0;
    while(fgets(buffer, sizeof(buffer), f) != NULL) {
         if (i < 10) { 
             ASSERT_STR(buffer2, buffer);
         } else if (i == 10) {
             ASSERT_STR("~eor\n", buffer);
         } else if (i == 11) {
             ASSERT_STR("~eof\n", buffer);
         } else if (i == 12) {
             ASSERT_STR("~eoi\n", buffer);
         } else {
             sprintf(buffer2, "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\n", i);
             ASSERT_STR(buffer2, buffer);
         }
         i++;
    }
    fclose(f);
    ASSERT_EQUAL(23, i);
    ASSERT_EQUAL(0, hopper_size(card_ctx));
    /* Now read the deck in and compare */
    ASSERT_EQUAL(1, read_deck(card_ctx, "file5.deck"));
    for (i = 0; hopper_size(card_ctx) != 0; i++) {
        ASSERT_EQUAL(1, read_card(card_ctx, &card_image));
        if (i < 10) {
            for (j = 0; j < 80; j++) {
                ASSERT_EQUAL( j, card_image[j]);
            }
        } else if (i == 10) {
            ASSERT_EQUAL(07, card_image[0]);
        } else if (i == 11) {
            ASSERT_EQUAL(015, card_image[0]);
        } else if (i == 12) {
            ASSERT_EQUAL(017, card_image[0]);
        } else {
            sprintf(buffer2, "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789  ", i);
            for (j = 0; j < 80; j++) {
                buffer[j] = hol_to_ascii(card_image[j]);
            }
            buffer[80] = '\0';
            ASSERT_STR(buffer2, buffer);
        }
        if (i >= 10 && i <= 12) {
            for (j = 1; j < 80; j++) {
                ASSERT_EQUAL( 0, card_image[j]);
            }
        }
    }
    ASSERT_EQUAL(23, i);
}

/* Test creating a binary deck, and make sure it is output 
   correctly and that it can be re-read
*/
CTEST2(card_test, binary2_deck) {
    FILE        *f;
    uint16_t    card_image[80];
    char        buffer[500];
    char        buffer2[500];
    int         i, j;

    for (j = 0; j < 10; j++) {
       for (i = 0; i < 80; i++) {
            card_image[i] = i;
       }
       stack_card(card_ctx, &card_image);
    }
    memset(card_image, 0, sizeof(card_image));
    card_image[0] = 07; /* Stack EOR card */
    stack_card(card_ctx, &card_image);
    card_image[0] = 015; /* Stack EOF card */
    stack_card(card_ctx, &card_image);
    card_image[0] = 017; /* Stack EOI card */
    stack_card(card_ctx, &card_image);
    /* Now stack some ascii text... should auto handle it */
    for (i = 13; i < 23; i++) {
        sprintf(buffer, "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789  ", i);
        for (j = 0; j < 80; j++) {
            card_image[j] = ascii_to_hol(buffer[j]);
        }
        stack_card(card_ctx, &card_image);
    }
    card_ctx->mode = MODE_BIN;
    ASSERT_EQUAL(0, save_deck(card_ctx, "file1.deck"));
    if ((f = fopen("file1.deck", "r")) == NULL)
        ASSERT_FAIL();
    i = 0;
    while(fread(buffer, 1, sizeof(card_image), f) > 0) {
         if (i < 10) {
            for (j = 0; j< 80; j++) {
                ASSERT_EQUAL((j & 0xf) << 4, buffer[j*2] & 0xff);
                ASSERT_EQUAL((j >> 4) & 0xff, buffer[(j*2) + 1] & 0xff);
             }
         } else if (i == 10) {
            ASSERT_EQUAL(07 << 4, buffer[0] & 0xff);
            ASSERT_EQUAL(00, buffer[1]);
            for (j = 1; j< 80; j++) {
                ASSERT_EQUAL(0, buffer[j*2] & 0xff);
                ASSERT_EQUAL(0, buffer[(j*2) + 1] & 0xff);
            }
         } else if (i == 11) {
            ASSERT_EQUAL(015 << 4, buffer[0] & 0xff);
            ASSERT_EQUAL(00, buffer[1]);
            for (j = 1; j< 80; j++) {
                ASSERT_EQUAL(0, buffer[j*2] & 0xff);
                ASSERT_EQUAL(0, buffer[(j*2) + 1] & 0xff);
            }
         } else if (i == 12) {
            ASSERT_EQUAL(017 << 4, buffer[0] & 0xff);
            ASSERT_EQUAL(00, buffer[1]);
            for (j = 1; j< 80; j++) {
                ASSERT_EQUAL(0, buffer[j*2] & 0xff);
                ASSERT_EQUAL(0, buffer[(j*2) + 1] & 0xff);
            }
         }
         i++;
    }
    fclose(f);
    ASSERT_EQUAL(23, i);
    ASSERT_EQUAL(0, hopper_size(card_ctx));
    /* Now read the deck in and compare */
    ASSERT_EQUAL(1, read_deck(card_ctx, "file1.deck"));
    for (i = 0; hopper_size(card_ctx) != 0; i++) {
        ASSERT_EQUAL(1, read_card(card_ctx, &card_image));
        if (i < 10) {
            for (j = 0; j < 80; j++) {
                ASSERT_EQUAL( j, card_image[j]);
            }
        } else if (i == 10) {
            ASSERT_EQUAL(07, card_image[0]);
        } else if (i == 11) {
            ASSERT_EQUAL(015, card_image[0]);
        } else if (i == 12) {
            ASSERT_EQUAL(017, card_image[0]);
        } else {
            sprintf(buffer2, "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789  ", i);
            for (j = 0; j < 80; j++) {
                buffer[j] = hol_to_ascii(card_image[j]);
            }
            buffer[80] = '\0';
            ASSERT_STR(buffer2, buffer);
        }
        if (i >= 10 && i <= 12) {
            for (j = 1; j < 80; j++) {
                ASSERT_EQUAL( 0, card_image[j]);
            }
        }
    }
    ASSERT_EQUAL(23, i);
}
     
/* Test autodecktion of binary deck.
   correctly and that it can be re-read
*/
CTEST2(card_test, auto_deck) {
    uint16_t    card_image[80];
    char        buffer[500];
    char        buffer2[500];
    int         i, j;

    for (j = 0; j < 10; j++) {
       for (i = 0; i < 80; i++) {
            card_image[i] = i;
       }
       stack_card(card_ctx, &card_image);
    }
    memset(card_image, 0, sizeof(card_image));
    card_image[0] = 07; /* Stack EOR card */
    stack_card(card_ctx, &card_image);
    card_image[0] = 015; /* Stack EOF card */
    stack_card(card_ctx, &card_image);
    card_image[0] = 017; /* Stack EOI card */
    stack_card(card_ctx, &card_image);
    /* Now stack some ascii text... should auto handle it */
    for (i = 13; i < 23; i++) {
        sprintf(buffer, "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789  ", i);
        for (j = 0; j < 80; j++) {
            card_image[j] = ascii_to_hol(buffer[j]);
        }
        stack_card(card_ctx, &card_image);
    }
    card_ctx->mode = MODE_BIN;
    ASSERT_EQUAL(0, save_deck(card_ctx, "file1.deck"));
    ASSERT_EQUAL(0, hopper_size(card_ctx));
    card_ctx->mode = MODE_AUTO;
    /* Now read the deck in and compare */
    ASSERT_EQUAL(1, read_deck(card_ctx, "file1.deck"));
    for (i = 0; hopper_size(card_ctx) != 0; i++) {
        ASSERT_EQUAL(1, read_card(card_ctx, &card_image));
        if (i < 10) {
            for (j = 0; j < 80; j++) {
                ASSERT_EQUAL( j, card_image[j]);
            }
        } else if (i == 10) {
            ASSERT_EQUAL(07, card_image[0]);
        } else if (i == 11) {
            ASSERT_EQUAL(015, card_image[0]);
        } else if (i == 12) {
            ASSERT_EQUAL(017, card_image[0]);
        } else {
            sprintf(buffer2, "%05d ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789  ", i);
            for (j = 0; j < 80; j++) {
                buffer[j] = hol_to_ascii(card_image[j]);
            }
            buffer[80] = '\0';
            ASSERT_STR(buffer2, buffer);
        }
        if (i >= 10 && i <= 12) {
            for (j = 1; j < 80; j++) {
                ASSERT_EQUAL( 0, card_image[j]);
            }
        }
    }
    ASSERT_EQUAL(23, i);
}
     


