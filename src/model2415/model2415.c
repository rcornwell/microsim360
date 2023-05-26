/*
 * microsim360 - Model 2415 tape controller.
 *
 * Copyright 2021, Richard Cornwell
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

#include <stdio.h>
#include <SDL.h>
#include <SDL_timer.h>
#include <SDL_thread.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <string.h>
#include "panel.h"
#include "logger.h"
#include "event.h"
#include "device.h"
#include "xlat.h"
#include "tape.h"

#include "model2415.xpm"
#include "tape_images.xpm"

/*
 *  Commands.
 *
 *            01234567
 *  Write     00000001
 *  Read      00000010
 *  Sense     00000100
 *  Readback  00001100
 *
 *  Control   00CCC111   Tape motion control
 *              000      Rewind
 *              001      Rewind and unload
 *              010      Erase Gap
 *              011      Write tape mark.
 *              100      Backspace block
 *              101      Backspace file.
 *              110      Forward space block.
 *              111      Forward space file.
 *  Mode      DDMMM011   7 track
 *                       den, odd, even, conv, noconv, trans, notrans
 *              000          NOP
 *              001          Reserved.
 *              010       y   y          y                     y
 *              011          9 track only
 *              100       y         y             y            y
 *              101       y         y             y      y
 *              110       y   y                   y            y
 *              111       y   y                   y      y
 *
 *            00          200bpi
 *            01          556bpi
 *            10          800bpi
 *            11          9 track mode. Models 4-6
 *
 *  Mode      11NNN011    9 track.
 *              000       1600 bpi
 *              001       800 bpi
 */

/* Sense byte 0 */
#define SENSE_CMDREJ    BIT0  /* Invalid command */
#define SENSE_INTERV    BIT1  /* Operator intervention, no tape */
#define SENSE_BUSCHK    BIT2  /* Bus parity error */
#define SENSE_EQUCHK    BIT3  /* Equipment check, not implemented */
#define SENSE_DATCHK    BIT4  /* More then 1 punch in rows 1-7 */
#define SENSE_OVRRUN    BIT5  /* Data missed */
#define SENSE_WCZERO    BIT6  /* No data transfered during write */
#define SENSE_DCCHK     BIT7  /* Data converter check */

/* Sense byte 1 */
#define SENSE_NOISE     BIT0  /* Noise record */
#define SENSE_TUA       BIT1  /* Selected and ready. */
#define SENSE_TUB       BIT2  /* not ready or rewinding */
#define SENSE_7TRACK    BIT3  /* 7 track tape device */
#define SENSE_LOAD      BIT4  /* Tape at load point */
#define SENSE_WRITE     BIT5  /* Tape in write status */
#define SENSE_NORING    BIT6  /* No write ring */

/* Sense byte 2 */
#define SENSE_2         BIT6|BIT7

/* Sense byte 3 */
#define SENSE_VCR       BIT0   /* Vertical parity check */
#define SENSE_LRCR      BIT1   /* Parity error during read */
#define SENSE_BACK      BIT6   /* Tape in backward status */

/* Sense byte 4 and 5, both 0 */

#define STATE_IDLE      0     /* Device in Idle state */
#define STATE_SEL       1     /* Device now selected */
#define STATE_CMD       2     /* Device awaiting command */
#define STATE_INIT_STAT 3     /* Sent init status */
#define STATE_OPR       4     /* Do operation */
#define STATE_OPR_REL   5     /* Operator but release */
#define STATE_REQ       6     /* Request the channel */
#define STATE_DATA_O    7     /* Data out to device */
#define STATE_DATA_I    8     /* Data in  to device */
#define STATE_DATA_END  9     /* Post end of channel usage */
#define STATE_END       10    /* Post ending status */
#define STATE_STACK     11    /* Channel polling */
#define STATE_STACK_SEL 12    /* Stack status select */
#define STATE_STACK_CMD 13    /* Stack command */
#define STATE_STACK_STA 14    /* Stack status */
#define STATE_STACK_HLD 15    /* Stack hold */
#define STATE_WAIT      16    /* After data transfer wait motion */
#define STATE_RDY       17    /* Wait for selection to give status */

#define FRAME_DELAY     33    /* 34 us per frame delay */
#define REWIND_DELAY    1000 /* 20 ms */
#define REW_FRAME       3840  /* Frames per 20ms */
#define START_DELAY     100
#define STOP_DELAY      (10 * FRAME_DELAY)

#define MT_ODD          0x01  /* Odd parity */
#define MT_TRANS        0x02  /* Translation turned on ignored 9 track  */
#define MT_CONV         0x04  /* Data converter on ignored 9 track  */

struct _2415_context {
    int                    addr;         /* Device address */
    int                    chan;
    int                    state;        /* Current channel state */
    int                    selected;     /* Device currently selected */
    int                    request;      /* Request pending */
    int                    addressed;    /* Device addressed */
    int                    stacked;      /* Device has stacked status */
    int                    sense[6];     /* Current sense value */
    int                    sense_cnt;    /* Sense counter */
    int                    chain_flag;   /* Command chaining in effect */
    int                    cmd;          /* Current command */
    int                    cmd_done;     /* Current read/write finished */
    int                    status;       /* Current bus status */
    uint8_t                data;         /* Current byte to send/recieve */
    uint16_t               cbuffer;      /* Conversion buffer */
    int                    data_rdy;     /* Data is valid */
    int                    data_end;     /* No more data to send/recieve */
    int                    delay;        /* Delay time till operation done */
    int                    nunits;       /* Number of units */
    int                    cur_unit;     /* Currently selected unit */
    int                    stat_unit;    /* Unit having pending sense data */
    int                    stk_unit;     /* Unit that has stacked status */
    int                    t_scan;       /* Tape scanner */
    int                    rew_flags;    /* Units doing rewind */
    int                    run_flags;    /* Units doing unload */
    int                    rew_delay;    /* Delay until processing rewinding units */
    int                    rdy_flags;    /* Unit has become ready */
    struct _tape_buffer   *tape[6];      /* Tape units */
    int                    track_7;      /* Support 7 track drives */
    int                    cc;           /* Character counter for 7 track tapes */
    int                    mode7;        /* Tape mode for 7 track tapes */
    int                    mode9;        /* Tape mode for 9 track tapes */
};

static void
rewind_callback(struct _device *unit, void *arg, int u)
{
    struct _2415_context *ctx = (struct _2415_context *)unit->dev;
    struct _tape_buffer  *tape = (struct _tape_buffer *)arg;
    int      i;
    int      r;

    if (tape->pos_frame <= (5 * 12 * 1600)) {
       log_device("Rewind Low speed %02x %02x\n", ctx->rew_flags, ctx->rdy_flags);
       r = tape_rewind_frames(tape, 1);
       if (r == 0) {
           log_device("Rewind done %d\n", u);
           ctx->rew_flags &= ~(1 << u);
           if (ctx->run_flags & (1 << u)) {
               log_device("Detach device %d\n", u);
               tape_detach(tape);
               ctx->run_flags &= ~(1 << u);
           } else if (ctx->chain_flag) {
               log_device("Chain done device %d\n", u);
               ctx->cmd_done = 1;
               ctx->status = SNS_DEVEND|SNS_CHNEND;
               ctx->cmd = 0;
           } else {
               if (ctx->cur_unit == u && ctx->state != STATE_IDLE) {
                  ctx->status &= ~(SNS_CTLEND|SNS_UNITCHK);
                  ctx->cmd_done = 1;
                  log_device("Rewind finished %d\n", u);
               } else {
                  ctx->rdy_flags |= 1 << u;
                  log_device("Rewind ready device %d\n", u);
               }
           }
       } else {
           add_event(unit, &rewind_callback, FRAME_DELAY, arg, u);
       }
       return;
    }

    log_device("Rewind high speed  %02x %02x\n", ctx->rew_flags, ctx->rdy_flags);
    r = tape_rewind_frames(tape, REW_FRAME);
    if (r == 0) {
        log_device("Rewind done %d\n", u);
        ctx->rew_flags &= ~(1 << u);
        if (ctx->run_flags & (1 << u)) {
            tape_detach(tape);
            ctx->run_flags &= ~(1 << u);
        } else if (ctx->chain_flag) {
            ctx->cmd_done = 1;
            ctx->status = SNS_DEVEND|SNS_CHNEND;
            ctx->cmd = 0;
        } else {
            if (ctx->cur_unit == u && ctx->state != STATE_IDLE) {
               ctx->status &= ~(SNS_CTLEND|SNS_UNITCHK);
               ctx->cmd_done = 1;
            } else {
               ctx->rdy_flags |= 1 << u;
            }
        }
    } else {
        add_event(unit, &rewind_callback, REWIND_DELAY, arg, u);
    }
}

static void
done_callback(struct _device *unit, void *arg, int u)
{
    struct _2415_context *ctx = (struct _2415_context *)unit->dev;
    struct _tape_buffer  *tape = (struct _tape_buffer *)arg;
    int      i;
    int      r;

    log_device("Do done command %d %d %d\n", ctx->data_end, ctx->data_rdy, ctx->state);
    ctx->cmd_done = 1;
    if ((ctx->cmd & 0x3) == 3) {
        ctx->status |= SNS_DEVEND;
    } else {
        ctx->status |= SNS_DEVEND|SNS_CHNEND;
    }
    ctx->cmd = 0;
}

