/*
 * microsim360 - Console keyboard/printer model 1052.
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

#include <SDL.h>
#include <SDL_thread.h>
#include <stdio.h>
#include <stdint.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#define closesocket close
#define SOCKET int
#else
#include <winsock.h>
#endif
#include "logger.h"
#include "device.h"
#include "model1052.h"
#include "xlat.h"

/* Telnet protocol constants - negatives are for init'ing signed char data */

#define TN_IAC          -1                              /* protocol delim */
#define TN_DONT         -2                              /* dont */
#define TN_DO           -3                              /* do */
#define TN_WONT         -4                              /* wont */
#define TN_WILL         -5                              /* will */
#define TN_BRK          -13                             /* break */
#define TN_BIN          0                               /* bin */
#define TN_ECHO         1                               /* echo */
#define TN_SGA          3                               /* sga */
#define TN_LINE         34                              /* line mode */
#define TN_CR           015                             /* carriage return */
#define TN_LF           012                             /* line feed */
#define TN_NUL          000                             /* null */

/* Telnet line states */

#define TNS_NORM        000                             /* normal */
#define TNS_IAC         001                             /* IAC seen */
#define TNS_WILL        002                             /* WILL seen */
#define TNS_WONT        003                             /* WONT seen */
#define TNS_SKIP        004                             /* skip next cmd */
#define TNS_CRPAD       005                             /* CR padding */

/*
 *  Commands.
 *
 *            01234567
 *  Write     00000001     Write to console
 *  Write ACR 00001001     Write to console, followed by carrage return
 *  Read      00001010     Read line from console
 *  NOP       00000011     No operation
 *  Sense     00000100
 */


#define SENSE_CMDREJ    BIT0  /* Invalid command */
#define SENSE_INTERV    BIT1  /* Operator intervention */
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


struct _1052_context {
    int                    addr;         /* Device address */
    int                    chan;         /* Channel address */
    int                    state;        /* Current channel state */
    int                    selected;     /* Device currently selected */
    int                    sense;        /* Current sense value */
    int                    cmd;          /* Current command */
    int                    status;       /* Current bus status */
    uint16_t               data;         /* Current byte to send/recieve */
    int                    data_rdy;     /* Data is valid */
    int                    data_end;     /* Data transfer over */
    int                    cmd_done;     /* Command done */
    SOCKET                 sock;         /* Socket to wait for connection on */
    SOCKET                 cons;         /* Socket to send data over */
    int                    key_buf[256]; /* Buffer holding input record */
    char                   out_buf;      /* Character to output */
    int                    out_flg;      /* Output character full */
    int                    out_cr;       /* Output carrage return when no charater in buffer */
    int                    in_flg;       /* Accept input */
    int                    in_ptr;       /* Pointer to where to insert data */
    int                    out_ptr;      /* Pointer to where to grab data */
    int                    in_len;       /* Number of characters pending */
    int                    home_loop;    /* Home loop flag */
    int                    attn_flg;     /* Attention key pressed */
    int                    cancel_flg;   /* Cancel key pressed */
    int                    eob_flg;      /* Eob key pressed */
    fd_set                 fds_socks;    /* Current scaning sockets */
    SDL_Thread             *thrd;        /* Pointer to thread */
    int                    running;      /* Device running. */
};

#ifdef _WIN32
WSADATA  wsaData = { 0 };
#endif
int model1052_thrd(void *data);

