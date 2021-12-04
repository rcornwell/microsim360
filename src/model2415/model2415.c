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

#include <string.h>
#include "device.h"
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
#define STATE_WAIT      12    /* After data transfer wait motion */
#define STATE_RDY       13    /* Wait for selection to give status */

#define FRAME_DELAY     10    /* 34 us per frame delay */
#define REWIND_DELAY    100
#define REW_FRAME       3840  /* Frames per 20ms */
#define START_DELAY     20

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

    printf("Tape tags %d: ", ctx->state);
    print_tags(*tags, bus_out);

    if (ctx->delay == 0) {
        ctx->delay = FRAME_DELAY;
        printf("Tape commnd scan\n");
        switch (ctx->cmd & 0xf) {
        case 0: /* Test I/O */
        case 4: /* Sense */
        case 3: /* Mode command */
        case 0xd: /* Mode command */
               break;
        case 1: /* Write */
               if (ctx->state == STATE_WAIT || ctx->state == STATE_STACK) {
                   r = tape_finish_rec(ctx->tape[ctx->cur_unit]);
                   ctx->state = STATE_END;
                   ctx->status = SNS_DEVEND;
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
                       ctx->state = STATE_DATA_END;
                   }
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
          printf("Send end status \n");
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
                       ctx->status |= SNS_CHNEND;
                       ctx->state = STATE_DATA_END;
                   } else if (r == 2) {
                       /* Read of tape mark */
                       ctx->status = SNS_CHNEND|SNS_UNITEXP;
                       ctx->state = STATE_DATA_END;
                       ctx->delay *= 3;
                   } else {
                       ctx->data_rdy = 1;
                       ctx->state = STATE_DATA_O;
                       printf("Queued\n");
                   }
               }
               break;
        
        case 7:  /* Tape motion control */
        case 0xf:
               switch (ctx->cmd) {
               case 0x0f:    /* Rewind and unload */
                    tape_start_rewind(ctx->tape[ctx->cur_unit]);
                    if (ctx->rew_delay == 0)
                        ctx->rew_delay = REWIND_DELAY;
                    ctx->rew_flags |= 1 << ctx->cur_unit;
                    if (ctx->chain_flag == 0) {
                        ctx->state = STATE_END;
                        ctx->status = SNS_CTLEND|SNS_CHNEND|SNS_DEVEND;
                        ctx->cmd = 0;
                    }
                    break;
               case 0x07:    /* Rewind */
                    printf("start rewind\n");
                    tape_start_rewind(ctx->tape[ctx->cur_unit]);
                    ctx->rew_flags |= 1 << ctx->cur_unit;
                    if (ctx->rew_delay == 0)
                        ctx->rew_delay = REWIND_DELAY;
                    if (ctx->chain_flag == 0) {
                        ctx->state = STATE_END;
                        ctx->status = SNS_CTLEND|SNS_CHNEND|SNS_DEVEND;
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
                    if (ctx->data_end) {
                        ctx->delay *= 3;;
                        ctx->status = SNS_DEVEND;
                        ctx->state = STATE_END;
                    } else {
                        r = tape_write_mark(ctx->tape[ctx->cur_unit]);
                        ctx->data_end = 1;
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
                         ctx->status = SNS_UNITEXP|SNS_DEVEND;
                    } else if (r == 0) {
                         /* End of record */
                         /* Terminate of record read */
                         r = tape_finish_rec(ctx->tape[ctx->cur_unit]);
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
                                 ctx->status = SNS_UNITEXP|SNS_DEVEND;
                             } else if (r == 2) {
                                 ctx->status = SNS_DEVEND;
                                 ctx->delay *= 3;   /* Inter record delay */
                             }
                         } else {
                             /* If space file, end */
                             ctx->status = SNS_DEVEND;
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
        printf("Rewind delay %02x %02x\n", ctx->rew_flags, ctx->rdy_flags);
        for ( i = 0; i < 6; i++) {
            if (ctx->tape[i] == NULL || (ctx->rew_flags & (1 << i)) == 0)
               continue;
            r = tape_rewind_frames(ctx->tape[i], REW_FRAME);
            if (r == 0) {
                ctx->rew_flags &= ~(1 << i);
                if (ctx->chain_flag) {
                    ctx->state = STATE_END;
                    ctx->status = SNS_DEVEND;
                    ctx->cmd = 0;
                } else 
                    ctx->rdy_flags |= 1 << i;
            }
        }
        if (ctx->rew_flags != 0) {
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
                 (bus_out & 0xf7) == ctx->addr && 
                 (bus_out & 0x7) < ctx->nunits) {
                 /* Device selected */
                 if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0)
                     ctx->sense[0] |= SENSE_BUSCHK;
                 i = bus_out & 0x7;
                /* Check if device is rewinding */
                 if (((ctx->rew_flags | ctx->run_flags) & (1 << i)) != 0) {
                      *tags &= ~(CHAN_SEL_OUT);      /* Clear select out and in */
                      *tags |= CHAN_STA_IN;
                      *bus_in = 0x100 | SNS_SMS | SNS_BSY;
                      ctx->selected = 1;
                      break;
                 }
                 ctx->cur_unit = i;
                 ctx->status = 0;
                 *tags &= ~(CHAN_SEL_OUT);      /* Clear select out and in */
                 *tags |= CHAN_OPR_IN;
                 ctx->state = STATE_SEL;
                 ctx->chain_flag = 0;
                 ctx->selected = 1;
 printf("tape selected unit: %d\n", ctx->cur_unit);
             }

             if (ctx->selected && *tags == (CHAN_OPR_OUT|CHAN_STA_IN)) {
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
                 *bus_in = ctx->addr | odd_parity[ctx->addr];
 printf("tape address\n");
            }

            /* Wait for Command out to raise */
            /* Can now drop address in */
            if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
 printf("tape command %02x\n", bus_out);
                 ctx->state = STATE_CMD; /* Wait for command out to return 
                                             initial status */
                 *tags &= ~(CHAN_ADR_IN);                 /* Clear address in */
                 if (ctx->stat_unit >= 0 && ctx->cur_unit != ctx->stat_unit) {
                     ctx->status = SNS_SMS|SNS_BSY;
                     *tags &= ~(CHAN_SEL_OUT);     /* Clear select out and in */
                     break;
                 }

                 /* If current unit has pending status, return busy */
                 if (ctx->cur_unit == ctx->stat_unit && bus_out & 0xff != 0x4 &&
                     (ctx->sense[0] != 0 || (ctx->sense[1] & SENSE_NOISE) != 0 ||
                     (ctx->sense[3] & (SENSE_VCR|SENSE_LRCR)) != 0)) {
                     ctx->status = SNS_BSY;
                     *tags &= ~(CHAN_SEL_OUT);     /* Clear select out and in */
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
                        ctx->stat_unit = ctx->cur_unit;
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
                        ctx->stat_unit = ctx->cur_unit;
                        if ((ctx->cmd & 0xfc) != 0) {
                            printf("Invalid command %x02\n", ctx->cmd);
                            ctx->sense[0] = SENSE_CMDREJ;
                            break;
                        }
                        /* Check if tape attached */
                        if (ctx->tape[ctx->cur_unit] == 0) {
                            printf("Tape not attached %d\n", ctx->cur_unit);
                            ctx->sense[0] = SENSE_INTERV;
                            break;
                        }
                        /* Do a read start */
                        r = tape_read_forw(ctx->tape[ctx->cur_unit]);
                        printf("Tape read forw %d : %d\n", ctx->cur_unit, r);
                        if (r == 1) {
                            ctx->delay = START_DELAY;
                        } else {
                            ctx->sense[0] = SENSE_INTERV;
                        }
                        break;
                 case 3: /* Mode command */
                        ctx->sense[0] = 0;
                        ctx->sense[2] = SENSE_2;
                        ctx->sense[3] = 0;
                        ctx->sense[4] = 0;
                        ctx->stat_unit = ctx->cur_unit;
                        if ((ctx->sense[1] & SENSE_7TRACK) != 0)
                            ctx->mode7 = ctx->cmd;
                        else
                            ctx->mode9 = ctx->cmd;
                        ctx->cmd = 0;
                        ctx->cmd_done = 1;
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
                            ctx->stat_unit = ctx->cur_unit;
                            if ((ctx->cmd & 0xfc) != 0) {
                                ctx->sense[0] = SENSE_CMDREJ;
                                break;
                            }
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
                            if (r == 1) {
                               ctx->delay = START_DELAY;
                            } else {
                                ctx->sense[0] = SENSE_INTERV;
                            }
                        } else if (ctx->cmd == 0x4) {
                            ctx->stat_unit = -1;
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
                        ctx->stat_unit = ctx->cur_unit;
                        /* Check if tape attached */
                        if (ctx->tape[ctx->cur_unit] == 0) {
                            ctx->sense[0] = SENSE_INTERV;
                            break;
                        }
                        switch (ctx->cmd) {
                        case 0x07:    /* Rewind */
                        case 0x0f:    /* Rewind and unload */
                        case 0x17:    /* Erase gap */
                        case 0x1f:    /* Write tape mark */
                              ctx->delay = 33;
                              break;
                        case 0x37:    /* Forward space block */
                        case 0x3f:    /* Forward space file */
                             /* Check if tape attached */
                             if (ctx->tape[ctx->cur_unit] == 0) {
                                 ctx->sense[0] = SENSE_INTERV;
                                 break;
                             }
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
                        }
                        ctx->status |= (SNS_CHNEND);
                        break;
                 default:
                        ctx->cmd = 0;
                        ctx->sense[0] = SENSE_CMDREJ;     /* Invalid command */
                        ctx->state = STATE_END;
                        break;
                 }
                 if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0) {
                     ctx->cmd = 0;
                     ctx->sense[0] |= SENSE_BUSCHK;
                 }
                 if (ctx->cmd != 4 && ((ctx->sense[0] & ~(SENSE_WCZERO)) != 0 ||
                                         (ctx->sense[1] & BIT7) != 0)) {
                     ctx->status |= (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                     ctx->cmd = 0;
                 }
                 if (ctx->sense[0] != 0) {
                     ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
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
 printf("init stat %02x\n", ctx->status);
             }

             /* When we get acknowlegment, go wait for it to go away */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_SUP_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~CHAN_STA_IN;
                 if ((*tags & CHAN_SUP_OUT) != 0)
                     ctx->chain_flag = 1;
                 ctx->state = STATE_INIT_STAT;
 printf("init stat 2 %02x\n", ctx->status);
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
                 if ((ctx->status & (SNS_DEVEND|SNS_UNITCHK|SNS_UNITEXP)) != 0)
                     ctx->state = STATE_IDLE;
                 else
                     ctx->state = STATE_OPR;
                 if ((ctx->status & SNS_CHNEND) != 0) {
                     if ((*tags & CHAN_SEL_OUT) == 0) {
                         *tags &= ~(CHAN_OPR_IN);           /* Clear operation in */
                         ctx->selected = 0;
                     }
                     ctx->state = STATE_WAIT;
                 }
                 /* If test I/O or no command back to idle state */
                 if (ctx->cmd == 0) {
                     if ((*tags & CHAN_SEL_OUT) == 0) {
                         *tags &= ~(CHAN_OPR_IN);           /* Clear operation in */
                         ctx->selected = 0;
                     }
                     ctx->state = STATE_IDLE;
                 }
                 /* If writing need a data byte ready before we get there */
                 if (ctx->sense[1] & SENSE_WRITE)
                     ctx->state = STATE_DATA_I;
 printf("state done\n");
             }
             *tags &= ~(CHAN_SEL_OUT);              /* Clear select out and in */
             break;

    case STATE_OPR:
 printf("opr %d\n", ctx->selected);
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
 
             /* If we get select out with address out, reselection */
             if (!ctx->selected && 
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT) && 
                (bus_out & 0xf8) == ctx->addr && (bus_out & 0x7) < ctx->nunits) {
                /* Device selected */
                *tags &= ~(CHAN_SEL_OUT);              /* Clear select out and in */
                *tags |= CHAN_STA_IN;                  /* Indicate busy status */
                *bus_in = SNS_SMS|SNS_BSY|0x100;
                ctx->selected = 0;
printf("reselect\n");
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
printf("Halt i/o\n");
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
printf("Reselect\n");
                 break;
             }

             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                 /* Put our address on the bus */
                 *tags &= ~(CHAN_SEL_OUT);                    /* Clear select out and in */
                 *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                 *bus_in = ctx->addr | ctx->cur_unit;
                 *bus_in |= odd_parity[*bus_in];              /* Put out our address */
