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
#include "model1442.h"

DEV_LIST_STRUCT(1442, DEV_TYPE, 0);
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
            if (ctx->data == 0x100) {
                ctx->data = 0;
                ctx->sense |= SENSE_DATCHK;
                log_device("Read error %d\n", ctx->rdr_col);
            } else {
            ch = ebcdic_to_ascii[ctx->data];
            if (!isprint(ch))
               ch = '.';
            log_device("Read data %d:%02x '%c'\n", ctx->rdr_col, ctx->data, ch);
            }
            ctx->rdr_col++;
            ctx->data_rdy = 1;
            add_event(unit, &read_callback, 100, NULL, 0);
        } else {
            ctx->status |= SNS_CHNEND;
            ctx->data_end = 1;
            add_event(unit, &feed_callback, 1000, NULL, 0);
        }
        unit->request = 1;
    } else {
        log_device("reader overrun\n");
        ctx->sense |= SENSE_OVRRUN;
        ctx->status |= SNS_CHNEND|SNS_UNITCHK;
        ctx->data_end = 1;
        add_event(unit, &feed_callback, 1000, NULL, 0);
        unit->request = 1;
    }

}

static void
write_callback(struct _device *unit, void *arg, int iarg)
{
    struct _1442_context *ctx = (struct _1442_context *)unit->dev;

    log_device("reader Write get1\n");
    if (ctx->data_rdy) {
        ctx->sense |= SENSE_OVRRUN;
        ctx->status |= SNS_CHNEND|SNS_UNITCHK;
        ctx->data_end = 1;
        unit->request = 1;
        if ((ctx->cmd & 0x80) != 0) {
             add_event(unit, &feed_callback, 1000, NULL, 0);
        } else {
             ctx->status |= SNS_DEVEND;
             ctx->cmd_done = 1;
             ctx->busy = 0;
             ctx->cmd = 0;
        }
        return;
    }

    if (ctx->pch_col < 80) {
        ctx->pch_card[ctx->pch_col] |= ebcdic_to_hol(ctx->data);
        ctx->pch_col++;
    }
    /* Set that we are ready for another byte of data */
    ctx->data_rdy = 1;
    unit->request = 1;
    add_event(unit, &write_callback, 100, NULL, 0);
}

static void
feed_callback(struct _device *unit, void *arg, int iarg)
{
    struct _1442_context *ctx = (struct _1442_context *)unit->dev;
    ctx->status |= SNS_DEVEND;
    ctx->busy = 0;
    ctx->cmd_done = 1;
    ctx->feed_done = 1;
    unit->request = 1;
    if (ctx->hop_cnt == 0 && ctx->eof_flag) {
        ctx->status |= SNS_UNITEXP;
        ctx->eof_flag = 0;
        /* Move card to punch */
        model1442_feed(ctx);
        return;
    }
    model1442_feed(ctx);
    if (ctx->sense != 0) {
        log_device("Sense %02x\n", ctx->sense);
        ctx->status |= SNS_UNITCHK;
    }
}

/* Stop device when channel has no more data */
static void
device_stop(struct _device *unit)
{
    struct _1442_context *ctx = (struct _1442_context *)unit->dev;

    log_device("1442: stop %02x\n", ctx->cmd);
    ctx->status |= SNS_CHNEND;
    ctx->data_end = 1;
    switch (ctx->cmd & 03) {
    case 1: /* Write */
            cancel_event(unit, &write_callback);
            if ((ctx->cmd & 0x80) != 0) {
                add_event(unit, &feed_callback, 1000, NULL, 0);
            } else {
                ctx->status |= SNS_DEVEND;
                if (ctx->sense != 0) {
                    ctx->status |= SNS_UNITCHK;
                }
                ctx->cmd_done = 1;
                ctx->busy = 0;
                ctx->cmd = 0;
           }
           break;

    case 2: /* Read */
            cancel_event(unit, &read_callback);
            add_event(unit, &feed_callback, 1000, NULL, 0);
            break;
    }
}