void
model1052_dev(struct _device *unit, uint16_t *tags, uint16_t bus_out, uint16_t *bus_in)
{
    static uint16_t last_tags = 0;
    struct _1052_context *ctx = (struct _1052_context *)unit->dev;
    int       i;
    uint16_t  t_request;
    uint16_t  in_tags;
    uint16_t  out_tags;

    if (last_tags != *tags) {
        print_tags("Console", ctx->state, *tags, bus_out);
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
                 log_device("console selected\n");
                 break;
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
                 log_device("console address\n");
                 break;
            }

            /* Wait for Command out to raise */
            /* Can now drop address in */
            if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                 log_device("console command %02x\n", bus_out);
                 ctx->cmd = bus_out & 0xff;
                 ctx->data_rdy = 0;
                 ctx->data_end = 0;
                 ctx->cmd_done = 0;
                 ctx->status = 0;
                 ctx->state = STATE_CMD;
                 *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN);              /* Clear address in */
                 switch (ctx->cmd & 07) {
                 case 0: /* Test I/O */
                        break;
                 case 1: /* Write */
                        ctx->sense = 0;
                        if ((ctx->cmd & 0xf6) != 0) {
                            ctx->cmd = 0;
                            ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                            ctx->sense = SENSE_CMDREJ;
                            break;
                        }
                        in_tags = BIT0;  /* Start Home loop */
                        model1052_func(ctx, &out_tags, in_tags, &t_request);
                        ctx->data_rdy = 1;
                        break;
                 case 2: /* Read */
                        ctx->sense = 0;
                        if (ctx->cmd  != 0xa) {
                            ctx->cmd = 0;
                            ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                            ctx->sense = SENSE_CMDREJ;
                            break;
                        }
                        in_tags = BIT1|BIT3;  /* Send proceed */
                        model1052_func(ctx, &out_tags, in_tags, &t_request);
                        break;
                 case 3: /* Nop */
                        if (ctx->cmd  != 0x3) {
                            ctx->cmd = 0;
                            ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                            ctx->sense = SENSE_CMDREJ;
                            break;
                        }
                        ctx->sense = 0;
                        ctx->status = SNS_CHNEND|SNS_DEVEND;
                        ctx->data_end = 1;
                        break;
                 case 4:  /* Sense */
                        if (ctx->cmd != 4) {
                            ctx->cmd = 0;
                            ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                            ctx->sense = SENSE_CMDREJ;
                            break;
                        }
                        ctx->data = ctx->sense;
                        ctx->data_rdy = 1;
                        ctx->data_end = 1;
                        log_device("console Sense %02x\n", ctx->sense);
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
                 log_device("console init stat\n");
             }

             /* When we get acknowlegment, go wait for it to go away */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_SUP_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~CHAN_STA_IN;
                 ctx->state = STATE_INIT_STAT;
                 log_device("console init stat\n");
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
                     log_device("console error state done\n");
                     break;
                 }

                 /* If initial status has Channel end, go wait for device to finish */
                 if ((ctx->status & SNS_CHNEND) != 0) {
                     *tags &= ~(CHAN_OPR_IN|CHAN_SEL_OUT);
                     ctx->selected = 0;
                     ctx->state = STATE_WAIT;
                     log_device("console channel end\n");
                     break;
                 }

                 /* If select out has dropped, drop operator in if no data to send */
                 if ((*tags & CHAN_SEL_OUT) == 0 && ctx->data_rdy == 0) {
                     *tags &= ~(CHAN_OPR_IN);           /* Clear select out and in */
                     ctx->selected = 0;
                 }

                 /* Wait for device to have some data to transmit */
                 ctx->state = STATE_OPR;

                 log_device("console state done\n");
                 break;
             }
             *tags &= ~(CHAN_SEL_OUT);                 /* Clear select out and in */
             break;

    case STATE_OPR:
             log_device("console opr %d\n", ctx->selected);

             /* If we are selected clear select in */
             if (ctx->selected)
                 *tags &= ~CHAN_SEL_OUT;

             /* If not ready, see if we can do something */
             if (ctx->data_rdy == 0 && ctx->data_end == 0) {
                 switch (ctx->cmd & 0x7) {
                 case 0: /* Test I/O */
                 case 3: /* Nop */
                 case 4:  /* Sense */
                 defualt:
                        ctx->status |= SNS_CHNEND|SNS_DEVEND;
                        break;
                 case 1: /* Write */
                        in_tags = 0;
                        out_tags = 0;
                        model1052_func(ctx, &out_tags, in_tags, &t_request);
                        if (out_tags & BIT1) {  /* If ready, go grab some more data */
                            ctx->data_rdy = 1;
                        }
                        break;

                 case 2: /* Read */
                        in_tags = 0;
                        out_tags = 0;
                        model1052_func(ctx, &out_tags, in_tags, &t_request);
                        if (out_tags & BIT1) {
                            model1052_in(ctx, &ctx->data);
                            ctx->data_rdy = 1;
                        } else if (out_tags & BIT0) {
                            ctx->data_end = 1;
                            ctx->state |= SNS_CHNEND|SNS_DEVEND|SNS_UNITEXP;
                        } else if (out_tags & BIT2) {
                            ctx->data_end = 1;
                            ctx->state |= SNS_CHNEND|SNS_DEVEND;
                        }
                        break;
                  }
             }

             /* If data ready, try and get/send it */
             if (ctx->data_rdy) {
                 ctx->state = (ctx->cmd & 1) ? STATE_DATA_I : STATE_DATA_O;
                 break;
             }

             /* If data end, tell CPU */
             if (ctx->data_end) {
                 switch (ctx->cmd & 0xf) {
                 case 0: /* Test I/O */
                 case 3: /* Nop */
                 case 4:  /* Sense */
                 default:
                        ctx->status |= SNS_CHNEND|SNS_DEVEND;
                        break;
                 case 9: /* Write ACR */
                        in_tags = BIT5;
                        model1052_func(ctx, &out_tags, in_tags, &t_request);
                        break;

                 case 1: /* Write */
                        break;
                 case 0xa: /* Read */
                        in_tags = BIT1|BIT3;
                        model1052_func(ctx, &out_tags, in_tags, &t_request);
                        break;
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
                log_device("console reselect\n");
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
                  log_device("console Halt i/o\n");
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
                  log_device("console deselected\n");
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
             log_device("console Request\n");
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_REQ_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_REQ_IN)) {
                 /* Put our address on the bus */
                 *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);        /* Clear select out and in */
                 *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                 *bus_in = ctx->addr | odd_parity[ctx->addr]; /* Put out our address */
                 log_device("console Reselect\n");
                 break;
             }

             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                 /* Put our address on the bus */
                 *tags &= ~(CHAN_SEL_OUT);                    /* Clear select out and in */
                 *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                 *bus_in = ctx->addr | odd_parity[ctx->addr]; /* Put out our address */
                 log_device("console Address\n");
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
                  log_device("console selected\n");
                  break;
              }

              /* See if another device got it. */
              if ((*tags & (CHAN_OPR_IN|CHAN_STA_IN)) != 0) {
                  /* Drop request out until channel free again */
                  log_device("console Other device\n");
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
                       ctx->data = bus_out & 0xff; /* Grab data */
                       model1052_out(ctx, (bus_out & 0xff));
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
                   log_device("console Data sent\n");
                   break;
              }
              /* If CMD out, indicates end of data */
              if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_SRV_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_SRV_IN)) {
                   *tags &= ~(CHAN_SEL_OUT|CHAN_SRV_IN);  /* Count reached zero, no more accepted */
                   ctx->data_rdy = 0;
                   ctx->data_end = 1;
                   ctx->state = STATE_INIT_STAT;           /* Back to operation. */
                   log_device("console Data End\n");
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
                 log_device("console Accepted data_end\n");
                 ctx->status &= ~SNS_CHNEND;
                 ctx->state = STATE_WAIT;              /* Wait for device to be done */
                 break;
             }

             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~(CHAN_SEL_OUT|CHAN_OPR_IN|CHAN_STA_IN);  /* Clear select out and in */
                  log_device("console Stacked data_end\n");
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
                 log_device("console Accepted end\n");
                  ctx->selected = 0;
                  ctx->state = STATE_IDLE;                          /* All done, back to idle state */
                  break;
             }

             /* Command out to status in indicates a stacking of status */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~(CHAN_SEL_OUT|CHAN_OPR_IN|CHAN_STA_IN);  /* Clear select out and in */
                  log_device("console Stacked\n");
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
                 log_device("console stack selected\n");
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
                 log_device("console stack address\n");
            }

            /* Wait for Command out to raise */
            /* Can now drop address in */
            if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                 log_device("console stack command %02x\n", bus_out);
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
                  log_device("console stack init stat %02x\n", ctx->status);
             }

             /* When we get acknowlegment, go wait for it to go away */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_SUP_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~(CHAN_STA_IN|CHAN_SEL_OUT);
                 ctx->state = STATE_STACK_HLD;
                 log_device("console stack init stat %02x done\n", ctx->status);
                 break;
             }
             /* When we get acknowlegment, go wait for it to go away */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_SUP_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~(CHAN_STA_IN|CHAN_OPR_IN);
                 ctx->state = STATE_STACK;
                 ctx->selected = 0;
                 log_device("console stack init stat %02x\n", ctx->status);
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
                 log_device("console stack done\n");
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
                 log_device("Command done %d\n", ctx->selected);
                 ctx->state = STATE_END;
             }
             break;

    }
}

