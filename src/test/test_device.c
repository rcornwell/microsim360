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
#include "event.h"
#include "device.h"
#include "test_device.h"

/*
 *  Commands.
 *
 *            01234567
 *  Write     00000001
 *  Read      00000010
 *  Read      00010010    Read by delay DE after 200 cycles.
 *  Nop       00000011
 *  One Byte  00001011    Read buffer size of bytes.
 *  One Byte  00011011    Read buffer size, then delay DE.
 *  End       00010011    Immediate channel end, device end after 100 cycles.
 *  Sense     00000100    Return one byte of sense data.
 *  Read Bk   00001100
 */

#define SENSE_CMDREJ    BIT0  /* Invalid command */
#define SENSE_INTERV    BIT1  /* Operator intervention, test empty */
#define SENSE_BUSCHK    BIT2  /* Bus parity error */
#define SENSE_EQUCHK    BIT3  /* Equipment check, not implemented */
#define SENSE_DATCHK    BIT4  /* More then 1 punch in rows 1-7 */
#define SENSE_OVRRUN    BIT5  /* Data missed */

/* Step command by one step. */
void
test_step(struct _device *unit)
{
    struct _test_context *ctx = (struct _test_context *)unit->dev;

    /* Wait until CPU handles data or done with command */
    if (ctx->data_rdy || ctx->cmd_done || ctx->cmd == 0) {
       return;
    }

    /* Wait until delay passed */
    if (ctx->delay != 0) {
        ctx->delay--;
        return;
    }

    switch (ctx->cmd & 0xf) {
    case 4:  /* Sense */
    case 0:  /* Test I/O */
           break;
    case 3: /* Control */
           /* Issue device end */
           unit->request = 1;
           if (ctx->data_end || ((ctx->cmd & 0x10) != 0 && ctx->data_cnt > ctx->max_data)) {
               ctx->cmd_done = 1;
                   ctx->busy = 0;
               ctx->status |= SNS_DEVEND;
               break;
           }
           ctx->buffer[ctx->data_cnt] = ctx->data;
           ctx->data_cnt++;
           if (ctx->data_cnt >= ctx->max_data) {
               log_trace("Test control end\n");
               if ((ctx->cmd & 0x10) == 0) {
                   ctx->cmd_done = 1;
                   ctx->busy = 0;
               ctx->status |= SNS_DEVEND;
               } else {
               ctx->data_end = 1;
               ctx->status |= SNS_CHNEND;
                   ctx->delay = 1000;
              }
           } else {
               ctx->data_rdy = 1;
               ctx->delay = 10;
           }
           break;
    case 0xb:   /* Grab a data byte */
    case 1: /* Write */
           log_trace("Test: %03x write data %02x %d %d\n",unit->addr, ctx->data,
                       ctx->data_cnt, ctx->max_data);
           unit->request = 1;
           if (ctx->data_end) {
               ctx->cmd_done = 1;
               ctx->status |= SNS_DEVEND;
               break;
           }
           ctx->buffer[ctx->data_cnt] = ctx->data;
           ctx->data_cnt++;
           if (ctx->data_cnt >= ctx->max_data) {
               log_trace("Test write end\n");
               ctx->data_end = 1;
                   ctx->busy = 0;
               ctx->cmd_done = 1;
               ctx->status |= SNS_CHNEND|SNS_DEVEND;
           } else {
               ctx->data_rdy = 1;
               ctx->delay = (ctx->burst) ? 20 : 100;
           }
           break;
    case 2: /* Read */
    case 0xc: /* Read backward. */
           unit->request = 1;
           if (ctx->data_end || ((ctx->cmd & 0x10) != 0 && ctx->data_cnt > ctx->max_data)) {
               ctx->cmd_done = 1;
                   ctx->busy = 0;
               ctx->status |= SNS_DEVEND;
               break;
           }
           ctx->data = ctx->buffer[ctx->data_cnt];
           ctx->data_cnt++;
           if (ctx->data_cnt > ctx->max_data) {
               ctx->data_end = 1;
               ctx->status |= SNS_CHNEND;
               if ((ctx->cmd & 0x10) == 0) {
                   ctx->busy = 0;
                   ctx->cmd_done = 1;
               ctx->status |= SNS_DEVEND;
               } else {
                   ctx->delay = 1000;
              }
           } else {
               ctx->data_rdy = 1;
               ctx->delay = (ctx->burst) ? 20 : 100;
           }
           log_trace("Test: %03x read data %02x %d %d\n",unit->addr, ctx->data,
                        ctx->data_cnt, ctx->max_data);
           break;
    default:
           break;
    }
}

