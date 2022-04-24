/*
 * microsim360 - Model 1442 card reader/punch.
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

#include <SDL.h>
#include <SDL_timer.h>
#include <SDL_thread.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <string.h>
#include "card.h"
#include "model1442.h"
#include "model1442.xpm"
#include "xlat.h"

/*
 *  Commands.
 *
 *            01234567
 *  Write     FSD00001       F= feed, S = stacker, D = data mode
 *  Read      0SD00010     
 *  Select    FS000011
 *  Sense     00000100
 */

#define SENSE_CMDREJ    BIT0  /* Invalid command */
#define SENSE_INTERV    BIT1  /* Operator intervention, reader empty */
#define SENSE_BUSCHK    BIT2  /* Bus parity error */
#define SENSE_EQUCHK    BIT3  /* Equipment check, not implemented */
#define SENSE_DATCHK    BIT4  /* More then 1 punch in rows 1-7 */
#define SENSE_OVRRUN    BIT5  /* Data missed */


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


struct _1442_context {
    int                    addr;         /* Device address */
    int                    chan;         /* Channel address */
    int                    state;        /* Current channel state */
    int                    selected;     /* Device currently selected */
    int                    sense;        /* Current sense value */
    int                    cmd;          /* Current command */
    int                    status;       /* Current bus status */
    int                    data;         /* Current byte to send/recieve */
    int                    data_rdy;     /* Data is valid */
    int                    data_end;     /* Data transfer over */
    int                    delay;        /* Delay time till operation done */
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
    int                    eof_flag;     /* End of file flag */
    int                    stop_flag;    /* Stop at end of current command */
};

void model1442_feed(struct _1442_context *ctx);

void
model1442_dev(struct _device *unit, uint16_t *tags, uint16_t bus_out, uint16_t *bus_in)
{
    struct _1442_context *ctx = (struct _1442_context *)unit->dev;
    int      i;
    static   uint16_t last_tags = 0;

    if (last_tags != *tags) {
        printf("Reader tags: %d ", ctx->state);
        print_tags(*tags, bus_out);
        last_tags = *tags;
    }
    /* Reset device if OPER OUT is dropped */
    if ((*tags & (CHAN_OPR_OUT|CHAN_SUP_OUT)) == 0) {
        if (ctx->selected) {
           *tags &= ~(CHAN_OPR_IN|CHAN_ADR_IN|CHAN_SRV_IN|CHAN_STA_IN);
        }
        ctx->selected = 0;
        ctx->state = STATE_IDLE;
        ctx->sense = 0;
        ctx->cmd = 0;
        ctx->delay = 0;
        return;
    }
    
    if (ctx->delay > 0) {
        ctx->delay--;
    }

    switch (ctx->state) {
    case STATE_IDLE:
            /* Wait until Channels asks for us */
            if ((*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT) ||
                *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_SUP_OUT)) &&
                 (bus_out & 0xff) == ctx->addr) {
                 /* Device selected */
                 if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0)
                     ctx->sense |= SENSE_BUSCHK;
                 *tags &= ~(CHAN_SEL_OUT);      /* Clear select out and in */
                 *tags |= CHAN_OPR_IN;
                 ctx->state = STATE_SEL;
                 ctx->selected = 1;
 printf("reader selected\n");
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
 printf("reader address\n");
            }

            /* Wait for Command out to raise */
            /* Can now drop address in */
            if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
 printf("reader command %02x\n", bus_out);
                 ctx->cmd = bus_out & 0xff;
                 ctx->data_rdy = 0;
                 ctx->data_end = 0;
                 ctx->status = 0;
                 ctx->state = STATE_CMD;
                 *tags &= ~(CHAN_ADR_IN);                 /* Clear address in */
                 switch (ctx->cmd & 07) {
                 case 0: /* Test I/O */
                        break;
                 case 1: /* Write */
                        ctx->sense = 0;
                        if ((ctx->cmd & 0x3c) != 0) {
                            ctx->cmd = 0;
                            ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                            ctx->sense = SENSE_CMDREJ;
                            break;
                        }
                        ctx->stk_sel = (ctx->cmd & 0x40) != 0;
                        /* Check if card in correct station */
                        if (ctx->pch_full == 0) {
                            ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                            ctx->sense = SENSE_INTERV;
                            break;
                        }
                        break;
                 case 2: /* Read */
                        ctx->sense = 0;
                        if ((ctx->cmd & 0xbc) != 0) {
                            ctx->cmd = 0;
                            ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                            ctx->sense = SENSE_CMDREJ;
                            break;
                        }
                        ctx->stk_sel = (ctx->cmd & 0x40) != 0;
                        ctx->cmd |= 0x80; /* Fake auto feed */
                        /* Check if card in correct station */
                        if (ctx->rdr_full == 0) {
                            if (ctx->eof_flag) {
                                ctx->status |= SNS_CHNEND|SNS_DEVEND|SNS_UNITEXP;
                                ctx->eof_flag = 0;
                            } else {
                                ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                                ctx->sense = SENSE_INTERV;
                            }
                            ctx->cmd = 0;
                        }
                        break;
                 case 3: /* Feed */
                        ctx->sense = 0;
                        if ((ctx->cmd & 0x7c) != 0) {
                            ctx->cmd = 0;
                            ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                            ctx->sense = SENSE_CMDREJ;
                            break;
                        }
                        if (ctx->rdr_full == 0) {
                            if (ctx->eof_flag) {
                                ctx->status |= SNS_CHNEND|SNS_DEVEND|SNS_UNITEXP;
                                ctx->eof_flag = 0;
                            } else {
                                ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                                ctx->sense = SENSE_INTERV;
                            }
                            ctx->cmd = 0;
                            break;
                        }
                        ctx->stk_sel = (ctx->cmd & 0x40) != 0;
                        ctx->status = SNS_CHNEND;
                        ctx->data_end = 1;
                        break;
                 case 4:  /* Sense */
                        if ((ctx->cmd & 0xf8) != 0) {
                            ctx->cmd = 0;
                            ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                            ctx->sense = SENSE_CMDREJ;
                            break;
                        }
                        break;
                 default:
                        ctx->cmd = 0;
                        ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                        ctx->sense = SENSE_CMDREJ;     /* Invalid command */
                        break;
                 }
                 if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0) {
                     ctx->cmd = 0;
                     ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                     ctx->sense |= SENSE_BUSCHK;
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
 printf("reader init stat\n");
             }

             /* When we get acknowlegment, go wait for it to go away */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_SUP_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~CHAN_STA_IN;
                 ctx->state = STATE_INIT_STAT;
 printf("reader init stat\n");
             }
             *bus_in = ctx->status | odd_parity[ctx->status];   /* Set initial status */
             /* Device selected */
             *tags &= ~(CHAN_SEL_OUT);                  /* Clear select out and in */
             break;

    case STATE_INIT_STAT:
             /* Wait for Service out to drop */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) || 
                 *tags == (CHAN_OPR_OUT|CHAN_OPR_IN)) {
                 /* If SELECT DEVICE */
                 if (ctx->cmd == 0 || (ctx->status & (SNS_UNITCHK|SNS_UNITEXP)) != 0) {
                     *tags &= ~(CHAN_OPR_IN);           /* Clear select out and in */
                     ctx->state = STATE_IDLE;
                     ctx->selected = 0;
                 } else {
                     ctx->state = STATE_OPR;
                     if ((*tags & CHAN_SEL_OUT) == 0) {
                         *tags &= ~(CHAN_OPR_IN);           /* Clear operation in */
                         ctx->selected = 0;
                     }

                 }
                 if (ctx->data_end && (*tags & CHAN_SEL_OUT) == 0) {
                     *tags &= ~(CHAN_OPR_IN);           /* Clear select out and in */
                     ctx->selected = 0;
                 }
 printf("reader state done\n");
                 break;
             }
             *tags &= ~(CHAN_SEL_OUT);                 /* Clear select out and in */
             break;

    case STATE_OPR:
 printf("reader opr %d\n", ctx->selected);
