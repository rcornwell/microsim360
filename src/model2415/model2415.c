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
#include "device.h"
#include "tape.h"
#include "model2415.h"

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
#define STATE_STACK_HLD 14    /* Stack hold */
#define STATE_WAIT      15    /* After data transfer wait motion */
#define STATE_RDY       16    /* Wait for selection to give status */

#define FRAME_DELAY     33    /* 34 us per frame delay */
#define REWIND_DELAY    10000
#define REW_FRAME       3840  /* Frames per 20ms */
#define START_DELAY     4000

static uint8_t       parity_table[64] = {
    /* 0    1    2    3    4    5    6    7 */
    0000, 0100, 0100, 0000, 0100, 0000, 0000, 0100,
    0100, 0000, 0000, 0100, 0000, 0100, 0100, 0000,
    0100, 0000, 0000, 0100, 0000, 0100, 0100, 0000,
    0000, 0100, 0100, 0000, 0100, 0000, 0000, 0100,
    0100, 0000, 0000, 0100, 0000, 0100, 0100, 0000,
    0000, 0100, 0100, 0000, 0100, 0000, 0000, 0100,
    0000, 0100, 0100, 0000, 0100, 0000, 0000, 0100,
    0100, 0000, 0000, 0100, 0000, 0100, 0100, 0000
};

static uint8_t       bcd_to_ebcdic[64] = {
     0x40, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
     0xf8, 0xf9, 0xf0, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
     0x7a, 0x61, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
     0xe8, 0xe9, 0xe0, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
     0x60, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
     0xd8, 0xd9, 0xd0, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
     0x50, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
     0xc8, 0xc9, 0xc0, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f};


struct _2415_context {
    int                    addr;         /* Device address */
    int                    chan;
    int                    state;        /* Current channel state */
    int                    selected;     /* Device currently selected */
    int                    sense[6];     /* Current sense value */
    int                    sense_cnt;    /* Sense counter */
    int                    chain_flag;   /* Command chaining in effect */
    int                    cmd;          /* Current command */
    int                    cmd_done;     /* Current read/write finished */
    int                    status;       /* Current bus status */
    uint8_t                data;         /* Current byte to send/recieve */
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
    int                    mode7;        /* Tape mode for 7 track tapes */
    int                    mode9;        /* Tape mode for 9 track tapes */
};


