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
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#include "logger.h"
#include "event.h"
#include "device.h"
#include "model1052.h"
#include "xlat.h"

DEV_LIST_STRUCT(1052, DEV_TYPE, 0);

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

struct _1052_context {
    int                    addr;         /* Device address */
    int                    chan;         /* Channel address */
    device_state_t         state;        /* Current channel state */
    int                    sense;        /* Current sense value */
    int                    cmd;          /* Current command */
    int                    status;       /* Current bus status */
    int                    busy;         /* Device operating */
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

#define SENSE_CMDREJ    BIT0  /* Invalid command */
#define SENSE_INTERV    BIT1  /* Operator intervention, test empty */
#define SENSE_BUSCHK    BIT2  /* Bus parity error */
#define SENSE_EQUCHK    BIT3  /* Equipment check, not implemented */
#define SENSE_DATCHK    BIT4  /* More then 1 punch in rows 1-7 */
#define SENSE_OVRRUN    BIT5  /* Data missed */

/* Check if anything ready to handle */
static void
poll_callback(struct _device *unit, void *arg, int iarg)
{
    struct _1052_context *ctx = (struct _1052_context *)unit->dev;
    uint16_t    out_tags;

    model1052_func(ctx, &out_tags, 0, NULL);

    if ((ctx->cmd & 1) == 0 && (out_tags & BIT1) != 0) {
       log_console("1052: ready\n");
       model1052_in(ctx, &ctx->data);
       ctx->data &= 0xff;
       ctx->data_rdy = 1;
       unit->request = 1;
    }
    if ((out_tags & (BIT0|BIT2)) != 0) {
       log_console("1052: data_end\n");
       if ((out_tags & BIT0) != 0) {
          ctx->status |= SNS_UNITEXP;
          ctx->out_cr = 1;
       }
       ctx->data_end = 1;
       ctx->cmd_done = 1;
       unit->request = 1;
    }
    if ((out_tags & BIT6) != 0) {
       unit->request = 1;
    }

    if (ctx->running) {
       add_event(unit, &poll_callback, 6000, NULL, 0);
    }
}

/* Delay after sending data */
static void
send_callback(struct _device *unit, void *arg, int iarg)
{
    struct _1052_context *ctx = (struct _1052_context *)unit->dev;
    uint16_t    out_tags;

    model1052_func(ctx, &out_tags, 0, NULL);

    if ((ctx->cmd & 1) != 0 && out_tags & BIT1) {
       log_console("1052: send ready\n");
       model1052_out(ctx, ctx->data);
       ctx->data_rdy = 1;
       unit->request = 1;
    } else {
       add_event(unit, &send_callback, 1000, NULL, 0);
    }
}


/* Decode command to device */
static void
device_cmd(struct _device *unit, uint8_t bus_out)
{
    struct _1052_context *ctx = (struct _1052_context *)unit->dev;
    uint16_t       out_tags;

    log_device("1052: %03x command %02x\n",unit->addr, bus_out);
    log_console("1052: %03x command %02x\n",unit->addr, bus_out);
    if (ctx->busy) {
         ctx->status = SNS_BSY;
         return;
    }
    ctx->cmd = bus_out & 0xff;
    ctx->data_rdy = 0;
    ctx->data_end = 0;
    ctx->cmd_done = 0;
    unit->stacked = 0;
    ctx->status = 0;
    if ((ctx->cmd & 0xf0) != 0) {
invalid:
        ctx->cmd = 0;
        ctx->cmd_done = 1;
        ctx->data_end = 1;
        ctx->busy = 0;
        ctx->status = SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK;
        ctx->sense = SENSE_CMDREJ;     /* Invalid command */
        return;
    }

    switch (ctx->cmd & 0xf) {
    case 0x0: /* Test I/O */
           break;
    case 0x1: /* Write */
    case 0x9: /* Write ACR */
           ctx->sense = 0;
           /* Start Home loop */
           model1052_func(ctx, &out_tags, BIT0|BIT7, NULL);
           ctx->data_rdy = 1;
           ctx->busy = 1;
           break;
    case 0xA: /* Read */
           ctx->sense = 0;
           /* Send proceed */
           model1052_func(ctx, &out_tags, BIT1|BIT3|BIT7, NULL);
           ctx->busy = 1;
           break;

    case 0x3: /* Nop */
           ctx->sense = 0;
           ctx->status = SNS_CHNEND|SNS_DEVEND;
           ctx->data_end = 1;
           ctx->busy = 1;
           ctx->cmd_done = 1;
           break;
    case 0x4:  /* Sense */
           ctx->data = ctx->sense;
           ctx->data_rdy = 1;
           ctx->data_end = 1;
           ctx->busy = 1;
           log_device("console Sense %02x\n", ctx->sense);
           break;
    default:
           goto invalid;
    }
}

/* Process command */
void
model1052_dev(struct _device *unit, uint16_t *tags, uint16_t bus_out, uint16_t *bus_in)
{
    struct _1052_context *ctx = (struct _1052_context *)unit->dev;
    static   uint16_t last_tags = 0;
    uint16_t       out_tags;

    if (last_tags != *tags || unit->selected) {
        print_tags("1052", ctx->state, *tags, bus_out);
        last_tags = 0; // *tags;
    }

    /* Reset device if OPER OUT is dropped */
    if ((*tags & (CHAN_OPR_OUT|CHAN_SUP_OUT)) == 0) {
        if (unit->selected) {
           *tags &= ~(CHAN_OPR_IN|CHAN_ADR_IN|CHAN_SRV_IN|CHAN_STA_IN);
        }
        log_device("1052: %03x reset\n",unit->addr);
        unit->selected = 0;
        unit->request = 0;
        ctx->state = STATE_IDLE;
        ctx->sense = 0;
        ctx->cmd = 0;
        ctx->cmd_done = 0;
        ctx->busy = 0;
        ctx->data_end = 0;
        ctx->data_rdy = 0;
        model1052_func(ctx, &out_tags, BIT7|BIT6, NULL);
        cancel_event(unit, &poll_callback);
        add_event(unit, &poll_callback, 500, NULL, 0);
        return;
    }

    switch (ctx->state) {
    /* Idle wait for CPU to talk to us */
    case STATE_IDLE:
            /* If operation out, reset device */
            if ((*tags & CHAN_OPR_OUT) == 0) {
                log_device("1052: %03x oper dropped\n",unit->addr);
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
                log_device("1052: %03x port request\n",unit->addr);
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
                     if ((bus_out & 0xff) == (unit->addr & 0xff)) {
                        *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);
                        /* Check if parity error on bus */
                        if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0) {
                            ctx->sense |= SENSE_BUSCHK;
                        }
                        /* If device in operation, respond with busy status */
                        if (ctx->busy) {
                            *bus_in = SNS_BSY | odd_parity[SNS_BSY];
                            *tags |= CHAN_STA_IN;             /* Put Busy flag on bus */
                            ctx->state = STATE_BUSY;
                            log_device("1052: %03x busy\n",unit->addr);
                            break;
                        }

                        /* Clear select in, and raise operation in */
                        *tags |= CHAN_OPR_IN;             /* Put our address on bus */
                        *bus_in = (unit->addr & 0xff) | odd_parity[unit->addr & 0xff];
                        ctx->state = STATE_INIT_SEL; /* Start initial select sequence */
                        unit->selected = 1;
                        log_device("1052: %03x selected\n",unit->addr);
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
                     log_device("1052: %03x polling\n",unit->addr);
                 }

             }
             break;