static void
tape_callback(struct _device *unit, void *arg, int u)
{
    struct _2415_context *ctx = (struct _2415_context *)unit->dev;
    struct _tape_buffer  *tape = (struct _tape_buffer *)arg;
    int      i;
    int      r;

    log_trace("Tape = %x, arg=%d\n", tape, u);

    switch (ctx->cmd & 0xf) {
    case 0: /* Test I/O */
    case 4: /* Sense */
    case 3: /* Mode command */
    case 0xd: /* Mode command */
           break;
    case 1: /* Write */
           log_device("Do write command %d %d %d\n", ctx->data_end, ctx->data_rdy, ctx->state);
           if (ctx->data_end) {
               /* If 7 track and conversion, write last character */
               if (!tape_9_track(ctx->tape[ctx->cur_unit]) && (ctx->mode7 & MT_CONV) != 0) {
                   uint8_t   mode = (ctx->mode7 & MT_ODD) ? 0 : 0x100;
                   switch(ctx->cc) {
                   case 0:   /* ----76 543210 */
                             /*     xx ^^^^^^ */
                       break;

                   case 1:   /* --76543210 76 */
                             /*   xxxx^^^^ ^^ */
                       ctx->data = (ctx->cbuffer & 03) << 4;
                       break;

                   case 2:    /* 76543210 7654 */
                              /* xxxxxx^^ ^^^^ */
                       ctx->data = (ctx->cbuffer & 0xf) << 2;
                       ctx->data_rdy = 1;
                       break;

                   case 3:    /* ---- 765432 */
                       ctx->data = ctx->cbuffer;
                       ctx->data_rdy = 1;
                       break;
                   }
                   ctx->data &= 077;
                   ctx->data |= parity_table[ctx->data] ^ mode;
                   if (ctx->data_rdy) {
                      r = tape_write_frame(tape, ctx->data);
                   }
               }
               r = tape_finish_rec(tape);
               add_event(unit, done_callback, STOP_DELAY, (void *)tape, u);
           } else if (ctx->data_rdy == 0) {
              /* Do a write start */
               ctx->sense[0] &= ~(SENSE_WCZERO);
               if (!tape_9_track(ctx->tape[ctx->cur_unit])) {
                   uint8_t   mode = (ctx->mode7 & MT_ODD) ? 0 : 0x100;
                   if (ctx->mode7 & MT_TRANS) {
                       ctx->data = ((ctx->data & 0x30) ^ 0x30) | (ctx->data & 0xf);
                       ctx->data_rdy = 1;
                   }
                   if (ctx->mode7 & MT_CONV) {
                       uint8_t    t;
                       switch(ctx->cc) {
                       case 0:   /* ----76 543210 */
                                 /*     ^^ ^^^^XX */
                           ctx->cbuffer = ctx->data & 3;
                           ctx->data >>= 2;
                           ctx->data_rdy = 1;
                           ctx->cc++;
                           break;

                       case 1:   /* --76543210 10 */
                                 /*   ^^^^^^^^ ^^ */
                           t = ctx->cbuffer;
                           ctx->cbuffer = (ctx->data & 0xf);
                           ctx->data = (t << 4) | ((ctx->data >> 4) & 0xf);
                           ctx->data_rdy = 1;
                           ctx->cc++;
                           break;

                       case 2:    /* 76543210 7654 */
                                  /* xxxxxx^^ ^^^^ */
                           t = ctx->cbuffer;
                           ctx->cbuffer = ctx->data & 077;
                           ctx->data = (t << 2) | ((ctx->data >> 6) & 0x3);
                           ctx->data_rdy = 1;
                           ctx->cc++;
                           break;

                       case 3:    /* ---- 765432 */
                           ctx->data = ctx->cbuffer;
                           ctx->data_rdy = 1;
                           ctx->cc = 0;
                           break;
                      }
                   }
                   ctx->data &= 077;
                   ctx->data |= parity_table[ctx->data] ^ mode;
               } else {
                   r = tape_write_frame(tape, ctx->data);
                   ctx->data_rdy = 1;
               }
               add_event(unit, tape_callback, FRAME_DELAY, (void *)tape, u);
           } else {
               /* If no more data, all is ok */
               ctx->sense[0] |= SENSE_OVRRUN;
               ctx->data_end = 1;
               add_event(unit, tape_callback, FRAME_DELAY * 4, (void *)tape, u);
           }
           break;
    case 2:  /* Read */
    case 0xc:  /* Read backward */
           /* If CPU does not want any more data, just read and ignore
              the rest of the data */
           log_device("Do read command %d %d %d\n", ctx->data_end, ctx->data_rdy, ctx->state);
           if (ctx->data_end) {
               r = tape_read_frame(tape, &ctx->data);
               log_device("Tape read frame dataend %d\n", r);
               if (r != 1) {
                   /* Process end */
                   r = tape_finish_rec(tape);
                   log_device("Tape finish read %d\n", r);
                   add_event(unit, done_callback, STOP_DELAY, (void *)tape, u);
               } else {
                   add_event(unit, tape_callback, FRAME_DELAY, (void *)tape, u);
               }
           } else if (ctx->data_rdy) {
               log_device("Tape read frame overrun \n");
               ctx->data_end = 1;
               ctx->sense[0] |= SENSE_OVRRUN;
               ctx->status |= SNS_UNITCHK;
               add_event(unit, done_callback, STOP_DELAY, (void *)tape, u);
           } else {
               r = tape_read_frame(tape, &ctx->data);
               log_device("Tape read frame %d\n", r);
               if (r < 0) {
                   /* Set up read error */
               } else if (r == 0) {
                   /* End of record */
                   ctx->data_end = 1;
                   add_event(unit, tape_callback, FRAME_DELAY * 4, (void *)tape, u);
                   if (!tape_9_track(ctx->tape[ctx->cur_unit])) {
                       if (ctx->mode7 & MT_CONV) {
                           switch(ctx->cc) {
                           case 0:   /* ------ BA8421 */
                               ctx->cbuffer = ctx->data;
                               ctx->data_rdy = 1;
                               break;

                           case 1:   /* BA8421 BA8421 */
                                     /*     ^^ ^^^^^^ */
                               ctx->cbuffer >>= 8;
                               ctx->data = (ctx->cbuffer & 0x0f);
                               ctx->data_rdy = 1;
                               break;

                           case 2:    /* --BA8421 BA84 */
                                      /*     ^^^^ ^^^^ */
                               ctx->cbuffer >>= 8;
                               ctx->data = (ctx->cbuffer & 0x03);
                               ctx->data_rdy = 1;
                               break;

                           case 3:    /* ---- BA8421 BA */
                               break;
                          }
                       }
                   }
               } else if (r == 2) {
                   /* Read of tape mark */
                   ctx->data_end = 1;
                   ctx->status |= SNS_UNITEXP;
                   add_event(unit, done_callback, STOP_DELAY, (void *)tape, u);
               } else {
                   add_event(unit, tape_callback, FRAME_DELAY, (void *)tape, u);
                   if (!tape_9_track(ctx->tape[ctx->cur_unit])) {
                       uint8_t   mode = (ctx->mode7 & MT_ODD) ? 0 : 0x100;
                       if ((parity_table[ctx->data & 077] ^ (ctx->data & 0100) ^ mode) == 0) {
                           ctx->sense[0] = SENSE_DCCHK;
                           ctx->sense[3] = SENSE_VCR;
                       }
                       ctx->data &= 077;
                       if (ctx->mode7 & MT_TRANS) {
                           ctx->data = bcd_to_ebcdic[ctx->data];
                       }
                       if (ctx->mode7 & MT_CONV) {
                           switch(ctx->cc) {
                           case 0:   /* ------ BA8421 */
                               ctx->cbuffer = ctx->data;
                               ctx->cc++;
                               break;

                           case 1:   /* BA8421 BA8421 */
                                     /*     ^^ ^^^^^^ */
                               ctx->cbuffer |= ctx->data << 6;
                               ctx->data = (ctx->cbuffer & 0xff);
                               ctx->data_rdy = 1;
                               ctx->cc++;
                               break;

                           case 2:    /* --BA8421 BA84 */
                                      /*     ^^^^ ^^^^ */
                               ctx->cbuffer >>= 8;
                               ctx->cbuffer |= ctx->data << 4;
                               ctx->data = (ctx->cbuffer & 0xff);
                               ctx->data_rdy = 1;
                               ctx->cc++;
                               break;

                           case 3:    /* ---- BA8421 BA */
                               ctx->cbuffer >>= 8;
                               ctx->cbuffer |= ctx->data << 2;
                               ctx->data = (ctx->cbuffer & 0xff);
                               ctx->data_rdy = 1;
                               ctx->cc = 0;
                               break;
                          }
                       }
                   } else {
                       ctx->data_rdy = 1;
                   }
                   log_device("Tape Queued: %02x\n", ctx->data);
               }
           }
           break;

    case 7:  /* Tape motion control */
    case 0xf:
           switch (ctx->cmd & 0xff) {
           case 0x0f:    /* Rewind and unload */
                tape_start_rewind(tape);
                add_event(unit, rewind_callback, REWIND_DELAY, (void *)tape, u);
                ctx->data_end = 1;
                ctx->cmd_done = 1;
                ctx->rew_flags |= 1 << ctx->cur_unit;
                ctx->run_flags |= 1 << ctx->cur_unit;
                ctx->status = SNS_CTLEND|SNS_DEVEND|SNS_UNITCHK;
                ctx->cmd = 0;
                break;
           case 0x07:    /* Rewind */
                log_device("start rewind %d\n", ctx->chain_flag);
                tape_start_rewind(tape);
                ctx->rew_flags |= 1 << ctx->cur_unit;
                add_event(unit, rewind_callback, REWIND_DELAY, (void *)tape, u);
                ctx->data_end = 1;
                if (ctx->chain_flag == 0) {
                    ctx->cmd_done = 1;
                    ctx->status |= SNS_DEVEND;
                    ctx->cmd = 0;
                }
                break;
           case 0x17:    /* Erase gap */
                add_event(unit, done_callback, 20*FRAME_DELAY, (void *)tape, u);
                break;
           case 0x1f:    /* Write tape mark */
                r = tape_write_mark(tape);
                log_device("Write tape mark %d\n", r);
                add_event(unit, done_callback, 10*FRAME_DELAY, (void *)tape, u);
                break;
           case 0x37:    /* Forward space block */
           case 0x3f:    /* Forward space file */
           case 0x27:    /* Backspace block */
           case 0x2f:    /* Backspace file */
                r = tape_read_frame(tape, &ctx->data);
                log_device("space tape %d\n", r);
                if (r < 0) {
                     /* Set up read error */
                     ctx->status |= SNS_UNITCHK;
                     add_event(unit, done_callback, STOP_DELAY, (void *)tape, u);
                } else if (r == 0) {
                     /* Terminate of record read */
                     r = tape_finish_rec(tape);
                     /* End of record */
                     if (tape_at_loadpt(tape)) {
                         ctx->status |= SNS_UNITCHK;
                         add_event(unit, done_callback, STOP_DELAY, (void *)tape, u);
                     } else {
                         if (ctx->cmd & 0x8) {
                             if (ctx->cmd & 0x10) {
                                r = tape_read_forw(tape);
                             } else {
                                r = tape_read_back(tape);
                             }
                             log_device("Tape start record %d\n", r);
                             /* Handle error */
                             if (r < 0) {
                                 ctx->status |= SNS_UNITCHK;
                                 add_event(unit, done_callback, STOP_DELAY, (void *)tape, u);
                             } else if (r == 1) {
                                 /* Spacing file, continue until tape mark */
                                 add_event(unit, tape_callback, FRAME_DELAY, (void *)tape, u);
                             } else {
                                 /* Either end of tape or tape mark, stop */
                                 add_event(unit, done_callback, STOP_DELAY, (void *)tape, u);
                             }
                         } else {
                             if (r == 2) {
                                 ctx->status |= SNS_UNITEXP;
                             }
                             /* If space block, end */
                             add_event(unit, done_callback, STOP_DELAY, (void *)tape, u);
                         }
                      }
                } else if (r == 2) {
                     /* Terminate on tape mark */
                     r = tape_finish_rec(tape);
                     if ((ctx->cmd & 0x8) == 0) {
                         /* If space block, end */
                         ctx->status |= SNS_UNITEXP;
                     }
                     add_event(unit, done_callback, STOP_DELAY, (void *)tape, u);
                } else {
                   add_event(unit, tape_callback, FRAME_DELAY, (void *)tape, u);
                }
                break;
           }
           break;
    default:
           break;
    }
}