/* Decode command to device */
static void
device_cmd(struct _device *unit, uint8_t bus_out)
{
    struct _1442_context *ctx = (struct _1442_context *)unit->dev;
    uint8_t cmd = bus_out & 0xff;

    log_device("1442: command %02x\n", bus_out);
    ctx->cmd = 0;
    ctx->data_rdy = 0;
    ctx->data_end = 1;
    ctx->feed_done = 0;
    ctx->cmd_done = 1;
    ctx->status = 0;
    switch (cmd & 07) {
    case 0: /* Test I/O */
           ctx->data_end = 0;
           ctx->cmd_done = 0;
           if (ctx->sense != 0) {
              ctx->status |= SNS_UNITCHK;
           }
           return;

    case 1: /* Write */
           ctx->sense &= SENSE_INTERV;
           if ((cmd & 0x5c) != 0) {
               ctx->sense |= SENSE_CMDREJ;
               break;
           }
           ctx->stk_sel = (cmd & 0x20) != 0;
           /* Check if card in correct station */
           if (ctx->pch_full == 0) {
               break;
           }
           ctx->cmd = cmd;
           ctx->data_end = 0;
           ctx->cmd_done = 0;
           ctx->data_rdy = 1;
           ctx->busy = 1;
           add_event(unit, &write_callback, 100, NULL, 0);
           break;

    case 2: /* Read */
           ctx->sense &= SENSE_INTERV;
           if ((cmd & 0xdc) != 0) {
               ctx->sense |= SENSE_CMDREJ;
               break;
           }
           ctx->stk_sel = (cmd & 0x20) != 0;
           /* Check if last card */
           if (ctx->rdy_flag == 0) {
               ctx->sense |= SENSE_INTERV;
           } else {
               ctx->cmd = cmd;
               ctx->data_end = 0;
               ctx->cmd_done = 0;
               ctx->busy = 1;
               add_event(unit, &read_callback, 100, NULL, 0);
           }
           break;

    case 3: /* Feed */
           ctx->sense &= SENSE_INTERV;
           if ((cmd & 0x5c) != 0) {
               ctx->sense |= SENSE_CMDREJ;
               break;
           }
           ctx->stk_sel = (cmd & 0x20) != 0;
           if ((cmd & 0x80) == 0) {
               ctx->status |= SNS_CHNEND|SNS_DEVEND;
               return;
           }
           if (ctx->rdy_flag == 0) {
               ctx->sense |= SENSE_INTERV;
               break;
           }
           ctx->cmd = cmd;
           ctx->cmd_done = 0;
           ctx->busy = 1;
           add_event(unit, &feed_callback, 1000, NULL, 0);
           break;

    case 4:  /* Sense */
           log_device("1442: Sense %02x\n", ctx->sense);
           if (cmd != 0x04) {
               ctx->sense |= SENSE_CMDREJ;
           } else {
               ctx->data = ctx->sense;
               ctx->cmd = cmd;
               ctx->cmd_done = 0;
               ctx->data_end = 0;
               ctx->data_rdy = 1;
               return;
           }
           break;

    default:
           ctx->sense |= SENSE_CMDREJ;     /* Invalid command */
           break;
    }
    if (ctx->data_end) {
        ctx->status |= SNS_CHNEND;
    }
    if (ctx->cmd_done) {
        ctx->status |= SNS_DEVEND;
        if (ctx->sense != 0) {
            ctx->status |= SNS_UNITCHK;
        }
    }
}