            /* Start of initial selection sequence */
    case STATE_INIT_SEL:
            *tags &= ~(CHAN_SEL_OUT);  /* Clear select in */
            *bus_in = (unit->addr & 0xff) | odd_parity[unit->addr & 0xff];
            log_device("1052: %03x address in\n",unit->addr);
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

            log_device("1052: %03x waiting command %02x\n",unit->addr, ctx->status);
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
                    if (ctx->cmd_done) {
                        ctx->status |= SNS_CHNEND|SNS_DEVEND;
                         model1052_func(ctx, &out_tags, BIT7, NULL);
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
                log_device("1052: Halt %03x device\n",unit->addr);
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

             if (ctx->attn_flg) {
                 model1052_func(ctx, &out_tags, BIT6, NULL);
                 ctx->status |= SNS_ATTN;
             }
             *bus_in = ctx->status | odd_parity[ctx->status];
             log_console("1052: %03x initial status %02x\n",unit->addr, ctx->status);
             *tags |= (CHAN_STA_IN);
             ctx->state = STATE_STATUS_ACCEPT;    /* Wait for device to accept out status */
             break;

    /* Wait for CPU to either accept or stack status */
    case STATE_STATUS_ACCEPT:
             /* CPU will respond in a couple ways. */
             *tags &= ~(CHAN_SEL_OUT);
             *bus_in = ctx->status | odd_parity[ctx->status];
             if ((*tags & CHAN_CMD_OUT) != 0) {      /* CPU does not want status, stack it */
                 log_device("1052: %03x status stacked\n",unit->addr);
                 unit->stacked = 1;
                 ctx->state = STATE_STATUS_WAIT;
                 *tags &= ~(CHAN_OPR_IN|CHAN_STA_IN);
                 break;
             }
             if ((*tags & CHAN_SRV_OUT) != 0) {   /* CPU accepted the status, continue on */
                 log_device("1052: %03x status accepted\n",unit->addr);

                ctx->status = 0;
                *tags &= ~(CHAN_STA_IN);
                /* If end of command, and status accepted, all done */
                if (ctx->cmd_done || ctx->cmd == 0) {
                    *tags &= ~(CHAN_OPR_IN);
                    unit->stacked = 0;
                    ctx->data_end = 0;
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
                     log_device("1052: %03x Halt IO\n",unit->addr);
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

             log_device("1052: %03x %02x end status %d\n",unit->addr, ctx->status, unit->request);
             ctx->state = STATE_END_ACCEPT;    /* Wait for CPU to accept out status */
             break;

     /* Wait for CPU to accept or stack status */
     case STATE_END_ACCEPT:
             *tags &= ~(CHAN_SEL_OUT);

             *bus_in = ctx->status | odd_parity[ctx->status];
             /* CPU does not want status right now. stack it */
             if ((*tags & CHAN_CMD_OUT) != 0) {
                 log_device("1052: %03x status stacked %d\n",unit->addr, unit->request);
                 unit->stacked = 1;
                 ctx->state = STATE_STATUS_WAIT;
                 *tags &= ~(CHAN_STA_IN|CHAN_OPR_IN);
                 break;
             }

             /* CPU accepted status */
             if ((*tags & CHAN_SRV_OUT) != 0) {

                 log_device("1052: %03x status accepted %d\n",unit->addr, unit->request);
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
            log_device("1052: %03x wait end b=%d cd=%d %02x %02x\n",unit->addr,
                ctx->busy, ctx->cmd_done, ctx->cmd, ctx->status);
             *tags &= ~(CHAN_SEL_OUT);
             if (ctx->cmd_done) {
                 unit->request = 0;
                 ctx->state = STATE_STATUS;
             }
             break;

      /* Handle normal operations */
      case STATE_OPR:
             log_device("1052: %03x opr %d r=%d e=%d\n",unit->addr, unit->selected,
                     ctx->data_rdy, ctx->data_end);
             unit->request = 0;
             *tags &= ~(CHAN_SEL_OUT);

             /* If address out, halt device */
             if ((*tags & CHAN_ADR_OUT) != 0) {
                 ctx->data_end = 1;
                 ctx->cmd_done = 1;
                 ctx->data_rdy = 0;
                 ctx->status |= SNS_CHNEND|SNS_DEVEND;
                 *tags &= ~(CHAN_OPR_IN);
                 unit->selected = 0;
                 ctx->state = STATE_IDLE;
                 break;
             }

             if (ctx->data_rdy) {
                 ctx->state = STATE_DATA_1;
                 break;
             }

             /* If data end, tell CPU */
             if (ctx->data_end || ctx->cmd_done) {
                 ctx->status |= SNS_CHNEND|SNS_DEVEND;
                 switch (ctx->cmd & 0xf) {
                 default:
                        break;
                 case 9: /* Write ACR */
                        model1052_func(ctx, &out_tags, BIT7|BIT5, NULL);
                        break;

                 case 1: /* Write */
                        model1052_func(ctx, &out_tags, BIT7, NULL);
                        break;

                 case 0xa: /* Read */
                        model1052_func(ctx, &out_tags, BIT1|BIT3, NULL);
                        break;
                 }
                 ctx->state = STATE_END_STATUS;
                 break;
             }

             /* If not on selector channel disconnect */
             if ((*tags & CHAN_HLD_OUT) == 0) {
                *tags &= ~(CHAN_OPR_IN);
                unit->selected = 0;
                ctx->state = STATE_IDLE;
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
                 if ((*tags & CHAN_SRV_OUT) != 0 && (ctx->cmd & 1) != 0) { /* Write command */
                      /* Device selected */
                      if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0) {
                          ctx->sense |= SENSE_BUSCHK;
                          ctx->status |= (SNS_UNITCHK);
                          ctx->data_end = 1;
                          ctx->status |= SNS_CHNEND|SNS_DEVEND;
                          ctx->busy = 0;
                          ctx->cmd_done = 1;
                      } else {
                          ctx->data = bus_out;
                          add_event(unit, &send_callback, 5000, NULL, 0);
                      }
                 }
                 if ((*tags & CHAN_CMD_OUT) != 0) {  /* CPU is done sending data */
                     ctx->data_end = 1;
                     ctx->cmd_done = 1;
                 }
                 ctx->state = STATE_OPR;   /* Go to process this data */
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
    ctx->thrd = SDL_CreateThread(model1052_thrd, "console", ctx);
    log_console("listener created\n");
    return ctx;
}

void
model1052_out(void *data, uint16_t out_char)
{
   struct _1052_context *ctx = (struct _1052_context *)data;
   char    ch = ebcdic_to_ascii[out_char & 0xff];
   log_console("send out %02x\n", ch);
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
       log_console("Recieve %02x\n", ch);
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

   if (t_request != NULL) {
       *t_request = 0;
   }
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
       if (t_request != NULL && (*tags_out & 0xef) != 0) {
           *t_request = 1;
       }
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
    if (in_char == '\f') {
       log_enable = !log_enable;
       log_console("Log %s\n", (log_enable) ? "enable" : "disable");
       return;
    }
    if (in_char == '\033') {
       ctx->attn_flg = 1;
       log_console("Cons attn\n");
    } else if (ctx->in_flg) {
       if (in_char == '\03') {
           ctx->cancel_flg = 1;
           log_console("Cons cancel\n");
       } else if (in_char == '\r') {
           log_console("Cons eob\n");
           ctx->eob_flg = 1;
           ctx->out_cr = 1;
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

    log_console("console started\n");

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
               static char *msg = "console already connected\n\r";
               send(newsock, msg, sizeof(msg), 0);
               closesocket(newsock);
           }
        }

        /* Send any data ready to send */
        if (ctx->out_flg) {
            send(ctx->cons, &ctx->out_buf, 1, 0);
            log_console("Cons send socket char(%02x)\n", ctx->out_buf);
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
    struct _1052_context *ctx;
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
    if ((ctx = (struct _1052_context *)model1052_init_ctx(port)) == NULL) {
         free(dev1052);
         return 0;
    }

    dev1052->bus_func = &model1052_dev;
    dev1052->dev = (void *)ctx;
    dev1052->draw_model = NULL;
    dev1052->create_ctrl =  NULL;
    dev1052->type_name = "1052";
    dev1052->rect[0].x = 0;
    dev1052->rect[0].y = 0;
    dev1052->rect[0].w = 0;
    dev1052->rect[0].h = 0;
    dev1052->n_units = 1;
    dev1052->addr = opt->addr;
    ctx->addr = opt->addr & 0xff;
    ctx->chan = (opt->addr >> 8) & 0xf;
    ctx->state = STATE_IDLE;

    if (opt->addr != 0)
       add_chan(dev1052, opt->addr);
    return 1;
}


