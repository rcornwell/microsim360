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
#include "logger.h"
#include "event.h"
#include "panel.h"
#include "card.h"
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
    int                    eof_flag;     /* End of file flag */
    int                    stop_flag;    /* Stop at end of current command */
};

void model1442_feed(struct _1442_context *ctx);


static void feed_callback(struct _device *unit, void *arg, int iarg);


static void
read_callback(struct _device *unit, void *arg, int iarg)
{
    struct _1442_context *ctx = (struct _1442_context *)unit->dev;

    /* If the channel ended early, feed the card */
    if (ctx->data_end) {
       add_event(unit, &feed_callback, 1000, NULL, 0);
       ctx->status |= SNS_CHNEND;
       return;
    }

    if (ctx->data_rdy == 0) {
        if (ctx->rdr_col < 80) {
            uint8_t ch;
            ctx->data = hol_to_ebcdic(ctx->rdr_card[ctx->rdr_col]);
            ch = ebcdic_to_ascii[ctx->data];
            if (!isprint(ch))
               ch = '.';
            log_device("Read data %d:%02x '%c'\n", ctx->rdr_col, ctx->data, ch);
            ctx->rdr_col++;
            ctx->data_rdy = 1;
            add_event(unit, &read_callback, 100, NULL, 0);
        } else {
            ctx->status |= SNS_CHNEND;
            ctx->data_end = 1;
            add_event(unit, &feed_callback, 1000, NULL, 0);
        }
    } else {
        ctx->sense |= SENSE_OVRRUN;
        ctx->status |= SNS_CHNEND|SNS_UNITCHK;
        ctx->data_end = 1;
    }

}

static void
write_callback(struct _device *unit, void *arg, int iarg)
{
    struct _1442_context *ctx = (struct _1442_context *)unit->dev;

    log_device("reader Write get1\n");
    if (ctx->pch_col < 80) {
        ctx->pch_card[ctx->pch_col] |= ebcdic_to_hol(ctx->data);
        ctx->pch_col++;
    }
    /* Set that we are ready for another byte of data */
    ctx->data_rdy = 1;
}

static void
feed_callback(struct _device *unit, void *arg, int iarg)
{
    struct _1442_context *ctx = (struct _1442_context *)unit->dev;
    model1442_feed(ctx);
    ctx->status |= SNS_DEVEND;
    ctx->feed_done = 1;
}