void
model2415_dev(struct _device *unit, uint16_t *tags, uint16_t bus_out, uint16_t *bus_in)
{
    struct _2415_context *ctx = (struct _2415_context *)unit->dev;
    int      i;
    int      r;

//    printf("Tape tags %d: ", ctx->state);
//    print_tags(*tags, bus_out);
    /* Reset device if OPER OUT is dropped */
    if ((*tags & (CHAN_OPR_OUT|CHAN_SUP_OUT)) == 0) {
 printf("Reset tape\n");
        if (ctx->selected) {
           *tags &= ~(CHAN_OPR_IN|CHAN_ADR_IN|CHAN_SRV_IN|CHAN_STA_IN);
        }
        ctx->selected = 0;
        ctx->state = STATE_IDLE;
        for (i = 0; i < 6; i++) 
           ctx->sense[i] = 0;
        ctx->cmd = 0;
        ctx->delay = 0;
        ctx->rdy_flags = 0;
        return;
    }

    if (ctx->delay == 0) {
        ctx->delay = FRAME_DELAY;
        if (ctx->rew_flags != 0) {
            printf("Rewind Low speed %02x %02x\n", ctx->rew_flags, ctx->rdy_flags);
            for ( i = 0; i < ctx->nunits; i++) {
                if (ctx->tape[i] == NULL || (ctx->rew_flags & (1 << i)) == 0)
                   continue;
                if (ctx->tape[i]->pos_frame > (5 * 12 * 1600))
                   continue;
                r = tape_rewind_frames(ctx->tape[i], 1);
                if (r == 0) {
                    printf("Rewind done %d\n", i);
                    ctx->rew_flags &= ~(1 << i);
                    if (ctx->run_flags & (1 << i)) {
                        tape_detach(ctx->tape[i]);
                        ctx->run_flags &= ~(1 << i);
                    } else if (ctx->chain_flag) {
                        ctx->state = STATE_END;
                        ctx->status = SNS_DEVEND|SNS_CHNEND;
                        ctx->cmd = 0;
                    } else {
                        if (ctx->cur_unit == i && ctx->state != STATE_IDLE) {
                           ctx->status &= ~(SNS_CTLEND|SNS_UNITCHK);
                           if (ctx->state == STATE_STACK) {
                               ctx->state = STATE_END;
                           }
                        } else {
                           ctx->rdy_flags |= 1 << i;
                        }
                    }
                }
            }
        }
 //       printf("Tape commnd scan %02x\n", ctx->cmd);
        switch (ctx->cmd & 0xf) {
        case 0: /* Test I/O */
        case 4: /* Sense */
        case 3: /* Mode command */
        case 0xd: /* Mode command */
               break;
        case 1: /* Write */
               printf("Do write command %d %d %d\n", ctx->data_end, ctx->data_rdy, ctx->state);
               if (ctx->cmd_done) {
                   r = tape_finish_rec(ctx->tape[ctx->cur_unit]);
                   ctx->state = STATE_END;
                   ctx->status |= SNS_DEVEND|SNS_CHNEND;
                   ctx->cmd = 0;
               } else if (ctx->data_rdy) {
                  /* Do a write start */
                   ctx->sense[0] &= ~(SENSE_WCZERO);
                   r = tape_write_frame(ctx->tape[ctx->cur_unit], ctx->data);
                   ctx->data_rdy = 0;
                   ctx->state = STATE_DATA_I;
               } else {
                   /* If no more data, all is ok */
                   if (ctx->data_end == 0) {
                       ctx->sense[0] |= SENSE_OVRRUN;
                       ctx->data_end = 1;
                   } else {
                       ctx->delay *= 3;
                       ctx->state = STATE_WAIT;
                   }
                   ctx->cmd_done = 1;
               }
               break;
        case 2:  /* Read */
        case 0xc:  /* Read backward */
               /* If CPU does not want any more data, just read and ignore
                  the rest of the data */
               printf("Do read command %d %d %d\n", ctx->data_end, ctx->data_rdy, ctx->state);
               if (ctx->cmd_done) {
                   ctx->state = STATE_END;
                   ctx->status |= SNS_DEVEND|SNS_CHNEND;
                   ctx->cmd = 0;
          printf("Tape Send end status \n");
               } else if (ctx->data_end) {
                   r = tape_read_frame(ctx->tape[ctx->cur_unit], &ctx->data);
                   printf("Tape read frame dataend %d\n", r);
                   if (r != 1) {
                       /* Process end */
                       r = tape_finish_rec(ctx->tape[ctx->cur_unit]);
       printf("Tape finish read %d\n", r);
                       ctx->delay *= 3;
                       ctx->cmd_done = 1;
                   }
               } else if (ctx->data_rdy) {
                   printf("Tape read frame overrun \n");
                   ctx->data_end = 1;
                   ctx->delay = 1;
                   ctx->sense[0] |= SENSE_OVRRUN;
                   ctx->status = SNS_UNITCHK;
               } else {
                   r = tape_read_frame(ctx->tape[ctx->cur_unit], &ctx->data);
                   printf("Tape read frame %d\n", r);
                   if (r < 0) {
                       /* Set up read error */
                   } else if (r == 0) {
                       /* End of record */
                       ctx->data_end = 1;
                       ctx->delay *= 3;
                       ctx->state = STATE_WAIT;
                   } else if (r == 2) {
                       /* Read of tape mark */
                       ctx->data_end = 1;
                       ctx->cmd_done = 1;
                       ctx->status = SNS_UNITEXP;
                       ctx->delay *= 3;
                   } else {
                       ctx->data_rdy = 1;
                       ctx->state = STATE_DATA_O;
                       printf("Tape Queued\n");
                   }
               }
               break;

        case 7:  /* Tape motion control */
        case 0xf:
               switch (ctx->cmd & 0xff) {
               case 0x0f:    /* Rewind and unload */
                    tape_start_rewind(ctx->tape[ctx->cur_unit]);
                    if (ctx->rew_delay == 0)
                        ctx->rew_delay = REWIND_DELAY;
                    ctx->rew_flags |= 1 << ctx->cur_unit;
                    ctx->run_flags |= 1 << ctx->cur_unit;
                    ctx->state = STATE_END;
                    ctx->status = SNS_CTLEND|SNS_DEVEND|SNS_UNITCHK;
                    ctx->cmd = 0;
                    break;
               case 0x07:    /* Rewind */
                    printf("start rewind %d\n", ctx->chain_flag);
                    tape_start_rewind(ctx->tape[ctx->cur_unit]);
                    ctx->rew_flags |= 1 << ctx->cur_unit;
                    if (ctx->rew_delay == 0)
                        ctx->rew_delay = REWIND_DELAY;
                    if (ctx->chain_flag == 0) {
                        ctx->state = STATE_END;
                        ctx->status = SNS_DEVEND;
                        ctx->cmd = 0;
                    }
                    break;
               case 0x17:    /* Erase gap */
                    ctx->delay *= 4;
                    ctx->status = SNS_DEVEND;
                    ctx->state = STATE_END;
                    break;
               case 0x1f:    /* Write tape mark */
                    ctx->delay = FRAME_DELAY;
                    if (ctx->cmd_done) {
                          printf("Write tape mark end\n");
                        ctx->delay *= 3;;
                        ctx->status = SNS_DEVEND;
                        ctx->state = STATE_END;
                    } else {
                        r = tape_write_mark(ctx->tape[ctx->cur_unit]);
                        printf("Write tape mark %d\n", r);
                        ctx->cmd_done = 1;
                    }
                    break;
               case 0x37:    /* Forward space block */
               case 0x3f:    /* Forward space file */
               case 0x27:    /* Backspace block */
               case 0x2f:    /* Backspace file */
                    ctx->delay = FRAME_DELAY;
                    r = tape_read_frame(ctx->tape[ctx->cur_unit], &ctx->data);
                    if (r < 0) {
                         /* Set up read error */
                         ctx->status = SNS_UNITCHK|SNS_DEVEND;
                    } else if (r == 0) {
                         /* Terminate of record read */
                         r = tape_finish_rec(ctx->tape[ctx->cur_unit]);
                         /* End of record */
                         if (tape_at_loadpt(ctx->tape[ctx->cur_unit])) {
                             ctx->status = SNS_UNITCHK|SNS_DEVEND;
                         } else {
                             if (ctx->cmd & 0x8) {
                                 if (ctx->cmd & 0x10) {
                                    r = tape_read_forw(ctx->tape[ctx->cur_unit]);
                                 } else {
                                    r = tape_read_back(ctx->tape[ctx->cur_unit]);
                                 }
                              printf("Tape start record %d\n", r);
                                 /* Handle error */
                                 if (r < 0) {
                                     /* If space block, set unit exception */
                                     ctx->status = SNS_UNITCHK|SNS_DEVEND;
                                 } else if (r == 2) {
                                     ctx->status = SNS_DEVEND;
                                     ctx->delay *= 3;   /* Inter record delay */
                                 }
                             } else {
                                 /* If space file, end */
                                 ctx->status = SNS_DEVEND;
                             }
                          }
                    }
                    if (ctx->status & SNS_DEVEND) {
                        ctx->state = STATE_END;
                    }
                    break;
               }
               break;
        default:
               break;
        }
    } else {
        ctx->delay--;
    }

    if (ctx->rew_delay == 0) {
        if (ctx->rew_flags != 0) {
            printf("Rewind delay %02x %02x\n", ctx->rew_flags, ctx->rdy_flags);
            for ( i = 0; i < 6; i++) {
                if (ctx->tape[i] == NULL || (ctx->rew_flags & (1 << i)) == 0)
                   continue;
                if (ctx->tape[i]->pos_frame <= (5 * 12 * 1600)) {
                   if (ctx->delay == 0)
                       ctx->delay = FRAME_DELAY;
                   continue;
                }
                r = tape_rewind_frames(ctx->tape[i], REW_FRAME);
                if (r == 0) {
                    printf("Rewind done %d\n", i);
                    ctx->rew_flags &= ~(1 << i);
                    if (ctx->run_flags & (1 << i)) {
                        tape_detach(ctx->tape[i]);
                        ctx->run_flags &= ~(1 << i);
                    } else if (ctx->chain_flag) {
                        ctx->state = STATE_END;
                        ctx->status = SNS_DEVEND|SNS_CHNEND;
                        ctx->cmd = 0;
                    } else {
                        if (ctx->cur_unit == i && ctx->state != STATE_IDLE) {
                           ctx->status &= ~(SNS_CTLEND|SNS_UNITCHK);
                           if (ctx->state == STATE_STACK) {
                               ctx->state = STATE_END;
                           }
                        } else {
                           ctx->rdy_flags |= 1 << i;
                        }
                    }
                }
            }
            ctx->rew_delay = REWIND_DELAY;
        }
    } else {
        ctx->rew_delay--;
    }

    switch (ctx->state) {
    case STATE_IDLE:
            /* Wait until Channels asks for us */
            if ((*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT) ||
                *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_SUP_OUT)) &&
                 (bus_out & 0xf8) == ctx->addr && (bus_out & 0x7) < ctx->nunits) {
                 /* Device selected */
                 if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0)
                     ctx->sense[0] |= SENSE_BUSCHK;
                 i = bus_out & 0x7;
                /* Check if device is rewinding */
                 if (((ctx->rew_flags | ctx->run_flags) & (1 << i)) != 0) {
                      printf("Unit busy rew=%02x run=%02x\n", ctx->rew_flags, ctx->run_flags);
                      *tags &= ~(CHAN_SEL_OUT);      /* Clear select out and in */
                      *tags |= CHAN_STA_IN;
                      *bus_in = 0x100 | SNS_SMS | SNS_BSY;
                      ctx->selected = 1;
                      break;
                 }
                 ctx->cur_unit = i;
                 ctx->status = 0;
                 ctx->stk_unit = -1;
                 *tags &= ~(CHAN_SEL_OUT);      /* Clear select out and in */
                 *tags |= CHAN_OPR_IN;
                 ctx->state = STATE_SEL;
                 ctx->chain_flag = 0;
                 ctx->selected = 1;
 printf("tape selected unit: %d\n", ctx->cur_unit);
             }

             /* If we are returning short busy keep value on bus */
             if (ctx->selected && *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_STA_IN)) {
                 *bus_in = 0x100 | SNS_SMS | SNS_BSY;
                 break;
             }

             if (ctx->selected && *tags == (CHAN_OPR_OUT|CHAN_ADR_OUT|CHAN_STA_IN)) {
                 *tags &= ~(CHAN_STA_IN);      /* Clear Status in */
                 ctx->selected = 0;
                 break;
             }

             /* Scan for rewind done, or unit becoming ready */
             if (!ctx->selected && ctx->rdy_flags != 0) {
                 /* Put request in up */
                 ctx->state = STATE_RDY;
             }
             break;

    case STATE_SEL:
            /* Wait until address out drops to put our address on bus */
            *tags |= CHAN_OPR_IN;
            /* When address out drops put our address on bus in */
            if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_SUP_OUT) ||
                *tags == (CHAN_OPR_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_OPR_IN|CHAN_ADR_IN|CHAN_SUP_OUT)) {
                 *tags |= CHAN_ADR_IN;      /* Return address until accepted */
                 *bus_in = (ctx->addr & 0xf8) | ctx->cur_unit;
                 *bus_in |= odd_parity[*bus_in];
 //printf("tape address\n");
            }

            /* Wait for Command out to raise */
            /* Can now drop address in */
            if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
 //printf("tape command %02x\n", bus_out);
                 ctx->state = STATE_CMD; /* Wait for command out to return
                                             initial status */
                 *tags &= ~(CHAN_ADR_IN);                 /* Clear address in */
                 if (ctx->stat_unit >= 0 && ctx->cur_unit != ctx->stat_unit) {
                     ctx->status = SNS_SMS|SNS_BSY;
                     *tags &= ~(CHAN_SEL_OUT);     /* Clear select out and in */
//printf(" Pending status: %d u=%d\n", ctx->stat_unit, ctx->cur_unit);
                     break;
                 }

                 /* If current unit has pending status and not sense, return busy */
                 if (ctx->cur_unit == ctx->stat_unit && bus_out & 0xff != 0x4 &&
                     (ctx->sense[0] != 0 || (ctx->sense[1] & SENSE_NOISE) != 0 ||
                     (ctx->sense[3] & (SENSE_VCR|SENSE_LRCR)) != 0)) {
                     ctx->status = SNS_BSY;
                     *tags &= ~(CHAN_SEL_OUT);     /* Clear select out and in */
//printf(" Unit busy: u=%d %02x %02x %02x\n", ctx->cur_unit, ctx->sense[0], ctx->sense[1], ctx->sense[3]);
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
                 ctx->delay = START_DELAY;
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
                        if (ctx->tape[ctx->cur_unit] == 0) {
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
                            //printf("Invalid command %x02\n", ctx->cmd);
                            ctx->sense[0] = SENSE_CMDREJ;
                            break;
                        }
                        /* Check if tape attached */
                        if (ctx->tape[ctx->cur_unit] == 0) {
                            //printf("Tape not attached %d\n", ctx->cur_unit);
                            ctx->sense[0] = SENSE_INTERV;
                            break;
                        }
                        /* Do a read start */
                        r = tape_read_forw(ctx->tape[ctx->cur_unit]);
                        //printf("Tape read forw %d : %d\n", ctx->cur_unit, r);
                        if (r < 0) {
                            ctx->sense[0] = SENSE_INTERV;
                        }
                        break;
                 case 3: /* Mode command */
                        ctx->sense[0] = 0;
                        ctx->sense[2] = SENSE_2;
                        ctx->sense[3] = 0;
                        ctx->sense[4] = 0;
                        if ((ctx->sense[1] & SENSE_7TRACK) != 0)
                            ctx->mode7 = ctx->cmd;
                        else
                            ctx->mode9 = ctx->cmd;
                        ctx->cmd = 0;
                        ctx->cmd_done = 1;
                        ctx->data_end = 1;
                        ctx->status = (SNS_DEVEND|SNS_CHNEND);
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
                            if (ctx->tape[ctx->cur_unit] == 0) {
                                ctx->sense[0] = SENSE_INTERV;
                                break;
                            }
                            /* If at load point. abort */
                            if (ctx->sense[1] & SENSE_LOAD) {
                                ctx->cmd = 0;
                                ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                                break;
                            }
                            /* Do a read start */
                            r = tape_read_back(ctx->tape[ctx->cur_unit]);
                            //printf("Tape read back %d : %d\n", ctx->cur_unit, r);
                            if (r < 0) {
                                ctx->sense[0] = SENSE_INTERV;
                            }
                        } else if (ctx->cmd == 0x4) {
                            ctx->sense_cnt = 0;
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
                        if (ctx->tape[ctx->cur_unit] == 0) {
                            ctx->sense[0] = SENSE_INTERV;
                            break;
                        }
                        switch (ctx->cmd & 0xff) {
                        case 0x17:    /* Erase gap */
                        case 0x1f:    /* Write tape mark */
                              ctx->data_end = 1;
                              ctx->delay = START_DELAY;
                              break;

                        case 0x07:    /* Rewind */
                        case 0x0f:    /* Rewind and unload */
                              ctx->data_end = 1;
                              ctx->delay = 33;
                              break;
                        case 0x37:    /* Forward space block */
                        case 0x3f:    /* Forward space file */
                             ctx->data_end = 1;
                             /* Do a read start */
                             r = tape_read_forw(ctx->tape[ctx->cur_unit]);
                             if (r == 1) {
                                 ctx->delay = START_DELAY;
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
                                ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                                break;
                            }
                            /* Do a read start */
                            r = tape_read_back(ctx->tape[ctx->cur_unit]);
                            if (r == 1) {
                                ctx->delay = START_DELAY;
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
                        }
                        break;
                 default:
                        ctx->cmd = 0;
                        ctx->sense[0] = SENSE_CMDREJ;     /* Invalid command */
                        ctx->data_end = 1;
                        break;
                 }
                 if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0) {
                     ctx->cmd = 0;
                     ctx->sense[0] |= SENSE_BUSCHK;
                     tape_unselect(ctx->tape[ctx->cur_unit]);
                 }
                 if (ctx->cmd != 4 && ((ctx->sense[0] & ~(SENSE_WCZERO)) != 0 ||
                                         (ctx->sense[1] & BIT7) != 0)) {
                     ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                     tape_unselect(ctx->tape[ctx->cur_unit]);
                     ctx->cmd = 0;
                 }