void
model2415_dev(struct _device *unit, uint16_t *tags, uint16_t bus_out, uint16_t *bus_in)
{
    struct _2415_context *ctx = (struct _2415_context *)unit->dev;
    int      i;
    int      r;
    static   uint16_t  last_tags = 0;

    if (last_tags != *tags) {
        print_tags("Tape", ctx->state, *tags, bus_out);
        last_tags = *tags;
    }
    /* Reset device if OPER OUT is dropped */
    if ((*tags & (CHAN_OPR_OUT|CHAN_SUP_OUT)) == 0) {
        log_device("Reset tape\n");
        if (ctx->selected || ctx->addressed) {
           *tags &= ~(CHAN_OPR_IN|CHAN_ADR_IN|CHAN_SRV_IN|CHAN_STA_IN);
        }
        if (ctx->request) {
           *tags &= ~(CHAN_REQ_IN);
        }
        if (ctx->cmd != 0) {
           r = tape_finish_rec(ctx->tape[ctx->cur_unit]);
        }
        ctx->cmd = 0;
        ctx->cmd_done = 0;
        ctx->selected = 0;
        ctx->addressed = 0;
        ctx->stacked = 0;
        ctx->state = STATE_IDLE;
        for (i = 0; i < 6; i++)
           ctx->sense[i] = 0;
        ctx->rdy_flags = 0;
        return;
    }

    switch (ctx->state) {
    case STATE_IDLE:
            /* Wait until Channels asks for us */
            if ((*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT) ||
                *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_SUP_OUT)) &&
                 (bus_out & 0xf8) == ctx->addr && (bus_out & 0x7) < ctx->nunits) {
                 /* Device selected */
                 *tags &= ~(CHAN_SEL_OUT);      /* Clear select out and in */
                 if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0)
                     ctx->sense[0] |= SENSE_BUSCHK;
                 i = bus_out & 0x7;
                 /* Check if device is rewinding */
                 if (((ctx->rew_flags | ctx->run_flags) & (1 << i)) != 0) {
                      log_device("Unit busy rew=%02x run=%02x\n", ctx->rew_flags, ctx->run_flags);
                      *tags |= CHAN_STA_IN;
                      *bus_in = 0x100 | SNS_SMS | SNS_BSY;
                      ctx->addressed = 1;
                      break;
                 }
                 /* Check if status pending for another unit */
                 if (ctx->stat_unit >= 0 && i != ctx->stat_unit) {
                     *tags |= CHAN_STA_IN;
                     *bus_in = 0x100 | SNS_SMS | SNS_BSY;
                     log_device("Pending status: %d u=%d\n", ctx->stat_unit, i);
                     ctx->addressed = 1;
                     break;
                 }
                 ctx->cur_unit = i;
                 ctx->status = 0;
                 ctx->stk_unit = -1;
                 *tags &= ~(CHAN_SEL_OUT);      /* Clear select out and in */
                 *tags |= CHAN_OPR_IN;
                 ctx->state = STATE_SEL;
                 ctx->chain_flag = 0;
                 ctx->addressed = 1;
                 log_device("tape selected unit: %d\n", ctx->cur_unit);
                 break;
             }

             /* If we are returning short busy keep value on bus */
             if (ctx->addressed) {
                 *tags &= ~(CHAN_SEL_OUT);      /* Clear select out and in */
                 *bus_in = 0x100 | SNS_SMS | SNS_BSY;
                 if ((*tags & (CHAN_HLD_OUT)) == 0) {
                     *tags &= ~(CHAN_STA_IN);      /* Clear Status in */
                     ctx->addressed = 0;
                     log_device("tape selected busy done\n");
                     break;
                 }
                 if ((*tags & (CHAN_STA_IN)) != 0) {
                     log_device("tape selected busy\n");
                     break;
                 }
             }

             /* Scan for rewind done, or unit becoming ready */
             if ((!ctx->selected || !ctx->addressed) && ctx->rdy_flags != 0) {
                 /* Put request in up */
                 ctx->state = STATE_RDY;
                 log_device("tape ready units: %02x\n", ctx->rdy_flags);
             }
             break;

    case STATE_SEL:
             /* If we are selected, drop select out to rest of channel */
             if (ctx->addressed || ctx->selected) {
                 *tags &= ~(CHAN_SEL_OUT);  /* Clear select in */
                 *tags |= CHAN_OPR_IN;
             }

             /* When address out drops put our address on bus in */
             if (ctx->addressed && (*tags & (CHAN_ADR_OUT|CHAN_CMD_OUT)) == 0) {
                  *tags |= CHAN_ADR_IN;      /* Return address until accepted */
                  *bus_in = (ctx->addr & 0xf8) | ctx->cur_unit;
                  *bus_in |= odd_parity[*bus_in];
                  ctx->addressed = 0;
                  ctx->selected = 1;         /* Mark us as selected */
                  log_device("tape address\n");
                  break;
             }

             if (ctx->selected && (*tags & (CHAN_ADR_OUT|CHAN_CMD_OUT)) == 0) {
                  *tags |= CHAN_ADR_IN;      /* Return address until accepted */
                  *bus_in = (ctx->addr & 0xf8) | ctx->cur_unit;
                  *bus_in |= odd_parity[*bus_in];
                  log_device("reader address\n");
                  break;
             }

             /* If we get Address out again, we need to halt */
             if (ctx->selected && (*tags & (CHAN_ADR_OUT)) != 0) {
                  *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN|CHAN_OPR_IN);  /* Clear select in */
                  *tags |= CHAN_ADR_IN;      /* Return address until accepted */
                  log_device("tape halt I/O\n");
             }

             /* Wait for Command out to raise */
             /* Can now drop address in */
             if (ctx->selected && (*tags & (CHAN_CMD_OUT)) != 0) {
                  log_device("tape command %02x\n", bus_out);
                  if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0)
                      ctx->sense[0] |= SENSE_BUSCHK;
                  ctx->state = STATE_CMD; /* Wait for command out to return
                                              initial status */
                  *tags &= ~(CHAN_ADR_IN);            /* Clear select in and address in */

                  /* If current unit has pending status and not sense, return busy */
                  if (ctx->cur_unit == ctx->stat_unit && bus_out & 0xff != 0x4 &&
                      (ctx->sense[0] != 0 || (ctx->sense[1] & SENSE_NOISE) != 0 ||
                      (ctx->sense[3] & (SENSE_VCR|SENSE_LRCR)) != 0)) {
                      ctx->status = SNS_BSY;
                      log_device(" Unit busy: u=%d %02x %02x %02x\n", ctx->cur_unit, ctx->sense[0], ctx->sense[1], ctx->sense[3]);
                      break;
                  }

                  /* Set up sense byte 1 */
                  ctx->sense[1] &= ~(SENSE_TUA|SENSE_TUB|SENSE_LOAD|SENSE_NORING|SENSE_7TRACK);
                  if (tape_ready(ctx->tape[ctx->cur_unit])) {
                      ctx->sense[1] |= SENSE_TUA;
                      if ((ctx->rew_flags & (1<<ctx->cur_unit)) != 0)
                          ctx->sense[1] |= SENSE_TUB;
                      if (!tape_ring(ctx->tape[ctx->cur_unit]))
                          ctx->sense[1] |= SENSE_NORING;
                      if (tape_at_loadpt(ctx->tape[ctx->cur_unit]))
                          ctx->sense[1] |= SENSE_LOAD;
                      if (!tape_9_track(ctx->tape[ctx->cur_unit]))
                          ctx->sense[1] |= SENSE_7TRACK;
                  }
                  ctx->cmd = bus_out & 0xff;
                  ctx->cmd_done = 0;
                  ctx->data_end = 0;
                  ctx->data_rdy = 0;
                  ctx->cc = 0;
                  ctx->rdy_flags &= ~(1 << ctx->cur_unit);
                  ctx->stat_unit = -1;
                  tape_select(ctx->tape[ctx->cur_unit]);
                  switch (ctx->cmd & 07) {
                  case 0: /* Test I/O */
                         break;
                  case 1: /* Write */
                         ctx->sense[0] = 0;
                         ctx->sense[1] &= (SENSE_TUA|SENSE_TUB|SENSE_7TRACK|
                                           SENSE_LOAD|SENSE_NORING);
                         ctx->sense[2] = SENSE_2;
                         ctx->sense[3] = 0;
                         ctx->sense[4] = 0;
                         if ((ctx->cmd & 0xfc) != 0) {
                             ctx->sense[0] = SENSE_CMDREJ;
                             break;
                         }
                         /* Check if tape attached */
                         if (ctx->tape[ctx->cur_unit]->file_name == NULL) {
                             ctx->sense[0] = SENSE_INTERV;
                             break;
                         }

                         /* Check if no write ring */
                         if (tape_ring(ctx->tape[ctx->cur_unit]) == 0) {
                             ctx->sense[0] = SENSE_CMDREJ;
                             break;
                         }
                         /* Do a write start */
                         r = tape_write_start(ctx->tape[ctx->cur_unit]);
                         if (r == 1) {
                            ctx->sense[0] = SENSE_WCZERO;
                            ctx->sense[1] |= SENSE_WRITE;
                            add_event(unit, &tape_callback, START_DELAY,
                                     (void *)ctx->tape[ctx->cur_unit], ctx->cur_unit);
                            ctx->data_rdy = 1;
                         } else if (r == 2) {
                             ctx->sense[0] = SENSE_CMDREJ;
                         } else {
                             ctx->sense[0] = SENSE_INTERV;
                         }
                         break;

                  case 2: /* Read */
                         ctx->sense[0] = 0;
                         ctx->sense[1] &= (SENSE_TUA|SENSE_TUB|SENSE_7TRACK|
                                           SENSE_LOAD|SENSE_NORING);
                         ctx->sense[2] = SENSE_2;
                         ctx->sense[3] = 0;
                         ctx->sense[4] = 0;
                         if ((ctx->cmd & 0xfc) != 0) {
                             //log_device("Invalid command %x02\n", ctx->cmd);
                             ctx->sense[0] = SENSE_CMDREJ;
                             break;
                         }
                         /* Check if tape attached */
                         if (ctx->tape[ctx->cur_unit]->file_name == NULL) {
                             ctx->sense[0] = SENSE_INTERV;
                             break;
                         }
                         /* Do a read start */
                         r = tape_read_forw(ctx->tape[ctx->cur_unit]);
                         if (r < 0) {
                             ctx->sense[0] = SENSE_INTERV;
                         } else {
                             log_trace("Buffer = %x\n", (void *)&ctx->tape[ctx->cur_unit]);
                             add_event(unit, &tape_callback, START_DELAY,
                                   (void *)ctx->tape[ctx->cur_unit], ctx->cur_unit);
                         }
                         break;

                  case 3: /* Mode command */
                         ctx->sense[0] = 0;
                         ctx->sense[2] = SENSE_2;
                         ctx->sense[3] = 0;
                         ctx->sense[4] = 0;
                         if ((ctx->sense[1] & SENSE_7TRACK) != 0) {
                             switch ((ctx->cmd >> 3) & 07) {
                             case 0:      /* NOP */
                             case 1:      /* Diagnostics */
                             case 3:
                                  break;
                             case 2:      /* Reset condition */
                                  ctx->mode7 = MT_ODD | MT_CONV;
                                  ctx->mode7 |= ctx->cmd & 0xc0;  /* Copy density */
                                  break;
                             case 4:
                                  ctx->mode7 = ctx->cmd & 0xc0;  /* Copy density */
                                  break;
                             case 5:
                                  ctx->mode7 = MT_TRANS;
                                  ctx->mode7 |= ctx->cmd & 0xc0;  /* Copy density */
                                  break;
                             case 6:
                                  ctx->mode7 = MT_ODD;
                                  ctx->mode7 |= ctx->cmd & 0xc0;  /* Copy density */
                                  break;
                             case 7:
                                  ctx->mode7 = MT_ODD | MT_TRANS;
                                  ctx->mode7 |= ctx->cmd & 0xc0;  /* Copy density */
                                  break;
                             }
                         } else {
                             ctx->mode9 = ctx->cmd & 0xc0;  /* Copy density */
                         }
                         ctx->cmd = 0;
                         ctx->cmd_done = 1;
                         ctx->data_end = 1;
                         break;

                  case 4:  /* Sense or read backward */
                         if (ctx->cmd == 0xc) {
                             ctx->sense[0] = 0;
                             ctx->sense[1] &= (SENSE_TUA|SENSE_TUB|SENSE_7TRACK|
                                               SENSE_LOAD|SENSE_NORING);
                             ctx->sense[2] = SENSE_2;
                             ctx->sense[3] = 0;
                             ctx->sense[4] = 0;
                             /* Check if tape attached */
                             if (ctx->tape[ctx->cur_unit]->file_name == 0) {
                                 ctx->sense[0] = SENSE_INTERV;
                                 break;
                             }
                             /* If at load point. abort */
                             if (ctx->sense[1] & SENSE_LOAD) {
                                 ctx->cmd = 0;
                                 ctx->status |= SNS_UNITCHK;
                                 ctx->data_end = 1;
                                 ctx->cmd_done = 1;
                                 break;
                             }
                             /* Do a read start */
                             r = tape_read_back(ctx->tape[ctx->cur_unit]);
                             log_device("Tape read back %d : %d\n", ctx->cur_unit, r);
                             if (r < 0) {
                                 ctx->sense[0] = SENSE_INTERV;
                             } else {
                                 add_event(unit, &tape_callback, START_DELAY,
                                       (void *)ctx->tape[ctx->cur_unit], ctx->cur_unit);
                             }
                         } else if (ctx->cmd == 0x4) {
                             ctx->data = ctx->sense[0];
                             ctx->sense_cnt = 1;
                             ctx->data_rdy = 1;
                         } else {
                             ctx->sense[0] = SENSE_CMDREJ;
                             ctx->sense[2] = SENSE_2;
                             ctx->sense[3] = 0;
                             ctx->sense[4] = 0;
                         }
                         break;

                  case 7:  /* Tape motion control */
                         ctx->sense[0] = 0;
                         ctx->sense[1] &= (SENSE_TUA|SENSE_TUB|SENSE_7TRACK|
                                           SENSE_LOAD|SENSE_NORING);
                         ctx->sense[2] = SENSE_2;
                         ctx->sense[3] = 0;
                         ctx->sense[4] = 0;
                         /* Check if tape attached */
                         if (ctx->tape[ctx->cur_unit]->file_name == 0) {
                             ctx->sense[0] = SENSE_INTERV;
                             break;
                         }
                         switch (ctx->cmd & 0xff) {
                         case 0x17:    /* Erase gap */
                         case 0x1f:    /* Write tape mark */
                               ctx->data_end = 1;
                               add_event(unit, &tape_callback, START_DELAY,
                                       (void *)ctx->tape[ctx->cur_unit], ctx->cur_unit);
                               break;

                         case 0x07:    /* Rewind */
                         case 0x0f:    /* Rewind and unload */
                               ctx->data_end = 1;
                               add_event(unit, &tape_callback, START_DELAY,
                                       (void *)ctx->tape[ctx->cur_unit], ctx->cur_unit);
                               break;
                         case 0x37:    /* Forward space block */
                         case 0x3f:    /* Forward space file */
                              ctx->data_end = 1;
                              /* Do a read start */
                              r = tape_read_forw(ctx->tape[ctx->cur_unit]);
                              if (r >= 0) {
                                  add_event(unit, &tape_callback, START_DELAY,
                                          (void *)ctx->tape[ctx->cur_unit], ctx->cur_unit);
                              } else {
                                  ctx->sense[0] = SENSE_INTERV;
                              }
                              break;
                         case 0x27:    /* Backspace block */
                         case 0x2f:    /* Backspace file */
                              ctx->data_end = 1;
                              /* If at load point. abort */
                              if (ctx->sense[1] & SENSE_LOAD) {
                                  ctx->cmd = 0;
                                  ctx->status |= SNS_UNITCHK;
                                  ctx->data_end = 1;
                                  ctx->cmd_done = 1;
                                  break;
                              }
                              /* Do a read start */
                              r = tape_read_back(ctx->tape[ctx->cur_unit]);
                              if (r >= 0) {
                                  add_event(unit, &tape_callback, START_DELAY,
                                           (void *)ctx->tape[ctx->cur_unit], ctx->cur_unit);
                              } else {
                                  ctx->sense[0] = SENSE_INTERV;
                              }
                              break;
                         default:
                              ctx->sense[0] = SENSE_CMDREJ;
                              ctx->sense[2] = SENSE_2;
                              ctx->sense[3] = 0;
                              ctx->sense[4] = 0;
                              ctx->cmd = 0;
                              ctx->data_end = 1;
                              ctx->cmd_done = 1;
                         }
                         break;
                  default:
                         ctx->cmd = 0;
                         ctx->sense[0] = SENSE_CMDREJ;     /* Invalid command */
                         ctx->data_end = 1;
                         ctx->cmd_done = 1;
                         break;
                  }
                  if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0) {
                      ctx->cmd = 0;
                      ctx->sense[0] |= SENSE_BUSCHK;
                      tape_unselect(ctx->tape[ctx->cur_unit]);
                      ctx->data_end = 1;
                      ctx->cmd_done = 1;
                  }
                  if (ctx->cmd != 4 && ((ctx->sense[0] & ~(SENSE_WCZERO)) != 0 ||
                                          (ctx->sense[1] & BIT7) != 0)) {
                      ctx->status |= SNS_UNITCHK;
                      tape_unselect(ctx->tape[ctx->cur_unit]);
                      ctx->cmd = 0;
                      ctx->data_end = 1;
                      ctx->cmd_done = 1;
                  }
                  if (ctx->data_end != 0) {
                      ctx->status |= SNS_CHNEND;
                  }
                  if (ctx->cmd_done != 0) {
                      ctx->status |= SNS_DEVEND;
                  }
              }
              break;

    case STATE_CMD:
             /* Wait for Command out to drop */
             if (ctx->selected) {
                 *tags &= ~(CHAN_SEL_OUT);   /* Clear select in */
                 *tags |= (CHAN_OPR_IN);

                 if ((*tags & (CHAN_CMD_OUT)) != 0) {
                     log_device("tape wait command out drop\n");
                     break;
                 }

                 /* Check if command chaining in effect */
                 if ((*tags & CHAN_SUP_OUT) != 0) {
                     ctx->chain_flag = 1;
                 }

                 /* On MPX channel select out will drop, along with command */
                 if ((*tags == (CHAN_CMD_OUT)) == 0) {
                      *tags |= CHAN_STA_IN;     /* Wait for acceptance of status */
                     /* Return initial status */
                     *bus_in = ctx->status | odd_parity[ctx->status];
                 }

                 /* When we get acknowlegment, go wait for it to go away */
                 if ((*tags & (CHAN_SRV_OUT)) != 0) {
                     *tags &= ~CHAN_STA_IN;
                     ctx->state = STATE_INIT_STAT;
                 }
             }
             break;

    case STATE_INIT_STAT:
             /* If we are slected, drop select out to rest of channel */
             if (ctx->selected) {
                 *tags &= ~(CHAN_SEL_OUT);    /* Clear select in */
                 *tags |= CHAN_OPR_IN;

                 /* Wait for Service out to drop */
                 if ((*tags & (CHAN_SRV_OUT)) == 0) {
                     /* If no command, or check status, go back to idle */
                     if (ctx->cmd == 0 || (ctx->status & (SNS_DEVEND|SNS_UNITCHK|SNS_UNITEXP)) != 0) {
                         *tags &= ~(CHAN_OPR_IN);           /* Clear operation in */
                         ctx->state = STATE_IDLE;
                         ctx->selected = 0;
                         log_device("tape error state done\n");
                         break;
                     }

                     /* If initial status has device end, go idle */
                     if ((ctx->status & SNS_DEVEND) != 0) {
                         ctx->selected = 0;
                         ctx->state = STATE_IDLE;
                         log_device("tape device end\n");
                         break;
                     }

                     /* If initial status has Channel end, go wait for device to finish */
                     if ((ctx->status & SNS_CHNEND) != 0) {
                         *tags &= ~(CHAN_SEL_OUT|CHAN_OPR_IN);  /* Clear operation in */
                         ctx->state = STATE_WAIT;
                         ctx->status &= ~SNS_CHNEND;
                         ctx->selected = 0;
                         log_device("tape channel end\n");
                         break;
                     }

                     /* Wait for device to have some data to transmit */
                     ctx->state = STATE_OPR;
                     log_device("tape state done\n");
                 }
             } else {
                 ctx->state = STATE_OPR;
                 log_device("tape state done\n");
             }
             break;

    case STATE_OPR:
             log_device("tape opr %d r=%d e=%d\n", ctx->selected, ctx->data_rdy, ctx->data_end);

             /* If we are selected clear select in */
             if (ctx->selected) {
                 *tags &= ~(CHAN_SEL_OUT);
                 *tags |= CHAN_OPR_IN;
             }

             /* If we get reselected, return fast busy */
             if (ctx->selected == 0 &&
                *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT) &&
                (bus_out & 0xf8) == ctx->addr) {
                *tags |= CHAN_STA_IN;
                *bus_in = 0x100 | SNS_SMS | SNS_BSY;
                ctx->addressed = 1;
                break;
             }

             /* If we returned fast busy, wait for SERVICE OUT */
             if (ctx->addressed &&
                (*tags & (CHAN_STA_IN|CHAN_SRV_OUT)) == (CHAN_STA_IN|CHAN_SRV_OUT)) {
                *tags &= ~CHAN_STA_IN;
                ctx->addressed = 0;
                break;
             }

             /* On Select channel, Select Out will not drop */
             /* Catch halt I/O */
             if (ctx->selected && (*tags & (CHAN_ADR_OUT)) != 0) {
                  /* Halt I/O */
                  /* Device selected */
                  ctx->data_end = 1;
                  ctx->state = STATE_DATA_END;          /* Return busy status */
                  break;
             }

             /* If sense command and need data, queue it up */
             if (ctx->cmd == 0x4 && ctx->data_rdy == 0 && ctx->data_end == 0) {
                 ctx->data = ctx->sense[ctx->sense_cnt];
                 ctx->sense_cnt++;
                 ctx->data_rdy = 1;
                 if (ctx->sense_cnt == 5)
                     ctx->data_end = 1;
             }

             /* If data ready, try and get/send it */
             if (ctx->data_rdy) {
                 /* If Write and 4th character, just send what we have */
                 if ((ctx->cmd & 1) != 0 && !tape_9_track(ctx->tape[ctx->cur_unit]) &&
                       (ctx->mode7 & MT_CONV) != 0 && ctx->cc == 3) {
                     ctx->data_rdy = 0;
                 } else {
                     ctx->state = (ctx->cmd & 1) ? STATE_DATA_I : STATE_DATA_O;
                 }
                 log_device("tape data transfer\n");
                 break;
             }

             /* If data end, tell CPU */
             if (ctx->data_end) {
                 /* Done after sending status */
                 if (ctx->cmd == 0x4) {
                     log_device("Sense %d:%02x\n", ctx->sense_cnt, ctx->sense[ctx->sense_cnt]);
                     ctx->status |= SNS_CHNEND|SNS_DEVEND;
                 } else /* If read or write command wait for command finish */
                     if ((ctx->cmd & 0x7) != 0x3) {
                         ctx->state = STATE_WAIT;
                         break;
                     }
                 /* If channel end, present it */
                 if ((ctx->status & SNS_CHNEND) != 0) {
                     ctx->state = STATE_DATA_END;
                 }
                 /* If device end present it */
                 if ((ctx->status & SNS_DEVEND) != 0) {
                     ctx->state = STATE_END;
                 }
                 break;
             }

             break;

    case STATE_REQ:   /* Data available and we are not talking on channel */
             log_device("Tape request\n");
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_SUP_OUT|CHAN_REQ_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_REQ_IN)) {
                  if ((bus_out & 0xf8) == ctx->addr && ctx->stk_unit >= 0 &&
                        (bus_out & 0x7) == ctx->stk_unit) {
                      /* Device selected */
                      *tags &= ~(CHAN_SEL_OUT);      /* Clear select out and in */
                      *tags |= CHAN_OPR_IN|CHAN_ADR_IN;
                      ctx->addressed = 1;
                      if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0)
                          ctx->sense[0] |= SENSE_BUSCHK;
                      log_device("tape stack selected\n");
                  } else if ((bus_out & 0xf8) == ctx->addr) {
                      /* Device selected */
                      *tags &= ~(CHAN_SEL_OUT);      /* Clear select out and in */
                      *tags |= CHAN_STA_IN;
                      *bus_in = 0x100 | SNS_SMS | SNS_BSY;
                      if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0)
                          ctx->sense[0] |= SENSE_BUSCHK;
                      ctx->addressed = 1;
                      log_device("Tape stack selected other\n");
                  }
             }

             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_REQ_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_REQ_IN)) {
                 /* Put our address on the bus */
                 *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);        /* Clear select out and in */
                 *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                 *bus_in = ctx->addr | ctx->cur_unit;
                 *bus_in |= odd_parity[*bus_in];              /* Put out our address */
                 ctx->addressed = 1;
                 ctx->request = 0;
                 log_device("Tape reselect\n");
                 break;
             }

             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                 /* Put our address on the bus */
                 *tags &= ~(CHAN_SEL_OUT);                    /* Clear select out and in */
                 *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                 *bus_in = ctx->addr | ctx->cur_unit;
                 *bus_in |= odd_parity[*bus_in];              /* Put out our address */
                 log_device("tape address\n");
                 break;
             }

             /* Put our address on the bus */
             if (ctx->addressed) {
                 /* If we are putting out status, wait for Address out to drop */
                 if ((*tags & (CHAN_STA_IN)) != 0) {
                     if ((*tags & (CHAN_HLD_OUT)) == 0) {
                         ctx->addressed = 0;
                         *tags &= ~(CHAN_STA_IN);
                     } else {
                         *bus_in = 0x100 | SNS_SMS | SNS_BSY;
                     }
                     break;
                  }

                 /* If we got bus, go and transfer */
                 if ((*tags & (CHAN_CMD_OUT)) != 0) {
                     *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN);  /* Clear select out and in */
                     ctx->selected = 1;
                     ctx->addressed = 0;
                     if (ctx->data_end) {
                         ctx->state = (ctx->status & SNS_DEVEND) ? STATE_END : STATE_DATA_END;
                     } else {
                         ctx->state = (ctx->cmd & 1) ? STATE_DATA_I : STATE_DATA_O;
                     }
                     log_device("tape selected\n");
                     break;
                 }

                 *tags &= ~CHAN_SEL_OUT;                   /* Clear select out */
                 *tags |= CHAN_OPR_IN|CHAN_ADR_IN;         /* Put address out */
                 *bus_in = ctx->addr | ctx->cur_unit;
                 *bus_in |= odd_parity[*bus_in];              /* Put out our address */
                 if (ctx->request) {
                     *tags &= ~CHAN_REQ_IN;
                     ctx->request = 0;
                 }
                 break;
              }

              /* See if another device got it. */
              if ((*tags & (CHAN_OPR_IN|CHAN_STA_IN)) != 0) {
                  /* Drop request out until channel free again */
                  if (ctx->request) {
                     *tags &= ~CHAN_REQ_IN;
                     ctx->request = 0;
                  }
                  break;
              }
              /* Put request in up */
              *tags |= CHAN_REQ_IN;
              ctx->request = 1;
              break;

    case STATE_DATA_I:    /* Request data from Channel, wait ready */
              log_device("Tape Data input\n");
              /* If we are not connected, go request bus */
              if (ctx->selected == 0) {
                  ctx->state = STATE_REQ;
                  break;
              }

              *tags |= CHAN_OPR_IN|CHAN_SRV_IN;
              *tags &= ~CHAN_SEL_OUT;

              /* Wait for service out */
              if ((*tags & (CHAN_SRV_OUT)) != 0) {
                   *tags &= ~(CHAN_SEL_OUT|CHAN_SRV_IN);  /* Clear select out and in */
                   /* Device selected */
                   ctx->data_rdy = 0;
                   if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0) {
                       ctx->sense[0] |= SENSE_BUSCHK;
                       ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                       ctx->data_end = 1;
                   } else {
                       ctx->data = (bus_out & 0xff); /* Grab data */
                   }
                   ctx->state = STATE_INIT_STAT;       /* Wait for channel to be idle again */
                   break;
              }

              /* Command out to service in indicates end of data */
              if ((*tags & (CHAN_CMD_OUT)) != 0) {
                   *tags &= ~(CHAN_SEL_OUT|CHAN_SRV_IN);  /* Count reached zero, no more accepted */
                   ctx->status |= SNS_CHNEND;
                   ctx->data_rdy = 0;
                   ctx->data_end = 1;
                   ctx->state = STATE_OPR;        /* Back to operation. */
                   break;
              }
              break;

    case STATE_DATA_O:    /* Request to send data to channel */
              log_device("Tape Data output %02x %d\n", ctx->data, ctx->selected);
              /* If we are not connected, go request bus */
              if (ctx->selected == 0) {
                  ctx->state = STATE_REQ;
                  break;
              }

              *tags |= CHAN_OPR_IN|CHAN_SRV_IN;
              *tags &= ~CHAN_SEL_OUT;
              *bus_in = ctx->data | odd_parity[ctx->data];
              /* Wait for data to be accepted */
              if ((*tags & (CHAN_SRV_OUT)) != 0) {
                   *tags &= ~(CHAN_SEL_OUT|CHAN_SRV_IN);  /* Clear select out and in */
                   ctx->data_rdy = 0;
                   ctx->state = STATE_INIT_STAT;       /* Wait for channel to be idle again */
                   log_device("tape Data sent\n");
              }

              /* CMD out indicates that the channel will accept no more data */
              if ((*tags & (CHAN_CMD_OUT)) != 0) {
                   *tags &= ~(CHAN_SEL_OUT|CHAN_SRV_IN);  /* Count reached zero, no more accepted */
                   ctx->status |= SNS_CHNEND;
                   ctx->data_rdy = 0;
                   ctx->data_end = 1;
                   ctx->state = STATE_OPR;
                   log_device("tape Data End\n");
                   break;
              }
              break;

    case STATE_DATA_END:  /* Wait for IDLE bus */
              ctx->stat_unit = -1;
              /* If we are not connected, go request bus */
              if (ctx->selected == 0) {
                  ctx->state = STATE_REQ;
                  break;
              }

              /* Mark channel still in use */
              *tags &= ~(CHAN_SEL_OUT);  /* Clear select out and in */
              *tags |= CHAN_OPR_IN;

              /* Wait for Service out to drop */
              if ((*tags & (CHAN_SRV_OUT|CHAN_STA_IN)) == (CHAN_SRV_OUT)) {
                  break;
              }

              if ((*tags & (CHAN_CMD_OUT|CHAN_SRV_OUT)) == 0) {
                  *tags |= CHAN_OPR_OUT|CHAN_STA_IN;
                  *bus_in = ctx->status | odd_parity[ctx->status];
                  log_device("Tape End channel status %02x %02x\n", ctx->status, ctx->cmd);
                  break;
              }

              /* Service out indicates status was excepted. If Suppress out, then command chaining */
              if ((*tags & (CHAN_SRV_OUT|CHAN_CMD_OUT)) != 0) {
                   if ((*tags & CHAN_HLD_OUT) == 0) {
                        ctx->selected = 0;
                   }
                   if ((*tags & (CHAN_CMD_OUT)) != 0) {
                       ctx->stk_unit = ctx->cur_unit;
                       ctx->stacked = 1;
                   }
                   *tags &= ~(CHAN_OPR_IN|CHAN_STA_IN);  /* Clear select out and in */
                   if ((*tags & (CHAN_SRV_OUT)) != 0) {
                       ctx->status &= ~(SNS_CHNEND);
                   }
                   ctx->state = STATE_WAIT;            /* Wait for operation to finish */
                   log_device("tape Accepted data_end\n");
                   break;
              }
              break;

    case STATE_END:
             /* If we are not connected, go request bus */
             if (ctx->selected == 0) {
                 ctx->state = STATE_REQ;
                 break;
             }

             /* Set status flags if status pending */
             ctx->stat_unit = -1;
             if ((ctx->status & (SNS_UNITCHK)) != 0) {
                 ctx->stat_unit = ctx->cur_unit;
             }

             /* Mark channel still in use */
             *tags &= ~(CHAN_SEL_OUT);  /* Clear select out and in */
             *tags |= CHAN_OPR_IN;

             /* Wait for Service out to drop */
             if ((*tags & (CHAN_SRV_OUT|CHAN_STA_IN)) == (CHAN_SRV_OUT)) {
                 break;
             }

             *bus_in = ctx->status | odd_parity[ctx->status];

             /* Wait for Service out to drop */
             if ((*tags & (CHAN_SRV_OUT|CHAN_STA_IN)) == 0) {
                 log_device("tape End status %02x %02x %02x\n", ctx->status, ctx->cmd, ctx->sense[0]);
                 *tags |= CHAN_STA_IN;
                 if (ctx->cmd != 0x4 && ctx->sense[0] != 0) {
                     ctx->status |= SNS_UNITCHK;
                 }
                 break;
             }


             /* Service out indicates status was excepted. If Suppress out, then command chaining */
             if ((*tags & (CHAN_SRV_OUT)) != 0) {
                 *tags &= ~(CHAN_OPR_IN|CHAN_STA_IN);  /* Clear select out and in */
                 log_device("Tape Accepted\n");
                 ctx->selected = 0;
                 ctx->cmd = 0;
                 tape_unselect(ctx->tape[ctx->cur_unit]);
                 ctx->state = STATE_IDLE;              /* All done, back to idle state */
                 break;
             }

             /* Command out to status in indicates a stacking of status */
             if ((*tags & (CHAN_CMD_OUT)) != 0) {
                 *tags &= ~(CHAN_OPR_IN|CHAN_STA_IN);  /* Clear select out and in */
                 log_device("Tape Stacked\n");
                 ctx->selected = 0;
                 ctx->stk_unit = ctx->cur_unit;
                 ctx->stacked = 1;
                 ctx->cmd = 0;
                 ctx->state = STATE_STACK;            /* Stack status */
                 break;
             }
             break;

    case STATE_STACK:  /* Stacked status */
             /* Wait until Channels asks for us */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_SUP_OUT) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_REQ_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_REQ_IN|CHAN_SUP_OUT)) {
                  if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0)
                      ctx->sense[0] |= SENSE_BUSCHK;
                  if ((bus_out & 0xf8) == ctx->addr && ctx->stk_unit >= 0 &&
                        (bus_out & 0x7) == ctx->stk_unit) {
                      /* Device selected */
                      *tags &= ~(CHAN_SEL_OUT);      /* Clear select out and in */
                      *tags |= CHAN_OPR_IN;
                      ctx->state = STATE_STACK_SEL;
                      ctx->addressed = 1;
                      log_device("tape stack selected\n");
                  } else if ((bus_out & 0xf8) == ctx->addr) {
                      /* Device selected */
                      *tags &= ~(CHAN_SEL_OUT);      /* Clear select out and in */
                      *tags |= CHAN_STA_IN;
                      *bus_in = 0x100 | SNS_SMS | SNS_BSY;
                      ctx->addressed = 1;
                      log_device("Tape stack selected other\n");
                  }
             }

             /* If stacked and cmd done, try to return status */
             if (ctx->stacked && ctx->cmd_done && *tags == (CHAN_OPR_OUT)) {
                 ctx->state = STATE_REQ;
             }

             /* If selected clear status in when select out drops */
             if (ctx->addressed) {
                /* Device selected */
                *tags &= ~(CHAN_SEL_OUT);          /* Clear status in */
                log_device("tape busy selected\n");

                /* If selected clear status in when select out drops */
                if ((*tags & CHAN_SRV_OUT) != 0) {
                   /* Device selected */
                   *tags &= ~(CHAN_STA_IN);          /* Clear status in */
                   ctx->addressed = 0;
                   log_device("tape busy release %02x done\n", ctx->status);
                }
             }
             break;

    case STATE_STACK_SEL:  /* Stacked status selected */
             /* If we are selected, drop select out to rest of channel */
             if (ctx->addressed || ctx->selected) {
                 *tags &= ~(CHAN_SEL_OUT);  /* Clear select in */
                 *tags |= CHAN_OPR_IN;
             }

             /* When address out drops put our address on bus in */
             if (ctx->addressed && (*tags & (CHAN_ADR_OUT|CHAN_CMD_OUT)) == 0) {
                 *tags |= CHAN_ADR_IN;      /* Return address until accepted */
                 *bus_in = (ctx->addr & 0xf8) | ctx->stk_unit;
                 *bus_in |= odd_parity[*bus_in];
                 ctx->addressed = 0;
                 ctx->selected = 1;         /* Mark us as selected */
                 log_device("tape stacked address\n");
                 break;
             }

             /* Wait for Command out to raise, give our address */
             if (ctx->selected) {
                 *bus_in = (ctx->addr & 0xf8) | ctx->stk_unit;
                 *bus_in |= odd_parity[*bus_in];

                 /* Can now drop address in */
                 if ((*tags & (CHAN_CMD_OUT)) != 0) {
                      log_device("test stack command %02x\n", bus_out);
                      ctx->state = STATE_STACK_CMD;
                      *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN);                 /* Clear address in */
                      if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0) {
                          ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                          ctx->sense[0] |= SENSE_BUSCHK;
                      }
                 }
             }
             break;

    case STATE_STACK_CMD:
             /* If we are selected, drop select out to rest of channel */
             *bus_in = ctx->status | odd_parity[ctx->status];   /* Set initial status */
             *tags &= ~(CHAN_SEL_OUT);  /* Clear select in */
             *tags |= CHAN_OPR_IN;

             /* Wait for Command out to drop */
             /* On MPX channel select out will drop, along with command */
             if ((*tags == (CHAN_CMD_OUT)) == 0) {
                  *tags |= CHAN_OPR_IN|CHAN_STA_IN;     /* Wait for acceptance of status */
                  ctx->state = STATE_STACK_STA;
                  log_device("test stack init stat %02x\n", ctx->status);
             }
             break;

    case STATE_STACK_STA:
             *bus_in = ctx->status | odd_parity[ctx->status];   /* Set initial status */
             *tags &= ~(CHAN_SEL_OUT);  /* Clear select in */
             *tags |= CHAN_OPR_IN;

             /* Wait for Service out to raise, status accepted */
             if ((*tags & (CHAN_SRV_OUT|CHAN_CMD_OUT)) != 0) {
                 *tags &= ~(CHAN_STA_IN|CHAN_SEL_OUT);
                 if ((*tags & (CHAN_CMD_OUT)) != 0) {
                     ctx->stacked = 1;
                 }
                 if ((*tags & (CHAN_SRV_OUT)) != 0) {
                     ctx->stacked = 0;
                     ctx->status &= ~SNS_CHNEND;
                 }
                 ctx->state = STATE_STACK_HLD;
                 log_device("tape stack init stat %02x done\n", ctx->status);
                 break;
             }
             break;

    case STATE_STACK_HLD:
             /* If we are selected, drop select out to rest of channel */
             *tags &= ~(CHAN_SEL_OUT);  /* Clear select in */
             *tags |= CHAN_OPR_IN;

             /* Wait for Service/command out to drop */
             if ((*tags & (CHAN_CMD_OUT|CHAN_SRV_OUT)) == 0) {
                 if (ctx->stacked) {
                     if (ctx->cmd_done) {
                        ctx->state = STATE_STACK;
                     } else {
                        ctx->state = STATE_WAIT;
                     }
                 } else {
                     ctx->state = STATE_IDLE;
                 }
                 *tags &= ~CHAN_OPR_IN;
                 ctx->selected = 0;
                 log_device("tape stack done\n");
             }
             break;

    case STATE_WAIT:
             /* If we get select out with address out, reselection */
             if (!ctx->selected &&
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT) &&
                (bus_out & 0xf8) == ctx->addr && (bus_out & 0x7) < ctx->nunits) {
                *tags &= ~CHAN_SEL_OUT;
                if (ctx->stk_unit >= 0 && (bus_out & 0x7) == ctx->stk_unit && ctx->stacked) {
                    ctx->state = STATE_STACK;
                    break;
                }
                /* Device selected */
                *tags |= CHAN_STA_IN;
                *bus_in = 0x100 | SNS_SMS | SNS_BSY;
                ctx->addressed = 1;
                log_device("Tape wait select attempt %d %03x\n", ctx->cur_unit, *bus_in);
             }

             /* If selected clear status in when select out drops */
             if (ctx->addressed) {
                /* Device selected */
                *tags &= ~(CHAN_SEL_OUT);          /* Clear status in */
                *tags |= CHAN_STA_IN;
                *bus_in = 0x100 | SNS_SMS | SNS_BSY;

                /* If selected clear status in when select out drops */
                if ((*tags & CHAN_HLD_OUT) == 0) {
                   /* Device selected */
                   *tags &= ~(CHAN_STA_IN);          /* Clear status in */
                   ctx->addressed = 0;
                   log_device("tape status received\n");
                }
             }

             /* If command done, signal it */
             if (ctx->cmd_done) {
                 log_device("Tape command done %d\n", ctx->selected);
                 ctx->state = STATE_END;
             }
             break;

    case STATE_RDY:
            /* Wait until Channels asks for us */
            if ((*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_REQ_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_SUP_OUT|CHAN_REQ_IN)) &&
                 (bus_out & 0xf8) == ctx->addr && (bus_out & 0x7) < ctx->nunits) {
                 /* Device selected */
                 if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0)
                     ctx->sense[0] |= SENSE_BUSCHK;
                 ctx->cur_unit = bus_out & 0x7;
                 *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);       /* Clear select out and in */
                 ctx->addressed = 1;
                 if ((ctx->rdy_flags & (1 << ctx->cur_unit)) == 0) {
                     *bus_in = 0x100 | SNS_SMS | SNS_BSY;
                     *tags |= CHAN_STA_IN;
                     break;
                 }
                 ctx->status = SNS_DEVEND;
                 *tags |= CHAN_OPR_IN;
 log_device("pending tape selected unit: %d\n", ctx->cur_unit);
             }

             if (ctx->addressed && (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                   *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN))) {
                 if ((ctx->rdy_flags & (1 << ctx->cur_unit)) == 0) {
                     ctx->state = STATE_IDLE;
                  } else {
                      ctx->rdy_flags &= ~(1 << ctx->cur_unit);
                      ctx->status = SNS_DEVEND;
                      ctx->state = STATE_END;              /* Go wait for everything to drop */
                  }
 log_device("selected unit pend device end: %d\n", ctx->cur_unit);
             }

             /* If we got selected in error */
             if (ctx->addressed && (*tags == (CHAN_OPR_OUT|CHAN_STA_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_SUP_OUT|CHAN_STA_IN))) {
                 ctx->addressed = 0;
                 ctx->status = 0;
                 if (ctx->rdy_flags == 0) {
                     ctx->state = STATE_IDLE;
                 }
                 *tags &= ~(CHAN_SEL_OUT);      /* Clear select out and in */
 log_device("pending tape deselected unit: %d\n", ctx->cur_unit);
             }

             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_REQ_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_REQ_IN)) {
                 /* Put our address on the bus */
                 i = ctx->t_scan;
                 do {
                    if (ctx->rdy_flags & (1 << i)) {
                        ctx->cur_unit = i;
                        break;
                    }
                    if (++i == 8)
                       i = 0;
                 } while (i != ctx->t_scan);
                 ctx->t_scan = i;
                 ctx->status = SNS_DEVEND;
                 ctx->request = 0;
                 *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);        /* Clear select out and in */
                 *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                 *bus_in = ctx->addr | ctx->cur_unit;
                 *bus_in |= odd_parity[*bus_in];              /* Put out our address */