void
model1442_dev(struct _device *unit, uint16_t *tags, uint16_t bus_out, uint16_t *bus_in)
{
    struct _1442_context *ctx = (struct _1442_context *)unit->dev;
    int      i;
    static   uint16_t last_tags = 0;

    if (last_tags != *tags) {
        print_tags("Reader", ctx->state, *tags, bus_out);
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
        return;
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
                 log_device("reader selected\n");
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
                 *tags &= ~(CHAN_SEL_OUT);  /* Clear select in */
                 *tags |= CHAN_ADR_IN;      /* Return address until accepted */
                 *bus_in = ctx->addr | odd_parity[ctx->addr];
                 log_device("reader address\n");
                 break;
            }

            /* When address out drops put our address on bus in */
            if (*tags == (CHAN_OPR_OUT|CHAN_OPR_IN|CHAN_ADR_IN|CHAN_ADR_OUT)) {
                 *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN|CHAN_OPR_IN);  /* Clear select in */
                 log_device("Halt device\n");
                 ctx->state = STATE_IDLE;
                 break;
            }

            /* Wait for Command out to raise */
            /* Can now drop address in */
            if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                 log_device("reader command %02x\n", bus_out);
                 ctx->cmd = bus_out & 0xff;
                 ctx->data_rdy = 0;
                 ctx->data_end = 0;
                 ctx->feed_done = 0;
                 ctx->status = 0;
                 ctx->state = STATE_CMD;
                 *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN);              /* Clear address in */
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
                        ctx->data_rdy = 1;
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
                        } else {
                            add_event(unit, &read_callback, 100, NULL, 0);
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
                        add_event(unit, &feed_callback, 1000, NULL, 0);
                        break;
                 case 4:  /* Sense */
                        if ((ctx->cmd & 0xf8) != 0) {
                            ctx->cmd = 0;
                            ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                            ctx->sense = SENSE_CMDREJ;
                            break;
                        }
                        ctx->data = ctx->sense;
                        ctx->data_rdy = 1;
                        ctx->data_end = 1;
                        log_device("reader Sense %02x\n", ctx->sense);
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
                 log_device("reader init stat\n");
             }

             /* When we get acknowlegment, go wait for it to go away */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_SUP_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~CHAN_STA_IN;
                 ctx->state = STATE_INIT_STAT;
                 log_device("reader init stat\n");
             }
             *bus_in = ctx->status | odd_parity[ctx->status];   /* Set initial status */
             /* Device selected */
             *tags &= ~(CHAN_SEL_OUT);                  /* Clear select out and in */
             break;

    case STATE_INIT_STAT:

             /* Wait for Service out to drop */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SUP_OUT|CHAN_OPR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_OPR_IN)) {
                 /* If no command, or check status, go back to idle */
                 if (ctx->cmd == 0 || (ctx->status & (SNS_UNITCHK|SNS_UNITEXP)) != 0) {
                     *tags &= ~(CHAN_OPR_IN|CHAN_SEL_OUT);   /* Clear select out and in */
                     ctx->state = STATE_IDLE;
                     ctx->selected = 0;
                     log_device("reader error state done\n");
                     break;
                 }

                 /* If initial status has Channel end, go wait for device to finish */
                 if ((ctx->status & SNS_DEVEND) != 0) {
                     *tags &= ~(CHAN_OPR_IN|CHAN_SEL_OUT);
                     ctx->selected = 0;
                     ctx->state = STATE_IDLE;
                     log_device("test device end\n");
                     break;
                 }

                 /* If initial status has Channel end, go wait for device to finish */
                 if ((ctx->status & SNS_CHNEND) != 0) {
                     *tags &= ~(CHAN_OPR_IN|CHAN_SEL_OUT);
                     ctx->selected = 0;
                     ctx->state = STATE_WAIT;
                     log_device("reader channel end\n");
                     break;
                 }

                 /* If select out has dropped, drop operator in if no data to send */
                 if ((*tags & CHAN_SEL_OUT) == 0 && ctx->data_rdy == 0 && ctx->cmd != 0x4) {
                     *tags &= ~(CHAN_OPR_IN);           /* Clear select out and in */
                     ctx->selected = 0;
                 }

                 /* Wait for device to have some data to transmit */
                 ctx->state = STATE_OPR;

                 log_device("reader state done\n");
                 break;
             }
             *tags &= ~(CHAN_SEL_OUT);                 /* Clear select out and in */
             break;

    case STATE_OPR:
             log_device("reader opr %d\n", ctx->selected);

             /* If we are selected clear select in */
             if (ctx->selected)
                 *tags &= ~CHAN_SEL_OUT;

             /* If we get select out with address out, reselection */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT) &&
                (bus_out & 0xff) == ctx->addr) {
                /* Device selected */
                *tags &= ~(CHAN_SEL_OUT);              /* Clear select out and in */
                *tags |= CHAN_STA_IN;                  /* Indicate busy status */
                *bus_in = SNS_BSY;
                ctx->selected = 1;
                log_device("reader reselect\n");
                break;
             }

             /* On Select channel, Select Out will not drop */
             /* Catch halt I/O */
             if (ctx->selected && *tags == (CHAN_OPR_OUT|CHAN_ADR_OUT|CHAN_OPR_IN) &&
                  (bus_out & 0xff) == ctx->addr)  {
                  /* Halt I/O */
                  /* Device selected */
                  *tags &= ~(CHAN_OPR_IN);             /* Clear select out and in */
                  ctx->state = STATE_OPR;              /* Return busy status */
                  ctx->data_end = 1;
                  ctx->selected = 0;
                  log_device("reader Halt i/o\n");
                  break;
             }

             /* Return status while waiting for Address out to drop */
             if (ctx->selected &&
                   *tags == (CHAN_OPR_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_STA_IN) &&
                   (bus_out & 0xff) == ctx->addr) {
                /* Device selected */
                *tags |= CHAN_STA_IN;                  /* Indicate busy status */
                *bus_in = SNS_BSY;
                break;
             }

             /* When select out drops, clear selected flag */
             if (ctx->selected && (*tags == (CHAN_OPR_OUT|CHAN_HLD_OUT|CHAN_STA_IN) ||
                                   *tags == (CHAN_OPR_OUT|CHAN_STA_IN))) {
                  *tags &= ~(CHAN_STA_IN);             /* Clear select out and in */
                  ctx->selected = 0;
                  log_device("reader deselected\n");
                  break;
             }

             /* If data ready, try and get/send it */
             if (ctx->data_rdy) {
                 ctx->state = (ctx->cmd & 1) ? STATE_DATA_I : STATE_DATA_O;
                 break;
             }

             /* If data end, tell CPU */
             if (ctx->data_end) {
                 if (ctx->cmd == 0x4) {
                    ctx->status |= SNS_CHNEND|SNS_DEVEND;
                 }
                 if ((ctx->cmd & 07) == 1) {
                    add_event(unit, &feed_callback, 1000, NULL, 0);
                    ctx->status |= SNS_CHNEND;
                 }
                 if ((ctx->status & SNS_CHNEND) != 0) {
                    ctx->state = STATE_DATA_END;
                 }
                 if ((ctx->status & SNS_DEVEND) != 0) {
                    ctx->state = STATE_END;
                 }
                 break;
             }

