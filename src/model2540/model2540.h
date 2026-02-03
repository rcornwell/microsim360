/*
 * microsim360 - Model 2540 card reader/punch.
 *
 * Copyright 2026, Richard Cornwell
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

struct _2540_context {
    struct _2821_dev_context *rdr_ctx;          /* Pointer to reader context */
    struct _2821_dev_context *pch_ctx;          /* Pointer to punch context */
    int                       rdr_feed_done;    /* Feed done */
    int                       pch_feed_done;    /* Feed done */
    struct card_context      *rdr_feed;         /* Context describing card feed */
    struct card_context      *pch_feed;         /* Context describing card feed */
    struct card_context      *stack[5];         /* Output stackers. */
    int                       stk_cnt[5];       /* Number of cards in hopper */
    void                     *stack_input[5];   /* Input controls for stack */
    void                     *rdr_input;        /* Reader input control */
    void                     *pch_input;        /* Reader input control */
    uint16_t                  rdr_card[80];     /* Current card in reader */
    uint16_t                  pch_card[80];     /* Current card in punch */
    int                       rdr_full;         /* There is card in reader */
    int                       pch_full;         /* There is card ready to punch */
    int                       rdr_stk_sel;      /* Stacker for cards read */
    int                       pch_stk_sel;      /* Stacker for cards punched */
    int                       eof_flag;         /* End of file flag */
    int                       rdr_ready;        /* Device ready */
    int                       pch_ready;        /* Device ready */
    int                       rdr_hop_cnt;      /* Number of cards in hopper */
    int                       pch_hop_cnt;      /* Number of cards in hopper */
    int                       rdr_stop_flag;    /* Stop at end of current command */
    int                       pch_stop_flag;    /* Stop at end of current command */
};

int model2540_create(struct _option *opt);

void model2540r_start(struct _2540_context *ctx);

void model2540p_start(struct _2540_context *ctx);

void model2540p_cmd(struct _2821_dev_context *ctx, uint16_t bus_out);

void model2540r_cmd(struct _2821_dev_context *ctx, uint16_t bus_out);

void model2540p_xfer_done(struct _2821_dev_context *ctx);

void model2540r_xfer_done(struct _2821_dev_context *ctx);

void *model2540_control(struct _device *unit, int u, int x, int y);

void model2540_draw(struct _device *unit, void *rend, int u);

void model2540_init_graphics(struct _device *unit, void *rend);

struct _device *model2540_init(uint16_t addr, uint16_t addr2821, int dev_type);