printf("Address\n");
                 break;
             }

             /* If we got bus, go and transfer */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                  *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN);  /* Clear select out and in */
                  ctx->selected = 1;
                  ctx->state = STATE_OPR;              /* Go wait for everything to drop */
printf("selected\n");
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
              }
              if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_SRV_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_SRV_IN)) {
                   *tags &= ~(CHAN_SEL_OUT|CHAN_SRV_IN);  /* Count reached zero, no more accepted */
                   ctx->status = SNS_CHNEND;
                   ctx->data_end = 1;
                   ctx->state = STATE_OPR;        /* Back to operation. */
              }
              /* Put request in up */
              *tags |= CHAN_OPR_IN|CHAN_SRV_IN;
             /* If we are selected clear select in */
             if (ctx->selected)
                 *tags &= ~CHAN_SEL_OUT;

              break;

    case STATE_DATA_O:    /* Request to send data to channel */
printf("Data output %02x %d\n", ctx->data, ctx->selected);
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
printf("Data sent\n");
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
                       ctx->state = STATE_OPR;
                   }
printf("Data End\n");
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
                             printf("selected other unit\n");
                             break;
                         }
                         *tags |= CHAN_OPR_IN;
                         ctx->state = STATE_SEL;
                         ctx->selected = 1;
                          printf("selected\n");
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
                     printf("Reselect\n");
                     break;
                 }
                
  
                 if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                     *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                     /* Put our address on the bus */
                     *tags &= ~(CHAN_SEL_OUT);                    /* Clear select out and in */
                     *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                     *bus_in = ctx->addr | ctx->cur_unit;
                     *bus_in |= odd_parity[*bus_in];              /* Put out our address */
                     printf("Address\n");
                     break;
                 }
                
                 /* If we got bus, go and transfer */
                 if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                     *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                      *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN);  /* Clear select out and in */
                      ctx->selected = 1;
                      printf("selected\n");
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
printf("End channel status %02x %02x\n", ctx->status, ctx->cmd);
                 *tags |= CHAN_OPR_IN|CHAN_STA_IN;
                 *bus_in = ctx->status | odd_parity[ctx->status];
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
printf("Accepted\n");
                  ctx->state = STATE_WAIT;            /* Wait for operation to finish */
                  break;
             }

             /* Response of CMD out indicates that channel wants to stack the status. */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~(CHAN_SEL_OUT|CHAN_OPR_IN|CHAN_STA_IN);  /* Clear select out and in */