log_device("Tape Reselect\n");
                 break;
             }

             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                 /* Put our address on the bus */
                 *tags &= ~(CHAN_SEL_OUT);                    /* Clear select out and in */
                 *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                 *bus_in = ctx->addr | ctx->cur_unit;
                 *bus_in |= odd_parity[*bus_in];              /* Put out our address */
                 log_device("Tape Address\n");
                 break;
             }

             /* If we got bus, go and transfer */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                 *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN);       /* Clear select out and in */
                 ctx->selected = 1;
                 ctx->rdy_flags &= ~(1 << ctx->cur_unit);
                 ctx->state = STATE_END;              /* Go wait for everything to drop */
                 log_device("Tape selected\n");
                 break;
              }

              /* See if another device got it. */
              if (ctx->selected == 0 && (*tags & (CHAN_OPR_IN|CHAN_STA_IN)) != 0) {
                  log_device("Tape ready other device\n");
                  /* Drop request out until channel free again */
                  if (ctx->request) {
                     *tags &= ~CHAN_REQ_IN;
                     ctx->request = 0;
                  }
                  break;
              }
              /* Put request in up */
             if (ctx->addressed == 0) {
                 log_device("Tape request ready\n");
                 *tags |= CHAN_REQ_IN;
                 ctx->request = 1;
             }
             break;
    }
}