#if 0
             /* Wait for Command out to drop */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN) || 
                 *tags == (CHAN_CMD_OUT|CHAN_OPR_OUT|CHAN_OPR_IN)) {
                 break;
             }
#endif

             /* If we are selected clear select in */
             if (ctx->selected)
                 *tags &= ~CHAN_SEL_OUT;

             if (ctx->data_end) {
                 if (ctx->cmd == 0x4) {
                     ctx->status = SNS_CHNEND|SNS_DEVEND;
                     ctx->state = STATE_END;
                     break;
                 }
                 if (ctx->cmd & 0x80) {
                     model1442_feed(ctx);
                     ctx->delay = 1000;
                     /* Check if hopper empty with eof file flag set */
//                     if ((ctx->cmd & 0x7) == 2 && hopper_size(ctx->feed) == 0 && ctx->eof_flag) {
 //                       ctx->status |= SNS_UNITEXP;
  //                      ctx->eof_flag = 0;
   //                  }
                 }
                 /* If not control, generate data end */
                 if ((ctx->cmd & 0x07) != 3) {
                     ctx->status |= SNS_CHNEND;
                     ctx->state = STATE_DATA_END;
                 }
                 break;
             }

             switch (ctx->cmd & 0xf) {
             case 0:  /* Test I/O, go return ending status */
             case 4:  /* Sense */
                    ctx->data = ctx->sense;
                    ctx->data_rdy = 1;
                    ctx->data_end = 1;
                    ctx->state = STATE_DATA_O;
printf("reader Sense %02x\n", ctx->sense);
                    break;
             case 1: /* Write */
                    /* Wait for data to be available */
                    if (ctx->data_rdy) {
                        if (ctx->pch_col < 80) {
                           ctx->pch_card[ctx->pch_col] |= ebcdic_to_hol(ctx->data);
                           ctx->pch_col++;
                        }
                        ctx->delay = 100;
                        ctx->data_rdy = 0;
                    }
                    ctx->state = STATE_DATA_I;
printf("reader Write get1\n");
                    break;
             case 2: /* Read */
                    if (ctx->data_rdy == 0) {
                        if (ctx->rdr_col < 80) {
                            uint8_t ch;
                            ctx->data = hol_to_ebcdic(ctx->rdr_card[ctx->rdr_col]);
                            ch = ebcdic_to_ascii[ctx->data];
                            if (!isprint(ch))
                               ch = '.';
printf("Read data %d:%02x '%c'", ctx->rdr_col, ctx->data, ch);
                            ctx->rdr_col++;
                            ctx->data_rdy = 1;
                            ctx->delay = 100;
                        } else {
                            ctx->data_end = 1;
                            break;
                        }
                     }
                     ctx->state = STATE_DATA_O;
                     break;
             case 3: /* Control */
                     break;
             }
 
             /* If we get select out with address out, reselection */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT) && 
                (bus_out & 0xff) == ctx->addr) {
                /* Device selected */
                *tags &= ~(CHAN_SEL_OUT);              /* Clear select out and in */
                *tags |= CHAN_STA_IN;                  /* Indicate busy status */
                *bus_in = SNS_BSY;
                ctx->selected = 1;
printf("reader reselect\n");
                break;
             }
            
             /* On Select channel, Select Out will not drop */
             /* Catch halt I/O */
             if (ctx->selected && *tags == (CHAN_OPR_OUT|CHAN_ADR_OUT|CHAN_OPR_IN) &&
                  (bus_out & 0xff) == ctx->addr)  {
                  /* Halt I/O */
                  /* Device selected */
                  *tags &= ~(CHAN_OPR_IN);             /* Clear select out and in */
                  *bus_in = SNS_CHNEND|SNS_DEVEND|0x100;
                  ctx->state = STATE_END;              /* Return busy status */
                  ctx->selected = 0;
printf("reader Halt i/o\n");
                  break;
             }

             /* Return status while waiting for Address out to drop */
             if (ctx->selected && *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_STA_IN) && 
                (bus_out & 0xff) == ctx->addr) {
                /* Device selected */
                *tags &= ~(CHAN_SEL_OUT);              /* Clear select out and in */
                *tags |= CHAN_STA_IN;                  /* Indicate busy status */
                *bus_in = SNS_CHNEND|SNS_DEVEND|0x100;
                break;
             }

             /* When select out drops, clear selected flag */
             if (ctx->selected && *tags == (CHAN_OPR_OUT|CHAN_STA_IN)) {
                  *tags &= ~(CHAN_STA_IN);             /* Clear select out and in */
                  ctx->selected = 0;
printf("reader deselected\n");
                  break;
             }


             break;

    case STATE_REQ:   /* Data available and we are not talking on channel */