/* Process channel */
void
model1442_dev(struct _device *unit, uint16_t *tags, uint16_t bus_out, uint16_t *bus_in)
{
    struct _1442_context *ctx = (struct _1442_context *)unit->dev;
    static   uint16_t last_tags = 0;

    if (last_tags != *tags || unit->selected) {
        print_tags("1442", ctx->state, *tags, bus_out);
        last_tags = 0; // *tags;
    }

    /* Reset device if OPER OUT is dropped */
    if ((*tags & (CHAN_OPR_OUT|CHAN_SUP_OUT)) == 0) {
        if (unit->selected) {
           *tags &= ~(CHAN_OPR_IN|CHAN_ADR_IN|CHAN_SRV_IN|CHAN_STA_IN);
        }
        log_device("1442: %03x reset\n",unit->addr);
        unit->selected = 0;
        unit->request = 0;
        ctx->state = STATE_IDLE;
        ctx->status = 0;
        ctx->sense = 0;
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
            ctx->disconnect = 0;
//            log_device("1442: %03x idle r=%d s=%d d=%d e=%d p=%d b=%d cd=%d %02x %02x\n",unit->addr,
 //               unit->request, unit->stacked, ctx->data_rdy, ctx->data_end, ctx->data_end_post,
  //              ctx->busy, ctx->cmd_done, ctx->cmd, ctx->status);
            /* If operation out, reset device */
            if ((*tags & CHAN_OPR_OUT) == 0) {
                log_device("1442: %03x oper dropped\n",unit->addr);
                break;
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
                log_device("1442: %03x port request\n",unit->addr);
                if ((*tags & (CHAN_SUP_OUT|CHAN_ADR_OUT)) == 0 || ctx->data_rdy) {
                    *tags |= CHAN_REQ_IN;
                } else {
                    *tags &= ~CHAN_REQ_IN;
                }
            }

            /* If select out check if channel is asking for us or we have status */
            if ((*tags & CHAN_SEL_OUT) != 0) {
                 /* Check if looking for this device */
                 if ((*tags & CHAN_ADR_OUT) != 0) {
                     if ((bus_out & 0xff) == (unit->addr & 0xff)) {
                        *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);
                        /* Check if parity error on bus */
                        if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0) {
                            ctx->sense |= SENSE_BUSCHK;
                        }
                        /* If device in operation, respond with busy status */
                        if (ctx->busy/* && ctx->data_end == 0*/) {
                            *bus_in = SNS_BSY | odd_parity[SNS_BSY];
                            *tags |= CHAN_STA_IN;             /* Put Busy flag on bus */
                            ctx->state = STATE_BUSY;
                            log_device("1442: %03x busy\n",unit->addr);
                            break;
                        }

                        /* Clear select in, and raise operation in */
                        *tags |= CHAN_OPR_IN;             /* Put our address on bus */
                        *bus_in = (unit->addr & 0xff) | odd_parity[unit->addr & 0xff];
                        ctx->state = STATE_INIT_SEL; /* Start initial select sequence */
                        unit->selected = 1;
                        log_device("1442: %03x selected\n",unit->addr);
                     }
                     break;
                 }

                 /* If no address out, see if we have request or stacked status */
                 if ((*tags & CHAN_SUP_OUT) == 0 && (unit->request || unit->stacked)) {
                     *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);
                     *tags |= CHAN_OPR_IN;      /* Put our address on bus */
                     *bus_in = (unit->addr & 0xff) | odd_parity[unit->addr & 0xff];
                     unit->selected = 1;
                     ctx->state = STATE_INIT_SEL;
                     log_device("1442: %03x polling\n",unit->addr);
                 }

             }
             break;

            /* Start of initial selection sequence */
    case STATE_INIT_SEL:
            *tags &= ~(CHAN_SEL_OUT);  /* Clear select in */
            *bus_in = (unit->addr & 0xff) | odd_parity[unit->addr & 0xff];
            log_device("1442: %03x address in\n",unit->addr);
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

            log_device("1442: %03x waiting command %02x\n",unit->addr, ctx->status);
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
                    ctx->sense |= SENSE_BUSCHK;
                    ctx->state = STATE_STATUS;
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
                log_device("1442: Halt %03x device\n",unit->addr);
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
             log_device("1442: %03x initial status %02x\n",unit->addr, ctx->status);
             *tags |= (CHAN_STA_IN);
             ctx->state = STATE_STATUS_ACCEPT;    /* Wait for device to accept out status */
             break;

    /* Wait for CPU to either accept or stack status */
    case STATE_STATUS_ACCEPT:
             /* CPU will respond in a couple ways. */
             *tags &= ~(CHAN_SEL_OUT);
             *bus_in = ctx->status | odd_parity[ctx->status];
             if ((*tags & CHAN_CMD_OUT) != 0) {      /* CPU does not want status, stack it */
                 log_device("1442: %03x status stacked\n",unit->addr);
                 unit->stacked = 1;
                 ctx->state = STATE_STATUS_WAIT;
                 *tags &= ~(CHAN_OPR_IN|CHAN_STA_IN);
                 break;
             }
             if ((*tags & CHAN_SRV_OUT) != 0) {   /* CPU accepted the status, continue on */
                log_device("1442: %03x status accepted\n",unit->addr);

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
                    break;
                }

                if (ctx->data_end) {
                    if ((*tags & CHAN_HLD_OUT) == 0) {
                        *tags &= ~(CHAN_OPR_IN);
                    }
                    ctx->state = STATE_STATUS_WAIT;
                    break;
                }
                if (ctx->data_rdy == 0) {
                    ctx->disconnect = 1;
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
             *bus_in = SNS_BSY | odd_parity[SNS_BSY];
             if ((*tags & CHAN_SEL_OUT) == 0) {
                 *tags &= ~(CHAN_SEL_OUT|CHAN_STA_IN);
                 unit->selected = 0;
                 ctx->state = STATE_IDLE;
                 /* If address out, halt device */
                 if ((*tags & CHAN_ADR_OUT) != 0) {
                     log_device("1442: %03x Halt IO\n",unit->addr);
                     if (ctx->data_end == 0) {
                         ctx->data_rdy = 0;
                         ctx->data_end = 1;
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

             log_device("1442: %03x %02x end status %d\n",unit->addr, ctx->status, unit->request);
             ctx->state = STATE_END_ACCEPT;    /* Wait for CPU to accept out status */
             break;

     /* Wait for CPU to accept or stack status */
     case STATE_END_ACCEPT:
             *tags &= ~(CHAN_SEL_OUT);

             *bus_in = ctx->status | odd_parity[ctx->status];
             /* CPU does not want status right now. stack it */
             if ((*tags & CHAN_CMD_OUT) != 0) {
                 log_device("1442: %03x status stacked %d\n",unit->addr, unit->request);
                 unit->stacked = 1;
                 ctx->state = STATE_STATUS_WAIT;
                 *tags &= ~(CHAN_STA_IN|CHAN_OPR_IN);
                 break;
             }

             /* CPU accepted status */
             if ((*tags & CHAN_SRV_OUT) != 0) {

                 log_device("1442: %03x status accepted %d\n",unit->addr, unit->request);
                 ctx->status = 0;
                 /* If end of command, and status accepted, all done */
                 if (ctx->cmd_done) {
                     *tags &= ~(CHAN_STA_IN|CHAN_OPR_IN);
                     unit->stacked = 0;
                     ctx->cmd = 0;
                     ctx->cmd_done = 0;
                     ctx->busy = 0;
                     ctx->data_end = 0;
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
            log_device("1442: %03x wait end b=%d cd=%d %02x %02x\n",unit->addr,
                ctx->busy, ctx->cmd_done, ctx->cmd, ctx->status);
             *tags &= ~(CHAN_SEL_OUT);
             if (ctx->cmd_done) {
                 unit->request = 0;
                 ctx->state = STATE_STATUS;
             }
             break;

      /* Handle normal operations */
      case STATE_OPR:
             log_device("1442: %03x opr %d r=%d e=%d d=%d\n",unit->addr, unit->selected,
                     ctx->data_rdy, ctx->data_end, ctx->disconnect);
             unit->request = 0;
             *tags &= ~(CHAN_SEL_OUT);

             /* If address out, halt device */
             if ((*tags & CHAN_ADR_OUT) != 0) {
                 ctx->data_end = 1;
                 ctx->data_rdy = 0;
                 ctx->status |= SNS_CHNEND;
                 *tags &= ~(CHAN_OPR_IN);
                 unit->selected = 0;
                 ctx->state = STATE_IDLE;
                 break;
             }

             /* If data ready, try and get/send it */
             if (ctx->data_rdy) {
                 ctx->state = STATE_DATA_1;
                 break;
             }

             /* If sense command advance counter */
             if (ctx->cmd == 0x04) {
                 ctx->disconnect = 0;
                 if (ctx->data_rdy == 0) {
                     ctx->data_end = 1;
                     ctx->cmd_done = 1;
                     ctx->busy = 0;
                     ctx->status |= SNS_CHNEND|SNS_DEVEND;
                }
             }

             /* If at end of data or command, present status */
             if (ctx->data_end || ctx->cmd_done) {
                 ctx->state = STATE_END_STATUS;
                 break;
             }

             /* If disconnect requested, and not on selector channel disconnect */
             if (ctx->disconnect) {
                 ctx->disconnect = 0;
                 if ((*tags & CHAN_HLD_OUT) == 0) {
                     *tags &= ~(CHAN_OPR_IN);
                     unit->selected = 0;
                     ctx->state = STATE_IDLE;
                     break;
                 }
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
                          ctx->sense |= SENSE_BUSCHK;
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
                     device_stop(unit);
                 }
                 ctx->disconnect = 1;
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

    /* If no more cards stop processing. */
    if (hopper_size(ctx->feed) == 0 && ctx->eof_flag == 0) {
        ctx->sense |= SENSE_INTERV;
        ctx->rdy_flag = 0;
        return;
    }

    /* If punch station full, put card out to stacker */
    if (ctx->pch_full) {
       log_device("Stack punch %d\n", ctx->stk_sel);
       stack_card(ctx->stack[ctx->stk_sel], &ctx->pch_card);
       ctx->pch_full = 0;
       ctx->pch_col = 0;
    }

    /* Move card from reader station to punch station */
    if (ctx->rdr_full) {
       log_device("move to punch\n");
       memcpy(&ctx->pch_card[0], &ctx->rdr_card, 80 * sizeof(uint16_t));
       ctx->pch_full = 1;
       ctx->rdr_full = 0;
    }

    if (ctx->stop_flag == 0) {
        ctx->rdr_full = read_card(ctx->feed, &ctx->rdr_card);
        ctx->hop_cnt = hopper_size(ctx->feed);
        ctx->rdy_flag = ctx->rdr_full;
        log_device("read card %d size=%d\n", ctx->rdr_full, ctx->hop_cnt);
        if (ctx->hop_cnt == 0 && ctx->eof_flag == 0) {
            ctx->rdy_flag = 0;
        }
    } else {
        ctx->rdy_flag = 0;
    }

    log_device("Size %d %d, %d %d %d\n", hopper_size(ctx->feed),
            stack_size(ctx->feed), hopper_size(ctx->stack[0]),
            stack_size(ctx->stack[0]), ctx->rdy_flag);
    ctx->stop_flag = 0;
    ctx->rdr_col = 0;
    ctx->stk_cnt[0] = stack_size(ctx->stack[0]);
    ctx->stk_cnt[1] = stack_size(ctx->stack[1]);
    if (ctx->rdy_flag == 0) {
log_device("intervent\n");
        ctx->sense |= SENSE_INTERV;
    } else {
        ctx->sense &= ~(SENSE_INTERV);
    }
}

struct _device *
model1442_init(uint16_t addr)
{
     struct _device       *dev1442;
     struct _1442_context *card;

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
     dev1442->draw_model = &model1442_draw;
     dev1442->create_ctrl = &model1442_control;
     dev1442->init_device = &model1442_init_graphics;
     dev1442->type_name = "1442";
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