SDL_Texture *model2415_img = NULL;
SDL_Texture *tape_images_img = NULL;
static int supply_color[8] = {1, 1, 1, 1, 1, 1, 1, 1};
static int takeup_color[8] = {2, 2, 2, 2, 2, 2, 2, 2};
static int supply_label[8] = {1, 1, 1, 1, 1, 1, 1, 1};
static int takeup_label[8] = {0, 0, 0, 0, 0, 0, 0, 0};

void
model2415_draw(struct _device *unit, void *rend)
{
    struct _2415_context *ctx = (struct _2415_context *)unit->dev;
    SDL_Renderer *render = (SDL_Renderer *)rend;
    struct _tape_image   *reel;
    SDL_Rect     rect;
    SDL_Rect     rect2;
    SDL_Surface *text;
    SDL_Texture *txt;
    int          i, j, k;
    int          t1, t2;
    int          x;
    int          y;
    char         buf[100];
    float        p;

    if (model2415_img == NULL) {
        SDL_Surface *text;

        text = IMG_ReadXPMFromArray(model2415_xpm);
        model2415_img = SDL_CreateTextureFromSurface(render, text);
        SDL_SetTextureBlendMode(model2415_img, SDL_BLENDMODE_BLEND);
        SDL_FreeSurface(text);
        text = IMG_ReadXPMFromArray(tape_images_xpm);
        tape_images_img = SDL_CreateTextureFromSurface(render, text);
        SDL_SetTextureBlendMode(tape_images_img, SDL_BLENDMODE_BLEND);
        SDL_FreeSurface(text);
    }

    for (i = 0; i < unit->n_units; i++) {
        x = unit->rect[i].x;
        y = unit->rect[i].y;

        if (ctx->tape[i]->file_name != NULL) {
            rect2.x = 0;
            rect2.y = 0;
            rect2.w = unit->rect[i].w;
            rect2.h = unit->rect[i].h;
            rect.x = x;
            rect.y = y;
            rect.w = unit->rect[i].w;
            rect.h = unit->rect[i].h;
            SDL_RenderCopy(render, model2415_img, &rect2, &rect);
            sprintf(buf, "%1X%02X", ctx->chan, ctx->addr + i);
            text = TTF_RenderText_Solid(font14, buf, c1);
            txt = SDL_CreateTextureFromSurface(render, text);
            SDL_FreeSurface(text);
            SDL_QueryTexture(txt, &t1, &t2, &rect2.w, &rect2.h);
            rect2.x = x + 52;
            rect2.y = y + 20;
            SDL_RenderCopy(render, txt, NULL, &rect2);
            SDL_DestroyTexture(txt);

            /* Do indicators */
            rect2.x = x + 85;
            rect2.y = y + 14;
            rect2.w = 9;
            rect2.h = 7;
            rect.x = 17;
            rect.y = 230;
            rect.w = 9;
            rect.h = 7;
            /* Draw select */
            if (tape_is_selected(ctx->tape[i])) {
                SDL_RenderCopy(render, model2415_img, &rect, &rect2);
            }
            rect.x += rect.w;
            rect2.x += rect.w;
            /* Draw ready */
            if (tape_ready(ctx->tape[i])) {
                SDL_RenderCopy(render, model2415_img, &rect, &rect2);
            }
            rect.x += rect.w;
            rect2.x += rect.w;
            /* Draw file protect */
            if (tape_ring(ctx->tape[i]) == 0) {
               SDL_RenderCopy(render, model2415_img, &rect, &rect2);
            }
            /* Draw supply reel */
            reel = tape_supply_image(ctx->tape[i], &j);
            rect2.x = x + 34;
            rect2.y = y + 131;
            rect2.w = 69;
            rect2.h = 69;
            rect.x = reel->x+4;
            rect.y = reel->y+4;
            rect.w = 69;
            rect.h = 69;
            SDL_RenderCopy(render, tape_images_img, &rect, &rect2);
            /* Draw tape to vacuum column */
            SDL_SetRenderDrawColor(render, 0x37, 0x37, 0x37, 255);
            SDL_RenderDrawLine(render, x+32, y+127,
                    x + (69 - reel->radius), y + 164 + (reel->radius / 2));
            SDL_RenderDrawLine(render, x+32, y+99, x + 119, y + 77);
            SDL_RenderDrawLine(render, x+177, y+99, x + 167, y + 85);
            /* Overlay reel */
            j = 35 - j;
            switch (supply_color[i]) {
            case 0: rect.x = (75 * 15) + 4; break;
            case 1: rect.x = (75 * 3) + 4; break;
            case 2: rect.x = (75 * 7) + 4; break;
            }
            if (j > 17) {
                rect.x += 75;
                rect.y = (75 * (j - 18)) + 4;
            } else {
                rect.y = (75 * (j)) + 4;
            }
            SDL_RenderCopy(render, tape_images_img, &rect, &rect2);
            if (supply_label[i]) {
                if (rect.x > 1125)
                    rect.x -= (75 * 2);
                else
                    rect.x += (75 * 2);
                SDL_RenderCopy(render, tape_images_img, &rect, &rect2);
            }
            /* Draw take up reel */
            reel = tape_takeup_image(ctx->tape[i], &k);
            rect2.x = x + 107;
            rect.x = reel->x+4;
            rect.y = reel->y+4;
            SDL_RenderCopy(render, tape_images_img, &rect, &rect2);
            switch (takeup_color[i]) {
            case 0: rect.x = (75 * 15) + 4; break;
            case 1: rect.x = (75 * 3) + 4; break;
            case 2: rect.x = (75 * 7) + 4; break;
            }
            if (k > 17) {
                rect.x += 75;
                rect.y = (75 * (k - 18) + 4);
            } else {
                rect.y = (75 * (k)) + 4;
            }
            SDL_RenderDrawLine(render, x + 177, y + 127,
                 x + (141 + reel->radius), y + 164 + (reel->radius / 2));
            SDL_RenderCopy(render, tape_images_img, &rect, &rect2);
            if (takeup_label[i]) {
                if (rect.x > 1125)
                    rect.x -= (75 * 2);
                else
                    rect.x += (75 * 2);
                SDL_RenderCopy(render, tape_images_img, &rect, &rect2);
            }
        } else {
            rect2.x = 250;
            rect2.y = 0;
            rect2.w = unit->rect[i].w;
            rect2.h = unit->rect[i].h;
            rect.x = x;
            rect.y = y;
            rect.w = unit->rect[i].w;
            rect.h = unit->rect[i].h;
            SDL_RenderCopy(render, model2415_img, &rect2, &rect);
            sprintf(buf, "%1X%02X", ctx->chan, ctx->addr + i);
            text = TTF_RenderText_Solid(font14, buf, c1);
            txt = SDL_CreateTextureFromSurface(render, text);
            SDL_FreeSurface(text);
            SDL_QueryTexture(txt, &t1, &t2, &rect2.w, &rect2.h);
            rect2.x = x + 52;
            rect2.y = y + 20;
            SDL_RenderCopy(render, txt, NULL, &rect2);
            SDL_DestroyTexture(txt);
            /* Draw supply reel */
            rect2.x = x + 34;
            rect2.y = y + 131;
            rect2.w = 69;
            rect2.h = 69;
            rect.x = 4;
            rect.y = 4;
            rect.w = 69;
            rect.h = 69;
            SDL_RenderCopy(render, tape_images_img, &rect, &rect2);
            /* Draw take up reel */
            rect2.x = x + 107;
            SDL_RenderCopy(render, tape_images_img, &rect, &rect2);
            switch (takeup_color[i]) {
            case 0: rect.x = (75 * 15) + 4; break;
            case 1: rect.x = (75 * 3) + 4; break;
            case 2: rect.x = (75 * 7) + 4; break;
            }
            rect.y = 4;
            SDL_RenderCopy(render, tape_images_img, &rect, &rect2);
            if (takeup_label[i]) {
                if (rect.x > 1125)
                    rect.x -= (75 * 2);
                else
                    rect.x += (75 * 2);
                SDL_RenderCopy(render, tape_images_img, &rect, &rect2);
            }
            /* Draw empty hub */
            rect2.x = x + 34 + 15;
            rect2.y = y + 131 + 15;
            rect2.w = 40;
            rect2.h = 40;
            rect.x = 4 + 15;
            rect.y = 4 + 15;
            rect.w = 40;
            rect.h = 40;
            rect.x = (75 * 15) + 4 + 15;
            rect.y = 4 + 15;
            rect.w = 40;
            rect.h = 40;
            SDL_RenderCopy(render, tape_images_img, &rect, &rect2);
        }
    }
}

