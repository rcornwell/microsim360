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
#include <string.h>
#include <stdlib.h>
#include "logger.h"
#include "event.h"
#include "device.h"
#include "xlat.h"
#include "tape.h"
#include "model2415.h"

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

#if 0
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
#endif

#define FRAME_DELAY     33    /* 34 us per frame delay */
#define REWIND_DELAY    1000 /* 20 ms */
#define REW_FRAME       3840  /* Frames per 20ms */
#define START_DELAY     100
#define STOP_DELAY      (10 * FRAME_DELAY)

#define MT_ODD          0x01  /* Odd parity */
#define MT_TRANS        0x02  /* Translation turned on ignored 9 track  */
#define MT_CONV         0x04  /* Data converter on ignored 9 track  */

void
model2415_rewind_callback(struct _device *unit, void *arg, int u)
{
    struct _2415_context *ctx = (struct _2415_context *)unit->dev;
    struct _tape_buffer  *tape = (struct _tape_buffer *)arg;
    int      r;

    if (tape->pos_frame <= (5 * 12 * 1600)) {
       log_device("Rewind Low speed %02x %02x\n", ctx->rew_flags, ctx->rdy_flags);
       r = tape_rewind_frames(tape, 1);
       if (r == TAPE_STATUS_BOT) {
           log_device("Rewind done %d\n", u);
           ctx->rew_flags &= ~(1 << u);
           if (ctx->run_flags & (1 << u)) {
               log_device("Detach device %d\n", u);
               tape_detach(tape);
               ctx->run_flags &= ~(1 << u);
           } else if (ctx->cur_unit == u && ctx->state != STATE_IDLE) {
               if (ctx->chaining) {
                   log_device("Chain done device %d\n", u);
               }
               ctx->status |= SNS_DEVEND;
               ctx->status &= ~(SNS_CTLEND|SNS_UNITCHK);
               ctx->cmd = 0;
               ctx->cmd_done = 1;
               log_device("Rewind finished %d\n", u);
           } else {
               ctx->rdy_flags |= 1 << u;
               log_device("Rewind ready device %d\n", u);
           }
       } else {
           add_event(unit, &model2415_rewind_callback, FRAME_DELAY, arg, u);
       }
       return;
    }

    log_device("Rewind high speed  %02x %02x\n", ctx->rew_flags, ctx->rdy_flags);
    r = tape_rewind_frames(tape, REW_FRAME);
    if (r == TAPE_STATUS_BOT) {
        log_device("Rewind done %d\n", u);
        ctx->rew_flags &= ~(1 << u);
        if (ctx->run_flags & (1 << u)) {
            tape_detach(tape);
            ctx->run_flags &= ~(1 << u);
        } else if (ctx->cur_unit == u && ctx->state != STATE_IDLE) {
            if (ctx->chaining) {
                log_device("Chain done device %d\n", u);
            }
            ctx->status |= SNS_DEVEND;
            ctx->cmd = 0;
            ctx->status &= ~(SNS_CTLEND|SNS_UNITCHK);
            ctx->cmd_done = 1;
        } else {
            ctx->rdy_flags |= 1 << u;
        }
    } else {
        add_event(unit, &model2415_rewind_callback, REWIND_DELAY, arg, u);
    }
}

static void
done_callback(struct _device *unit, void *arg, int u)
{
    struct _2415_context *ctx = (struct _2415_context *)unit->dev;

    ctx->cmd_done = 1;
    if ((ctx->cmd & 0x3) == 0x3) {
        ctx->rdy_flags |= 1 << u;  /* Set ready flag for device */
    }
    ctx->status |= SNS_DEVEND;
    log_device("Do done command %d %d %02x\n", ctx->data_end, ctx->data_rdy, ctx->status);
}

