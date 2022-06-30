/* Generic Card read/punch routines for simulators.
 *
 * Copyright (c) 2021, Richard Cornwell
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

   Input/output formats are accepted in a variaty of formats:
        Standard ASCII: one record per line.
                returns are ignored.
                tabs are expanded to modules 8 characters.
                ~ in first column is treated as a EOF.

        Binary Card format:
                Each record 160 characters.
                First characters 6789----
                Second character 21012345
                                 111
                Top 4 bits of second character are 0.
                It is unlikely that any other format could
                look like this.

         Standard EBCDIC: one 80 character record per card.
                Cards must be fixed length.


    ASCII mode recognizes some additional forms of input which allows the
    intermixing of binary cards with text cards.

    Lines beginning with ~raw are taken as a number of 4 digit octal values
    with represent each column of the card from 12 row down to 9 row. If there
    is not enough octal numbers to span a full card the remainder of the
    card will not be punched.

    Also ~eor, will generate a 7/8/9 punch card. An ~eof will gernerate a
    6/7/9 punch card, and a ~eoi will generate a 6/7/8/9 punch.

    A single line of ~ will set the EOF flag when that card is read.

    For autodetection of card format, there can be no parity errors.
    All undeterminate formats are treated as ASCII.

    Auto output format is ASCII if card has only printable characters
    or card format binary.


*/

#include <stdio.h>
#include <stdint.h>

#ifndef _CARD_H_
#define _CARD_H_

#define MODE_AUTO       (0)
#define MODE_TEXT       (1)
#define MODE_EBCDIC     (2)
#define MODE_BIN        (3)
#define MODE_OCTAL      (4)

/* Card Reader Return Status code */
#define CDSE_OK     0   /* Good */
#define CDSE_EOF    1   /* End of File */
#define CDSE_EMPTY  2   /* Input Hopper Empty */
#define CDSE_ERROR  3   /* Error Card Read */

#define DECK_SIZE         1000     /* Number of cards to allocate at a time */

struct card_context
{
    char           *file_name;        /* Pointer to input/output file */
    FILE           *file;             /* Pointer to file to read/write cards from */
    int             mode;             /* Current input/output mode */
    int             hopper_size;      /* Size of buffer */
    int             hopper_cards;     /* Number of cards in hopper */
    int             hopper_pos;       /* Position in hopper */
    uint16_t        (*images)[1][80]; /* Card images */
};

/* Convert EBCDIC character into hollerith code */
uint16_t ebcdic_to_hol(uint8_t ebcdic);

/* Returns the EBCDIC of the hollerith code or 0x7f if error */
uint16_t hol_to_ebcdic(uint16_t hol);

/* Returns the ASCII of the hollerith code or 0xff if error */
uint8_t hol_to_ascii(uint16_t hol);

/* Returns the hollerith code of the ASCII value */
uint16_t ascii_to_hol(uint8_t ascii);

/* Return number of cards in hopper */
int hopper_size(struct card_context *card_ctx);

/* Reutn number of cards in stacker */
int stack_size(struct card_context *card_ctx);

/* Read the next card from the hopper */
/* Return 0 if hopper is empty, or card read error */
int read_card(struct card_context *card_ctx, uint16_t (*image)[80]);

/* Fill a hopper from a file */
int read_deck(struct card_context *card_ctx, char *file_name);

/* Add into hopper cards blank cards.  */
void empty_cards(struct card_context *card_ctx);

/* Add into hopper cards blank cards.  */
void blank_deck(struct card_context *card_ctx, int cards);

/* Put a card image on stacker. */
int stack_card(struct card_context *card_ctx, uint16_t (*image)[80]);

/* Save deck on file */
int save_deck(struct card_context *card_ctx, char *file_name);

/* Initialize a card reader context */
struct card_context *init_card_context();

#endif