static void model2415_update(struct _popup *popup, void *device, int index)
{
    struct _device *unit = (struct _device *)device;
    struct _2415_context *ctx = (struct _2415_context *)unit->dev;

fprintf (stderr, "Tape key %d\n", index);
    switch (index) {
    case 0:  /* Load Rewind */
          if ((ctx->tape[popup->unit_num]->format & ONLINE) == 0) {
              if (ctx->tape[popup->unit_num]->file_name == NULL ||
                  strcmp(ctx->tape[popup->unit_num]->file_name, popup->text[0].text) != 0) {
                  tape_detach(ctx->tape[popup->unit_num]);
                  tape_attach(ctx->tape[popup->unit_num],
                                  popup->text[0].text, popup->temp[0],
                                  popup->temp[3], popup->temp[1]);
              }
              tape_start_rewind(ctx->tape[popup->unit_num]);
              if (ctx->rew_delay == 0)
                  ctx->rew_delay = REWIND_DELAY;
              ctx->rew_flags |= 1 << popup->unit_num;
              add_event(unit, rewind_callback, REWIND_DELAY,
                        (void *)ctx->tape[popup->unit_num], popup->unit_num);
          }
          break;
    case 1:  /* Start */
          if ((ctx->tape[popup->unit_num]->format & ONLINE) == 0 &&
              (ctx->tape[popup->unit_num]->fd >= 0)) {
              ctx->tape[popup->unit_num]->format |= ONLINE;
              ctx->rdy_flags |= 1 << popup->unit_num;
          }
          break;
    case 2:  /* Unload */
          if ((ctx->tape[popup->unit_num]->format & ONLINE) == 0) {
              tape_start_rewind(ctx->tape[popup->unit_num]);
              if (ctx->rew_delay == 0)
                  ctx->rew_delay = REWIND_DELAY;
              ctx->rew_flags |= 1 << popup->unit_num;
              ctx->run_flags |= 1 << popup->unit_num;
              add_event(unit, rewind_callback, REWIND_DELAY,
                        (void *)ctx->tape[popup->unit_num], popup->unit_num);
          }
          break;
    case 3:  /* Reset */
          ctx->tape[popup->unit_num]->format &= ~ONLINE;
          break;

    case 4:  /* end */
          ctx->tape[popup->unit_num]->pos_frame = max_tape_length - 1;
          break;
    }
}

extern int textpos(struct _text *text, int pos);

static struct _l {
     char        *top;
     char        *bot;
     int         ind;
     int         x;
     int         y;
     SDL_Color   col_t;
     SDL_Color   col_on;
     SDL_Color   col_off;
} labels[] = {
     {"SELECT", NULL,    1, 0, 0, {0, 0, 0}, {0x96, 0x8f, 0x85}, {0xfd, 0xfd, 0xfd}},  /* White */
     {"READY", NULL,     1, 1, 0, {0xff, 0xff, 0xff}, {0x7f,0xc0, 0x86}, {0x0c, 0x2e, 0x30}}, /* Green */
     {"FILE", "PROTECT", 1, 2, 0, {0xff, 0xff, 0xff}, {0xd0, 0x08, 0x42}, {0xff, 0x00, 0x4a}},  /* Red */
     {"TAPE", "INDICATOR", 1, 3, 0, {0, 0, 0}, {0xff, 0xfd, 0x5e}, {0xdd, 0xdc, 0x8a}}, /* White */
     {"LOAD", "REWIND",   0, 0, 1, {0xff, 0xff, 0xff}, {0x0a, 0x52, 0x9a}, {0, 0, 0}}, /* Blue */
     {"START", NULL,      0, 1, 1, {0xff, 0xff, 0xff}, {0x0c, 0x2e, 0x30}, {0, 0, 0}}, /* Green */
     {"UNLOAD", "REWIND", 0, 2, 1, {0xff, 0xff, 0xff}, {0x0a, 0x52, 0x9a}, {0, 0, 0}}, /* Blue */
     {"RESET", NULL,      0, 3, 1, {0xff, 0xff, 0xff}, {0xc8, 0x3a, 0x30}, {0, 0, 0}}, /* Blue */
     {"EOM", NULL,         0, 0, 4, {0xff, 0xff, 0xff}, {0x0a, 0x052, 0x9a}, {0, 0, 0,}},
     { NULL, NULL, 0}
};

static char *format_type[] = { "SIMH", "E11", "P7B", NULL };
static char *density_type[] = { "1600", "800", NULL };
static char *tracks[] = { "9 track", "7 track", NULL };
static char *ring_mode[] = { "Ring", "No Ring", NULL };
static char *reel_color[] = { "Clear", "Red", "Blue" };
static char *label_mode[] = { "No", "Yes", NULL };