//                 if (ctx->sense[0] != 0) {
 //                    ctx->status |= (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
  //               }
                 if (ctx->data_end != 0) {
                     ctx->status |= SNS_CHNEND;
                 }
             }
             *tags &= ~(CHAN_SEL_OUT);             /* Clear select out and in */
             break;

    case STATE_CMD:
             /* Wait for Command out to drop */
             /* On MPX channel select out will drop, along with command */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_SUP_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SUP_OUT|CHAN_OPR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_OPR_IN)) {
                  *tags |= CHAN_OPR_IN|CHAN_STA_IN;     /* Wait for acceptance of status */
                 if ((*tags & CHAN_SUP_OUT) != 0)
                     ctx->chain_flag = 1;
 //printf("init stat %02x %d\n", ctx->status, ctx->chain_flag);
             }

             /* When we get acknowlegment, go wait for it to go away */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_SUP_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SUP_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~CHAN_STA_IN;
                 if ((*tags & CHAN_SUP_OUT) != 0)
                     ctx->chain_flag = 1;
                 ctx->state = STATE_INIT_STAT;
 //printf("init stat 2 %02x\n", ctx->status);
             }
             /* Return initial status */
             *bus_in = ctx->status | odd_parity[ctx->status];
             /* Device selected */
             *tags &= ~(CHAN_SEL_OUT);              /* Clear select out and in */
             break;

    case STATE_INIT_STAT:
             /* Wait for Service out to drop */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_OPR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SUP_OUT|CHAN_OPR_IN)) {
                 /* If SELECT DEVICE */
                 if ((ctx->status & (SNS_DEVEND|SNS_UNITCHK|SNS_UNITEXP)) != 0) {
                     *tags &= ~(CHAN_OPR_IN);           /* Clear operation in */
                     ctx->state = STATE_IDLE;
                     ctx->selected = 0;
                 } else
                     ctx->state = STATE_OPR;
                 if ((ctx->status & SNS_CHNEND) != 0) {
                     *tags &= ~(CHAN_OPR_IN);           /* Clear operation in */
                     ctx->state = STATE_WAIT;
                     ctx->selected = 0;
                 }
                 /* If test I/O or no command back to idle state */
                 if (ctx->cmd == 0) {
                     if ((*tags & CHAN_SEL_OUT) == 0) {
                         *tags &= ~(CHAN_OPR_IN);           /* Clear operation in */
                         ctx->selected = 0;
                     }
                     ctx->state = STATE_IDLE;
                 }
 //printf("state done\n");
             }
             *tags &= ~(CHAN_SEL_OUT);              /* Clear select out and in */
             break;

    case STATE_OPR:
 //printf("opr %d\n", ctx->selected);
             /* Wait for Command out to drop */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN) ||
                 *tags == (CHAN_CMD_OUT|CHAN_OPR_OUT|CHAN_OPR_IN)) {
                 break;
             }

             /* Send data if sense command */
             if (ctx->cmd == 0x4) {
printf("Sense %d:%02x\n", ctx->sense_cnt, ctx->sense[ctx->sense_cnt]);
                 /* Done after sending status */
                 if (ctx->sense_cnt == 6) {
                     ctx->status |= SNS_CHNEND|SNS_DEVEND;
                     ctx->state = STATE_END;
                 } else {
                     ctx->data = ctx->sense[ctx->sense_cnt];
                     ctx->sense_cnt++;
                     ctx->data_rdy = 1;
                     ctx->state = STATE_DATA_O;
                }
             }

             /* If writing need a data byte ready before we get there */
             if (ctx->sense[1] & SENSE_WRITE && ctx->data_rdy == 0 && ctx->data_end == 0) {
                 ctx->state = STATE_DATA_I;
                 break;
             }

             /* If we get select out with address out, reselection */
             if (!ctx->selected &&
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT) &&
                (bus_out & 0xf8) == ctx->addr && (bus_out & 0x7) < ctx->nunits) {
                /* Device selected */
                *tags &= ~(CHAN_SEL_OUT);              /* Clear select out and in */
                *tags |= CHAN_STA_IN;                  /* Indicate busy status */
                *bus_in = SNS_SMS|SNS_BSY|0x100;
                ctx->selected = 0;
//printf("reselect\n");
                break;
             }

             /* On Select channel, Select Out will not drop */
             /* Catch halt I/O */
             if (ctx->selected && *tags == (CHAN_OPR_OUT|CHAN_ADR_OUT|CHAN_OPR_IN) &&
                 (bus_out & 0xf8) == ctx->addr && (bus_out & 0x7) == ctx->cur_unit) {
                  /* Halt I/O */
                  /* Device selected */
//                  *tags &= ~(CHAN_OPR_IN);             /* Clear select out and in */
                  *bus_in = SNS_CHNEND;
                  ctx->data_end = 1;
                  ctx->state = STATE_DATA_END;          /* Return busy status */
 //                 ctx->selected = 0;
//printf("Halt i/o\n");
                  break;
             }

             /* Return status while waiting for Address out to drop */
             if (ctx->selected && *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_STA_IN) &&
                 (bus_out & 0xf8) == ctx->addr && (bus_out & 0x7) == ctx->cur_unit) {
                /* Device selected */
                *tags &= ~(CHAN_SEL_OUT);              /* Clear select out and in */
                *tags |= CHAN_STA_IN;                  /* Indicate busy status */
                ctx->data_end = 1;
                *bus_in = SNS_CHNEND|SNS_DEVEND|0x100;
                break;
             }

             /* If we are selected clear select in */
             if (ctx->selected)
                 *tags &= ~CHAN_SEL_OUT;

             break;

    case STATE_REQ:   /* Data available and we are not talking on channel */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_REQ_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_REQ_IN)) {
                 /* Put our address on the bus */
                 *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);        /* Clear select out and in */
                 *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                 *bus_in = ctx->addr | ctx->cur_unit;
                 *bus_in |= odd_parity[*bus_in];              /* Put out our address */
