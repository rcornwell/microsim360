/*
 * microsim360 - Model 2540 card reader.
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

#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include "logger.h"
#include "event.h"
#include "config.h"
#include "device.h"
#include "card.h"
#include "xlat.h"
#include "model2821/model2821.h"
#include "model2540.h"

/*
 *  Commands.
 *
 *  Reader:
 *               01234567
 *  Read Feed    00C00010       R1  Type AA
 *               01C00010       R2
 *               10C00010       RP3
 *  Read No Feed 11C00010           Type AB
 *  Read Feed    11C10010       No stacker selected. Type BA
 *  Feed         00100011       R1  Type BA
 *               01100011       R2
 *               10100011       RP3
 *  Sense        00000100
 *  Nop          00000011
 *  Read Check   11000110
 *  Diag Write   00100101
 *
 *  Punch:
 *  Write Feed   00C00001       P1  type BB
 *               01C00001       P2
 *               10C00001       RP3
 *  Read         11C00010           Type AB
 *  Sense        00000100
 *  Nop          00000011
 *  Read Check   11000110
 *
 *
 * Transfer times:
 * Reader:
 *       18ms read start.
 *       35ms card read.
 *        8ms clutch decision.
 *       25ms clutch wait time.
 *       30s  motor stop.
 *      500ms motor start.
 *
 * Punch:
 *       35ms punch start.
 *      151ms punching.
 *       14ms clutch decision.
 *       50ms clutch wait time.
 *       30s  motor stop.
 *      500ms motor start.
 */

#define SENSE_CMDREJ    BIT0  /* Invalid command */
#define SENSE_INTERV    BIT1  /* Operator intervention, reader empty */
#define SENSE_BUSCHK    BIT2  /* Bus parity error */
#define SENSE_EQUCHK    BIT3  /* Equipment check, not implemented */
#define SENSE_DATCHK    BIT4  /* More then 1 punch in rows 1-7 */
#define SENSE_OVRRUN    BIT5  /* Data missed */

static void
feed_callback(struct _device *unit, void *arg, int iarg)
{
    struct _2821_dev_context *ctx = (struct _2821_dev_context *)unit->dev;
    struct _2540_context *dev = (struct _2540_context *)ctx->ctx;
    ctx->status |= SNS_DEVEND;
    ctx->busy = 0;
    ctx->cmd_done = 1;
    ctx->request = 1;

    if (ctx->sense != 0) {
        log_device("Sense %02x\n", ctx->sense);
        ctx->status |= SNS_UNITCHK;
    }

    if (dev->rdr_full) {
       stack_card(dev->stack[dev->rdr_stk_sel], &dev->rdr_card);
       dev->stk_cnt[dev->rdr_stk_sel] = stack_size(dev->stack[dev->rdr_stk_sel]);
       dev->rdr_full = 0;
    }

    if (dev->rdr_hop_cnt == 0 && dev->eof_flag) {
        ctx->status |= SNS_UNITEXP;
        dev->eof_flag = 0;
        return;
    }

    if (dev->rdr_stop_flag == 0) {
        dev->rdr_full = read_card(dev->rdr_feed, &dev->rdr_card);
        dev->rdr_hop_cnt = hopper_size(dev->rdr_feed);
        dev->rdr_ready = dev->rdr_full;
        log_device("read card %d size=%d\n", dev->rdr_full, dev->rdr_hop_cnt);
        if (dev->rdr_hop_cnt == 0 && dev->eof_flag == 0) {
            dev->rdr_ready = 0;
        }
    } else {
        dev->rdr_ready = 0;
        dev->rdr_stop_flag = 0;
    }
}


/* Transfer is done, read in next card */
void
model2540r_xfer_done(struct _2821_dev_context *ctx)
{
    /* If diagnostics write, no feed  */
    if ((ctx->cmd & 0x3f) == 0x25) {
        ctx->status |= SNS_CHNEND;
        ctx->cmd_done = 1;
        return;
    }
    if ((ctx->cmd & 0xdf) == 0xc2) {
        ctx->status |= SNS_CHNEND|SNS_DEVEND;
        ctx->cmd_done = 1;
        return;
    }
    add_event(ctx->device, feed_callback, 20000, NULL, 0);
    ctx->status |= SNS_CHNEND;
}

