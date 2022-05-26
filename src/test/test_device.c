/*
 * microsim360 - Test device controller.
 *
 * Copyright 2022, Richard Cornwell
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
#include <stdint.h>
#include "xlat.h"
#include "logger.h"
#include "device.h"

#define SENSE_CMDREJ    BIT0  /* Invalid command */
#define SENSE_INTERV    BIT1  /* Operator intervention, test empty */
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


struct _test_context {
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
    int                    cmd_done;     /* Command finished */
    uint8_t                buffer[256];  /* Data buffer. */
    int                    max_data;     /* Max counter. */
    int                    data_cnt;     /* Data counter */
};

void
test_dev(struct _device *unit, uint16_t *tags, uint16_t bus_out, uint16_t *bus_in)
{
    struct _test_context *ctx = (struct _test_context *)unit->dev;
    static   uint16_t last_tags = 0;

    if (last_tags != *tags) {
        print_tags("Test", ctx->state, *tags, bus_out);
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
                 log_device("test selected\n");
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
                 log_device("test address\n");
                 break;
            }

            /* Wait for Command out to raise */
            /* Can now drop address in */
            if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                 log_device("test command %02x\n", bus_out);
                 ctx->cmd = bus_out & 0xff;
                 ctx->data_rdy = 0;
                 ctx->data_end = 0;
                 ctx->status = 0;
                 ctx->state = STATE_CMD;
                 *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN);              /* Clear address in */
                 switch (ctx->cmd & 07) {
                 case 0: /* Test I/O */
                        break;
                 case 1: /* Write */
                        ctx->sense = 0;
                        ctx->data_rdy = 1;
                        ctx->data_cnt = 0;
                        break;
                 case 2: /* Read */
                        ctx->sense = 0;
                        ctx->data_cnt = 0;
                        break;
                 case 3: /* Control */
                        /* Nop or control */
                        ctx->sense = 0;
                        if (ctx->cmd == 3) {
                            ctx->status = SNS_CHNEND|SNS_DEVEND;
                            ctx->data_end = 1;
                        }
                        /* Grab a data byte */
                        if (ctx->cmd == 0xb) {
                            ctx->data_rdy = 1;
                            ctx->data_cnt = 0;
                        }
                        break;
                 case 4:  /* Sense */
                        ctx->data = ctx->sense;
                        ctx->data_rdy = 1;
                        ctx->data_end = 1;
                        log_device("test Sense %02x\n", ctx->sense);
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
                 log_device("test init stat\n");
             }

             /* When we get acknowlegment, go wait for it to go away */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_SUP_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~CHAN_STA_IN;
                 ctx->state = STATE_INIT_STAT;
                 log_device("test init stat\n");
             }
             *bus_in = ctx->status | odd_parity[ctx->status];   /* Set initial status */
             /* Device selected */
             *tags &= ~(CHAN_SEL_OUT);                  /* Clear select out and in */
             break;

    case STATE_INIT_STAT:

             /* Wait for Service out to drop */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_OPR_IN)) {
                 /* If no command, or check status, go back to idle */
                 if (ctx->cmd == 0 || (ctx->status & (SNS_UNITCHK|SNS_UNITEXP)) != 0) {
                     *tags &= ~(CHAN_OPR_IN|CHAN_SEL_OUT);   /* Clear select out and in */
                     ctx->state = STATE_IDLE;
                     ctx->selected = 0;
                     log_device("test error state done\n");
                     break;
                 }

                 /* If initial status has Channel end, go wait for device to finish */
                 if ((ctx->status & SNS_CHNEND) != 0) {
                     *tags &= ~(CHAN_OPR_IN|CHAN_SEL_OUT);
                     ctx->selected = 0;
                     ctx->state = STATE_WAIT;
                     log_device("test channel end\n");
                     break;
                 }

                 /* If select out has dropped, drop operator in if no data to send */
                 if ((*tags & CHAN_SEL_OUT) == 0 && ctx->data_rdy == 0) {
                     *tags &= ~(CHAN_OPR_IN);           /* Clear select out and in */
                     ctx->selected = 0;
                 }

                 /* Wait for device to have some data to transmit */
                 ctx->state = STATE_OPR;

                 log_device("test state done\n");
                 break;
             }
             *tags &= ~(CHAN_SEL_OUT);                 /* Clear select out and in */
             break;

    case STATE_OPR:
             log_device("test opr %d\n", ctx->selected);

             /* If we are selected clear select in */
             if (ctx->selected)
                 *tags &= ~CHAN_SEL_OUT;

             /* If data ready, try and get/send it */
             if (ctx->data_rdy) {
                 if (ctx->cmd == 2) {
                     ctx->data = ctx->buffer[ctx->data_cnt];
                     if (ctx->data_cnt > ctx->max_data)
                         ctx->data_end = 1;
                     else
                         ctx->data_cnt++;
                 }
                 ctx->state = (ctx->cmd & 1) ? STATE_DATA_I : STATE_DATA_O;
                 break;
             }

             /* If data end, tell CPU */
             if (ctx->data_end) {
                 if (ctx->cmd == 0x4) {
                    ctx->status |= SNS_CHNEND|SNS_DEVEND;
                 }
                 if ((ctx->status & SNS_CHNEND) != 0) {
                    ctx->state = STATE_DATA_END;
                 }
                 if ((ctx->status & SNS_DEVEND) != 0) {
                    ctx->state = STATE_END;
                 }
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
                log_device("test reselect\n");
                break;
             }

             /* On Select channel, Select Out will not drop */
             /* Catch halt I/O */
             if (ctx->selected && *tags == (CHAN_OPR_OUT|CHAN_ADR_OUT|CHAN_OPR_IN) &&
                  (bus_out & 0xff) == ctx->addr)  {
                  /* Halt I/O */
                  /* Device selected */
                  *tags &= ~(CHAN_OPR_IN);             /* Clear select out and in */
                  ctx->state = STATE_END;              /* Return busy status */
                  ctx->data_end = 1;
                  ctx->selected = 0;
                  log_device("test Halt i/o\n");
                  break;
             }

             /* Return status while waiting for Address out to drop */
             if (ctx->selected &&
                   *tags == (CHAN_OPR_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_STA_IN) &&
                   (bus_out & 0xff) == ctx->addr) {
                /* Device selected */
                *tags &= ~(CHAN_SEL_OUT);              /* Clear select out and in */
                *tags |= CHAN_STA_IN;                  /* Indicate busy status */
                *bus_in = SNS_CHNEND|SNS_DEVEND|0x100;
                break;
             }

             /* When select out drops, clear selected flag */
             if (ctx->selected && (*tags == (CHAN_OPR_OUT|CHAN_HLD_OUT|CHAN_STA_IN) ||
                                   *tags == (CHAN_OPR_OUT|CHAN_STA_IN))) {
                  *tags &= ~(CHAN_STA_IN);             /* Clear select out and in */
                  ctx->selected = 0;
                  log_device("test deselected\n");
                  break;
             }

             /* If select out dropped, and no data, drop oper in */
             if (ctx->selected && (*tags == (CHAN_OPR_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                                   *tags == (CHAN_OPR_OUT|CHAN_OPR_IN))) {
                *tags &= ~(CHAN_OPR_IN);
                ctx->selected = 0;
                break;
             }

             break;

    case STATE_REQ:   /* Data available and we are not talking on channel */
             log_device("test Request\n");
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_REQ_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_REQ_IN)) {
                 /* Put our address on the bus */
                 *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);        /* Clear select out and in */
                 *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                 *bus_in = ctx->addr | odd_parity[ctx->addr]; /* Put out our address */
                 log_device("test Reselect\n");
                 break;
             }

             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
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
                  log_device("test selected\n");
                  break;
              }

              /* See if another device got it. */
              if ((*tags & (CHAN_OPR_IN|CHAN_STA_IN)) != 0) {
                  /* Drop request out until channel free again */
                  log_device("test Other device\n");
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
                       ctx->buffer[ctx->data_cnt] = (bus_out & 0xff);
                       if (ctx->data_cnt > ctx->max_data)
                           ctx->data_end = 1;
                       else
                           ctx->data_cnt++;
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
                   log_device("test Data sent\n");
                   break;
              }
              /* If CMD out, indicates end of data */
              if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_SRV_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_SRV_IN)) {
                   *tags &= ~(CHAN_SEL_OUT|CHAN_SRV_IN);  /* Count reached zero, no more accepted */
                   ctx->data_rdy = 0;
                   ctx->data_end = 1;
                   ctx->state = STATE_INIT_STAT;           /* Back to operation. */
                   log_device("test Data End\n");
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
             if (ctx->selected && (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_OPR_IN))) {
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
                 log_device("test Accepted data_end\n");
                 ctx->status &= ~SNS_CHNEND;
                 ctx->state = STATE_WAIT;              /* Wait for device to be done */
                 break;
             }

             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~(CHAN_SEL_OUT|CHAN_OPR_IN|CHAN_STA_IN);  /* Clear select out and in */
                  log_device("test Stacked data_end\n");
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
                 log_device("test Accepted end\n");
                  ctx->selected = 0;
                  ctx->state = STATE_IDLE;                          /* All done, back to idle state */
                  break;
             }

             /* Command out to status in indicates a stacking of status */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~(CHAN_SEL_OUT|CHAN_OPR_IN|CHAN_STA_IN);  /* Clear select out and in */
                  log_device("test Stacked\n");
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
                 log_device("test stack selected\n");
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
                 log_device("test stack address\n");
            }

            /* Wait for Command out to raise */
            /* Can now drop address in */
            if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                 log_device("test stack command %02x\n", bus_out);
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
                  log_device("test stack init stat %02x\n", ctx->status);
             }

             /* When we get acknowlegment, go wait for it to go away */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_SUP_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~(CHAN_STA_IN|CHAN_SEL_OUT);
                 ctx->state = STATE_STACK_HLD;
                 log_device("test stack init stat %02x done\n", ctx->status);
                 break;
             }
             /* When we get acknowlegment, go wait for it to go away */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_SUP_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~(CHAN_STA_IN|CHAN_OPR_IN);
                 ctx->state = STATE_STACK;
                 ctx->selected = 0;
                 log_device("test stack init stat %02x\n", ctx->status);
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
                 log_device("test stack done\n");
             }
             *tags &= ~(CHAN_SEL_OUT);                 /* Clear select out and in */
             break;

    case STATE_WAIT:
             /* If we get select out with address out, reselection */
             if (!ctx->selected &&
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT) &&
                (bus_out & 0xff) == ctx->addr) {
                /* Device selected */
                *tags |= CHAN_STA_IN;
                *bus_in = SNS_BSY;
                ctx->selected = 1;
                log_device("wait select attempt\n");
             }

             /* If selected clear status in when select out drops */
             if (ctx->selected && *tags == (CHAN_ADR_OUT|CHAN_OPR_OUT|CHAN_STA_IN)) {
                /* Device selected */
                *tags &= ~(CHAN_SEL_OUT|CHAN_STA_IN);          /* Clear status in */
                ctx->selected = 0;
                log_device("wait deselect\n");
             }

             if (ctx->cmd_done) {
                 log_device("feed done %d\n", ctx->selected);
                 ctx->state = STATE_END;
             }
             break;

    }
}