//printf("Reselect\n");
                 break;
             }

             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                 /* Put our address on the bus */
                 *tags &= ~(CHAN_SEL_OUT);                    /* Clear select out and in */
                 *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                 *bus_in = ctx->addr | ctx->cur_unit;
                 *bus_in |= odd_parity[*bus_in];              /* Put out our address */
//printf("Address\n");
                 break;
             }

             /* If we got bus, go and transfer */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                  *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN);  /* Clear select out and in */
                  ctx->selected = 1;
                  ctx->state = STATE_OPR;              /* Go wait for everything to drop */
//printf("selected\n");
              }

              /* See if another device got it. */
              if ((*tags & (CHAN_OPR_IN|CHAN_STA_IN)) != 0) {
                  /* Drop request out until channel free again */
                  break;
              }
              /* Put request in up */
              *tags |= CHAN_REQ_IN;
              break;

    case STATE_DATA_I:    /* Request data from Channel, wait ready */
              /* If we are not connected, go request bus */
              if (ctx->selected == 0) {
                  ctx->state = STATE_REQ;
                  break;
              }
              /* Wait for command out to drop */
              if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_SRV_IN) ||
                  *tags == (CHAN_OPR_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_SRV_IN)) {
                   *tags &= ~(CHAN_SEL_OUT|CHAN_SRV_IN);  /* Clear select out and in */
                   /* Device selected */
                   if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0) {
                       ctx->sense[0] |= SENSE_BUSCHK;
                       ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                       ctx->state = STATE_END;
                   } else {
                       ctx->data = (bus_out & 0xff); /* Grab data */
                       ctx->data_rdy = 1;
                       ctx->state = STATE_INIT_STAT;       /* Wait for channel to be idle again */
                   }
                   break;
              }
              if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_SRV_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_SRV_IN)) {
                   *tags &= ~(CHAN_SEL_OUT|CHAN_SRV_IN);  /* Count reached zero, no more accepted */
                   ctx->status = SNS_CHNEND;
                   ctx->data_end = 1;
                   ctx->state = STATE_OPR;        /* Back to operation. */
                   break;
              }
              /* Put request in up */
              *tags |= CHAN_OPR_IN|CHAN_SRV_IN;
             /* If we are selected clear select in */
             if (ctx->selected)
                 *tags &= ~CHAN_SEL_OUT;

              break;

    case STATE_DATA_O:    /* Request to send data to channel */