//static SDL_Renderer *render;
struct _popup *
model2415_control(struct _device *unit, int hd, int wd, int u)
{
    struct _popup  *popup;
    struct _2415_context *ctx = (struct _2415_context *)unit->dev;
    SDL_Surface *text;
    int    i, j;
    int    w, h;
    char   buffer[100];

    if ((popup = (struct _popup *)calloc(1, sizeof(struct _popup))) == NULL)
        return popup;
    sprintf(buffer, "IBM2415 Dev 0x'%03X'", ctx->addr + u);
    popup->screen = SDL_CreateWindow(buffer, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        800, 200, SDL_WINDOW_RESIZABLE );
    popup->render = SDL_CreateRenderer( popup->screen, -1, SDL_RENDERER_ACCELERATED);
    popup->device = unit;
    popup->unit_num = u;
    popup->areas[popup->area_ptr].rect.x = 0;
    popup->areas[popup->area_ptr].rect.y = 0;
    popup->areas[popup->area_ptr].rect.h = 200;
    popup->areas[popup->area_ptr].rect.w = 800;
    popup->areas[popup->area_ptr].c = &c;
    popup->area_ptr++;
    for (i = 0; labels[i].top != NULL; i++) {
        if (labels[i].ind) {
            popup->ind[popup->ind_ptr].lab = labels[i].top;
            popup->ind[popup->ind_ptr].c[0] = &labels[i].col_off;
            popup->ind[popup->ind_ptr].c[1] = &labels[i].col_on;
            popup->ind[popup->ind_ptr].ct = &labels[i].col_t;
            text = TTF_RenderText_Solid(font1, labels[i].top, labels[i].col_t);
            popup->ind[popup->ind_ptr].top = SDL_CreateTextureFromSurface( popup->render, text);
            popup->ind[popup->ind_ptr].top_len = strlen(labels[i].top);
            SDL_FreeSurface(text);
            if (labels[i].bot != NULL) {
                text = TTF_RenderText_Solid(font1, labels[i].bot, labels[i].col_t);
                popup->ind[popup->ind_ptr].bot = SDL_CreateTextureFromSurface( popup->render, text);
                popup->ind[popup->ind_ptr].bot_len = strlen(labels[i].bot);
                SDL_FreeSurface(text);
            }
            popup->ind[popup->ind_ptr].rect.x = 20 + ((12* wd) * labels[i].x);
            popup->ind[popup->ind_ptr].rect.y = 20 + ((3 *hd) * labels[i].y);
            popup->ind[popup->ind_ptr].rect.h = 2 * hd;
            popup->ind[popup->ind_ptr].rect.w = 10 * wd;
            popup->ind_ptr++;
        } else {
            popup->sws[popup->sws_ptr].lab = labels[i].top;
            popup->sws[popup->sws_ptr].c[0] = &labels[i].col_on;
            text = TTF_RenderText_Solid(font1, labels[i].top, labels[i].col_t);
            popup->sws[popup->sws_ptr].top = SDL_CreateTextureFromSurface( popup->render, text);
            popup->sws[popup->sws_ptr].top_len = strlen(labels[i].top);
            SDL_FreeSurface(text);
            if (labels[i].bot != NULL) {
                text = TTF_RenderText_Solid(font1, labels[i].bot, labels[i].col_t);
                popup->sws[popup->sws_ptr].bot = SDL_CreateTextureFromSurface( popup->render, text);
                popup->sws[popup->sws_ptr].bot_len = strlen(labels[i].bot);
                SDL_FreeSurface(text);
            }
            popup->sws[popup->sws_ptr].rect.x = 20 + ((12 *wd) * labels[i].x);
            popup->sws[popup->sws_ptr].rect.y = 20 + ((3 *hd) * labels[i].y);
            popup->sws[popup->sws_ptr].rect.h = 2 * hd;
            popup->sws[popup->sws_ptr].rect.w = 10 * wd;
            popup->sws_ptr++;
        }
    }

    popup->ind[0].value = &ctx->tape[u]->format;  /* Select */
    popup->ind[0].shift = 8;
    popup->ind[1].value = &ctx->tape[u]->format;  /* Ready */
    popup->ind[1].shift = 9;
    popup->ind[2].value = &ctx->tape[u]->format;  /* File protect */
    popup->ind[2].shift = 2;
    popup->ind[3].value = &ctx->tape[u]->format;  /* Tape indicator */
    popup->ind[3].shift = 3;

    text = TTF_RenderText_Solid(font14, "Tape: ", c1);

    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 25 + (12 * wd) * 4;
    popup->ctl_label[popup->ctl_ptr].rect.y = 20;
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;
    popup->text[popup->txt_ptr].rect.x = 25 + (12 * wd) * 5;
    popup->text[popup->txt_ptr].rect.y = 20;
    popup->text[popup->txt_ptr].rect.w = 45 * wd;
    popup->text[popup->txt_ptr].rect.h = h + 5;
    if (ctx->tape[u]->file_name != NULL)
        strncpy(popup->text[popup->txt_ptr].text, ctx->tape[u]->file_name, 256);
    else
        popup->text[popup->txt_ptr].text[0] = '\0';
    popup->text[popup->txt_ptr].len = strlen(popup->text[popup->txt_ptr].text);
    popup->text[popup->txt_ptr].pos = popup->text[popup->txt_ptr].len;
    popup->text[popup->txt_ptr].cpos = textpos(&popup->text[popup->txt_ptr],
                                        popup->text[popup->txt_ptr].pos);
    popup->txt_ptr++;

    text = TTF_RenderText_Solid(font14, "Type: ", c1);

    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 25 + (12 * wd) * 4;
    popup->ctl_label[popup->ctl_ptr].rect.y = 20 + (2 * hd);
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;
    popup->combo[popup->cmb_ptr].rect.x = 25 + (12 * wd) * 5;
    popup->combo[popup->cmb_ptr].rect.y = 20 + (2 * hd);
    popup->combo[popup->cmb_ptr].rect.w = 16 * wd;
    popup->combo[popup->cmb_ptr].rect.h = h;
    popup->combo[popup->cmb_ptr].urect.x = popup->combo[popup->cmb_ptr].rect.x;
    popup->combo[popup->cmb_ptr].urect.y = popup->combo[popup->cmb_ptr].rect.y;
    popup->combo[popup->cmb_ptr].urect.w = 2 * wd;
    popup->combo[popup->cmb_ptr].urect.h = h;
    popup->combo[popup->cmb_ptr].drect.x = popup->combo[popup->cmb_ptr].rect.x + (14 * wd) - 1;
    popup->combo[popup->cmb_ptr].drect.y = popup->combo[popup->cmb_ptr].rect.y;
    popup->combo[popup->cmb_ptr].drect.w = 2 * wd;
    popup->combo[popup->cmb_ptr].drect.h = h;
    for (i = 0; format_type[i] != NULL; i++) {
        text = TTF_RenderText_Solid(font14, format_type[i], c1);
        popup->combo[popup->cmb_ptr].label[i] = SDL_CreateTextureFromSurface( popup->render,
                                      text);
        SDL_QueryTexture(popup->combo[popup->cmb_ptr].label[i], NULL, NULL,
             &popup->combo[popup->cmb_ptr].lw[i], &popup->combo[popup->cmb_ptr].lh[i]);
    }
    popup->temp[0] = ctx->tape[u]->format & TAPE_FMT;
    popup->combo[popup->cmb_ptr].num = popup->temp[0];
    popup->combo[popup->cmb_ptr].value = &popup->temp[0];
    popup->combo[popup->cmb_ptr].max = i - 1;
    popup->cmb_ptr++;

    text = TTF_RenderText_Solid(font14, "Density: ", c1);

    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 25 + (12 * wd) * 4;
    popup->ctl_label[popup->ctl_ptr].rect.y = 20 + (4 * hd);
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;
    popup->combo[popup->cmb_ptr].rect.x = 25 + (12 * wd) * 5;
    popup->combo[popup->cmb_ptr].rect.y = 20 + (4 * hd);
    popup->combo[popup->cmb_ptr].rect.w = 16 * wd;
    popup->combo[popup->cmb_ptr].rect.h = h;
    popup->combo[popup->cmb_ptr].urect.x = popup->combo[popup->cmb_ptr].rect.x;
    popup->combo[popup->cmb_ptr].urect.y = popup->combo[popup->cmb_ptr].rect.y;
    popup->combo[popup->cmb_ptr].urect.w = 2 * wd;
    popup->combo[popup->cmb_ptr].urect.h = h;
    popup->combo[popup->cmb_ptr].drect.x = popup->combo[popup->cmb_ptr].rect.x + (14 * wd) - 1;
    popup->combo[popup->cmb_ptr].drect.y = popup->combo[popup->cmb_ptr].rect.y;
    popup->combo[popup->cmb_ptr].drect.w = 2 * wd;
    popup->combo[popup->cmb_ptr].drect.h = h;
    for (i = 0; density_type[i] != NULL; i++) {
        text = TTF_RenderText_Solid(font14, density_type[i], c1);
        popup->combo[popup->cmb_ptr].label[i] = SDL_CreateTextureFromSurface( popup->render,
                                      text);
        SDL_QueryTexture(popup->combo[popup->cmb_ptr].label[i], NULL, NULL,
             &popup->combo[popup->cmb_ptr].lw[i], &popup->combo[popup->cmb_ptr].lh[i]);
    }
    popup->temp[1] = (ctx->tape[u]->format & DEN_MASK) == DEN_800;
    popup->combo[popup->cmb_ptr].num = popup->temp[1];
    popup->combo[popup->cmb_ptr].value = &popup->temp[1];
    popup->combo[popup->cmb_ptr].max = i - 1;
    popup->cmb_ptr++;

    text = TTF_RenderText_Solid(font14, "Tracks: ", c1);

    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 25 + (12 * wd) * 4;
    popup->ctl_label[popup->ctl_ptr].rect.y = 20 + (6 * hd);
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;
    popup->combo[popup->cmb_ptr].rect.x = 25 + (12 * wd) * 5;
    popup->combo[popup->cmb_ptr].rect.y = 20 + (6 * hd);
    popup->combo[popup->cmb_ptr].rect.w = 16 * wd;
    popup->combo[popup->cmb_ptr].rect.h = h;
    popup->combo[popup->cmb_ptr].urect.x = popup->combo[popup->cmb_ptr].rect.x;
    popup->combo[popup->cmb_ptr].urect.y = popup->combo[popup->cmb_ptr].rect.y;
    popup->combo[popup->cmb_ptr].urect.w = 2 * wd;
    popup->combo[popup->cmb_ptr].urect.h = h;
    popup->combo[popup->cmb_ptr].drect.x = popup->combo[popup->cmb_ptr].rect.x + (14 * wd) - 1;
    popup->combo[popup->cmb_ptr].drect.y = popup->combo[popup->cmb_ptr].rect.y;
    popup->combo[popup->cmb_ptr].drect.w = 2 * wd;
    popup->combo[popup->cmb_ptr].drect.h = h;
    for (i = 0; tracks[i] != NULL; i++) {
        text = TTF_RenderText_Solid(font14, tracks[i], c1);
        popup->combo[popup->cmb_ptr].label[i] = SDL_CreateTextureFromSurface( popup->render,
                                      text);
        SDL_QueryTexture(popup->combo[popup->cmb_ptr].label[i], NULL, NULL,
             &popup->combo[popup->cmb_ptr].lw[i], &popup->combo[popup->cmb_ptr].lh[i]);
    }
    popup->temp[2] = (ctx->tape[u]->format & TRACK9) == 0;
    popup->combo[popup->cmb_ptr].num = popup->temp[2];
    popup->combo[popup->cmb_ptr].value = &popup->temp[2];
    popup->combo[popup->cmb_ptr].max = i - 1;
    popup->cmb_ptr++;

    text = TTF_RenderText_Solid(font14, "Write: ", c1);

    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 25 + (12 * wd) * 4;
    popup->ctl_label[popup->ctl_ptr].rect.y = 20 + (8 * hd);
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;
    popup->combo[popup->cmb_ptr].rect.x = 25 + (12 * wd) * 5;
    popup->combo[popup->cmb_ptr].rect.y = 20 + (8 * hd);
    popup->combo[popup->cmb_ptr].rect.w = 16 * wd;
    popup->combo[popup->cmb_ptr].rect.h = h;
    popup->combo[popup->cmb_ptr].urect.x = popup->combo[popup->cmb_ptr].rect.x;
    popup->combo[popup->cmb_ptr].urect.y = popup->combo[popup->cmb_ptr].rect.y;
    popup->combo[popup->cmb_ptr].urect.w = 2 * wd;
    popup->combo[popup->cmb_ptr].urect.h = h;
    popup->combo[popup->cmb_ptr].drect.x = popup->combo[popup->cmb_ptr].rect.x + (14 * wd) - 1;
    popup->combo[popup->cmb_ptr].drect.y = popup->combo[popup->cmb_ptr].rect.y;
    popup->combo[popup->cmb_ptr].drect.w = 2 * wd;
    popup->combo[popup->cmb_ptr].drect.h = h;
    for (i = 0; ring_mode[i] != NULL; i++) {
        text = TTF_RenderText_Solid(font14, ring_mode[i], c1);
        popup->combo[popup->cmb_ptr].label[i] = SDL_CreateTextureFromSurface( popup->render,
                                      text);
        SDL_QueryTexture(popup->combo[popup->cmb_ptr].label[i], NULL, NULL,
             &popup->combo[popup->cmb_ptr].lw[i], &popup->combo[popup->cmb_ptr].lh[i]);
    }
    popup->temp[3] = (ctx->tape[u]->format & WRITE_RING) == 0;
    popup->combo[popup->cmb_ptr].num = popup->temp[3];
    popup->combo[popup->cmb_ptr].value = &popup->temp[3];
    popup->combo[popup->cmb_ptr].max = i - 1;
    popup->cmb_ptr++;
    text = TTF_RenderText_Solid(font14, "Color: ", c1);

    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 25 + (12 * wd) * 4;
    popup->ctl_label[popup->ctl_ptr].rect.y = 20 + (10 * hd);
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;
    popup->combo[popup->cmb_ptr].rect.x = 25 + (12 * wd) * 5;
    popup->combo[popup->cmb_ptr].rect.y = 20 + (10 * hd);
    popup->combo[popup->cmb_ptr].rect.w = 16 * wd;
    popup->combo[popup->cmb_ptr].rect.h = h;
    popup->combo[popup->cmb_ptr].urect.x = popup->combo[popup->cmb_ptr].rect.x;
    popup->combo[popup->cmb_ptr].urect.y = popup->combo[popup->cmb_ptr].rect.y;
    popup->combo[popup->cmb_ptr].urect.w = 2 * wd;
    popup->combo[popup->cmb_ptr].urect.h = h;
    popup->combo[popup->cmb_ptr].drect.x = popup->combo[popup->cmb_ptr].rect.x + (14 * wd) - 1;
    popup->combo[popup->cmb_ptr].drect.y = popup->combo[popup->cmb_ptr].rect.y;
    popup->combo[popup->cmb_ptr].drect.w = 2 * wd;
    popup->combo[popup->cmb_ptr].drect.h = h;
    for (i = 0; reel_color[i] != NULL; i++) {
        text = TTF_RenderText_Solid(font14, reel_color[i], c1);
        popup->combo[popup->cmb_ptr].label[i] = SDL_CreateTextureFromSurface( popup->render,
                                      text);
        SDL_QueryTexture(popup->combo[popup->cmb_ptr].label[i], NULL, NULL,
             &popup->combo[popup->cmb_ptr].lw[i], &popup->combo[popup->cmb_ptr].lh[i]);
    }
    popup->combo[popup->cmb_ptr].num = supply_color[u];
    popup->combo[popup->cmb_ptr].value = &supply_color[u];
    popup->combo[popup->cmb_ptr].max = i - 1;
    popup->cmb_ptr++;
    text = TTF_RenderText_Solid(font14, "Label: ", c1);

    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 25 + (12 * wd) * 4;
    popup->ctl_label[popup->ctl_ptr].rect.y = 20 + (12 * hd);
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;
    popup->combo[popup->cmb_ptr].rect.x = 25 + (12 * wd) * 5;
    popup->combo[popup->cmb_ptr].rect.y = 20 + (12 * hd);
    popup->combo[popup->cmb_ptr].rect.w = 16 * wd;
    popup->combo[popup->cmb_ptr].rect.h = h;
    popup->combo[popup->cmb_ptr].urect.x = popup->combo[popup->cmb_ptr].rect.x;
    popup->combo[popup->cmb_ptr].urect.y = popup->combo[popup->cmb_ptr].rect.y;
    popup->combo[popup->cmb_ptr].urect.w = 2 * wd;
    popup->combo[popup->cmb_ptr].urect.h = h;
    popup->combo[popup->cmb_ptr].drect.x = popup->combo[popup->cmb_ptr].rect.x + (14 * wd) - 1;
    popup->combo[popup->cmb_ptr].drect.y = popup->combo[popup->cmb_ptr].rect.y;
    popup->combo[popup->cmb_ptr].drect.w = 2 * wd;
    popup->combo[popup->cmb_ptr].drect.h = h;
    for (i = 0; label_mode[i] != NULL; i++) {
        text = TTF_RenderText_Solid(font14, label_mode[i], c1);
        popup->combo[popup->cmb_ptr].label[i] = SDL_CreateTextureFromSurface( popup->render,
                                      text);
        SDL_QueryTexture(popup->combo[popup->cmb_ptr].label[i], NULL, NULL,
             &popup->combo[popup->cmb_ptr].lw[i], &popup->combo[popup->cmb_ptr].lh[i]);
    }
    popup->combo[popup->cmb_ptr].num = supply_label[u];
    popup->combo[popup->cmb_ptr].value = &supply_label[u];
    popup->combo[popup->cmb_ptr].max = i - 1;
    popup->cmb_ptr++;
    text = TTF_RenderText_Solid(font14, "Take Up", c1);

    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 25 + (20 * wd) * 4;
    popup->ctl_label[popup->ctl_ptr].rect.y = 20 + (8 * hd);
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;
    text = TTF_RenderText_Solid(font14, "Color: ", c1);

    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 25 + (20 * wd) * 4;
    popup->ctl_label[popup->ctl_ptr].rect.y = 20 + (10 * hd);
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;
    popup->combo[popup->cmb_ptr].rect.x = 25 + (20 * wd) * 5;
    popup->combo[popup->cmb_ptr].rect.y = 20 + (10 * hd);
    popup->combo[popup->cmb_ptr].rect.w = 16 * wd;
    popup->combo[popup->cmb_ptr].rect.h = h;
    popup->combo[popup->cmb_ptr].urect.x = popup->combo[popup->cmb_ptr].rect.x;
    popup->combo[popup->cmb_ptr].urect.y = popup->combo[popup->cmb_ptr].rect.y;
    popup->combo[popup->cmb_ptr].urect.w = 2 * wd;
    popup->combo[popup->cmb_ptr].urect.h = h;
    popup->combo[popup->cmb_ptr].drect.x = popup->combo[popup->cmb_ptr].rect.x + (14 * wd) - 1;
    popup->combo[popup->cmb_ptr].drect.y = popup->combo[popup->cmb_ptr].rect.y;
    popup->combo[popup->cmb_ptr].drect.w = 2 * wd;
    popup->combo[popup->cmb_ptr].drect.h = h;
    for (i = 0; reel_color[i] != NULL; i++) {
        text = TTF_RenderText_Solid(font14, reel_color[i], c1);
        popup->combo[popup->cmb_ptr].label[i] = SDL_CreateTextureFromSurface( popup->render,
                                      text);
        SDL_QueryTexture(popup->combo[popup->cmb_ptr].label[i], NULL, NULL,
             &popup->combo[popup->cmb_ptr].lw[i], &popup->combo[popup->cmb_ptr].lh[i]);
    }
    popup->combo[popup->cmb_ptr].num = takeup_color[u];
    popup->combo[popup->cmb_ptr].value = &takeup_color[u];
    popup->combo[popup->cmb_ptr].max = i - 1;
    popup->cmb_ptr++;
    text = TTF_RenderText_Solid(font14, "Label: ", c1);

    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 25 + (20 * wd) * 4;
    popup->ctl_label[popup->ctl_ptr].rect.y = 20 + (12 * hd);
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;
    popup->combo[popup->cmb_ptr].rect.x = 25 + (20 * wd) * 5;
    popup->combo[popup->cmb_ptr].rect.y = 20 + (12 * hd);
    popup->combo[popup->cmb_ptr].rect.w = 16 * wd;
    popup->combo[popup->cmb_ptr].rect.h = h;
    popup->combo[popup->cmb_ptr].urect.x = popup->combo[popup->cmb_ptr].rect.x;
    popup->combo[popup->cmb_ptr].urect.y = popup->combo[popup->cmb_ptr].rect.y;
    popup->combo[popup->cmb_ptr].urect.w = 2 * wd;
    popup->combo[popup->cmb_ptr].urect.h = h;
    popup->combo[popup->cmb_ptr].drect.x = popup->combo[popup->cmb_ptr].rect.x + (14 * wd) - 1;
    popup->combo[popup->cmb_ptr].drect.y = popup->combo[popup->cmb_ptr].rect.y;
    popup->combo[popup->cmb_ptr].drect.w = 2 * wd;
    popup->combo[popup->cmb_ptr].drect.h = h;
    for (i = 0; label_mode[i] != NULL; i++) {
        text = TTF_RenderText_Solid(font14, label_mode[i], c1);
        popup->combo[popup->cmb_ptr].label[i] = SDL_CreateTextureFromSurface( popup->render,
                                      text);
        SDL_QueryTexture(popup->combo[popup->cmb_ptr].label[i], NULL, NULL,
             &popup->combo[popup->cmb_ptr].lw[i], &popup->combo[popup->cmb_ptr].lh[i]);
    }
    popup->combo[popup->cmb_ptr].num = takeup_label[u];
    popup->combo[popup->cmb_ptr].value = &takeup_label[u];
    popup->combo[popup->cmb_ptr].max = i - 1;
    popup->cmb_ptr++;
    popup->update = &model2415_update;
    return popup;
}