printf("reader Request\n");
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_REQ_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_REQ_IN)) {
                 /* Put our address on the bus */
                 *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);        /* Clear select out and in */
                 *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                 *bus_in = ctx->addr | odd_parity[ctx->addr]; /* Put out our address */
printf("reader Reselect\n");
                 break;
             }

             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                 /* Put our address on the bus */
                 *tags &= ~(CHAN_SEL_OUT);                    /* Clear select out and in */
                 *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                 *bus_in = ctx->addr | odd_parity[ctx->addr]; /* Put out our address */
printf("reader Address\n");
                 break;
             }

             /* If we got bus, go and transfer */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                  *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN);  /* Clear select out and in */
                  ctx->selected = 1;
                  ctx->state = STATE_OPR;              /* Go wait for everything to drop */
printf("reader selected\n");
              }

              /* See if another device got it. */
              if ((*tags & (CHAN_OPR_IN|CHAN_STA_IN)) != 0) {
                  /* Drop request out until channel free again */
printf("reader Other device\n");
                  break;
              }
              /* Put request in up */
              *tags |= CHAN_REQ_IN;
              break;

    case STATE_DATA_I:    /* Request data from Channel, wait ready */
              if (ctx->delay > 0) {
                  break;
              }

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
                       ctx->sense |= SENSE_BUSCHK;
                       ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                       ctx->state = STATE_END;
                   } else {
                       ctx->data = bus_out; /* Grab data */
                       ctx->data_rdy = 1;
                       ctx->state = STATE_INIT_STAT;       /* Wait for channel to be idle again */
                   }
                   break;
              }
              if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_SRV_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_SRV_IN)) {
                   *tags &= ~(CHAN_SEL_OUT|CHAN_SRV_IN);  /* Count reached zero, no more accepted */
                   ctx->data_end = 1;
                   ctx->state = STATE_INIT_STAT;          /* Back to operation. */
                   break;
              }
              /* Put request in up */
              *tags |= CHAN_OPR_IN|CHAN_SRV_IN;
             /* If we are selected clear select in */
             if (ctx->selected)
                 *tags &= ~CHAN_SEL_OUT;

              break;

    case STATE_DATA_O:    /* Request to send data to channel */
              if (ctx->delay > 0) {
                  break;
              }

//printf("Data output %02x %d\n", ctx->data, ctx->selected);
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
printf("reader Data sent\n");
                   break;
              }
              if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_SRV_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_SRV_IN)) {
                   *tags &= ~(CHAN_SEL_OUT|CHAN_SRV_IN);  /* Count reached zero, no more accepted */
                   ctx->data_end = 1;
                   ctx->state = STATE_DATA_END;           /* Back to operation. */
printf("reader Data End\n");
                   break;
              }
              /* If we are selected clear select in */
              if (ctx->selected)
                 *tags &= ~CHAN_SEL_OUT;
              break;

    case STATE_DATA_END:  /* Wait for IDLE bus */
             if (ctx->selected == 0) {
                 /* If we are directly selected go to selected state */
                 if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_REQ_IN) ||
                     *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT)) {
                     /* If select out, wait for channel available again */
                     if ((bus_out & 0xff) == ctx->addr) {
                         /* If it us, start initial selection */
                         if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0)
                             ctx->sense |= SENSE_BUSCHK;
                         *tags &= ~(CHAN_SEL_OUT);      /* Clear select out and in */
                         *tags |= CHAN_OPR_IN;
                         ctx->selected = 1;
 printf("reader selected data_end\n");
                     }
                     break;
                 }

                 /* If we get Select out, and are requesting service, give our address */
                 if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_REQ_IN) ||
                     *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_REQ_IN)) {
                     /* Put our address on the bus */
                     *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);        /* Clear select out and in */
                     *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                     *bus_in = ctx->addr | odd_parity[ctx->addr]; /* Put out our address */
                     printf("reader Reselect data_end\n");
                     break;
                 }
                
                 if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                     *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                     /* Put our address on the bus */
                     *tags &= ~(CHAN_SEL_OUT);                    /* Clear select out and in */
                     *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                     *bus_in = ctx->addr | odd_parity[ctx->addr]; /* Put out our address */
                     printf("reader Address data_end\n");
                     break;
                 }
                
                 /* If we got bus, go and transfer */
                 if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                     *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                      *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN);  /* Clear select out and in */
                      ctx->selected = 1;
                      printf("reader selected data_end\n");
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

             /* Service out indicates status was excepted. If Suppress out, then command chaining */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SRV_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 if ((*tags & CHAN_SEL_OUT) == 0) {
                      ctx->selected = 0;
                      *tags &= ~(CHAN_OPR_IN);          /* Clear Operation in if not selected */
                 }
                 *tags &= ~(CHAN_SEL_OUT|CHAN_STA_IN);  /* Clear select out and in */