printf("Tape Data output %02x %d\n", ctx->data, ctx->selected);
              /* If we are not connected, go request bus */
              if (ctx->selected == 0) {
                  ctx->state = STATE_REQ;
                  break;
              }
              *tags |= CHAN_OPR_IN|CHAN_SRV_IN;
              *bus_in = ctx->data | odd_parity[ctx->data];
              /* Wait for data to be accepted */
              if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_SRV_IN) ||
                  *tags == (CHAN_OPR_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_SRV_IN)) {
                   *tags &= ~(CHAN_SEL_OUT|CHAN_SRV_IN);  /* Clear select out and in */
                   ctx->data_rdy = 0;
                   ctx->state = STATE_INIT_STAT;       /* Wait for channel to be idle again */
//printf("Data sent\n");
              }

              /* CMD out indicates that the channel will accept no more data */
              if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_SRV_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_SRV_IN)) {
                   *tags &= ~(CHAN_SEL_OUT|CHAN_SRV_IN);  /* Count reached zero, no more accepted */
                   if (ctx->cmd == 0x4) {
                       ctx->status = SNS_CHNEND|SNS_DEVEND;
                       ctx->state = STATE_END;
                   } else {
                       ctx->data_end = 1;
                       ctx->status |= SNS_CHNEND;
                       ctx->state = STATE_OPR;
                   }
//printf("Data End\n");
                   break;
              }
              /* If we are selected clear select in */
              if (ctx->selected)
                 *tags &= ~CHAN_SEL_OUT;
              break;

    case STATE_DATA_END:  /* Wait for IDLE bus */
             if (ctx->selected == 0) {
                 if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_REQ_IN) ||
                     *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT)) {
                     /* If select out, wait for channel available again */
                     if ((bus_out & 0xf8) == ctx->addr) {
                         /* If it us, start initial selection */
                         if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0)
                             ctx->sense[0] |= SENSE_BUSCHK;
                         *tags &= ~(CHAN_SEL_OUT);      /* Clear select out and in */
                         if (ctx->cur_unit != bus_out & 0x7) {
                             *bus_in = SNS_BSY;
                             *tags |= CHAN_STA_IN;
                             ctx->selected = 1;
                     //        printf("selected other unit\n");
                             break;
                         }
                         *tags |= CHAN_OPR_IN;
                         ctx->state = STATE_SEL;
                         ctx->selected = 1;
                      //   printf("selected\n");
                     }
                     break;
                 }


                 /* If we get Select out, and are requesting service, give our address */
                 if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_REQ_IN) ||
                     *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_REQ_IN)) {
                     /* Put our address on the bus */
                     *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);        /* Clear select out and in */
                     *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                      *bus_in = ctx->addr | ctx->cur_unit;
                      *bus_in |= odd_parity[*bus_in];              /* Put out our address */