printf("Stacked\n");
                  ctx->selected = 0;
                  ctx->state = STATE_STACK;                         /* Stack status */
                  break;
             }

             *bus_in = ctx->status | odd_parity[ctx->status];
             /* Mark channel still in use */
             *tags &= ~(CHAN_SEL_OUT);  /* Clear select out and in */
             *tags |= CHAN_OPR_IN;
             break;

    case STATE_END:
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
                         printf("selected other unit\n");
                         break;
                     }
                     *tags |= CHAN_OPR_IN;
                     printf("selected\n");
                     break;
                 }
                 if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                    *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN)) {
                     /* Put our address on the bus */
                     *tags &= ~(CHAN_SEL_OUT);                    /* Clear select out and in */
                     *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                     *bus_in = ctx->addr | ctx->cur_unit;
                     *bus_in |= odd_parity[*bus_in];              /* Put out our address */
                     printf("Reselect\n");
                     break;
                 }
                 if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_REQ_IN) ||
                     *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_REQ_IN)) {
                     /* Put our address on the bus */
                     *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);        /* Clear select out and in */
                     *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                     *bus_in = ctx->addr | ctx->cur_unit;
                     *bus_in |= odd_parity[*bus_in];              /* Put out our address */
                     printf("Reselect\n");
                     break;
                 }
                
                 if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                     *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                     /* Put our address on the bus */
                     *tags &= ~(CHAN_SEL_OUT);                    /* Clear select out and in */
                     *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                     *bus_in = ctx->addr | ctx->cur_unit;
                     *bus_in |= odd_parity[*bus_in];              /* Put out our address */
                     printf("Address\n");
                     break;
                 }
                
                 /* If we got bus, go and transfer */
                 if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                     *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                      *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN);  /* Clear select out and in */
                      ctx->selected = 1;
                      printf("selected\n");
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
printf("End status %02x %02x\n", ctx->status, ctx->cmd);
                 *tags |= CHAN_STA_IN;