/* Decode command to device */
void
device_cmd(struct _device *unit, uint8_t bus_out)
{
    struct _test_context *ctx = (struct _test_context *)unit->dev;

    log_device("test: %03x command %02x\n",unit->addr, bus_out);
    if (ctx->busy) {
         ctx->status = SNS_BSY;
         return;
    }
    ctx->cmd = bus_out & 0xff;
    ctx->data_rdy = 0;
    ctx->data_cnt = 0;
    ctx->data_end = 0;
    ctx->cmd_done = 0;
    unit->stacked = 0;
    ctx->status = 0;
    ctx->busy = 1;
    ctx->burst_cnt = ctx->burst_max;
    ctx->delay = 100;
    switch (ctx->cmd & 0xf) {
    case 0: /* Test I/O */
           ctx->busy = 0;
           break;
    case 0xb:  /* Grab a data byte */
           ctx->sense[0] = 0;
           ctx->data_rdy = 1;
           ctx->burst = 1;
           break;
    case 1: /* Write */
           ctx->sense[0] = 0;
           ctx->data_rdy = 1;
           if ((ctx->cmd & 0xf0) != 0) {
              goto invalid;
           }
           break;
    case 2: /* Read */
           ctx->sense[0] = 0;
           if ((ctx->cmd & 0xe0) != 0) {
              goto invalid;
           }
           break;
    case 3: /* Control */
           /* Nop or control */
           ctx->sense[0] = 0;
           /* Issue Channel end */
           if ((ctx->cmd & 0xe0) != 0) {
              goto invalid;
           }
           if (ctx->cmd == 0x13) {
               ctx->delay = 200;
               ctx->disconnect = 1;
           } else {
               ctx->cmd_done = 1;
           ctx->status |= SNS_DEVEND;
               ctx->busy = 0;
           }
           ctx->data_end = 1;
           ctx->status |= SNS_CHNEND;
           break;
    case 0xc: /* Read backward */
           ctx->sense[0] = 0;
           break;
    case 4:  /* Sense */
           ctx->data = ctx->sense[0];
           ctx->sense_cnt = 1;
           ctx->data_rdy = 1;
           ctx->delay = 0;
           log_device("test: %03x Sense %02x\n",unit->addr, ctx->sense[0]);
           break;
    default:

invalid:
           ctx->cmd = 0;
           ctx->cmd_done = 1;
           ctx->data_end = 1;
           ctx->busy = 0;
           ctx->status = SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK;
           ctx->sense[0] = SENSE_CMDREJ;     /* Invalid command */
           break;
    }
}