//                     printf("Reselect\n");
                     break;
                 }


                 if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                     *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                     /* Put our address on the bus */
                     *tags &= ~(CHAN_SEL_OUT);                    /* Clear select out and in */
                     *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                     *bus_in = ctx->addr | ctx->cur_unit;
                     *bus_in |= odd_parity[*bus_in];              /* Put out our address */
 //                    printf("Address\n");
                     break;
                 }

                 /* If we got bus, go and transfer */
                 if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                     *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                      *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN);  /* Clear select out and in */
                      ctx->selected = 1;
  //                    printf("selected\n");
                  }

                  /* See if another device got it. */
                  if ((*tags & (CHAN_OPR_IN|CHAN_STA_IN)) != 0) {
                      /* Drop request out until channel free again */
                      break;
                  }
                  /* Put request in up */
                  *tags |= CHAN_REQ_IN;
                  /* If we are selected clear select in */
                  if (ctx->selected)
                     *tags &= ~CHAN_SEL_OUT;
                  break;
             }

             /* Wait for Service out to drop */
             if (ctx->selected && (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                          *tags == (CHAN_OPR_OUT|CHAN_OPR_IN))) {
                 *tags &= ~(CHAN_SEL_OUT);  /* Clear select out and in */
printf("Tape End channel status %02x %02x\n", ctx->status, ctx->cmd);
                 *tags |= CHAN_OPR_IN|CHAN_STA_IN;
                 *bus_in = ctx->status & SNS_CHNEND;
                 *bus_in |= odd_parity[*bus_in];
                 break;
             }

             /* If another unit, remove status in */
             if (ctx->selected && *tags == (CHAN_OPR_OUT|CHAN_STA_IN) ||
                     *tags == (CHAN_OPR_OUT|CHAN_STA_IN)) {
                    *tags &= ~(CHAN_SEL_OUT|CHAN_STA_IN);  /* Clear select out and in */
             }

             /* Service out indicates status was excepted. If Suppress out, then command chaining */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SRV_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 if ((*tags & CHAN_SEL_OUT) == 0) {
                      ctx->selected = 0;
                      *tags &= ~(CHAN_OPR_IN);          /* Clear Operation in if not selected */
                 }
                 if (ctx->cmd == 0) {
                     ctx->state = STATE_IDLE;
                 }
                 *tags &= ~(CHAN_SEL_OUT|CHAN_STA_IN);  /* Clear select out and in */
//printf("Accepted\n");
                  ctx->status &= ~SNS_CHNEND;
                  ctx->state = STATE_WAIT;            /* Wait for operation to finish */
                  break;
             }

             /* Response of CMD out indicates that channel wants to stack the status. */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~(CHAN_SEL_OUT|CHAN_OPR_IN|CHAN_STA_IN);  /* Clear select out and in */
