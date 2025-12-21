/*
 * microsim360 - Model 1442 card reader/punch.
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

#include "device.h"
#include "card.h"

/*
 *  Commands.
 *
 *            01234567
 *  Write     FSD00001       F= feed, S = stacker, D = data mode
 *  Read      0SD00010
 *  Select    FS000011
 *  Sense     00000100
 */

struct _1442_context {
    device_state_t         state;             /* Current channel state */
    int                    addr;         /* Device address */
    int                    chan;         /* Channel address */
    int                    selected;     /* Device currently selected */
    int                    request;      /* Request channel */
    int                    addressed;    /* Device has been addressed */
    int                    disconnect;        /* Disconnect device if in operation */
    int                    stacked;      /* Device has stack status */
    int                    busy;    /* If we returned fast busy */
    int                    sense;  /* Current sense value */
    int                    cmd;          /* Current command */
    int                    cmd_done;     /* Command finished */
    int                    status;       /* Current bus status */
    int                    data;         /* Current byte to send/recieve */
    int                    data_rdy;     /* Data is valid */
    int                    data_end;     /* Data transfer over */
    int                    data_end_post;     /* Data end sent to CPU */
    int                    feed_done;    /* Feed done */
    struct card_context   *feed;         /* Context describing card feed */
    struct card_context   *stack[2];     /* Output stackers. */
    uint16_t               rdr_card[80]; /* Current card in reader */
    int                    rdr_col;      /* Current reader column */
    int                    rdr_full;     /* Card in reader */
    int                    hop_cnt;      /* Number of cards in hopper */
    int                    stk_cnt[2];   /* Number of cards in stacker */
    uint16_t               pch_card[80]; /* Current card in punch */
    int                    pch_col;      /* Current punch column */
    int                    pch_full;     /* Card in punch */
    int                    stk_sel;      /* Current stacker */
    int                    rdy_flag;     /* Reader ready. */
    int                    eof_flag;     /* End of file flag */
    int                    stop_flag;    /* Stop at end of current command */
};

extern char *type_label[6];

void model1442_feed(struct _1442_context *ctx);

void model1442_dev(struct _device *unit, uint16_t *tags, uint16_t bus_out, uint16_t *bus_in);

struct _popup * model1442_control(struct _device *unit, int hd, int wd, int u);

void model1442_draw(struct _device *unit, void *rend);

void model1442_init(struct _device *unit, void *rend);

//int model1442_create(struct _option *opt);