static void
tape_callback(struct _device *unit, void *arg, int u)
{
    struct _2415_context *ctx = (struct _2415_context *)unit->dev;
    struct _tape_buffer  *tape = (struct _tape_buffer *)arg;
    int      r;

    log_trace("Tape = %x, arg=%d\n", tape, u);

    switch (ctx->cmd & 0xf) {
    case 0x0: /* Test I/O */
    case 0x4: /* Sense */
    case 0x3: /* Mode command */
    case 0xb: /* Mode command */
           break;
    case 0x1: /* Write */
    case 0x5: /* Write */
    case 0x9: /* Write */
    case 0xD: /* Write */
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
    case 0x2: /* Read */
    case 0x6: /* Read */
    case 0xa: /* Read */
    case 0xe: /* Read */
    case 0xc:  /* Read backward */
           /* If CPU does not want any more data, just read and ignore
              the rest of the data */
           log_device("Do read command %d %d %d\n", ctx->data_end, ctx->data_rdy, ctx->state);
           if (ctx->data_end) {
               r = tape_read_frame(tape, &ctx->data);
               log_device("Tape read frame dataend %d\n", r);
               if (r != TAPE_STATUS_OK) {
                   /* Process end */
                   r = tape_finish_rec(tape);
                   log_device("Tape finish read %d\n", r);
                   ctx->status |= SNS_CHNEND;
                   add_event(unit, done_callback, STOP_DELAY, (void *)tape, u);
               } else {
                   add_event(unit, tape_callback, FRAME_DELAY, (void *)tape, u);
               }
           } else if (ctx->data_rdy) {
               log_device("Tape read frame overrun \n");
               ctx->data_end = 1;
               ctx->sense[0] |= SENSE_OVRRUN;
               ctx->status |= SNS_CHNEND|SNS_UNITCHK;
               add_event(unit, done_callback, STOP_DELAY, (void *)tape, u);
           } else {
               r = tape_read_frame(tape, &ctx->data);
               log_device("Tape read frame %d\n", r);
               switch(r) {
               case TAPE_STATUS_FILE_ERROR:
                   /* Set up read error */
                    break;

               case TAPE_STATUS_EOT:
               case TAPE_STATUS_BOT:
               case TAPE_STATUS_EOB:
                   /* End of record */
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
                   ctx->data_end = 1;
                   break;

               case TAPE_STATUS_MARK:
                   /* Read of tape mark */
                   ctx->data_end = 1;
                   ctx->mrk_flags |= (1 << ctx->cur_unit);
                   ctx->status |= SNS_UNITEXP|SNS_CHNEND;
                   add_event(unit, done_callback, STOP_DELAY, (void *)tape, u);
                   break;

               case TAPE_STATUS_OK:
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
                   log_device("Tape Queued: %02x %d\n", ctx->data, ctx->data_rdy);
                   break;

               default:
                   log_device("Unhandled status\n");
               }
           }
           break;

    case 0x7:  /* Tape motion control */
    case 0xf:
           switch (ctx->cmd & 0xff) {
           case 0x0f:    /* Rewind and unload */
                tape_start_rewind(tape);
                add_event(unit, model2415_rewind_callback, REWIND_DELAY, (void *)tape, u);
                ctx->data_end = 1;
                ctx->cmd_done = 1;
                ctx->rew_flags |= 1 << ctx->cur_unit;
                ctx->run_flags |= 1 << ctx->cur_unit;
                ctx->status = SNS_CTLEND|SNS_DEVEND|SNS_UNITCHK;
                ctx->cmd = 0;
                break;
           case 0x07:    /* Rewind */
                log_device("start rewind %d\n", ctx->chaining);
                tape_start_rewind(tape);
                ctx->rew_flags |= 1 << ctx->cur_unit;
                add_event(unit, model2415_rewind_callback, REWIND_DELAY, (void *)tape, u);
                ctx->data_end = 1;
                if (ctx->chaining == 0) {
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
                switch(r) {
                case TAPE_STATUS_FILE_ERROR:
                     /* Set up read error */
                     ctx->status |= SNS_UNITCHK;
                     add_event(unit, done_callback, STOP_DELAY, (void *)tape, u);
                     break;

                case TAPE_STATUS_EOB:
                     /* Terminate of record read */
                     r = tape_finish_rec(tape);
                     /* End of record */
                     if (tape_at_loadpt(tape)) {  /* If at load point, done */
                         ctx->status |= SNS_UNITCHK;
                         add_event(unit, done_callback, STOP_DELAY, (void *)tape, u);
                     } else {
                         if ((ctx->cmd & 0x8) != 0) {  /* Check if spacing a file */
                             if ((ctx->cmd & 0x10) != 0) {
                                r = tape_read_forw(tape);
                             } else {
                                r = tape_read_back(tape);
                             }
                             log_device("Tape start record %d\n", r);
                             /* Handle error */
                             if (r == TAPE_STATUS_FILE_ERROR) {
                                 ctx->status |= SNS_UNITCHK;
                                 add_event(unit, done_callback, STOP_DELAY, (void *)tape, u);
                             } else if (r == TAPE_STATUS_OK) {
                                 /* Spacing file, continue until tape mark */
                                 add_event(unit, tape_callback, FRAME_DELAY, (void *)tape, u);
                             } else {
                                 /* Either end of tape or tape mark, stop */
                                 add_event(unit, done_callback, STOP_DELAY, (void *)tape, u);
                             }
                         } else {  /* At end of block, all done */
                             if (r == TAPE_STATUS_MARK) {
                                 ctx->status |= SNS_UNITEXP;
                             }
                             /* If space block, end */
                             add_event(unit, done_callback, STOP_DELAY, (void *)tape, u);
                         }
                      }
                      break;

                  case TAPE_STATUS_MARK:
                     /* Terminate on tape mark */
                     r = tape_finish_rec(tape);
                     if ((ctx->cmd & 0x8) == 0) {
                         /* If space block, end */
                         ctx->mrk_flags |= (1 << ctx->cur_unit);
                         ctx->status |= SNS_UNITEXP;
                     }
                     add_event(unit, done_callback, STOP_DELAY, (void *)tape, u);
                     break;

                 case TAPE_STATUS_OK:
                   add_event(unit, tape_callback, FRAME_DELAY, (void *)tape, u);
                }
                break;
           }
           break;
    default:
           break;
    }
}

/* Decode command to device */
static void
device_cmd(struct _device *unit, uint8_t bus_out)
{
    struct _2415_context *ctx = (struct _2415_context *)unit->dev;
    int      r;

    /* If current unit has pending status and not sense, return busy */
    if (ctx->cur_unit == ctx->stat_unit && (bus_out & 0xf) != 0x4 &&
        (ctx->sense[0] != 0 || (ctx->sense[1] & SENSE_NOISE) != 0 ||
        (ctx->sense[3] & (SENSE_VCR|SENSE_LRCR)) != 0)) {
        ctx->status |= SNS_BSY;
        log_device(" Unit busy: u=%d %02x %02x %02x\n", ctx->cur_unit, ctx->sense[0], ctx->sense[1], ctx->sense[3]);
        return;
    }

//    /* Check if device is rewinding */
//    if (((ctx->rew_flags | ctx->run_flags) & (1 << ctx->cur_unit)) != 0) {
//        log_device("Unit busy rew=%02x run=%02x\n", ctx->rew_flags, ctx->run_flags);
//        ctx->status |= SNS_BSY;
//        return;
//    }

    /* Set up sense byte 1 */
    ctx->sense[1] = 0;
    if (ctx->tape[ctx->cur_unit] != NULL) {
        if (tape_ready(ctx->tape[ctx->cur_unit])) {
            ctx->sense[1] |= SENSE_TUA;
            if (((ctx->rew_flags | ctx->run_flags) & (1<<ctx->cur_unit)) != 0)
                ctx->sense[1] |= SENSE_TUB;
            if (!tape_ring(ctx->tape[ctx->cur_unit]))
                ctx->sense[1] |= SENSE_NORING;
            if (tape_at_loadpt(ctx->tape[ctx->cur_unit]))
                ctx->sense[1] |= SENSE_LOAD;
            if (!tape_9_track(ctx->tape[ctx->cur_unit]))
                ctx->sense[1] |= SENSE_7TRACK;
            tape_select(ctx->tape[ctx->cur_unit]);
        } else {
            ctx->sense[1] |= SENSE_TUB;
        }
    }
    ctx->sense[2] = SENSE_2;
    ctx->sense[4] = 0;
    ctx->sense[5] = 0;
    log_device(" Unit u=%d sense %02x %02x %02x %02x %02x\n", ctx->cur_unit,
                         ctx->sense[0], ctx->sense[1], ctx->sense[3], ctx->sense[4], ctx->sense[5]);
    ctx->cmd = bus_out & 0xff;
    ctx->cmd_done = 1;
    ctx->data_end = 0;
    ctx->data_rdy = 0;
    ctx->cc = 0;
    ctx->rdy_flags &= ~(1 << ctx->cur_unit);
    ctx->mrk_flags &= ~(1 << ctx->cur_unit);
    ctx->stat_unit = -1;
    ctx->status = 0;
    switch (ctx->cmd & 0xF) {
    case 0x0: /* Test I/O */
           return;

    case 0x1: /* Write */
    case 0x5: /* Write */
    case 0x9: /* Write */
    case 0xD: /* Write */
           if ((ctx->sense[1] & SENSE_TUB) != 0) {
               ctx->status = SNS_BSY;
               break;
           }
           ctx->sense[0] = 0;
           ctx->sense[1] &= (SENSE_TUA|SENSE_TUB|SENSE_7TRACK|
                             SENSE_LOAD|SENSE_NORING);
           ctx->sense[3] = 0;
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
              ctx->cmd_done = 0;
              ctx->data_rdy = 1;
           } else if (r == 2) {
               ctx->sense[0] = SENSE_CMDREJ;
           } else {
               ctx->sense[0] = SENSE_INTERV;
           }
           break;

    case 0x2: /* Read */
    case 0x6: /* Read */
    case 0xa: /* Read */
    case 0xe: /* Read */
           if ((ctx->sense[1] & SENSE_TUB) != 0) {
               ctx->status = SNS_BSY;
               break;
           }
           ctx->sense[0] = 0;
           ctx->sense[1] &= (SENSE_TUA|SENSE_TUB|SENSE_7TRACK|
                             SENSE_LOAD|SENSE_NORING);
           ctx->sense[2] = SENSE_2;
           ctx->sense[3] = 0;
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
               ctx->cmd_done = 0;
           }
           break;

      case 0xc:  /* Read backwords. */
           if ((ctx->sense[1] & SENSE_TUB) != 0) {
               ctx->status = SNS_BSY;
               break;
           }
           ctx->sense[0] = 0;
           ctx->sense[1] &= (SENSE_TUA|SENSE_TUB|SENSE_7TRACK|
                             SENSE_LOAD|SENSE_NORING);
           ctx->sense[3] = 0;

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
               ctx->cmd_done = 0;
           }
           break;

    case 0x4:  /* Sense or read backward */
           ctx->data = ctx->sense[0];
           ctx->sense_cnt = 1;
           ctx->data_rdy = 1;
           ctx->cmd_done = 0;
           break;

    case 0x3: /* Mode command */
    case 0xb: /* Mode command */
           if ((ctx->sense[1] & SENSE_TUB) != 0) {
               ctx->status = SNS_BSY;
               ctx->cmd = 0;
               ctx->cmd_done = 1;
               ctx->data_end = 1;
               break;
           }
           ctx->sense[0] = 0;
           ctx->sense[3] = 0;
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

    case 0x7:  /* Tape motion control */
    case 0xf:  /* Tape motion control */
           if ((ctx->sense[1] & SENSE_TUB) != 0) {
               ctx->status = SNS_BSY;
               break;
           }
           ctx->sense[0] = 0;
           ctx->sense[1] &= (SENSE_TUA|SENSE_TUB|SENSE_7TRACK|
                             SENSE_LOAD|SENSE_NORING);
           ctx->sense[3] = 0;
           /* Check if tape attached */
           if (ctx->tape[ctx->cur_unit]->file_name == 0) {
               ctx->sense[0] = SENSE_INTERV;
               break;
           }
           switch (ctx->cmd & 0xff) {
           case 0x17:    /* Erase gap */
           case 0x1f:    /* Write tape mark */
                 /* Check if no write ring */
                 if (tape_ring(ctx->tape[ctx->cur_unit]) == 0) {
                     ctx->sense[0] = SENSE_CMDREJ;
                     break;
                 }
                 ctx->data_end = 1;
                 add_event(unit, &tape_callback, START_DELAY,
                         (void *)ctx->tape[ctx->cur_unit], ctx->cur_unit);
                 ctx->cmd_done = 0;
                 break;

           case 0x07:    /* Rewind */
           case 0x0f:    /* Rewind and unload */
                 ctx->data_end = 1;
                 add_event(unit, &tape_callback, START_DELAY,
                         (void *)ctx->tape[ctx->cur_unit], ctx->cur_unit);
                 ctx->cmd_done = 0;
                 break;
           case 0x37:    /* Forward space block */
           case 0x3f:    /* Forward space file */
                ctx->data_end = 1;
                /* Do a read start */
                r = tape_read_forw(ctx->tape[ctx->cur_unit]);
                if (r >= 0) {
                    add_event(unit, &tape_callback, START_DELAY,
                            (void *)ctx->tape[ctx->cur_unit], ctx->cur_unit);
                 ctx->cmd_done = 0;
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
                 ctx->cmd_done = 0;
                } else {
                    ctx->sense[0] = SENSE_INTERV;
                }
                break;
           default:
                ctx->sense[0] = SENSE_CMDREJ;
                ctx->sense[2] = SENSE_2;
                ctx->sense[3] = 0;
                ctx->cmd = 0;
                ctx->data_end = 1;
           }
           break;
    default:
           ctx->cmd = 0;
           ctx->data_end = 1;
           ctx->cmd_done = 1;
           break;
    }
    if (ctx->cmd != 4 && ((ctx->sense[0] & ~(SENSE_WCZERO)) != 0 ||
                            (ctx->sense[1] & BIT7) != 0)) {
        ctx->status |= SNS_UNITCHK;
        if (ctx->tape[ctx->cur_unit] != NULL) {
            tape_unselect(ctx->tape[ctx->cur_unit]);
        }
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

/* Process channel */
void
model2415_dev(struct _device *unit, uint16_t *tags, uint16_t bus_out, uint16_t *bus_in)
{
    struct _2415_context *ctx = (struct _2415_context *)unit->dev;
    static   uint16_t  last_tags = 0;

    if (last_tags != *tags || unit->selected) {
        print_tags("2415", ctx->state, *tags, bus_out);
        last_tags = 0; // *tags;
    }

    /* Reset device if OPER OUT is dropped */
    if ((*tags & (CHAN_OPR_OUT|CHAN_SUP_OUT)) == 0) {
        if (unit->selected) {
           *tags &= ~(CHAN_OPR_IN|CHAN_ADR_IN|CHAN_SRV_IN|CHAN_STA_IN);
        }
        log_device("2415: %03x reset\n",unit->addr);
        unit->selected = 0;
        unit->request = 0;
        ctx->state = STATE_IDLE;
        ctx->sense[0] = 0;
        ctx->cmd = 0;
        ctx->cmd_done = 0;
        ctx->busy = 0;
        ctx->data_end = 0;
        ctx->data_rdy = 0;
        return;
    }

    switch (ctx->state) {
    /* Idle wait for CPU to talk to us */
    case STATE_IDLE:
//            log_device("2415: %03x idle r=%d s=%d e=%d p=%d b=%d cd=%d dly=%d %02x %02x\n",unit->addr,
//                unit->request, unit->stacked, ctx->data_end, ctx->data_end_post,
//                ctx->busy, ctx->cmd_done, ctx->delay, ctx->cmd, ctx->status);
            /* If operation out, reset device */
            if ((*tags & CHAN_OPR_OUT) == 0) {
                log_device("2415: %03x oper dropped\n",unit->addr);
                break;
            }

            /* Scan if device is ready */
            if ((ctx->rdy_flags & (1 << ctx->t_scan)) == 0) {
               ctx->t_scan++;
               if (ctx->t_scan >= 6) {
                   ctx->t_scan = 0;
               }
            } else {
               unit->request = 1;
            }

            /* If operation in, another device has channel */
            if ((*tags & CHAN_OPR_IN) != 0) {
                if (unit->request || unit->stacked) {
                    *tags &= ~(CHAN_REQ_IN);
                }
                break;
            }

            /* If we have request and suppress out is down, post request */
            if (unit->request || unit->stacked) {
                log_device("2415: %03x port request\n",unit->addr);
                if ((*tags & (CHAN_SUP_OUT|CHAN_ADR_OUT)) == 0) {
                    *tags |= CHAN_REQ_IN;
                } else {
                    *tags &= ~CHAN_REQ_IN;
                }
            }

            /* If select out check if channel is asking for us or we have status */
            if ((*tags & CHAN_SEL_OUT) != 0) {
                 /* Check if looking for this device */
                 if ((*tags & CHAN_ADR_OUT) != 0) {
                     if ((bus_out & 0xf8) == (unit->addr & 0xf8)) {
                        *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);
                        /* Check if parity error on bus */
                        if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0) {
                            ctx->sense[0] |= SENSE_BUSCHK;
                        }
                        /* If device in operation, respond with busy status */
                        /* If another unit has pending status. */
                        if (ctx->busy || (ctx->rdy_flags != 0 && (ctx->rdy_flags & (1 << (bus_out & 0x7))) == 0)) {
                            *bus_in = SNS_BSY|SNS_SMS | odd_parity[SNS_BSY|SNS_SMS];
                            *tags |= CHAN_STA_IN;             /* Put Busy flag on bus */
                            ctx->state = STATE_BUSY;
                            ctx->ctrl_busy = 1;
                            log_device("2415: %03x busy\n",unit->addr);
                            break;
                        }
                        ctx->cur_unit = bus_out & 0x7;

                        /* Clear select in, and raise operation in */
                        *tags |= CHAN_OPR_IN;             /* Put our address on bus */
                        *bus_in = (unit->addr & 0xf8) | ctx->cur_unit;
                        *bus_in |= odd_parity[*bus_in];
                        ctx->state = STATE_INIT_SEL; /* Start initial select sequence */
                        unit->selected = 1;
                        log_device("2415: %03x selected\n",unit->addr);
                     }
                     break;
                 }

                 /* If no address out, see if we have request or stacked status */
                 if ((*tags & CHAN_SUP_OUT) == 0 && (unit->request || unit->stacked)) {
                     *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);
                     *tags |= CHAN_OPR_IN;      /* Put our address on bus */
                     if ((ctx->rdy_flags & (1 << ctx->t_scan)) == 0) {
                         *bus_in = (unit->addr & 0xf8) | ctx->t_scan;
                         *bus_in |= odd_parity[*bus_in];
                         ctx->cur_unit = ctx->t_scan;
                         ctx->status |= SNS_DEVEND;
                     }
                     unit->selected = 1;
                     ctx->state = STATE_INIT_SEL;
                     log_device("2415: %03x polling\n",unit->addr);
                 }

             }
             break;

            /* Start of initial selection sequence */
    case STATE_INIT_SEL:
            *tags &= ~(CHAN_SEL_OUT);  /* Clear select in */
            *bus_in = ctx->cur_unit | (unit->addr & 0xf8);
            *bus_in |= odd_parity[*bus_in];
            log_device("2415: %03x address in unit %d\n",unit->addr, ctx->cur_unit);
            /* Wait for Address out to drop */
            if ((*tags & (CHAN_ADR_OUT)) == 0) {
                 *tags |= CHAN_ADR_IN;
                 ctx->state = STATE_COMMAND;
            }
            break;

     case STATE_COMMAND:
            /* Wait for command or address out */
            *tags &= ~(CHAN_SEL_OUT);
            unit->request = 0;

            log_device("2415: %03x waiting command %02x\n",unit->addr, ctx->status);
            *bus_in = (unit->addr & 0xff) | odd_parity[unit->addr & 0xff];
            /* we get command out, process command */
            if ((*tags & (CHAN_CMD_OUT)) != 0) {
                *tags &= ~(CHAN_ADR_IN);        /* Command out, drop addressin */
                /* Check if parity error on bus */
                if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0) {
                    ctx->cmd = 0;
                    ctx->cmd_done = 0;
                    ctx->busy = 0;
                    ctx->data_end = 0;
                    ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                    ctx->sense[0] |= SENSE_BUSCHK;
                    ctx->state = STATE_STATUS;
                    ctx->rdy_flags &= ~(1 << ctx->cur_unit);
                    break;
                }
                if (ctx->busy) {
                    if (ctx->data_end == 0) {
                        ctx->state = STATE_DATA_1;
                        break;
                    }
                    if (ctx->status == 0) {
                        ctx->status = SNS_BSY;
                    }
                    ctx->state = STATE_STATUS; /* Present status out */
                    break;
                }
                ctx->state = STATE_STATUS; /* Present status out */
                if (!unit->stacked && ctx->status == 0) {       /* If no stacked status, process command */
                    device_cmd(unit, bus_out & 0xff);
                }
                break;
            }

            /* If we get Address out again, we need to halt */
            if ((*tags & (CHAN_ADR_OUT)) != 0 && (*tags & CHAN_HLD_OUT) == 0) {
                *tags &= ~(CHAN_ADR_IN|CHAN_OPR_IN);  /* Clear select in */
                log_device("2415: Halt %03x device\n",unit->addr);
                if (ctx->data_end == 0) {
                    ctx->data_end = 1;
                    ctx->status |= SNS_CHNEND;
                }
                ctx->state = STATE_STATUS_WAIT;
                break;
            }

            break;

    /* Present initial status */
    case STATE_STATUS:
             /* Wait for Command out to drop */
             *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN);      /* Drop address in */
             *bus_in = ctx->status | odd_parity[ctx->status];
             log_device("2415: %03x initial status %02x\n",unit->addr, ctx->status);
             *tags |= (CHAN_STA_IN);
             ctx->state = STATE_STATUS_ACCEPT;    /* Wait for device to accept out status */
             break;

    /* Wait for CPU to either accept or stack status */
    case STATE_STATUS_ACCEPT:
             /* CPU will respond in a couple ways. */
             *tags &= ~(CHAN_SEL_OUT);
             *bus_in = ctx->status | odd_parity[ctx->status];
             if ((*tags & CHAN_CMD_OUT) != 0) {      /* CPU does not want status, stack it */
                 log_device("2415: %03x status stacked\n",unit->addr);
                 unit->stacked = 1;
                 ctx->state = STATE_STATUS_WAIT;
                 *tags &= ~(CHAN_OPR_IN|CHAN_STA_IN);
                 break;
             }
             if ((*tags & CHAN_SRV_OUT) != 0) {   /* CPU accepted the status, continue on */
                 log_device("2415: %03x status accepted c=%02x d=%d\n",unit->addr,
                              ctx->cmd, ctx->cmd_done);
                 if ((*tags & CHAN_SUP_OUT) != 0) {
                    ctx->chaining = 1;
                 } else {
                    ctx->chaining = 0;
                 }

                ctx->status = 0;
                *tags &= ~(CHAN_STA_IN);
                /* If end of command, and status accepted, all done */
                if (ctx->cmd_done || ctx->cmd == 0) {
                    *tags &= ~(CHAN_OPR_IN);
                    unit->stacked = 0;
                    ctx->data_end = 0;
                    ctx->data_end_post = 0;
                    ctx->cmd_done = 0;
                    ctx->cmd = 0;
                    ctx->busy = 0;
                    ctx->state = STATE_STATUS_WAIT;
                    ctx->rdy_flags &= ~(1 << ctx->cur_unit);
                    break;
                }

                if (ctx->data_end) {
                    if ((*tags & CHAN_HLD_OUT) == 0) {
                        *tags &= ~(CHAN_OPR_IN);
                    }
                    ctx->state = STATE_STATUS_WAIT;
                    break;
                }
                ctx->state = STATE_OPR;
                break;
             }
             if ((*tags & CHAN_ADR_OUT) != 0) {   /* CPU wants to talk to device */
                 ctx->state = STATE_IDLE;
                 *tags &= ~(CHAN_OPR_IN|CHAN_STA_IN);
             }
             break;

    /* Wait for CPU to disconnect from channel */
    case STATE_STATUS_WAIT:
             *tags &= ~(CHAN_SEL_OUT);
             if ((*tags & (CHAN_CMD_OUT|CHAN_SRV_OUT|CHAN_ADR_OUT)) == 0) {
                 if ((*tags & CHAN_HLD_OUT) == 0 || ctx->busy == 0) {
                     unit->selected = 0;
                     *tags &= ~(CHAN_OPR_IN);
                     ctx->state = STATE_IDLE;
                 } else {
                     ctx->state = STATE_WAIT_DEVEND;
                 }
             }
             break;

    /* On busy, wait for channel to drop select out */
    case STATE_BUSY:
             *bus_in = SNS_BSY|SNS_SMS | odd_parity[SNS_BSY|SNS_SMS];
             if ((*tags & CHAN_SEL_OUT) == 0) {
                 *tags &= ~(CHAN_SEL_OUT|CHAN_STA_IN);
                 unit->selected = 0;
                 ctx->state = STATE_IDLE;
                 /* If address out, halt device */
                 if ((*tags & CHAN_ADR_OUT) != 0) {
                     log_device("2415: %03x Halt IO\n",unit->addr);
                     if (ctx->data_end == 0) {
                         ctx->data_rdy = 0;
                         ctx->data_end = 1;
                         ctx->status |= SNS_CHNEND|SNS_DEVEND;
                         ctx->cmd_done = 1;
                         unit->request = 1;
                     }
                 }
             }
             *tags &= ~(CHAN_SEL_OUT);
             break;

    /* Present ending status to CPU */
    case STATE_END_STATUS:
             *tags &= ~(CHAN_SEL_OUT);
             /* Wait for both command out and service out to drop */
             if ((*tags & (CHAN_CMD_OUT|CHAN_SRV_OUT)) != 0) {
                  break;
             }
             *bus_in = ctx->status | odd_parity[ctx->status];
             *tags |= (CHAN_STA_IN);

             log_device("2415: %03x %02x end status %d\n",unit->addr, ctx->status, unit->request);
             ctx->state = STATE_END_ACCEPT;    /* Wait for CPU to accept out status */
             break;

     /* Wait for CPU to accept or stack status */
     case STATE_END_ACCEPT:
             *tags &= ~(CHAN_SEL_OUT);

             *bus_in = ctx->status | odd_parity[ctx->status];
             /* CPU does not want status right now. stack it */
             if ((*tags & CHAN_CMD_OUT) != 0) {
                 log_device("2415: %03x status stacked %d\n",unit->addr, unit->request);
                 unit->stacked = 1;
                 ctx->state = STATE_STATUS_WAIT;
                 *tags &= ~(CHAN_STA_IN|CHAN_OPR_IN);
                 break;
             }

             /* CPU accepted status */
             if ((*tags & CHAN_SRV_OUT) != 0) {

                 log_device("2415: %03x status accepted %d %02x\n",unit->addr, unit->request, ctx->status);
                 ctx->status = 0;
                 /* If end of command, and status accepted, all done */
                 if (ctx->cmd_done) {
                     *tags &= ~(CHAN_STA_IN|CHAN_OPR_IN);
                     unit->stacked = 0;
                     ctx->cmd = 0;
                     ctx->cmd_done = 0;
                     ctx->busy = 0;
                     ctx->state = STATE_STATUS_WAIT;
                     break;
                 }

                 if (ctx->data_end) {
                     *tags &= ~(CHAN_STA_IN|CHAN_OPR_IN);
                     ctx->state = STATE_STATUS_WAIT;
                     break;
                 }
                 /* Check if on selector channel */
                 if ((*tags & CHAN_HLD_OUT) != 0) {
                     *tags &= ~(CHAN_STA_IN);
                     ctx->state = STATE_WAIT_DEVEND;
                 } else {
                     /* Otherwise wait disconnect and let device connect when done */
                     *tags &= ~(CHAN_STA_IN|CHAN_OPR_IN);
                     ctx->state = STATE_STATUS_WAIT;
                 }
             }
             break;

      /* Wait on device to finish, before posting status */
      case STATE_WAIT_DEVEND:
            log_device("2415: %03x wait end b=%d cd=%d dly=%d %02x %02x\n",unit->addr,
                ctx->busy, ctx->cmd_done, ctx->delay, ctx->cmd, ctx->status);
             *tags &= ~(CHAN_SEL_OUT);
             if (ctx->cmd_done) {
                 unit->request = 0;
                 ctx->state = STATE_STATUS;
             }
             break;

      /* Handle normal operations */
      case STATE_OPR:
             log_device("2415: %03x opr %d r=%d e=%d\n",unit->addr, unit->selected,
                     ctx->data_rdy, ctx->data_end);
             unit->request = 0;
             *tags &= ~(CHAN_SEL_OUT);

             /* If address out, halt device */
             if ((*tags & CHAN_ADR_OUT) != 0) {
                 ctx->data_end = 1;
                 ctx->data_rdy = 0;
                 ctx->status |= SNS_CHNEND;
                 *tags &= ~(CHAN_OPR_IN);
                 unit->selected = 0;
                 if ((ctx->cmd & 0x10) != 0) {
                    ctx->delay = 1000;
                    ctx->state = STATE_END_STATUS;
                 } else {
                    ctx->state = STATE_IDLE;
                 }
                 break;
             }

             /* If data ready, try and get/send it */
             if (ctx->data_rdy) {
                 ctx->state = STATE_DATA_1;
                 break;
             }

             /* If sense command advance counter */
             if ((ctx->cmd & 0xf) == 0x04) {
                 if (ctx->sense_cnt < 6 && !ctx->data_end) {
                     ctx->data = ctx->sense[ctx->sense_cnt++];
                     ctx->data_rdy = 1;
                 } else {
                     ctx->data_end = 1;
                     ctx->cmd_done = 1;
                     ctx->busy = 0;
                     ctx->status |= SNS_CHNEND|SNS_DEVEND;
                }
             }

             /* If at end of data or command, present status */
             if (ctx->cmd_done) {
                 ctx->status |= SNS_DEVEND|SNS_CHNEND;
                 ctx->state = STATE_END_STATUS;
                 break;
             }

             break;

      case STATE_DATA_1:      /* Send or recieve data from CPU */
             *tags &= ~CHAN_SEL_OUT;
             if ((*tags & CHAN_SRV_OUT) != 0) {
                  /* Wait for service out to drop */
                  break;
             }
             if ((*tags & CHAN_SUP_OUT) != 0) {
                 /* If suppress out on, wait until sending request */
                 break;
             }
             *tags |= CHAN_SRV_IN;   /* Request tranfer */
             *bus_in = ctx->data | odd_parity[ctx->data];
             ctx->state = STATE_DATA_2;
             break;

      case STATE_DATA_2:      /* Complete transfer */
             *tags &= ~CHAN_SEL_OUT;
             *bus_in = ctx->data | odd_parity[ctx->data];
             if ((*tags & (CHAN_SRV_OUT|CHAN_CMD_OUT)) != 0) {
                 /* Wait for service out or command out */
                 *tags &= ~(CHAN_SRV_IN);   /* Clear service in request */
                 ctx->data_rdy = 0;
                 if ((ctx->cmd & 1) != 0) { /* Write command */
                      /* Device selected */
                      if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0) {
                          ctx->sense[0] |= SENSE_BUSCHK;
                          ctx->status |= (SNS_UNITCHK);
                          ctx->data_end = 1;
                          ctx->status |= SNS_CHNEND|SNS_DEVEND;
                          ctx->busy = 0;
                          ctx->cmd_done = 1;
                      } else {
                          ctx->data = bus_out; /* Grab data */
                      }
                 }
                 ctx->state = STATE_OPR;   /* Go to process this data */
                 if ((*tags & CHAN_CMD_OUT) != 0) {  /* CPU is done sending data */
                     ctx->data_end = 1;
                     ctx->status = SNS_CHNEND;
                 }
             }
             break;
    }
}
#if 0
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
                *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_SUP_OUT) ||
                *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_REQ_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_SUP_OUT|CHAN_REQ_IN)) &&
                 (bus_out & 0xf8) == ctx->addr && (bus_out & 0x7) < ctx->nunits) {
                 /* Device selected */
                 *tags &= ~(CHAN_SEL_OUT);      /* Clear select out and in */
                 if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0)
                     ctx->sense[0] |= SENSE_BUSCHK;
                 i = bus_out & 0x7;
                 /* Check if status pending for another unit */
                 if (ctx->stat_unit >= 0 && i != ctx->stat_unit) {
                     *tags |= CHAN_STA_IN;
                     *bus_in = 0x100 | SNS_SMS | SNS_BSY;
                     log_device("Pending status: %d u=%d\n", ctx->stat_unit, i);
                     ctx->addressed = 1;
                     ctx->busy_flag = 1;
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
             if ((!ctx->selected || !ctx->addressed) &&
                                   ctx->chain_flag == 0 && ctx->rdy_flags != 0) {
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
                     ctx->status |= SNS_BSY;
                     log_device(" Unit busy: u=%d %02x %02x %02x\n", ctx->cur_unit, ctx->sense[0], ctx->sense[1], ctx->sense[3]);
                     break;
                 }

                 /* Check if device is rewinding */
                 if (((ctx->rew_flags | ctx->run_flags) & (1 << ctx->cur_unit)) != 0) {
                     log_device("Unit busy rew=%02x run=%02x\n", ctx->rew_flags, ctx->run_flags);
                     ctx->status |= SNS_BSY;
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
                 ctx->chaining = 0;
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

                 /* If CMD out to initial select, stack the status */
                 if ((*tags & (CHAN_CMD_OUT|CHAN_STA_IN)) == (CHAN_CMD_OUT|CHAN_STA_IN)) {
                     log_device("test stacking inital\n");
                     *tags &= ~(CHAN_STA_IN|CHAN_OPR_IN);
                     ctx->state = STATE_STACK;
                     ctx->stacked = 1;
                     ctx->selected = 0;
                     break;
                 }

                 /* Wait for Command out to drop */
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
                         if (ctx->chain_flag == 0) {
                             *tags &= ~(CHAN_OPR_IN);  /* Clear operation in */
                             ctx->selected = 0;
                         }
                         ctx->state = STATE_WAIT;
                         ctx->status &= ~SNS_CHNEND;
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
                ctx->busy_flag = 1;
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
                  *tags &= ~CHAN_OPR_IN;
                  ctx->selected = 0;
                  ctx->data_end = 1;
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
                      ctx->busy_flag = 1;
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
                         ctx->busy_flag = 1;
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
              if (*tags == (CHAN_OPR_OUT)) {
                  *tags |= CHAN_REQ_IN;
                  ctx->request = 1;
              }
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
                  *tags |= CHAN_STA_IN;
                  *bus_in = ctx->status | odd_parity[ctx->status];
                  log_device("Tape Data End channel status %02x %02x %d\n", ctx->status,
                             ctx->cmd, ctx->chain_flag);
                  break;
              }

              /* Service out indicates status was excepted. If Suppress out, then command chaining */
              if ((*tags & (CHAN_SRV_OUT|CHAN_CMD_OUT)) != 0) {
                   if ((*tags & (CHAN_SUP_OUT)) == 0) {
                       ctx->selected = 0;
                       ctx->chain_flag = 0;
                       *tags &= ~(CHAN_OPR_IN);
                   }
                   if ((*tags & (CHAN_CMD_OUT)) != 0) {
                       ctx->stk_unit = ctx->cur_unit;
                       ctx->stacked = 1;
                   }
                   *tags &= ~(CHAN_STA_IN);             /* Clear Status in */
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

             /* If we replied with busy, return control end */
             if (ctx->busy_flag) {
                ctx->status |= SNS_CTLEND;
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
                 if ((*tags & (CHAN_SUP_OUT)) != 0) {
                     ctx->chain_flag = 1;
                 }
                 *tags &= ~(CHAN_OPR_IN|CHAN_STA_IN);               /* Clear Status in */
                 log_device("Tape Accepted\n");
                 ctx->selected = 0;
                 ctx->cmd = 0;
                 ctx->busy_flag = 0;
                 ctx->status = 0;
                 tape_unselect(ctx->tape[ctx->cur_unit]);
                 ctx->state = STATE_IDLE;              /* All done, back to idle state */
                 break;
             }

             /* Command out to status in indicates a stacking of status */
             if ((*tags & (CHAN_CMD_OUT)) != 0) {
                 if ((*tags & (CHAN_SUP_OUT)) != 0) {
                     ctx->chain_flag = 1;
                 }
                 *tags &= ~(CHAN_OPR_IN|CHAN_STA_IN);            /* Clear Status in */
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
                      ctx->busy_flag = 1;
                      ctx->addressed = 1;
                      log_device("Tape stack selected other\n");
                  }
                  break;
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
                ctx->busy_flag = 1;
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
                 ctx->request = 0;
                 /* If status pending for this other units, but not this one, return short busy */
                 if (ctx->rdy_flags != 0 && (ctx->rdy_flags & (1 << ctx->cur_unit)) == 0) {
                     *bus_in = 0x100 | SNS_SMS | SNS_BSY;
                     *tags |= CHAN_STA_IN;
                     ctx->busy_flag = 1;
                     break;
                 }
                 ctx->status = SNS_DEVEND;
                 *tags |= CHAN_OPR_IN;
 log_device("pending tape selected unit: %d\n", ctx->cur_unit);
                 break;
             }

             /* If we got bus, go and transfer */
             if (ctx->addressed && (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN))) {
                 *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN);       /* Clear select out and in */
                 ctx->addressed = 0;
                 ctx->selected = 1;
                 if ((ctx->rdy_flags & (1 << ctx->cur_unit)) == 0) {
                     ctx->state = STATE_IDLE;
                 } else {
                     ctx->rdy_flags &= ~(1 << ctx->cur_unit);
                     ctx->state = STATE_END;              /* Go wait for everything to drop */
                 }
                 log_device("Tape selected\n");
                 break;
              }

             /* If we got addressed, clear rdy bit and return DEV END */
             if (ctx->addressed && (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_IN|CHAN_OPR_IN) ||
                   *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_ADR_IN|CHAN_OPR_IN))) {
                 *tags &= ~(CHAN_SEL_OUT);               /* Clear select out and in */
                 *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                 *bus_in = ctx->addr | ctx->cur_unit;
                 *bus_in |= odd_parity[*bus_in];              /* Put out our address */
                 log_device("Tape return ready address\n");
                 break;
             }

             /* If we got selected in error, or returned busy */
             if (ctx->addressed && (*tags == (CHAN_OPR_OUT|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SUP_OUT|CHAN_STA_IN))) {
                 ctx->addressed = 0;
                 ctx->status = 0;
                 if (ctx->rdy_flags == 0) {
                     ctx->state = STATE_IDLE;
                 }
                 *tags &= ~(CHAN_SEL_OUT|CHAN_STA_IN);      /* Clear select out and in */
 log_device("pending tape deselected unit: %d\n", ctx->cur_unit);
                 break;
             }

             /* If we got addressed and are responding to busy, continue */
             if (ctx->addressed && (*tags == (CHAN_OPR_OUT|CHAN_ADR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_ADR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_STA_IN))) {
                 *tags &= ~(CHAN_SEL_OUT);               /* Clear select out and in */
                 *bus_in = 0x100 | SNS_SMS | SNS_BSY;
                 *tags |= CHAN_STA_IN;
                 break;
             }

             /* If channel polling, give out address */
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
                 ctx->addressed = 1;
                 *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);        /* Clear select out and in */
                 *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                 *bus_in = ctx->addr | ctx->cur_unit;
                 *bus_in |= odd_parity[*bus_in];              /* Put out our address */
log_device("Tape reselect for status %d\n", ctx->cur_unit);
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
              if (*tags == (CHAN_OPR_OUT)) {
                 log_device("Tape request ready\n");
                 *tags |= CHAN_REQ_IN;
                 ctx->request = 1;
             }
             break;
    }
}
#endif

/*
 * Create a 2415 tape device.
 */
static int
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
         tape->supply_color[i] = 2;
         tape->supply_label[i] = 1;
         tape->takeup_color[i] = 1;
         tape->takeup_label[i] = 0;
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
                   fmt = get_index(&opts, tape_format_type);
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
         dev2415->init_device = (void *)&model2415_init;
         dev2415->type_name = "2415";
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
                   fprintf(stderr, "Invalid option %s to 2415\n", opts.opt);
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