/* Process command */
void
test_dev(struct _device *unit, uint16_t *tags, uint16_t bus_out, uint16_t *bus_in)
{
    struct _test_context *ctx = (struct _test_context *)unit->dev;
    static   uint16_t last_tags = 0;

    if (last_tags != *tags || unit->selected) {
        print_tags("Test", ctx->state, *tags, bus_out);
        last_tags = 0; // *tags;
    }

    /* Reset device if OPER OUT is dropped */
    if ((*tags & (CHAN_OPR_OUT|CHAN_SUP_OUT)) == 0) {
        if (unit->selected) {
           *tags &= ~(CHAN_OPR_IN|CHAN_ADR_IN|CHAN_SRV_IN|CHAN_STA_IN);
        }
        log_device("test: %03x reset\n",unit->addr);
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
            ctx->disconnect = 0;
            log_device("test: %03x idle r=%d s=%d e=%d p=%d b=%d cd=%d dly=%d %02x %02x\n",unit->addr,
                unit->request, unit->stacked, ctx->data_end, ctx->data_end_post,
                ctx->busy, ctx->cmd_done, ctx->delay, ctx->cmd, ctx->status);
            /* If operation out, reset device */
            if ((*tags & CHAN_OPR_OUT) == 0) {
                log_device("test: %03x oper dropped\n",unit->addr);
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
                log_device("test: %03x port request\n",unit->addr);
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
                            ctx->sense[0] |= SENSE_BUSCHK;
                        }
                        /* If device in operation, respond with busy status */
                        if (ctx->busy/* && ctx->data_end == 0*/) {
                            *bus_in = SNS_BSY | odd_parity[SNS_BSY];
                            *tags |= CHAN_STA_IN;             /* Put Busy flag on bus */
                            ctx->state = STATE_BUSY;
                            log_device("test: %03x busy\n",unit->addr);
                            break;
                        }

                        /* Clear select in, and raise operation in */
                        *tags |= CHAN_OPR_IN;             /* Put our address on bus */
                        *bus_in = (unit->addr & 0xff) | odd_parity[unit->addr & 0xff];
                        ctx->state = STATE_INIT_SEL; /* Start initial select sequence */
                        unit->selected = 1;
                        log_device("test: %03x selected\n",unit->addr);
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
                     log_device("test: %03x polling\n",unit->addr);
                 }

             }
             break;

            /* Start of initial selection sequence */
    case STATE_INIT_SEL:
            *tags &= ~(CHAN_SEL_OUT);  /* Clear select in */
            *bus_in = (unit->addr & 0xff) | odd_parity[unit->addr & 0xff];
            log_device("test: %03x address in\n",unit->addr);
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

            log_device("test: %03x waiting command %02x\n",unit->addr, ctx->status);
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
                log_device("Halt: %03x device\n",unit->addr);
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

             /* If SMS flag is set, return SMS status */
             if ((ctx->status & SNS_DEVEND) != 0 && ctx->sms) {
                     ctx->status |= SNS_SMS;
                     ctx->sms = 0;
             }

             *bus_in = ctx->status | odd_parity[ctx->status];
             log_device("test: %03x initial status %02x\n",unit->addr, ctx->status);
             *tags |= (CHAN_STA_IN);
             ctx->state = STATE_STATUS_ACCEPT;    /* Wait for device to accept out status */
             break;

    /* Wait for CPU to either accept or stack status */
    case STATE_STATUS_ACCEPT:
             /* CPU will respond in a couple ways. */
             *tags &= ~(CHAN_SEL_OUT);
             *bus_in = ctx->status | odd_parity[ctx->status];
             if ((*tags & CHAN_CMD_OUT) != 0) {      /* CPU does not want status, stack it */
                 log_device("test: %03x status stacked\n",unit->addr);
                 unit->stacked = 1;
                 ctx->state = STATE_STATUS_WAIT;
                 *tags &= ~(CHAN_OPR_IN|CHAN_STA_IN);
                 break;
             }
             if ((*tags & CHAN_SRV_OUT) != 0) {   /* CPU accepted the status, continue on */
                 log_device("test: %03x status accepted\n",unit->addr);
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
                    break;
                }

                if (ctx->data_end) {
                    if ((*tags & CHAN_HLD_OUT) == 0) {
                        *tags &= ~(CHAN_OPR_IN);
                    }
                    ctx->state = STATE_STATUS_WAIT;
                    break;
                }
                if (ctx->burst == 0 && ctx->data_rdy == 0) {
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
                     log_device("test: %03x Halt IO\n",unit->addr);
                     if (ctx->data_end == 0) {
                         ctx->data_rdy = 0;
                         ctx->data_end = 1;
                         ctx->status |= SNS_CHNEND;
                         if ((ctx->cmd & 0x10) != 0) {
                            ctx->delay = 1000;
                         } else {
                            ctx->status |= SNS_DEVEND;
                            ctx->cmd_done = 1;
                         }
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

             /* If SMS flag is set, return SMS status */
             if ((ctx->status & SNS_DEVEND) != 0 && ctx->sms) {
                     ctx->status |= SNS_SMS;
                     ctx->sms = 0;
             }
             *bus_in = ctx->status | odd_parity[ctx->status];
             *tags |= (CHAN_STA_IN);

             log_device("test: %03x %02x end status %d\n",unit->addr, ctx->status, unit->request);
             ctx->state = STATE_END_ACCEPT;    /* Wait for CPU to accept out status */
             break;

     /* Wait for CPU to accept or stack status */
     case STATE_END_ACCEPT:
             *tags &= ~(CHAN_SEL_OUT);

             *bus_in = ctx->status | odd_parity[ctx->status];
             /* CPU does not want status right now. stack it */
             if ((*tags & CHAN_CMD_OUT) != 0) {
                 log_device("test: %03x status stacked %d\n",unit->addr, unit->request);
                 unit->stacked = 1;
                 ctx->state = STATE_STATUS_WAIT;
                 *tags &= ~(CHAN_STA_IN|CHAN_OPR_IN);
                 break;
             }

             /* CPU accepted status */
             if ((*tags & CHAN_SRV_OUT) != 0) {

                 log_device("test: %03x status accepted %d\n",unit->addr, unit->request);
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
                     if (ctx->burst) {
                         *tags &= ~(CHAN_STA_IN|CHAN_OPR_IN);
                     } else {
                         *tags &= ~(CHAN_STA_IN|CHAN_OPR_IN);
                     }
                     ctx->state = STATE_STATUS_WAIT;
                 }
             }
             break;

      /* Wait on device to finish, before posting status */
      case STATE_WAIT_DEVEND:
            log_device("test: %03x wait end b=%d cd=%d dly=%d %02x %02x\n",unit->addr,
                ctx->busy, ctx->cmd_done, ctx->delay, ctx->cmd, ctx->status);
             *tags &= ~(CHAN_SEL_OUT);
             if (ctx->cmd_done) {
                 unit->request = 0;
                 ctx->state = STATE_STATUS;
             }
             break;

      /* Handle normal operations */
      case STATE_OPR:
             log_device("test: %03x opr %d r=%d e=%d d=%d\n",unit->addr, unit->selected,
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
             if (ctx->cmd == 0x04) {
                 ctx->disconnect = 0;
                 if (ctx->sense_cnt < SENSE_MAX) {
                     ctx->data = ctx->sense[++ctx->sense_cnt];
                     ctx->data_rdy = 1;
                 } else {
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
                     ctx->status |= SNS_CHNEND;
                     if ((ctx->cmd & 0x10) == 0) {
                         ctx->cmd_done = 1;
                         ctx->status |= SNS_DEVEND;
                     }
                 } else {
                     if (ctx->burst) {
                         if (ctx->burst_cnt == 0) {
                             ctx->burst_cnt = ctx->burst_max;
                             ctx->disconnect = 1;
                         } else {
                             ctx->burst_cnt--;
                         }
                     } else {
                         ctx->disconnect = 1;
                     }
                 }
             }
             break;
    }
}