printf("reader Accepted data_end\n");
//                  ctx->selected = 0;
                  ctx->status &= ~SNS_CHNEND;
                  ctx->status |= SNS_DEVEND;
                  ctx->state = STATE_WAIT;              /* All done, back to idle state */
                  break;
             }

             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~(CHAN_SEL_OUT|CHAN_OPR_IN|CHAN_STA_IN);  /* Clear select out and in */
printf("reader Stacked data_end\n");
                  ctx->selected = 0;
                  ctx->status &= ~SNS_CHNEND;
                  ctx->status |= SNS_DEVEND;
                  ctx->state = STATE_WAIT;            /* Stack status */
                  break;
             }

             *bus_in = ctx->status | odd_parity[ctx->status];
             /* Mark channel still in use */
             *tags &= ~(CHAN_SEL_OUT);  /* Clear select out and in */
             *tags |= CHAN_OPR_IN;
             break;

    case STATE_END:
              if (ctx->delay > 0) {
                  break;
              }

             if (ctx->selected == 0) {
                if ((*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_REQ_IN) ||
                    *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_SUP_OUT|CHAN_REQ_IN)) &&
                     (bus_out & 0xff) == ctx->addr) {
                     /* Device selected */
                     if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0)
                         ctx->sense |= SENSE_BUSCHK;
                     *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);      /* Clear select out and in */
                     *tags |= CHAN_OPR_IN;
                     printf("reader selected end\n");
                     break;
                 }
                 if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                    *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN)) {
                     /* Put our address on the bus */
                     *tags &= ~(CHAN_SEL_OUT);                    /* Clear select out and in */
                     *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                     *bus_in = ctx->addr | odd_parity[ctx->addr]; /* Put out our address */
                     printf("reader Reselect end\n");
                     break;
                 }
                 if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_REQ_IN) ||
                     *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_REQ_IN)) {
                     /* Put our address on the bus */
                     *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);        /* Clear select out and in */
                     *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                     *bus_in = ctx->addr | odd_parity[ctx->addr]; /* Put out our address */
                     printf("reader Reselect end\n");
                     break;
                 }
                
                 if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                     *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                     /* Put our address on the bus */
                     *tags &= ~(CHAN_SEL_OUT);                    /* Clear select out and in */
                     *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                     *bus_in = ctx->addr | odd_parity[ctx->addr]; /* Put out our address */
                     printf("reader Address end\n");
                     break;
                 }
                
                 /* If we got bus, go and transfer */
                 if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                     *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                      *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN);  /* Clear select out and in */
                      ctx->selected = 1;
                      printf("reader selected end\n");
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
                 *tags |= CHAN_OPR_IN|CHAN_STA_IN;
                 *bus_in = ctx->status | odd_parity[ctx->status];
                 ctx->cmd = 0;
                 break;
             }

             /* Service out indicates status was excepted. If Suppress out, then command chaining */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SRV_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~(CHAN_SEL_OUT|CHAN_OPR_IN|CHAN_STA_IN);  /* Clear select out and in */
printf("reader Accepted end\n");
                  ctx->selected = 0;
                  ctx->state = STATE_IDLE;                          /* All done, back to idle state */
                  break;
             }

             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~(CHAN_SEL_OUT|CHAN_OPR_IN|CHAN_STA_IN);  /* Clear select out and in */
printf("reader Stacked\n");
                  ctx->selected = 0;
                  ctx->state = STATE_STACK;                         /* Stack status */
                  break;
             }

             *bus_in = ctx->status | odd_parity[ctx->status];
             /* Mark channel still in use */
             *tags &= ~(CHAN_SEL_OUT);  /* Clear select out and in */
             *tags |= CHAN_OPR_IN;
//printf("End status ready %02x\n", ctx->status);
             break;

    case STATE_STACK:
            /* Wait until Channels asks for us */
            if ((*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT) ||
                *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_SUP_OUT)) &&
                 (bus_out & 0xff) == ctx->addr) {
                 /* Device selected */
                 if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0)
                     ctx->sense |= SENSE_BUSCHK;
                 *tags &= ~(CHAN_SEL_OUT);      /* Clear select out and in */
                 *tags |= CHAN_OPR_IN;
                 ctx->state = STATE_STACK_SEL;
                 ctx->selected = 1;
 printf("reader stack selected\n");
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
                 *tags &= ~(CHAN_SEL_OUT);             /* Clear select out and in */
                 *tags |= CHAN_ADR_IN;      /* Return address until accepted */
                 *bus_in = ctx->addr | odd_parity[ctx->addr];
 printf("reader stack address\n");
            }

            /* Wait for Command out to raise */
            /* Can now drop address in */
            if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
 printf("reader stack command %02x\n", bus_out);
                 ctx->state = STATE_STACK_CMD;
                 *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN);                 /* Clear address in */
                 if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0) {
                     ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                     ctx->sense |= SENSE_BUSCHK;
                 }
             }
             break;

    case STATE_STACK_CMD:
             /* Wait for Command out to drop */
             /* On MPX channel select out will drop, along with command */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_OPR_IN)) {
                  *tags |= CHAN_OPR_IN|CHAN_STA_IN;     /* Wait for acceptance of status */
 printf("reader stack init stat %02x\n", ctx->status);
             }

             /* When we get acknowlegment, go wait for it to go away */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_SUP_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~(CHAN_STA_IN|CHAN_SEL_OUT);
                 ctx->state = STATE_STACK_HLD;
 printf("reader stack init stat %02x done\n", ctx->status);
                 break;
             }
             /* When we get acknowlegment, go wait for it to go away */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_SUP_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~(CHAN_STA_IN|CHAN_OPR_IN);
                 ctx->state = STATE_STACK;
                 ctx->selected = 0;
 printf("reader stack init stat %02x\n", ctx->status);
             }
             *bus_in = ctx->status | odd_parity[ctx->status];   /* Set initial status */
             /* Device selected */
             *tags &= ~(CHAN_SEL_OUT);                  /* Clear select out and in */
             break;

    case STATE_STACK_HLD:
             /* Wait for Service out to drop */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) || 
                 *tags == (CHAN_OPR_OUT|CHAN_OPR_IN)) {
                 ctx->state = STATE_IDLE;
                 *tags &= ~CHAN_OPR_IN;
                 ctx->selected = 0;
  printf("reader stack done\n");
             }
             *tags &= ~(CHAN_SEL_OUT);                 /* Clear select out and in */
             break;

    case STATE_WAIT:
// printf("wait %d %d\n", ctx->selected, ctx->delay);
             /* If we get select out with address out, reselection */
             if (!ctx->selected &&
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT) &&
                (bus_out & 0xff) == ctx->addr) {
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

             /* Count delay until finished */
             if (ctx->delay > 0) {
                 break;
             }
             ctx->status |= SNS_DEVEND;
             ctx->state = STATE_END;
             break;

    }
}

/*
 * Move a card from punch stack to stacker and from reader to punch.
 * And grab new card to read.
 */
void
model1442_feed(struct _1442_context *ctx)
{
    /* If punch station full, put card out to stacker */
    if (ctx->pch_full) {
       stack_card(ctx->stack[ctx->stk_sel], &ctx->pch_card);
       ctx->pch_full = 0;
       ctx->pch_col = 0;
    }

    /* Move card from reader station to punch station */
    if (ctx->rdr_full) {
       memcpy(&ctx->pch_card[0], &ctx->rdr_card, 80 * sizeof(uint16_t));
       ctx->pch_full = ctx->rdr_full;
       ctx->rdr_full = 0;
    }

    if (ctx->stop_flag == 0) {
        ctx->rdr_full = read_card(ctx->feed, &ctx->rdr_card);
    }
    ctx->stop_flag = 0;
    ctx->rdr_col = 0;
    ctx->hop_cnt = hopper_size(ctx->feed);
    ctx->stk_cnt[0] = stack_size(ctx->stack[0]);
    ctx->stk_cnt[1] = stack_size(ctx->stack[1]);
    if (ctx->rdr_full == 0 && ctx->eof_flag == 0) {
        ctx->sense |= SENSE_INTERV;
    }
}

SDL_Texture *model1442_img;


void
model1442_draw(struct _device *unit, SDL_Renderer *render)
{
    struct _1442_context *ctx = (struct _1442_context *)unit->dev;
    int          i, j;
    SDL_Rect     rect;
    SDL_Rect     rect2;
    SDL_Surface *text;
    SDL_Texture *txt;
    int           x = unit->rect[0].x;
    int           y = unit->rect[0].y;
    char          buf[100];

    /* Draw basic device */
    rect.x = x;
    rect.y = y;
    rect.h = 142;
    rect.w = 305;
    rect2.x = 0;
    rect2.y = 0;
    rect2.h = 142;
    rect2.w = 305;
    SDL_RenderCopy(render, model1442_img, &rect2, &rect);
    /* Draw device number */
    sprintf(buf, "%0X%02X", ctx->chan, ctx->addr);
    text = TTF_RenderText_Solid(font14, buf, c1);
    txt = SDL_CreateTextureFromSurface(render, text);
    SDL_FreeSurface(text);
    SDL_QueryTexture(txt, &i, &j, &rect2.w, &rect2.h);
    rect2.x = 20;
    rect2.y = 20;
    SDL_RenderCopy(render, txt, NULL, & rect2);
    SDL_DestroyTexture(txt);
 
    /* Draw stacked cards */
    rect2.x = 351;
    rect2.w = 399 - rect2.x;
    rect2.y = 10;
    rect2.h = hopper_size(ctx->feed) / 30;
    rect2.y = 40 - rect2.h;
    rect.x = 184;
    rect.y = 56 - rect2.h;
    rect.h = rect2.h;
    rect.w = rect2.w;
    SDL_RenderCopy(render, model1442_img, &rect2, &rect);
    /* Draw weight */
    rect2.x = 351;
    rect2.y = 0;
    rect2.h = 10;
    rect.x = 184;
    rect.y -= 8;
    rect.h = rect2.h;
    rect.w = rect2.w;
    SDL_RenderCopy(render, model1442_img, &rect2, &rect);
    /* Draw front */
    rect2.x = 351;
    rect2.y = 51;
    rect2.w++;
    rect2.h = 15;
    rect.x = 182;
    rect.y = 60 - rect2.h;
    rect.h = rect2.h;
    rect.w = rect2.w;
    SDL_RenderCopy(render, model1442_img, &rect2, &rect);
    /* Draw Stacker 2 */
    rect2.x = 344;
    rect2.y = 104;
    rect2.w = stack_size(ctx->stack[1]) / 30;
    rect2.h = 30;
    rect.x = 122;
    rect.y = 75;
    rect.h = rect2.h;
    rect.w = rect2.w;
    SDL_RenderCopy(render, model1442_img, &rect2, &rect);
    /* Draw Stacker 1 */
    rect2.x = 344;
    rect2.y = 104;
    rect2.w = stack_size(ctx->stack[0]) / 30;
    rect2.h = 30;
    rect.x = 122;
    rect.y = 75;
    rect.h = rect2.h;
    rect.w = rect2.w;
    SDL_RenderCopy(render, model1442_img, &rect2, &rect);
    /* Overlay */
    rect2.x = 343;
    rect2.y = 69;
    rect2.w = 400 - 343;
    rect2.h = 101 - 69;
    rect.x = 122;
    rect.y = 97;
    rect.h = rect2.h;
    rect.w = rect2.w;
    SDL_RenderCopy(render, model1442_img, &rect2, &rect);
}