struct _device *
model1052_init(void *render, uint16_t addr)
{
    struct _device  *dev1052;
    void            *ctx;

    if ((dev1052 = calloc(1, sizeof(struct _device))) == NULL)
         return NULL;
    if ((ctx = model1052_init_ctx(3270)) == NULL) {
         free(dev1052);
         return NULL;
    }

    dev1052->bus_func = &model1052_dev;
    dev1052->dev = (void *)ctx;
    dev1052->draw_model = NULL;
    dev1052->create_ctrl =  NULL;
    dev1052->rect[0].x = 0;
    dev1052->rect[0].y = 0;
    dev1052->rect[0].w = 0;
    dev1052->rect[0].h = 0;
    dev1052->n_units = 1;
    dev1052->addr = addr;

    if (addr != 0)
       add_chan(dev1052, addr);
    return dev1052;
}

void *
model1052_init_ctx(uint16_t port)
{
    struct _1052_context *ctx;
    struct sockaddr_in   locAddr;
    int                  on = 1;
	int                  i;

    if ((ctx = (struct _1052_context *)calloc(1, sizeof(struct _1052_context))) == NULL) {
         return NULL;
    }

#ifdef _WIN32
	i = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (i != 0) {
		log_console("WSAStartup failed: %d\n", i);
		return NULL;
	}
#endif
    FD_ZERO(&ctx->fds_socks);
    ctx->cons = 0;
    if ((ctx->sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket Open");
        return NULL;
    }

    setsockopt(ctx->sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    locAddr.sin_family = AF_INET;
    locAddr.sin_port = htons(port);
    locAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(ctx->sock, (struct sockaddr *)&locAddr, sizeof(locAddr)) < 0) {
        perror("Socked Bind");
        close(ctx->sock);
        return NULL;
    }
    FD_SET(ctx->sock, &ctx->fds_socks);
    listen(ctx->sock, 1);
    log_console("socket open\n");
    ctx->running = 1;
    ctx->thrd = SDL_CreateThread(model1052_thrd, "Console", ctx);
    log_console("listern created\n");
    return ctx;
}

void
model1052_out(void *data, uint16_t out_char)
{
   struct _1052_context *ctx = (struct _1052_context *)data;
   char    ch = ebcdic_to_ascii[out_char & 0xff];
   ctx->out_buf = ch;
   ctx->out_flg = 1;
}

/*
 * Input one character from interface queue.
 */
void
model1052_in(void *data, uint16_t *in_char)
{
   struct _1052_context *ctx = (struct _1052_context *)data;
   char   ch;
   if (ctx->in_flg && ctx->in_len > 0) {
       ch = ctx->key_buf[ctx->out_ptr++];
       *in_char = ascii_to_ebcdic[(ch & 0xff)];
       *in_char |= odd_parity[*in_char];
       ctx->out_ptr &= 0xff;
       ctx->in_len--;
   }
}

/*
 * Functions for 1052 interface.
 *
 *  Tags_in
 *   Bit 0 - Home loop reader latch start
 *   Bit 1 - Read on latch
 *   Bit 2 - Micro Share
 *   Bit 3 - Proceed.
 *   Bit 4 - Audible Alarm * - Not used.
 *   Bit 5 - Send CR.
 *   Bit 6 - Reset attention signal.
 *   Bit 7 - Reset latch.
 *
 *
 *  Tags_out
 *   Bit 0  - Cancel
 *   Bit 1  - Ready.
 *   Bit 2  - EOB
 *   Bit 3  - Operational
 *   Bit 4  - Home start. * Not used
 *   Bit 5  - Intervention required * Not used
 *   Bit 6  - Attention.
 *   Bit 7  - Data Check * not possible.
 *
 */
void
model1052_func(void *data, uint16_t *tags_out, uint16_t tags_in, uint16_t *t_request)
{
   struct _1052_context *ctx = (struct _1052_context *)data;

   *t_request = 0;
   *tags_out = 0;
   if (ctx->cons != 0) {
       /* Set default tags out */
       *tags_out = BIT3;

       /* If we have reset signal, clear pending input and flags */
       if (tags_in & BIT7) {
          ctx->in_flg = 0;
          ctx->in_len = 0;
          ctx->out_ptr = ctx->in_ptr;
          ctx->cancel_flg = 0;
          ctx->eob_flg = 0;
          ctx->home_loop = 0;
       }

       /* Clear attention signal */
       if (tags_in & BIT6) {
           ctx->attn_flg = 0;
       }

       /* If Output enabled, if input done, signal ready */
       if ((tags_in & BIT0) != 0) {
           ctx->home_loop = 1;
       }

       /* If microshare enabled */
       if ((tags_in & BIT2) != 0) {
           *tags_out |= 0x00;
       }

       /* If proceed set, signal that we can accept input */
       if ((tags_in & (BIT1|BIT3)) == (BIT1|BIT3)) {
          ctx->in_flg = 1;
       }

       /* Check if sending characters and finished */
       if (ctx->home_loop != 0 && ctx->out_flg == 0 && ctx->out_cr == 0) {
           *tags_out |= BIT1;
       }

       /* If we have any input ready flag it as available */
       if (ctx->in_len > 0) {
           *tags_out |= BIT1;
       } else {
           /* Input all drained check of cancel or EOB set */
           if (ctx->cancel_flg) {
               *tags_out |= BIT0;
           }
           if (ctx->eob_flg) {
               *tags_out |= BIT2;
           }
       }

       /* If we have attention signal to CPU */
       if (ctx->attn_flg) {
           *tags_out |= BIT6;
       }

       /* If request CR signal to send one */
       if ((tags_in & BIT5) != 0) {
           ctx->out_cr = 1;
       }

       /* Check if we need to notify the CPU of anything */
       if ((*tags_out & 0xef) != 0) {
          *t_request = 1;
       }

       log_console("Cons %02x %02x %d\n", tags_in, *tags_out, *t_request);
   }
}

void
model1052_done(void *data)
{
   struct _1052_context *ctx = (struct _1052_context *)data;
   if (ctx->running) {
       log_console("Kill console\n");
       ctx->running = 0;
       SDL_WaitThread(ctx->thrd, NULL);
   }
   if (ctx->cons > 0)
       closesocket(ctx->cons);
   if (ctx->sock > 0)
       closesocket(ctx->sock);
#ifdef _WIN32
   WSACleanup();
#endif
   free(ctx);
}


static void
push_char(struct _1052_context *ctx, char in_char)
{
    if (in_char == '\033') {
       ctx->attn_flg = 1;
    } else if (ctx->in_flg) {
       if (in_char == '\03') {
           ctx->cancel_flg = 1;
       } else if (in_char == '\r') {
           ctx->eob_flg = 1;
       } else {
           ctx->key_buf[ctx->in_ptr++] = in_char;
           ctx->in_ptr &= 0xff;
           ctx->in_len++;
           log_console("Cons push_char(%02x)\n", in_char);
           send(ctx->cons, &in_char, 1, 0);
       }
    }
}

static char init_string[] = {
        TN_IAC, TN_WILL, TN_LINE,
        TN_IAC, TN_WILL, TN_SGA,
        TN_IAC, TN_WILL, TN_ECHO,
        TN_IAC, TN_WILL, TN_BIN,
        TN_IAC, TN_DO, TN_BIN
};

int
model1052_thrd(void *data)
{
    struct _1052_context *ctx = (struct _1052_context *)data;
    int            j, k;
    int            maxfd;
    int            t_state;      /* Current tellnet state */
    struct timeval tv = {1,0};
    fd_set         read_set;
    struct sockaddr_in client;
    socklen_t      size;
    SOCKET         newsock;
    char           buffer[256];

    log_console("Console started\n");

    while(ctx->running) {
        maxfd = ctx->sock;
        if (ctx->cons > 0 && ctx->cons > maxfd)
            maxfd = ctx->cons;
        read_set = ctx->fds_socks;
        tv.tv_sec = 0;
        tv.tv_usec = 33000;
        (void)select(maxfd+1, &read_set, NULL, NULL, &tv);

        /* Do accept on socket. */
        if (FD_ISSET(ctx->sock, &read_set)) {
            size = sizeof(client);
            newsock = accept(ctx->sock, (struct sockaddr *)&client, &size);
            log_console("Accept\n\r");
            if (ctx->cons == 0) {
               log_console("Connected\n");
               ctx->cons = newsock;
               FD_SET(newsock, &ctx->fds_socks);
               send(newsock, init_string, 15, 0);
               ctx->in_ptr = 0;
               ctx->out_ptr = 0;
               ctx->in_len = 0;
               t_state = TNS_NORM;
           } else {
               static char *msg = "Console already connected\n\r";
               send(newsock, msg, sizeof(msg), 0);
               closesocket(newsock);
           }
        }

        /* Send any data ready to send */
        if (ctx->out_flg) {
            send(ctx->cons, &ctx->out_buf, 1, 0);
            ctx->out_flg = 0;
            if (ctx->out_buf == '\r')
               send(ctx->cons, "\n", 1, 0);
        }
        if (ctx->out_cr) {
            send(ctx->cons, "\r\n", 2, 0);
            ctx->out_cr = 0;
        }

        /* Collect any waiting input */
        if (FD_ISSET(ctx->cons, &read_set)) {
            j = recv(ctx->cons, buffer, 256, 0);
            if (j == 0) {
               log_console("Disonnected\n");
               FD_CLR(ctx->cons, &ctx->fds_socks);
               close(ctx->cons);
               ctx->cons = 0;
            }
            k = 0;
            while(k < j && ctx->in_len < 256) {
               char t;

               t = buffer[k++];
               switch(t_state) {
               case TNS_NORM:
                    if (t == TN_IAC)
                       t_state = TNS_IAC;
                    else
                       push_char(ctx, t);
                    break;
               case TNS_IAC:
                    if (t == TN_IAC) {
                       push_char(ctx, t);
                       t_state = TNS_NORM;
                    } else if (t == TN_BRK) {
                       t_state = TNS_NORM;
                    } else if (t == TN_WILL) {
                       t_state = TNS_WILL;
                    } else if (t == TN_WONT) {
                       t_state = TNS_WONT;
                    } else
                       t_state = TNS_SKIP;
                    break;
                case TNS_WILL:
                case TNS_WONT:
                case TNS_SKIP:
                    t_state = TNS_NORM;
                    break;
                }
            }
        }
   }
   return 0;
}

int
model1052_create(struct _option *opt)
{
    struct _device  *dev1052;
    void            *ctx;
    uint16_t         port = 3270;
    struct _option   opts;

    while (get_option(&opts)) {
         int       v;
         char      *p;
         if (strcmp(opts.opt, "PORT") == 0 && opts.string[0] != '\0') {
             v = 0;
             for (p = &opts.string[0]; *p != '\0'; p++) {
                 if (*p >= '0' && *p <= '9') {
                     v = (v * 10) + (*p - '0');
                 } else {
                     fprintf(stderr, "Port not numeric %s\n", opts.string);
                     return 0;
                 }
             }
             port = v;
         } else {
             fprintf(stderr, "Invalid option %s\n", opts.opt);
             return 0;
         }
    }
    if ((dev1052 = calloc(1, sizeof(struct _device))) == NULL)
         return 0;
    if ((ctx = model1052_init_ctx(port)) == NULL) {
         free(dev1052);
         return 0;
    }

    dev1052->bus_func = &model1052_dev;
    dev1052->dev = (void *)ctx;
    dev1052->draw_model = NULL;
    dev1052->create_ctrl =  NULL;
    dev1052->rect[0].x = 0;
    dev1052->rect[0].y = 0;
    dev1052->rect[0].w = 0;
    dev1052->rect[0].h = 0;
    dev1052->n_units = 1;
    dev1052->addr = opt->addr;

    if (opt->addr != 0)
       add_chan(dev1052, opt->addr);
    return 1;
}

DEV_LIST_STRUCT(1052, DEVICE_TYPE, 0);