//                 ctx->rdy_flags &= ~(1 << ctx->cur_unit);
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
printf("Accepted\n");
                  ctx->selected = 0;
                  ctx->state = STATE_IDLE;           /* All done, back to idle state */
                  break;
             }

             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~(CHAN_SEL_OUT|CHAN_OPR_IN|CHAN_STA_IN);  /* Clear select out and in */
printf("Stacked\n");
                  ctx->selected = 0;
                  ctx->state = STATE_STACK;                         /* Stack status */
                  break;
             }

             *bus_in = ctx->status | odd_parity[ctx->status];
             /* Mark channel still in use */
             *tags &= ~(CHAN_SEL_OUT);  /* Clear select out and in */
             *tags |= CHAN_OPR_IN;
printf("End status ready\n");
             break;

    case STATE_STACK:  /* Stacked status */
             /* Wait until Channels asks for us */
             if ((*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT) ||
                  *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_OPR_IN) ||
                  *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_SUP_OUT|CHAN_OPR_IN) ||
                  *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_SUP_OUT)) &&
                  (bus_out & 0xf8) == ctx->addr) {
                  /* Device selected */
                  if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0)
                      ctx->sense[0] |= SENSE_BUSCHK;
                  *tags &= ~(CHAN_SEL_OUT);      /* Clear select out and in */
                  *tags |= CHAN_OPR_IN;