static void model1442_update(struct _popup *popup, void *device, int index)
{
    struct _device *unit = (struct _device *)device;
    struct _1442_context *ctx = (struct _1442_context *)unit->dev;
    int r;
    int cards;

    switch (index) {
    case 0:  /* End of file key */
          ctx->eof_flag = !ctx->eof_flag;
          break;

    case 1:  /* Start Key */
printf("Start key\n");
          if (ctx->state == STATE_IDLE) {
printf("Start reader\n");
              if (ctx->rdr_full == 0) {
                  model1442_feed(ctx);
              }
              if (ctx->rdr_full) {
                  ctx->state = STATE_END;
                  ctx->status = SNS_DEVEND;
              }
          }
          break;

    case 2:  /* NPRO */
          if (ctx->selected == 0)
              model1442_feed(ctx);
          ctx->eof_flag = 0;
          break;

    case 3:  /* STOP */
          ctx->stop_flag = 1;
          break;

    case 4: /* Empty hopper */
          empty_cards(ctx->feed);
          break;

    case 5: /* Load hopper */
          r = read_deck(ctx->feed, popup->text[0].text);
          break;

    case 6: /* Load hopper blanks */
          cards = atoi(popup->text[0].text);
          if (cards > 0) {
              blank_deck(ctx->feed, cards);
          }
          break;

    case 7: /* Empty Stacker 1 */
          empty_cards(ctx->stack[0]);
          break;

    case 8: /* Save Stacker 1 */
          r = save_deck(ctx->stack[0], popup->text[1].text);
          break;

    case 9: /* Empty Stacker 2 */
          empty_cards(ctx->stack[1]);
          break;

    case 10: /* Save Stacker 2 */
          r = save_deck(ctx->stack[1], popup->text[2].text);
          break;
    }
    ctx->hop_cnt = hopper_size(ctx->feed);
    ctx->stk_cnt[0] = stack_size(ctx->stack[0]);
    ctx->stk_cnt[1] = stack_size(ctx->stack[1]);
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
     {"POWER", "ON", 1, 0, 0, {0, 0, 0}, {0x96, 0x8f, 0x85}, {0xfd, 0xfd, 0xfd}},
     {"READY", NULL, 1, 1, 0, {0xff, 0xff, 0xff}, {0x7f,0xc0, 0x86}, {0x0c, 0x2e, 0x30}},
     {"END OF", "FILE", 0, 2, 0, {0xff, 0xff, 0xff}, {0x0c, 0x2e, 0x30}, {0,0,0}},
     {"CHECK", NULL, 1, 0, 1, {0, 0, 0}, {0xff, 0xfd, 0x5e}, {0xdd, 0xdc, 0x8a}},
     {"CHIP BOX", NULL, 1, 1, 1, {0, 0, 0}, {0xff, 0xfd, 0x5e}, {0xdd, 0xdc, 0x8a}},
     {"END OF", "FILE", 1, 2, 1, {0, 0, 0}, {0xff, 0xfd, 0x5e}, {0xdd, 0xdc, 0x8a} },
     {"START", NULL,  0, 0, 2, {0xff, 0xff, 0xff}, {0x0c, 0x2e, 0x30}, {0, 0, 0}},
     {"NPRO", NULL, 0, 1, 2, {0xff, 0xff, 0xff}, {0x0a, 0x52, 0x9a}, {0, 0, 0}},
     {"STOP", NULL, 0, 2, 2, {0xff, 0xff, 0xff}, {0xc8, 0x3a, 0x30}, {0, 0, 0}},
     {"EMPTY", NULL, 0, 8, 0, {0, 0, 0}, {0x80, 0x80, 0x80}, {0, 0, 0}},
     {"LOAD", NULL, 0, 9, 0, {0, 0, 0}, {0x80, 0x80, 0x80}, {0, 0, 0}},
     {"BLANK", NULL, 0, 10, 0, {0, 0, 0}, {0x80, 0x80, 0x80}, {0, 0, 0}},
     {"EMPTY", NULL, 0, 8, 1, {0, 0, 0}, {0x80, 0x80, 0x80}, {0, 0, 0}},
     {"SAVE", NULL, 0, 9, 1, {0, 0, 0}, {0x80, 0x80, 0x80}, {0, 0, 0}},
     {"EMPTY", NULL, 0, 8, 2, {0, 0, 0}, {0x80, 0x80, 0x80}, {0, 0, 0}},
     {"SAVE", NULL, 0, 9, 2, {0, 0, 0}, {0x80, 0x80, 0x80}, {0, 0, 0}},
     {NULL, NULL, 0},
};

static char *type_label[] = { "AUTO", "ASCII", "EBCDIC", "BIN", "OCTAL", NULL};