#if 0
             /* If select out dropped, and no data, drop oper in */
             if (ctx->selected && (*tags == (CHAN_OPR_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                                   *tags == (CHAN_OPR_OUT|CHAN_OPR_IN))) {
                *tags &= ~(CHAN_OPR_IN);
                ctx->selected = 0;
                break;
             }
#endif

             break;

    case STATE_REQ:   /* Data available and we are not talking on channel */
             log_device("reader Request\n");
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_REQ_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_REQ_IN)) {
                 /* Put our address on the bus */
                 *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);        /* Clear select out and in */
                 *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                 *bus_in = ctx->addr | odd_parity[ctx->addr]; /* Put out our address */
                 log_device("reader Reselect\n");
                 break;
             }

            /* Channel asking for us */
            if ((*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_REQ_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_SUP_OUT|CHAN_REQ_IN)) &&
                 ctx->feed_done && (bus_out & 0xff) == ctx->addr) {
                 /* Device selected */
                 if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0)
                     ctx->sense |= SENSE_BUSCHK;
                 *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);      /* Clear select out and in */
                 *tags |= CHAN_OPR_IN;
                 ctx->state = STATE_STACK_SEL;
                 ctx->selected = 1;
                 log_device("test request selected\n");
                 break;
             }

             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                 /* Put our address on the bus */
                 *tags &= ~(CHAN_SEL_OUT);                    /* Clear select out and in */
                 *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                 *bus_in = ctx->addr | odd_parity[ctx->addr]; /* Put out our address */
                 log_device("test Address\n");
                 break;
             }

             /* If we got bus, go and transfer */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                  *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN);  /* Clear select out and in */
                  ctx->selected = 1;
                  if (ctx->data_end) {
                      ctx->state = (ctx->status & SNS_DEVEND) ? STATE_END : STATE_DATA_END;
                  } else {
                      ctx->state = (ctx->cmd & 1) ? STATE_DATA_I : STATE_DATA_O;
                  }
                  log_device("reader selected\n");
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
                   ctx->data_rdy = 0;
                   /* Device selected */
                   if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0) {
                       ctx->sense |= SENSE_BUSCHK;
                       ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                       ctx->data_end = 1;
                   } else {
                       ctx->data = bus_out; /* Grab data */
                       add_event(unit, &write_callback, 100, NULL, 0);
                   }
                   ctx->state = STATE_INIT_STAT;       /* Wait for channel to be idle again */
                   break;
              }
              /* Command out to service in indicates end of data */
              if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_SRV_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_SRV_IN)) {
                   *tags &= ~(CHAN_SEL_OUT|CHAN_SRV_IN);  /* Count reached zero, no more accepted */
                   ctx->data_rdy = 0;
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
                   log_device("reader Data sent\n");
                   break;
              }
              /* If CMD out, indicates end of data */
              if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_SRV_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_SRV_IN)) {
                   *tags &= ~(CHAN_SEL_OUT|CHAN_SRV_IN);  /* Count reached zero, no more accepted */
                   ctx->data_rdy = 0;
                   ctx->data_end = 1;
                   ctx->state = STATE_INIT_STAT;           /* Back to operation. */
                   log_device("reader Data End\n");
                   break;
              }
              /* If we are selected clear select in */
              if (ctx->selected)
                 *tags &= ~CHAN_SEL_OUT;
              break;

    case STATE_DATA_END:  /* Send channel end to channel indicating end of data */
             if (ctx->selected == 0) {      /* Request channel if we don't have it */
                  ctx->state = STATE_REQ;
                  break;
             }

             /* Wait for Service out to drop */
             if (ctx->selected && *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_OPR_IN)) {
                 *tags &= ~(CHAN_SEL_OUT);  /* Clear select out and in */
                 *tags |= CHAN_OPR_IN|CHAN_STA_IN;
                 *bus_in = ctx->status | odd_parity[ctx->status];
                 log_device("End channel status %02x %02x\n", ctx->status, ctx->cmd);
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
                 log_device("reader Accepted data_end\n");
                 ctx->status &= ~SNS_CHNEND;
                 ctx->state = STATE_WAIT;              /* Wait for device to be done */
                 break;
             }

             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~(CHAN_SEL_OUT|CHAN_OPR_IN|CHAN_STA_IN);  /* Clear select out and in */
                  log_device("reader Stacked data_end\n");
                  ctx->selected = 0;
                  ctx->status &= ~SNS_CHNEND;
                  ctx->state = STATE_WAIT;            /* Wait for device to be done */
                  break;
             }

             *bus_in = ctx->status | odd_parity[ctx->status];
             /* Mark channel still in use */
             *tags &= ~(CHAN_SEL_OUT);  /* Clear select out and in */
             *tags |= CHAN_OPR_IN;
             break;

    case STATE_END:
             if (ctx->selected == 0) {
                  ctx->state = STATE_REQ;
                  break;
             }

             /* Wait for Service out to drop */
             if (ctx->selected && (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                          *tags == (CHAN_OPR_OUT|CHAN_SUP_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                          *tags == (CHAN_OPR_OUT|CHAN_OPR_IN))) {
                 *tags &= ~(CHAN_SEL_OUT);  /* Clear select out and in */
                 log_device("End status %02x %02x\n", ctx->status, ctx->cmd);
                 *tags |= CHAN_OPR_IN|CHAN_STA_IN;
                 if (ctx->sense != 0) {
                     ctx->status |= SNS_UNITCHK;
                 }
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
                 log_device("reader Accepted end\n");
                  ctx->selected = 0;
                  ctx->state = STATE_IDLE;                          /* All done, back to idle state */
                  break;
             }

             /* Command out to status in indicates a stacking of status */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~(CHAN_SEL_OUT|CHAN_OPR_IN|CHAN_STA_IN);  /* Clear select out and in */
                  log_device("reader Stacked\n");
                  ctx->selected = 0;
                  ctx->state = STATE_STACK;                         /* Stack status */
                  break;
             }

             *bus_in = ctx->status | odd_parity[ctx->status];
             /* Mark channel still in use */
             *tags &= ~(CHAN_SEL_OUT);  /* Clear select out and in */
             *tags |= CHAN_OPR_IN;
             log_device("Reader End status ready\n");
             break;

    case STATE_STACK:
             /* Wait until Channels asks for us */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_SUP_OUT) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_REQ_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_REQ_IN|CHAN_SUP_OUT)) {
                  if ((bus_out & 0xff) == ctx->addr) {
                      /* Device selected */
                      if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0)
                          ctx->sense |= SENSE_BUSCHK;
                      *tags &= ~(CHAN_SEL_OUT);      /* Clear select out and in */
                      *tags |= CHAN_OPR_IN;
                      ctx->state = STATE_STACK_SEL;
                      ctx->selected = 1;
                      log_device("printer stack selected\n");
                  } else {  /* Somebody else */
                      if ((ctx->status & SNS_DEVEND) != 0) {
                           *tags &= ~CHAN_REQ_IN;    /* Clear request in */
                      }
                  }
             }

             /* If we have end status and suppress out no longer up, try and give channel our status */
             if (*tags == (CHAN_OPR_OUT) && (ctx->status & SNS_DEVEND) != 0) {
                 *tags |= CHAN_REQ_IN;
             }

             /* If request in and and select out, end we have end status, give channel our address */
             if (*tags == (CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_OUT|CHAN_REQ_IN) &&
                         (ctx->status & SNS_DEVEND) != 0) {
                 *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);      /* Clear select out and in */
                 *tags |= CHAN_OPR_IN;
                 ctx->state = STATE_STACK_SEL;
                 ctx->selected = 1;
                 log_device("reader stack selected\n");
             }
             break;

    case STATE_STACK_SEL:  /* Stacked status selected */
            /* Wait until address out drops to put our address on bus */
            *tags |= CHAN_OPR_IN;
            /* When address out drops put our address on bus in */
            if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_SUP_OUT) ||
                *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_ADR_IN|CHAN_SUP_OUT) ||
                *tags == (CHAN_OPR_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_OPR_IN|CHAN_ADR_IN|CHAN_SUP_OUT)) {
                 *tags &= ~(CHAN_SEL_OUT);             /* Clear select out and in */
                 *tags |= CHAN_ADR_IN;      /* Return address until accepted */
                 *bus_in = ctx->addr | odd_parity[ctx->addr];
                 log_device("reader stack address\n");
            }

            /* Wait for Command out to raise */
            /* Can now drop address in */
            if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                 log_device("reader stack command %02x\n", bus_out);
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
                  log_device("reader stack init stat %02x\n", ctx->status);
             }

             /* When we get acknowlegment, go wait for it to go away */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_SUP_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~(CHAN_STA_IN|CHAN_SEL_OUT);
                 ctx->state = STATE_STACK_HLD;
                 log_device("reader stack init stat %02x done\n", ctx->status);
                 break;
             }
             /* When we get acknowlegment, go wait for it to go away */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_SUP_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~(CHAN_STA_IN|CHAN_OPR_IN);
                 ctx->state = STATE_STACK;
                 ctx->selected = 0;
                 log_device("reader stack init stat %02x\n", ctx->status);
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
                 log_device("reader stack done\n");
             }
             *tags &= ~(CHAN_SEL_OUT);                 /* Clear select out and in */
             break;

    case STATE_WAIT:
             /* If we get select out with address out, reselection */
             if (!ctx->selected &&
                 (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT) ||
                  *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_ADR_OUT)) &&
                 (bus_out & 0xff) == ctx->addr) {
                /* Device selected */
                *tags |= CHAN_STA_IN;
                *tags &= ~(CHAN_SEL_OUT);                 /* Clear select out and in */
                *bus_in = SNS_BSY;
                ctx->selected = 1;
                log_device("wait select attempt\n");
             }

             /* If selected clear status in when select out drops */
             if (ctx->selected) {
                /* Device selected */
                *tags &= ~(CHAN_SEL_OUT);          /* Clear status in */
                log_device("wait selected\n");
             }

             /* If selected clear status in when select out drops */
             if (ctx->selected && (*tags & CHAN_SRV_OUT) != 0) {
                /* Device selected */
                *tags &= ~(CHAN_SEL_OUT|CHAN_STA_IN);          /* Clear status in */
                log_device("wait status received\n");
             }

             /* Wait until Address out drops */
             if (ctx->selected && (*tags == (CHAN_OPR_OUT) || *tags == (CHAN_OPR_OUT|CHAN_SUP_OUT))) {
                /* Device selected */
                ctx->selected = 0;
                log_device("wait deselect\n");
             }

             if (ctx->feed_done) {
                 log_device("feed done %d\n", ctx->selected);
                 ctx->state = STATE_END;
             }
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