//                  ctx->state = STATE_END;
//                  ctx->selected = 1;
 printf("stack selected\n");
                  break;
             }

             /* Channel does select without suppress out */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_REQ_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                 /* Put our address on the bus */
printf("stack address\n");
                 *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);               /* Clear select out and in */
                 *tags |= CHAN_OPR_IN|CHAN_ADR_IN;                   /* Put out device on request */
                 *bus_in = ctx->addr | ctx->cur_unit;
                 *bus_in |= odd_parity[*bus_in];              /* Put out our address */
                 break;
             }
             /* If we got bus, go and transfer */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                  *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN|CHAN_REQ_IN);  /* Clear select out and in */
printf("stack cmd\n");
                  ctx->selected = 1;
                  ctx->state = STATE_END;                            /* Go return status */
              }

              /* See if another device got it. */
              if ((*tags & (CHAN_OPR_IN|CHAN_STA_IN)) != 0) {
                  /* Drop request out until channel free again */
                  *tags &= CHAN_REQ_IN;
                  break;
              }
              break;

    case STATE_WAIT:
 printf("wait %d\n", ctx->selected);
             /* If we get select out with address out, reselection */
             if (!ctx->selected && 
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT) && 
                (bus_out & 0xf8) == ctx->addr && (bus_out & 0x7) < ctx->nunits) {
                /* Device selected */
                *tags |= CHAN_STA_IN;
                *bus_in = SNS_BSY;
                ctx->selected = 1;
printf("wait select attempt\n");
             }
            
             /* If selected clear status in when select out drops */
             if (ctx->selected && *tags == (CHAN_OPR_OUT|CHAN_STA_IN)) {
                /* Device selected */
                *tags &= ~(CHAN_STA_IN);              /* Clear status in */
                ctx->selected = 0;
printf("wait deselect\n");
             }
             break;

    case STATE_RDY:
            /* Wait until Channels asks for us */
            if ((*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_REQ_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_SUP_OUT|CHAN_REQ_IN)) &&
                 (bus_out & 0xf7) == ctx->addr && (bus_out & 0x7) < ctx->nunits) {
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
                 *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);        /* Clear select out and in */
                 *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                 *bus_in = ctx->addr | ctx->cur_unit;
                 *bus_in |= odd_parity[*bus_in];              /* Put out our address */