printf("Tape Stacked\n");
                  ctx->selected = 0;
                  ctx->stk_unit = ctx->cur_unit;
                  ctx->state = STATE_STACK;                         /* Stack status */
                  break;
             }

             *bus_in = ctx->status & SNS_CHNEND;
             *bus_in |= odd_parity[*bus_in];
             /* Mark channel still in use */
             *tags &= ~(CHAN_SEL_OUT);  /* Clear select out and in */
             *tags |= CHAN_OPR_IN;
             break;

    case STATE_END:
             /* Set status flags if status pending */
             ctx->stat_unit = -1;
             if ((ctx->status & (SNS_UNITCHK)) != 0) {
                 ctx->stat_unit = ctx->cur_unit;
             }
             /* Wait until end delay to report end status */
             if (ctx->selected == 0) {
                if ((*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_REQ_IN) ||
                    *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_SUP_OUT|CHAN_REQ_IN)) &&
                     (bus_out & 0xf8) == ctx->addr) {
                     /* Device selected */
                     if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0)
                         ctx->sense[0] |= SENSE_BUSCHK;
                     ctx->selected = 1;
                     *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);      /* Clear select out and in */
                     if (ctx->cur_unit != bus_out & 0x7) {
                         *bus_in = SNS_BSY;
                         *tags |= CHAN_STA_IN;
                         printf("Tape selected other unit\n");
                         break;
                     }
                     *tags |= CHAN_OPR_IN;
                     printf("Tape selected\n");
                     break;
                 }
                 if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                    *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN)) {
                     /* Put our address on the bus */
                     *tags &= ~(CHAN_SEL_OUT);                    /* Clear select out and in */
                     *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                     *bus_in = ctx->addr | ctx->cur_unit;
                     *bus_in |= odd_parity[*bus_in];              /* Put out our address */
                     printf("Tape Reselect\n");
                     break;
                 }
                 if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_REQ_IN) ||
                     *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_REQ_IN)) {
                     /* Put our address on the bus */
                     *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);        /* Clear select out and in */
                     *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                     *bus_in = ctx->addr | ctx->cur_unit;
                     *bus_in |= odd_parity[*bus_in];              /* Put out our address */
                     printf("Tape Reselect\n");
                     break;
                 }

                 if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                     *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                     /* Put our address on the bus */
                     *tags &= ~(CHAN_SEL_OUT);                    /* Clear select out and in */
                     *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                     *bus_in = ctx->addr | ctx->cur_unit;
                     *bus_in |= odd_parity[*bus_in];              /* Put out our address */
                     printf("Tape Address\n");
                     break;
                 }

                 /* If we got bus, go and transfer */
                 if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                     *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                      *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN);  /* Clear select out and in */
                      ctx->selected = 1;
                      printf("Tape selected\n");
                  }

                  /* See if another device got it. */
                  if ((*tags & (CHAN_OPR_IN|CHAN_STA_IN)) != 0) {
                      /* Drop request out until channel free again */
                      break;
                  }
                  /* Put request in up */
                  *tags |= CHAN_REQ_IN;
                  /* If we are selected clear select in */
                  if (ctx->selected)
                     *tags &= ~CHAN_SEL_OUT;
                  break;
             }

             /* Wait for Service out to drop */
             if (ctx->selected && (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                          *tags == (CHAN_OPR_OUT|CHAN_SUP_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                          *tags == (CHAN_OPR_OUT|CHAN_OPR_IN))) {
                 *tags &= ~(CHAN_SEL_OUT);  /* Clear select out and in */
printf("Tape End status %02x %02x\n", ctx->status, ctx->cmd);
                 *tags |= CHAN_STA_IN;
                 *bus_in = ctx->status | odd_parity[ctx->status];
                 ctx->cmd = 0;
                 break;
             }

             /* If another unit, remove status in */
             if (ctx->selected && *tags == (CHAN_OPR_OUT|CHAN_STA_IN) ||
                     *tags == (CHAN_OPR_OUT|CHAN_STA_IN)) {
                    *tags &= ~(CHAN_SEL_OUT|CHAN_STA_IN);  /* Clear select out and in */
             }

             /* Service out indicates status was excepted. If Suppress out, then command chaining */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SRV_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~(CHAN_SEL_OUT|CHAN_OPR_IN|CHAN_STA_IN);  /* Clear select out and in */
//printf("Accepted\n");
                  ctx->selected = 0;
                  tape_unselect(ctx->tape[ctx->cur_unit]);
                  ctx->state = STATE_IDLE;           /* All done, back to idle state */
                  break;
             }

             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~(CHAN_SEL_OUT|CHAN_OPR_IN|CHAN_STA_IN);  /* Clear select out and in */
printf("Tape Stacked\n");
                  ctx->selected = 0;
                  ctx->stk_unit = ctx->cur_unit;
                  ctx->state = STATE_STACK;                         /* Stack status */
                  break;
             }

             *bus_in = ctx->status | odd_parity[ctx->status];
             /* Mark channel still in use */
             *tags &= ~(CHAN_SEL_OUT);  /* Clear select out and in */
             *tags |= CHAN_OPR_IN;