static SDL_Renderer *render;
struct _popup *
model1442_control(struct _device *unit, int hd, int wd, int u)
{
    struct _popup  *popup;
    struct _1442_context *ctx = (struct _1442_context *)unit->dev;
    SDL_Surface *text;
    int    i, j;
    int    w, h;
    char   buffer[100];

    if ((popup = (struct _popup *)calloc(1, sizeof(struct _popup))) == NULL)
        return popup;
    sprintf(buffer, "IBM1442 Dev 0x'%03X'", ctx->addr);
    popup->screen = SDL_CreateWindow(buffer, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        1000, 200, SDL_WINDOW_RESIZABLE );
    popup->render = SDL_CreateRenderer( popup->screen, -1, SDL_RENDERER_ACCELERATED);
    popup->device = unit; 
    popup->areas[popup->area_ptr].rect.x = 20 + (12 * wd) * 3;
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

    popup->ind[1].value = &ctx->rdr_full;
    popup->ind[4].value = &ctx->eof_flag;

    text = TTF_RenderText_Solid(font14, "Hopper: ", c1);

    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 25 + (12 * wd) * 3;
    popup->ctl_label[popup->ctl_ptr].rect.y = 20;
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;
    popup->text[popup->txt_ptr].rect.x = 25 + (12 * wd) * 3;
    popup->text[popup->txt_ptr].rect.y = 20;
    popup->text[popup->txt_ptr].rect.w = 45 * wd;
    popup->text[popup->txt_ptr].rect.h = h + 5;
    if (ctx->feed->file_name != NULL) 
        strncpy(popup->text[popup->txt_ptr].text, ctx->feed->file_name, 256);
    else
        popup->text[popup->txt_ptr].text[0] = '\0';
    popup->text[popup->txt_ptr].len = strlen(popup->text[popup->txt_ptr].text);
    popup->text[popup->txt_ptr].pos = popup->text[popup->txt_ptr].len;
    popup->text[popup->txt_ptr].cpos = textpos(&popup->text[popup->txt_ptr], 
                                        popup->text[popup->txt_ptr].pos);
    popup->txt_ptr++;
    popup->number[popup->num_ptr].rect.x = 25 + (12 * wd) * 13;
    popup->number[popup->num_ptr].rect.y = 20;
    popup->number[popup->num_ptr].rect.w = 5 * wd;
    popup->number[popup->num_ptr].rect.h = h;
    popup->number[popup->num_ptr].value = &ctx->hop_cnt;
    popup->number[popup->num_ptr].c = &c;
    popup->num_ptr++;

    popup->combo[popup->cmb_ptr].rect.x = 25 + (12 * wd) * 11;
    popup->combo[popup->cmb_ptr].rect.y = 20;
    popup->combo[popup->cmb_ptr].rect.w = 12 * wd;
    popup->combo[popup->cmb_ptr].rect.h = h;
    popup->combo[popup->cmb_ptr].urect.x = popup->combo[popup->cmb_ptr].rect.x;
    popup->combo[popup->cmb_ptr].urect.y = popup->combo[popup->cmb_ptr].rect.y;
    popup->combo[popup->cmb_ptr].urect.w = 2 * wd;
    popup->combo[popup->cmb_ptr].urect.h = h;
    popup->combo[popup->cmb_ptr].drect.x = popup->combo[popup->cmb_ptr].rect.x + (10 * wd) - 1;
    popup->combo[popup->cmb_ptr].drect.y = popup->combo[popup->cmb_ptr].rect.y;
    popup->combo[popup->cmb_ptr].drect.w = 2 * wd;
    popup->combo[popup->cmb_ptr].drect.h = h;
    for (i = 0; type_label[i] != NULL; i++) {
        text = TTF_RenderText_Solid(font14, type_label[i], c1);
        popup->combo[popup->cmb_ptr].label[i] = SDL_CreateTextureFromSurface( popup->render,
                                      text);
        SDL_QueryTexture(popup->combo[popup->cmb_ptr].label[i], NULL, NULL, 
             &popup->combo[popup->cmb_ptr].lw[i], &popup->combo[popup->cmb_ptr].lh[i]);
    }
    popup->combo[popup->cmb_ptr].num = ctx->feed->mode;
    popup->combo[popup->cmb_ptr].value = &ctx->feed->mode;
    popup->combo[popup->cmb_ptr].max = i - 1;
    popup->cmb_ptr++;
    
    text = TTF_RenderText_Solid(font14, "Stack 1: ", c1);

    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 25 + (12 * wd) * 3;
    popup->ctl_label[popup->ctl_ptr].rect.y = 20 + (hd * 3);
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;
    popup->text[popup->txt_ptr].rect.x = 25 + (12 * wd) * 3;
    popup->text[popup->txt_ptr].rect.y = 20 + (hd * 3);
    popup->text[popup->txt_ptr].rect.w = 45 * wd;
    popup->text[popup->txt_ptr].rect.h = h + 5;
    if (ctx->stack[0]->file_name != NULL) 
        strncpy(popup->text[popup->txt_ptr].text, ctx->stack[0]->file_name, 256);
    else
        popup->text[popup->txt_ptr].text[0] = '\0';
    popup->text[popup->txt_ptr].len = strlen(popup->text[popup->txt_ptr].text);
    popup->text[popup->txt_ptr].pos = popup->text[popup->txt_ptr].len;
    popup->text[popup->txt_ptr].cpos = textpos(&popup->text[popup->txt_ptr], 
                                        popup->text[popup->txt_ptr].pos);
    popup->txt_ptr++;
    popup->number[popup->num_ptr].rect.x = 25 + (12 * wd) * 13;
    popup->number[popup->num_ptr].rect.y = 20 + (hd * 3);
    popup->number[popup->num_ptr].rect.w = 5 * wd;
    popup->number[popup->num_ptr].rect.h = h;
    popup->number[popup->num_ptr].value = &ctx->stk_cnt[0];
    popup->number[popup->num_ptr].c = &c;
    popup->num_ptr++;
    popup->combo[popup->cmb_ptr].rect.x = 25 + (12 * wd) * 11;
    popup->combo[popup->cmb_ptr].rect.y = 20 + (hd * 3);
    popup->combo[popup->cmb_ptr].rect.w = 12 * wd;
    popup->combo[popup->cmb_ptr].rect.h = h;
    popup->combo[popup->cmb_ptr].urect.x = popup->combo[popup->cmb_ptr].rect.x;
    popup->combo[popup->cmb_ptr].urect.y = popup->combo[popup->cmb_ptr].rect.y;
    popup->combo[popup->cmb_ptr].urect.w = 2 * wd;
    popup->combo[popup->cmb_ptr].urect.h = h;
    popup->combo[popup->cmb_ptr].drect.x = popup->combo[popup->cmb_ptr].rect.x + (10 * wd) - 1;
    popup->combo[popup->cmb_ptr].drect.y = popup->combo[popup->cmb_ptr].rect.y;
    popup->combo[popup->cmb_ptr].drect.w = 2 * wd;
    popup->combo[popup->cmb_ptr].drect.h = h;
    for (i = 0; type_label[i] != NULL; i++) {
        text = TTF_RenderText_Solid(font14, type_label[i], c1);
        popup->combo[popup->cmb_ptr].label[i] = SDL_CreateTextureFromSurface( popup->render,
                                      text);
        SDL_QueryTexture(popup->combo[popup->cmb_ptr].label[i], NULL, NULL, 
             &popup->combo[popup->cmb_ptr].lw[i], &popup->combo[popup->cmb_ptr].lh[i]);
    }
    popup->combo[popup->cmb_ptr].num = ctx->stack[0]->mode;
    popup->combo[popup->cmb_ptr].value = &ctx->stack[0]->mode;
    popup->combo[popup->cmb_ptr].max = i - 1;
    popup->cmb_ptr++;

    text = TTF_RenderText_Solid(font14, "Stack 2: ", c1);

    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 25 + (12 * wd) * 3;
    popup->ctl_label[popup->ctl_ptr].rect.y = 20 + (hd * 6);
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;
    popup->text[popup->txt_ptr].rect.x = 25 + (12 * wd) * 3;
    popup->text[popup->txt_ptr].rect.y = 20 + (hd * 6);
    popup->text[popup->txt_ptr].rect.w = 45 * wd;
    popup->text[popup->txt_ptr].rect.h = h;
    if (ctx->stack[1]->file_name != NULL) 
        strncpy(popup->text[popup->txt_ptr].text, ctx->stack[1]->file_name, 256);
    else
        popup->text[popup->txt_ptr].text[0] = '\0';
    popup->text[popup->txt_ptr].len = strlen(popup->text[popup->txt_ptr].text);
    popup->text[popup->txt_ptr].pos = popup->text[popup->txt_ptr].len;
    popup->text[popup->txt_ptr].cpos = textpos(&popup->text[popup->txt_ptr], 
                                        popup->text[popup->txt_ptr].pos);
    popup->txt_ptr++;
    popup->number[popup->num_ptr].rect.x = 25 + (12 * wd) * 13;
    popup->number[popup->num_ptr].rect.y = 20 + (hd * 6);
    popup->number[popup->num_ptr].rect.w = 5 * wd;
    popup->number[popup->num_ptr].rect.h = h;
    popup->number[popup->num_ptr].value = &ctx->stk_cnt[1];
    popup->number[popup->num_ptr].c = &c;
    popup->num_ptr++;

    popup->combo[popup->cmb_ptr].rect.x = 25 + (12 * wd) * 11;
    popup->combo[popup->cmb_ptr].rect.y = 20 + (hd * 6);
    popup->combo[popup->cmb_ptr].rect.w = 12 * wd;
    popup->combo[popup->cmb_ptr].rect.h = h;
    popup->combo[popup->cmb_ptr].urect.x = popup->combo[popup->cmb_ptr].rect.x;
    popup->combo[popup->cmb_ptr].urect.y = popup->combo[popup->cmb_ptr].rect.y;
    popup->combo[popup->cmb_ptr].urect.w = 2 * wd;
    popup->combo[popup->cmb_ptr].urect.h = h;
    popup->combo[popup->cmb_ptr].drect.x = popup->combo[popup->cmb_ptr].rect.x + (10 * wd) - 1;
    popup->combo[popup->cmb_ptr].drect.y = popup->combo[popup->cmb_ptr].rect.y;
    popup->combo[popup->cmb_ptr].drect.w = 2 * wd;
    popup->combo[popup->cmb_ptr].drect.h = h;
    for (i = 0; type_label[i] != NULL; i++) {
        text = TTF_RenderText_Solid(font14, type_label[i], c1);
        popup->combo[popup->cmb_ptr].label[i] = SDL_CreateTextureFromSurface( popup->render,
                                      text);
        SDL_QueryTexture(popup->combo[popup->cmb_ptr].label[i], NULL, NULL, 
             &popup->combo[popup->cmb_ptr].lw[i], &popup->combo[popup->cmb_ptr].lh[i]);
    }
    popup->combo[popup->cmb_ptr].num = ctx->stack[1]->mode;
    popup->combo[popup->cmb_ptr].value = &ctx->stack[1]->mode;
    popup->combo[popup->cmb_ptr].max = i - 1;
    popup->cmb_ptr++;

    w = 0;
    for (i = 0; i < popup->ctl_ptr; i++) {
        if (popup->ctl_label[i].rect.w > w)
            w = popup->ctl_label[i].rect.w;
    }
    for (i = 0; i < popup->txt_ptr; i++) {
        popup->text[i].rect.x += w;
    }

    popup->update = &model1442_update;
    return popup;
}