SDL_Texture *model1442_img = NULL;

void
model1442_draw(struct _device *unit, void *rend)
{
    struct _1442_context *ctx = (struct _1442_context *)unit->dev;
    SDL_Renderer *render = (SDL_Renderer *)rend;
    int          i, j;
    SDL_Rect     rect;
    SDL_Rect     rect2;
    SDL_Surface *text;
    SDL_Texture *txt;
    int           x = unit->rect[0].x;
    int           y = unit->rect[0].y;
    char          buf[100];

    /* Create the image on first run of draw function */
    if (model1442_img == NULL) {
        SDL_Surface *text;
        text = IMG_ReadXPMFromArray(model1442_xpm);
        model1442_img = SDL_CreateTextureFromSurface(render, text);
        SDL_SetTextureBlendMode(model1442_img, SDL_BLENDMODE_BLEND);
        SDL_FreeSurface(text);
    }

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
    rect2.x = x + 20;
    rect2.y = y + 20;
    SDL_RenderCopy(render, txt, NULL, & rect2);
    SDL_DestroyTexture(txt);

    /* Draw stacked cards */
    rect2.x = x + 351;
    rect2.w = 399 - rect2.x;
    rect2.y = y + 10;
    rect2.h = hopper_size(ctx->feed) / 30;
    rect2.y = 40 - rect2.h;
    rect.x = x + 184;
    rect.y = y + 56 - rect2.h;
    rect.h = rect2.h;
    rect.w = rect2.w;
    SDL_RenderCopy(render, model1442_img, &rect2, &rect);
    /* Draw weight */
    rect2.x = x + 351;
    rect2.y = y + 0;
    rect2.h = 10;
    rect.x = x + 184;
    rect.y -= 8;
    rect.h = rect2.h;
    rect.w = rect2.w;
    SDL_RenderCopy(render, model1442_img, &rect2, &rect);
    /* Draw front */
    rect2.x = x + 351;
    rect2.y = y + 51;
    rect2.w++;
    rect2.h = 15;
    rect.x = x + 182;
    rect.y = y + 60 - rect2.h;
    rect.h = rect2.h;
    rect.w = rect2.w;
    SDL_RenderCopy(render, model1442_img, &rect2, &rect);
    /* Draw Stacker 2 */
    rect2.x = x + 344;
    rect2.y = y + 104;
    rect2.w = stack_size(ctx->stack[1]) / 30;
    rect2.h = 30;
    rect.x = x + 122;
    rect.y = y + 75;
    rect.h = rect2.h;
    rect.w = rect2.w;
    SDL_RenderCopy(render, model1442_img, &rect2, &rect);
    /* Draw Stacker 1 */
    rect2.x = x + 344;
    rect2.y = y + 104;
    rect2.w = stack_size(ctx->stack[0]) / 30;
    rect2.h = 30;
    rect.x = x + 122;
    rect.y = y + 75;
    rect.h = rect2.h;
    rect.w = rect2.w;
    SDL_RenderCopy(render, model1442_img, &rect2, &rect);
    /* Overlay */
    rect2.x = x + 343;
    rect2.y = y + 69;
    rect2.w = 400 - 343;
    rect2.h = 101 - 69;
    rect.x = x + 122;
    rect.y = y + 97;
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
          log_device("Start key\n");
          if (ctx->state == STATE_IDLE) {
              log_device("Start reader\n");
              if (ctx->rdr_full == 0) {
                  model1442_feed(ctx);
              }
              if (ctx->rdr_full) {
                  ctx->state = STATE_END;
                  ctx->status = SNS_DEVEND;
                  ctx->data_end = 1;
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
model1442_init(void *rend, uint16_t addr)
{
     SDL_Surface *text;
//     SDL_Renderer *render = (SDL_Renderer *)rend;
     struct _device *dev1442;
     struct _1442_context *card;

     if ((dev1442 = calloc(1, sizeof(struct _device))) == NULL)
         return NULL;
     if ((card = calloc(1, sizeof(struct _1442_context))) == NULL) {
         free(dev1442);
         return NULL;
     }
  //   text = IMG_ReadXPMFromArray(model1442_xpm);
 //    model1442_img = SDL_CreateTextureFromSurface(render, text);
   //  SDL_SetTextureBlendMode(model1442_img, SDL_BLENDMODE_BLEND);
    // SDL_FreeSurface(text);

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

int
model1442_create(struct _option *opt)
{
     struct _device       *dev1442;
     struct _1442_context *card;
     struct _option       opts;
     int             i;

     /* Check for valid address */
     if (opt->addr == 0) {
         fprintf(stderr, "Missing address on 1442 device\n");
         return 0;
     }

     /* Allocate structures to hold device information */
     if ((dev1442 = calloc(1, sizeof(struct _device))) == NULL)
         return 0;
     if ((card = calloc(1, sizeof(struct _1442_context))) == NULL) {
         free(dev1442);
         return 0;
     }

     /* Fill in structures */
     dev1442->bus_func = &model1442_dev;
     dev1442->dev = (void *)card;
     dev1442->draw_model = (void *)&model1442_draw;
     dev1442->create_ctrl = (void *)&model1442_control;
     dev1442->rect[0].x = 0;
     dev1442->rect[0].y = 0;
     dev1442->rect[0].w = 305;
     dev1442->rect[0].h = 142;
     dev1442->n_units = 1;
     dev1442->addr = opt->addr;
     card->addr = opt->addr & 0xff;
     card->chan = (opt->addr >> 8) & 0xf;
     card->state = STATE_IDLE;
     card->selected = 0;
     card->sense = 0;
     card->feed = init_card_context();
     card->stack[0] = init_card_context();
     card->stack[1] = init_card_context();
     card->hop_cnt = hopper_size(card->feed);
     card->stk_cnt[0] = stack_size(card->stack[0]);
     card->stk_cnt[1] = stack_size(card->stack[1]);
     add_chan(dev1442, opt->addr);

     /* Parse options given on definition */
     while (get_option(&opts)) {
           if (strcmp(opts.opt, "FILE") == 0 && opts.flags == 1) {
               if (read_deck(card->feed, opts.string) != 1) {
                  log_error("Unable to attach deck %s\n", opts.string);
                  return 0;
               }
           } else if (strcmp(opts.opt, "EMPTY") == 0) {
               empty_cards(card->feed);
           } else if (strcmp(opts.opt, "BLANK") == 0 && opts.flags == 1) {
               int num;
               if (get_integer(&opts, &num) != 0)
                   return 0;
               blank_deck(card->feed, num);
           } else if (strcmp(opts.opt, "FORMAT") == 0) {
               i = get_index(&opts, type_label);
               if (i >= 0)
                   card->feed->mode = i;
           } else {
               fprintf(stderr, "Invalid option %s to 1442\n", opts.opt);
               free(card);
               free(dev1442);
               return 0;
           }
     }

     return 1;
}

DEV_LIST_STRUCT(1442, DEV_TYPE, 0);