printf("Tape End status ready\n");
             break;

    case STATE_STACK:  /* Stacked status */
             /* Wait until Channels asks for us */
             if ((*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT) ||
                  *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_SUP_OUT)) &&
                  (bus_out & 0xf8) == ctx->addr && (bus_out & 0x7) == ctx->stk_unit) {
                  /* Device selected */
                  if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0)
                      ctx->sense[0] |= SENSE_BUSCHK;
                  *tags &= ~(CHAN_SEL_OUT);      /* Clear select out and in */
                  *tags |= CHAN_OPR_IN;
                  ctx->state = STATE_STACK_SEL;
                  ctx->selected = 1;
 printf("Tape stack selected\n");
                  break;
             }
             break;

    case STATE_STACK_SEL:  /* Stacked status selected */
            /* Wait until address out drops to put our address on bus */
            *tags |= CHAN_OPR_IN;
            /* When address out drops put our address on bus in */
            if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_SUP_OUT) ||
                *tags == (CHAN_OPR_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_OPR_IN|CHAN_ADR_IN|CHAN_SUP_OUT)) {
                 *tags |= CHAN_ADR_IN;      /* Return address until accepted */
                 *bus_in = (ctx->addr & 0xf8) | ctx->stk_unit;
                 *bus_in |= odd_parity[*bus_in];
printf("tape stacked address\n");
            }

            /* Wait for Command out to raise */
            /* Can now drop address in */
            if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
 //printf("tape command %02x\n", bus_out);
                 ctx->state = STATE_STACK_CMD; /* Wait for command out to return
                                             initial status */
                 *tags &= ~(CHAN_ADR_IN);                 /* Clear address in */
             }
             *tags &= ~(CHAN_SEL_OUT);             /* Clear select out and in */
             break;

    case STATE_STACK_CMD:
             /* Wait for Command out to drop */
             /* On MPX channel select out will drop, along with command */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_OPR_IN)) {
                  *tags |= CHAN_OPR_IN|CHAN_STA_IN;     /* Wait for acceptance of status */
 printf("tape stack init stat\n");
             }

             /* When we get acknowlegment, go wait for it to go away */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_SUP_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~(CHAN_STA_IN|CHAN_OPR_IN);
                 ctx->state = STATE_IDLE;
 printf("tape stack init stat\n");
             }
             /* When we get acknowlegment, go wait for it to go away */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_SUP_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~(CHAN_STA_IN|CHAN_OPR_IN);
                 ctx->state = STATE_STACK;
 printf("tape stack init stat\n");
             }
             *bus_in = ctx->status | odd_parity[ctx->status];   /* Set initial status */
             /* Device selected */
             *tags &= ~(CHAN_SEL_OUT);                  /* Clear select out and in */
             break;

    case STATE_STACK_HLD:
             /* Wait for Service out to drop */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_OPR_IN)) {
                 ctx->stk_unit = -1;
                 if (ctx->cmd == 0 || (ctx->status & (SNS_UNITCHK|SNS_UNITEXP)) != 0) {
                     ctx->state = STATE_IDLE;
                 } else {
                     ctx->state = STATE_OPR;
     printf("tape state done\n");
                 }
                 *tags &= ~CHAN_OPR_IN;
                 ctx->selected = 0;
             }
             *tags &= ~(CHAN_SEL_OUT);                 /* Clear select out and in */
             break;

    case STATE_WAIT:
 printf("Tape wait %d\n", ctx->selected);
             /* If we get select out with address out, reselection */
             if (!ctx->selected &&
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT) &&
                (bus_out & 0xf8) == ctx->addr && (bus_out & 0x7) < ctx->nunits) {
                /* Device selected */
                *tags |= CHAN_STA_IN;
                *bus_in = 0x100 | SNS_SMS | SNS_BSY;
                ctx->selected = 1;
printf("Tape wait select attempt %d\n", ctx->cur_unit);
             }

             /* If we get select out with address out, reselection */
             if (ctx->selected &&
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_STA_IN)) {
                /* Device selected */
                *bus_in = 0x100 | SNS_SMS | SNS_BSY;
printf("Tape wait busy status %d\n", ctx->cur_unit);
             }

             /* If selected clear status in when select out drops */
             if (ctx->selected && (*tags == (CHAN_OPR_OUT|CHAN_STA_IN)) ||
                                   *tags == (CHAN_OPR_OUT|CHAN_ADR_OUT|CHAN_STA_IN) ||
                                   *tags == (CHAN_OPR_OUT|CHAN_SUP_OUT|CHAN_ADR_OUT|CHAN_STA_IN) ||
                                   *tags == (CHAN_OPR_OUT|CHAN_SUP_OUT|CHAN_STA_IN)) {
                /* Device selected */
                *tags &= ~(CHAN_STA_IN);              /* Clear status in */
                ctx->selected = 0;
printf("Tape wait deselect\n");
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
                 ctx->selected = 1;
                 if ((ctx->rdy_flags & (1 << ctx->cur_unit)) == 0) {
                     ctx->status = SNS_BSY;
                     *tags |= CHAN_STA_IN;
                     *bus_in = ctx->status | odd_parity[ctx->status];
                     break;
                 }
                 ctx->status = SNS_DEVEND;
                 *tags |= CHAN_OPR_IN;
 printf("pending tape selected unit: %d\n", ctx->cur_unit);
             }

             if (ctx->selected && (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                   *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN))) {
                 if ((ctx->rdy_flags & (1 << ctx->cur_unit)) == 0) {
                     ctx->state = STATE_IDLE;
                  } else {
                      ctx->rdy_flags &= ~(1 << ctx->cur_unit);
                      ctx->status = SNS_DEVEND;
                      ctx->state = STATE_END;              /* Go wait for everything to drop */
                  }
 printf("selected unit pend device end: %d\n", ctx->cur_unit);
             }

             /* If we got selected in error */
             if (ctx->selected && (*tags == (CHAN_OPR_OUT|CHAN_STA_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_SUP_OUT|CHAN_STA_IN))) {
                 ctx->selected = 0;
                 ctx->status = 0;
                 if (ctx->rdy_flags == 0) {
                     ctx->state = STATE_IDLE;
                 }
                 *tags &= ~(CHAN_SEL_OUT);      /* Clear select out and in */
 printf("pending tape deselected unit: %d\n", ctx->cur_unit);
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
                 *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);        /* Clear select out and in */
                 *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                 *bus_in = ctx->addr | ctx->cur_unit;
                 *bus_in |= odd_parity[*bus_in];              /* Put out our address */
printf("Tape Reselect\n");
                 break;
             }

             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                 /* Put our address on the bus */
                 *tags &= ~(CHAN_SEL_OUT);                    /* Clear select out and in */
                 *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                 *bus_in = ctx->addr | ctx->cur_unit;
                 *bus_in |= odd_parity[*bus_in];              /* Put out our address */
printf("Tape Address\n");
                 break;
             }

             /* If we got bus, go and transfer */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                  *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN);       /* Clear select out and in */
                  ctx->selected = 1;
                  ctx->rdy_flags &= ~(1 << ctx->cur_unit);
                  ctx->state = STATE_END;              /* Go wait for everything to drop */
printf("Tape selected\n");
              }

              /* See if another device got it. */
              if (ctx->selected == 0 && (*tags & (CHAN_OPR_IN|CHAN_STA_IN)) != 0) {
                  /* Drop request out until channel free again */
                  break;
              }
              /* Put request in up */
             if (ctx->selected == 0) {
                 *tags |= CHAN_REQ_IN;
             }
             break;
    }
}

SDL_Texture *model2415_img;
SDL_Texture *tape_images_img;
static int supply_color[8] = {1, 1, 1, 1, 1, 1, 1, 1};
static int takeup_color[8] = {2, 2, 2, 2, 2, 2, 2, 2};
static int supply_label[8] = {1, 1, 1, 1, 1, 1, 1, 1};
static int takeup_label[8] = {0, 0, 0, 0, 0, 0, 0, 0};

void
model2415_draw(struct _device *unit, SDL_Renderer *render)
{
    struct _2415_context *ctx = (struct _2415_context *)unit->dev;
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

    for (i = 0; i < unit->n_units; i++) {
        x = unit->rect[i].x;
        y = unit->rect[i].y;

        if (ctx->tape[i]->file_name != NULL) {
            rect2.x = 0;
            rect2.y = 0;
            rect2.w = unit->rect[i].w;
            rect2.h = unit->rect[i].h;
            SDL_RenderCopy(render, model2415_img, &rect2, &unit->rect[i]);
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
            SDL_RenderCopy(render, model2415_img, &rect2, &unit->rect[i]);
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
static char *label_mode[] = { "No", "Yes" };

static SDL_Renderer *render;
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

    popup->ind[0].value = &ctx->tape[u]->format;
    popup->ind[0].shift = 8;
    popup->ind[1].value = &ctx->tape[u]->format;
    popup->ind[1].shift = 9;
    popup->ind[2].value = &ctx->tape[u]->format;
    popup->ind[2].shift = 2;
    popup->ind[3].value = &ctx->tape[u]->format;
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
model2415_init(SDL_Renderer *render, uint16_t addr) {
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