struct _device *
model1442_init(SDL_Renderer *render, uint16_t addr)
{
     SDL_Surface *text;
     struct _device *dev1442;
     struct _1442_context *card;

     if ((dev1442 = calloc(1, sizeof(struct _device))) == NULL)
         return NULL;
     if ((card = calloc(1, sizeof(struct _1442_context))) == NULL) {
         free(dev1442);
         return NULL;
     }
     text = IMG_ReadXPMFromArray(model1442_xpm);
     model1442_img = SDL_CreateTextureFromSurface(render, text);
     SDL_SetTextureBlendMode(model1442_img, SDL_BLENDMODE_BLEND);
     SDL_FreeSurface(text);

     dev1442->bus_func = &model1442_dev;
     dev1442->dev = (void *)card;
     dev1442->draw_model = (void *)&model1442_draw;
     dev1442->create_ctrl = (void *)&model1442_control;
     dev1442->rect[0].x = 0;
     dev1442->rect[0].y = 0;
     dev1442->rect[0].w = 305;
     dev1442->rect[0].h = 142;
     dev1442->n_units = 1;
     dev1442->addr = addr;
     card->addr = addr & 0xff;
     card->chan = (addr >> 8) & 0xf;
     card->state = STATE_IDLE;
     card->selected = 0;
     card->sense = 0;
     card->feed = init_card_context();
     card->stack[0] = init_card_context();
     card->stack[1] = init_card_context();
     card->hop_cnt = hopper_size(card->feed);
     card->stk_cnt[0] = stack_size(card->stack[0]);
     card->stk_cnt[1] = stack_size(card->stack[1]);
     add_chan(dev1442, addr);
     return dev1442;
}