/* Decode command to device */
void
model2540r_cmd(struct _2821_dev_context *unit, uint16_t bus_out)
{
    struct _2540_context *ctx = (struct _2540_context *)unit->ctx;
    uint16_t cmd = bus_out & 0xff;
    int      col;

    log_device("2540r: command %02x\n", bus_out);
    unit->status = 0;

    switch (cmd & 07) {
    case 0: /* Test I/O */
           if (ctx->rdr_ready == 0) {
               unit->sense |= SENSE_INTERV;
           }
           if (cmd != 0) {
               unit->sense |= SENSE_CMDREJ;
           }
           if (unit->sense != 0) {
              unit->status = SNS_UNITCHK;
           }
           return;

    case 1: /* Write */
           unit->sense |= SENSE_INTERV;
           if (cmd != 0x25) {
               unit->sense |= SENSE_CMDREJ;
               unit->status = SNS_UNITCHK;
               break;
           }
           unit->cmd = cmd;
           unit->cmd_done = 0;
           unit->bptr = 0;
           unit->busy = 1;
           break;

    case 2: /* Read */
           unit->sense &= SENSE_INTERV;
           if ((cmd & 0x3f) != 0x02 && cmd != 0xd2) {
               unit->sense |= SENSE_CMDREJ;
               unit->status = SNS_UNITCHK;
               break;
           }
           /* Check if device not ready */
           if (ctx->rdr_ready == 0) { // || ctx->rdr_hop_cnt == 0) {
               unit->sense |= SENSE_INTERV;
               unit->status = SNS_UNITCHK;
               return;
           }
           for (col = 0; col < 80; col++) {
                uint16_t ch;
                uint8_t  c;
                ch = hol_to_ebcdic(ctx->rdr_card[col]);
                if (ch == 0x100) {
                    unit->sense |= SENSE_DATCHK;
                    log_device("Read error %d\n", col);
                } else {
                    unit->buffer[col] = ch & 0xff;
                }
                c = ebcdic_to_ascii[ch];
                if (!isprint(c))
                   c = '.';
                log_device("Read data %d:%02x '%c'\n", col, ch, c);
           }
           /* Set stacker */
           if ((cmd & 0xc0) != 0xc0) {
               ctx->rdr_stk_sel = (cmd >> 6) & 3;
           }
           unit->cmd = cmd;
           unit->cmd_done = 0;
           unit->blen = 80;
           unit->bptr = 0;
           unit->busy = 1;
           break;

    case 3: /* Feed */
           unit->sense &= SENSE_INTERV;
           if (cmd == 0x3) {
               unit->cmd = cmd;
               unit->cmd_done = 1;
               unit->status = SNS_CHNEND|SNS_DEVEND;
               break;
           }
           if ((cmd & 0x3f) != 0x23) {
               unit->sense |= SENSE_CMDREJ;
               unit->status = SNS_UNITCHK;
               break;
           }
           if (ctx->rdr_ready == 0) {
               unit->sense |= SENSE_INTERV;
               unit->status = SNS_UNITCHK;
               break;
           }
           if ((cmd & 0xc0) != 0xc0) {
               ctx->rdr_stk_sel = (cmd >> 6) & 3;
           } else {
               unit->sense |= SENSE_CMDREJ;
               unit->status = SNS_UNITCHK;
               break;
           }
           unit->cmd_done = 0;
           unit->busy = 1;
           model2540r_xfer_done(unit);
           break;

    case 4:  /* Sense */
           log_device("2540: Sense %02x\n", unit->sense);
           if ((cmd & 0xf) != 0x04) {
               unit->sense &= SENSE_INTERV;
               unit->sense |= SENSE_CMDREJ;
               unit->status = SNS_UNITCHK;
           } else {
               unit->cmd = cmd;
               unit->cmd_done = 0;
           }
           break;

    default:
           unit->sense |= SENSE_CMDREJ;     /* Invalid command */
           unit->status = SNS_UNITCHK;
           break;
    }
}

/*
 * Process NPRO for reader.
 */
static void
npro_callback(struct _device *unit, void *arg, int iarg)
{
    struct _2821_dev_context *ctx = (struct _2821_dev_context *)unit->dev;
    struct _2540_context *dev = (struct _2540_context *)ctx->ctx;

    if (dev->rdr_full) {
       stack_card(dev->stack[0], &dev->rdr_card);
       dev->stk_cnt[0] = stack_size(dev->stack[0]);
       dev->rdr_full = 0;
    }

    if (dev->rdr_hop_cnt == 0) {
        return;
    }

    if (dev->rdr_stop_flag == 0) {
        dev->rdr_full = read_card(dev->rdr_feed, &dev->rdr_card);
        dev->rdr_hop_cnt = hopper_size(dev->rdr_feed);
        log_device("read card %d size=%d\n", dev->rdr_full, dev->rdr_hop_cnt);
        add_event(ctx->device, npro_callback, 20000, NULL, 0);
    }
    dev->stk_cnt[0] = stack_size(dev->stack[0]);
}


/*
 * Move a card from reader to selected stacker.
 */
void
model2540r_start(struct _2540_context *ctx, int attn)
{
    int                   i;

    /* If card in feed, and eof flag is off, perform NPRO function */
    if (ctx->rdr_full && ctx->eof_flag == 0) {
       add_event(ctx->rdr_ctx->device, npro_callback, 20000, NULL, 0);
       return;
    }

    /* If no more cards stop processing. */
    if (hopper_size(ctx->rdr_feed) == 0) {
        ctx->rdr_ctx->sense |= SENSE_INTERV;
        ctx->rdr_ready = 0;
        return;
    }

    if (ctx->rdr_stop_flag == 0) {
        ctx->rdr_full = read_card(ctx->rdr_feed, &ctx->rdr_card);
        ctx->rdr_hop_cnt = hopper_size(ctx->rdr_feed);
        ctx->rdr_ready = ctx->rdr_full;
        log_device("read card %d size=%d\n", ctx->rdr_full, ctx->rdr_hop_cnt);
        if (ctx->rdr_hop_cnt == 0 && ctx->eof_flag == 0) {
            ctx->rdr_ready = 0;
        }
        if (ctx->rdr_ready == 1 && attn == 1) {
            ctx->rdr_ctx->status = SNS_DEVEND;
            ctx->rdr_ctx->request = 1;
            ctx->rdr_ctx->cmd_done = 1;
        }
    } else {
        ctx->rdr_ready = 0;
    }

    log_device("Size %d %d, %d %d %d\n", hopper_size(ctx->rdr_feed),
            stack_size(ctx->rdr_feed), hopper_size(ctx->stack[0]),
            stack_size(ctx->stack[0]), ctx->rdr_ready);
    ctx->rdr_stop_flag = 0;
    for (i = 0; i < 5; i++) {
       ctx->stk_cnt[i] = stack_size(ctx->stack[i]);
    }
    if (ctx->rdr_ready == 0) {
log_device("intervent\n");
        ctx->rdr_ctx->sense |= SENSE_INTERV;
    } else {
        ctx->rdr_ctx->sense &= ~(SENSE_INTERV);
    }
}