struct _device *
model2415_init(void *rend, uint16_t addr) {
     SDL_Renderer *render = (SDL_Renderer *)rend;
     SDL_Surface *text;
     struct _device *dev2415;
     struct _2415_context *tape;
     int     i;

     if ((dev2415 = calloc(1, sizeof(struct _device))) == NULL)
         return NULL;
     if ((tape = calloc(1, sizeof(struct _2415_context))) == NULL) {
         free(dev2415);
         return NULL;
     }

     tape_init();
     text = IMG_ReadXPMFromArray(model2415_xpm);
     model2415_img = SDL_CreateTextureFromSurface(render, text);
     SDL_SetTextureBlendMode(model2415_img, SDL_BLENDMODE_BLEND);
     SDL_FreeSurface(text);
     text = IMG_ReadXPMFromArray(tape_images_xpm);
     tape_images_img = SDL_CreateTextureFromSurface(render, text);
     SDL_SetTextureBlendMode(tape_images_img, SDL_BLENDMODE_BLEND);
     SDL_FreeSurface(text);

     dev2415->bus_func = &model2415_dev;
     dev2415->dev = (void *)tape;
     dev2415->draw_model = (void *)&model2415_draw;
     dev2415->create_ctrl = (void *)&model2415_control;
     dev2415->n_units = 6;
     for (i = 0; i < dev2415->n_units; i++) {
         dev2415->rect[i].x = 210 * i;
         dev2415->rect[i].y = 200;
         dev2415->rect[i].w = 210;
         dev2415->rect[i].h = 220;
         if (dev2415->rect[i].x > 800) {
            dev2415->rect[i].y += dev2415->rect[i].h;
            dev2415->rect[i].x = 210 * (i - 4);
         }
     }
     tape->addr = (addr & 0xf8);
     tape->chan = (addr >> 8) & 0xf;
     tape->state = STATE_IDLE;
     tape->selected = 0;
     for (i = 0; i < dev2415->n_units; i++) {
        tape->sense[i] = 0;
        tape->tape[i] = (struct _tape_buffer *)calloc(1, sizeof(struct _tape_buffer));
        tape->tape[i]->format = TRACK9;
     }
     tape->nunits = dev2415->n_units;
//     tape_attach(tape->tape[0], "../test_progs/bos.tap", TYPE_E11, 0, 1);
     tape_attach(tape->tape[0], "../test_progs/sysres.tap", TYPE_E11, 0, 1);
     tape_attach(tape->tape[1], "sys001.tap", TYPE_E11, 1, 1);
     tape_attach(tape->tape[2], "sys002.tap", TYPE_E11, 1, 1);
     tape_attach(tape->tape[3], "sys003.tap", TYPE_E11, 1, 1);
     tape_attach(tape->tape[4], "sys004.tap", TYPE_E11, 1, 1);
     tape_attach(tape->tape[5], "sys005.tap", TYPE_E11, 1, 1);
     for (i = 0; i < 6; i++)
         tape->tape[i]->format |= ONLINE;
     add_chan(dev2415, addr);
     return dev2415;
}

int
model2415_create(struct _option *opt)
{
     struct _option  opts;
     struct _device  *dev2415;
     struct _2415_context *tape;
     char            *file;
     int             i;
     int             track7;
     int             ring;
     int             fmt;
     int             den;

     /* Check if dealing with unit */
     if (opt->model == 'U') {
         dev2415 = find_chan(opt->addr, 0xf8);
         if (dev2415 == NULL) {
             fprintf(stderr, "Device not found %s %03x\n", opt->opt, opt->addr);
             return 0;
         }
         i = opt->addr & 0x7;
         tape = (struct _2415_context *)dev2415->dev;
         tape->tape[i] = (struct _tape_buffer *)calloc(1, sizeof(struct _tape_buffer));
         tape->tape[i]->format = TRACK9;
         /* Parse options given on definition */
         file = NULL;
         track7 = 0;
         ring = 0;
         den = 1;
         fmt = TYPE_TAP;
         while (get_option(&opts)) {
               if (strcmp(opts.opt, "FILE") == 0 && opts.flags == 1) {
                   file = strdup(opts.string);
               } else if (strcmp(opts.opt, "7TRACK") == 0) {
                   track7 = 1;
               } else if (strcmp(opts.opt, "NORING") == 0) {
                   ring = 0;
               } else if (strcmp(opts.opt, "RING") == 0) {
                   ring = 1;
               } else if (strcmp(opts.opt, "FORMAT") == 0) {
                   fmt = get_index(&opts, format_type);
               } else if (strcmp(opts.opt, "800") == 0) {
                   den = 0;
               } else if (strcmp(opts.opt, "1600") == 0) {
                   den = 1;
               } else {
                   fprintf(stderr, "Invalid option %s to 2415 Unit\n", opts.opt);
                   free(tape->tape[i]);
                   tape->tape[i] = NULL;
                   return 0;
               }
         }
         if (track7) {
            tape->tape[i]->format &= ~TRACK9;
         }
         if (file != NULL) {
             if (tape_attach(tape->tape[i], file, fmt, ring, den) == 0) {
                log_warn("Unable to open file %s\n", file);
             } else {
                tape->tape[i]->format |= ONLINE;
             }
             free(file);
         }
     } else {
         if ((dev2415 = calloc(1, sizeof(struct _device))) == NULL)
             return 0;
         if ((tape = calloc(1, sizeof(struct _2415_context))) == NULL) {
             free(dev2415);
             return 0;
         }

         tape_init();

         dev2415->bus_func = &model2415_dev;
         dev2415->dev = (void *)tape;
         dev2415->draw_model = (void *)&model2415_draw;
         dev2415->create_ctrl = (void *)&model2415_control;
         dev2415->n_units = 6;
         if (opt->flags & 1) {
             if (opt->dash_num >= 4)
                 dev2415->n_units = (opt->dash_num - 3) * 2;
             else
                 dev2415->n_units = (opt->dash_num) * 2;
         }
         for (i = 0; i < dev2415->n_units; i++) {
             dev2415->rect[i].x = 210 * i;
             dev2415->rect[i].y = 200;
             dev2415->rect[i].w = 210;
             dev2415->rect[i].h = 220;
             if (dev2415->rect[i].x > 800) {
                dev2415->rect[i].y += dev2415->rect[i].h;
                dev2415->rect[i].x = 210 * (i - 4);
             }
         }
         tape->addr = (opt->addr & 0xf8);
         tape->chan = (opt->addr >> 8) & 0xf;
         tape->state = STATE_IDLE;
         tape->selected = 0;
         tape->nunits = dev2415->n_units;
         /* Parse options given on definition */
         while (get_option(&opts)) {
               if (strcmp(opts.opt, "7TRACK") == 0) {
                   tape->track_7 = 1;
               } else {
                   fprintf(stderr, "Invalid option %s to 1442\n", opts.opt);
                   free(tape);
                   free(dev2415);
                   return 0;
               }
         }

         for (i = 0; i < 6; i++)
             tape->sense[i] = 0;
         add_chan(dev2415, opt->addr);
     }
     return 1;
}

DEV_LIST_STRUCT(2415, CTRL_TYPE, CHAR_OPT|NUM_MOD);