printf("Reselect\n");
                 break;
             }

             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                 /* Put our address on the bus */
                 *tags &= ~(CHAN_SEL_OUT);                    /* Clear select out and in */
                 *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                 *bus_in = ctx->addr | ctx->cur_unit;
                 *bus_in |= odd_parity[*bus_in];              /* Put out our address */
printf("Address\n");
                 break;
             }

             /* If we got bus, go and transfer */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                  *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN);       /* Clear select out and in */
                  ctx->selected = 1;
                  ctx->state = STATE_CMD;              /* Go wait for everything to drop */
printf("selected\n");
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
     {"FILE", "PROTECT", 1, 2, 0, {0xff, 0xff, 0xff}, {0x0c, 0x2e, 0x30}, {0,0,0}},  /* Red */
     {"TAPE", "INDICATOR", 1, 3, 0, {0, 0, 0}, {0xff, 0xfd, 0x5e}, {0xdd, 0xdc, 0x8a}}, /* White */
     {"LOAD", "REWIND",   0, 0, 1, {0, 0, 0}, {0xff, 0xfd, 0x5e}, {0xdd, 0xdc, 0x8a}}, /* Blue */
     {"START", NULL,      0, 1, 1, {0xff, 0xff, 0xff}, {0x0c, 0x2e, 0x30}, {0, 0, 0}}, /* Green */
     {"UNLOAD", "REWIND", 0, 2, 1, {0xff, 0xff, 0xff}, {0x0a, 0x52, 0x9a}, {0, 0, 0}}, /* Blue */
     {"RESET", NULL,      0, 3, 1, {0xff, 0xff, 0xff}, {0xc8, 0x3a, 0x30}, {0, 0, 0}}, /* Blue */
     { NULL, NULL, 0}
};

struct _device *
model2415_init(SDL_Renderer *render) {
     SDL_Surface *text;
     struct _device *dev2415;
     struct _2415_context *tape;

     if ((dev2415 = calloc(1, sizeof(struct _device))) == NULL)
         return NULL;
     if ((tape = calloc(1, sizeof(struct _2415_context))) == NULL) {
         free(dev2415);
         return NULL;
     }

     dev2415->bus_func = &model2415_dev;
     dev2415->dev = (void *)tape;
     dev2415->n_units = 0;
     tape->addr = 0x10;
     tape->state = STATE_IDLE;
     tape->selected = 0;
     tape->sense[0] = 0;
     tape->nunits = 1;
     tape->tape[0] = (struct _tape_buffer *)calloc(1, sizeof(struct _tape_buffer));
     tape_attach(tape->tape[0], "../test_progs/bos.tap", TYPE_E11, 0, 1);
     return dev2415;
}

