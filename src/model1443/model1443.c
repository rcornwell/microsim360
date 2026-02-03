/*
 * microsim360 - Model 1443 printer
 *
 * Copyright 2026, Richard Cornwell
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
#include <ctype.h>
#include "logger.h"
#include "device.h"
#include "config.h"
#include "event.h"
#include "xlat.h"
#include "model1443.h"

/*
 *  Commands.
 *
 *                 01234567
 *  Write & Space  0LLLL001       L = 0000 to 0011
 *  Space Immedate 0LLLL011       L = 0000 to 0011
 *  Write & Skip   1CCCC001       C = 0001 to 1100
 *  Skip Immediate 1CCCC011       C = 0001 to 1100
 *  Sense          00000100
 */

DEV_LIST_STRUCT(1443, DEV_TYPE, 0);

#define SENSE_CMDREJ    BIT0  /* Invalid command */
#define SENSE_INTERV    BIT1  /* Operator intervention, stop key pressed */
                              /* No paper */
#define SENSE_BUSCHK    BIT2  /* Bus parity error */
#define SENSE_EQUCHK    BIT3  /* Equipment check, not implemented */
#define SENSE_DATCHK    BIT5  /* Character send not on print train */
#define SENSE_CHAN9     BIT7  /* Channel 9 skipped over */



static const uint16_t cctape_legacy[] = {
/* 1      2      3      4      5      6      7      8      9     10       lines  */
0x800, 0x000, 0x000, 0x000, 0x000, 0x000, 0x400, 0x000, 0x000, 0x000, /*  1 - 10 */
0x000, 0x000, 0x200, 0x000, 0x000, 0x000, 0x000, 0x000, 0x100, 0x000, /* 11 - 20 */
0x000, 0x000, 0x000, 0x000, 0x080, 0x000, 0x000, 0x000, 0x000, 0x000, /* 21 - 30 */
0x040, 0x000, 0x000, 0x000, 0x000, 0x000, 0x020, 0x000, 0x000, 0x000, /* 31 - 40 */
0x000, 0x000, 0x010, 0x000, 0x000, 0x000, 0x000, 0x000, 0x004, 0x000, /* 41 - 50 */
0x000, 0x000, 0x000, 0x000, 0x002, 0x000, 0x000, 0x000, 0x000, 0x000, /* 51 - 60 */
0x001, 0x000, 0x008, 0x000, 0x000, 0x000, };                          /* 61 - 66 */
/*
    PROGRAMMMING NOTE:  the below cctape value SHOULD match
                        the same corresponding fcb value!
*/
static const uint16_t cctape_std1[] = {
/* 1      2      3      4      5      6      7      8      9     10       lines  */
0x800, 0x000, 0x000, 0x000, 0x000, 0x000, 0x400, 0x000, 0x000, 0x000, /*  1 - 10 */
0x000, 0x000, 0x200, 0x000, 0x000, 0x000, 0x000, 0x000, 0x100, 0x000, /* 11 - 20 */
0x000, 0x000, 0x000, 0x000, 0x080, 0x000, 0x000, 0x000, 0x000, 0x000, /* 21 - 30 */
0x040, 0x000, 0x000, 0x000, 0x000, 0x000, 0x020, 0x000, 0x000, 0x000, /* 31 - 40 */
0x000, 0x000, 0x010, 0x000, 0x000, 0x000, 0x000, 0x000, 0x008, 0x000, /* 41 - 50 */
0x000, 0x000, 0x000, 0x000, 0x004, 0x000, 0x000, 0x000, 0x000, 0x000, /* 51 - 60 */
0x002, 0x000, 0x001, 0x000, 0x000, 0x000, };                          /* 61 - 66 */

const static uint8_t ebcdic_to_out[256] = {
/*      0     1     2     3    4     5      6   7 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,      /* 0x */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00,      /* 1x */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,      /* 2x */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,      /* 3x */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,      /* 4x */
    /*           [     .     <     (     +     |   */
    0x00, 0x00, 0x40, 0x30, 0x40, 0x40, 0x2b, 0x40,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,      /* 5x */
    /*           !     $     *     )     ;     ^   */
    0x00, 0x00, 0x40, 0x2f, 0x33, 0x40, 0x40, 0x40,
    /* -  / */
    0x2c, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,      /* 6x */
    /*                 ,     %     _     >     ?   */
    0x00, 0x00, 0x00, 0x2e, 0x32, 0x40, 0x40, 0x40,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,      /* 7x */
    /*     `     :     #     @     \     =     "   */
    0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
    /*     a     b     c     d     e     f     g   */
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,      /* 8x */
    /* h  i */
    0x08, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /*     j     k     l     m     n     o     p    */
    0x00, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,      /* 9x */
    /* q  r */
    0x11, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /*     ~     s     t     u     v     w     x   */
    0x00, 0x2c, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,      /* Ax */
    /* y  z */
    0x19, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,      /* Bx */
    /*{    A     B     C     D     E     F     G   */
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,      /* 8x */
    /* H  I */
    0x08, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /*}    J     K     L     M     N     O     P    */
    0x00, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,      /* 9x */
    /* Q   R */
    0x11, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* \         S     T     U     V     W     X   */
    0x40, 0x00, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,      /* Ax */
    /* Y   Z */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x19, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /*  0     1     2     3     4     5     6     7    */
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,      /* Fx */
    /* 8  9 */
    0x29, 0x2a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


int print_line(struct _1443_context *ctx);

static void
done_callback(struct _device *unit, void *arg, int iarg)
{
    struct _1443_context *ctx = (struct _1443_context *)unit->dev;
    log_device("1443: %03x finish\n",unit->addr);
    unit->request = 1;
    ctx->busy = 0;
    ctx->cmd_done = 1;
    ctx->status |= SNS_DEVEND;
    if (ctx->ch9) {
        ctx->sense |= SENSE_CHAN9;
        ctx->status |= SNS_UNITCHK;
    }
    if (ctx->ch12) {
        ctx->status |= SNS_UNITEXP;
    }
}

/* Stop device when channel has no more data */
static void
device_stop(struct _device *unit)
{
    struct _1443_context *ctx = (struct _1443_context *)unit->dev;

    log_device("1443: stop %02x\n", ctx->cmd);
    ctx->status |= SNS_CHNEND;
    if ((ctx->cmd & 0x03) == 0x01) {
        ctx->status |= SNS_DEVEND;
        if (ctx->sense != 0) {
            ctx->status |= SNS_UNITCHK;
        }
        ctx->cmd_done = 1;
        ctx->busy = 0;
        ctx->cmd = 0;
    }
}

/* Decode command to device */
static void
device_cmd(struct _device *unit, uint8_t bus_out)
{
    struct _1443_context *ctx = (struct _1443_context *)unit->dev;
    uint8_t cmd = bus_out & 0xff;

    log_device("printer command %02x\n", bus_out);
    ctx->status = 0;
    /* Check if device not ready */
    if (ctx->ready == 0 || ctx->file == 0) {
        ctx->sense |= SENSE_INTERV;
    } else {
        ctx->sense &= ~SENSE_INTERV;
    }

    switch (cmd & 07) {
    case 0: /* Test I/O */
           if (ctx->sense != 0) {
               ctx->status |= SNS_UNITCHK;
           }
           return;

    case 1: /* Write */
           ctx->cmd = cmd;
           ctx->cmd_done = 0;

           ctx->sense &= SENSE_INTERV;
           /* Check if skip invalid */
           if ((ctx->cmd & 0x80) != 0 && ((ctx->cmd & 0x78) == 0 || (ctx->cmd & 0x78) > 0x60)) {
               ctx->sense |= SENSE_CMDREJ;
               ctx->status = SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK;
               ctx->cmd_done = 1;
               break;
           }
           /* Check if space invalid */
           if ((ctx->cmd & 0x80) == 0 && (ctx->cmd & 0x78) > 0x18) {
               ctx->sense |= SENSE_CMDREJ;
               ctx->status = SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK;
               ctx->cmd_done = 1;
               break;
           }

           /* Check if device not ready */
           if ((ctx->sense & SENSE_INTERV) != 0) {
               ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
               ctx->cmd_done = 1;
               break;
           }
           ctx->ch9 = 0;
           ctx->ch12 = 0;
           ctx->col = 0;
           ctx->busy = 1;
           break;

    case 3: /* Feed */
           ctx->cmd = cmd;
           ctx->cmd_done = 1;
           /* Check for NOP */
           ctx->sense &= SENSE_INTERV;
           if (cmd == 0x03) {
               ctx->cmd = 0;
               ctx->status = SNS_CHNEND|SNS_DEVEND;
               break;
           }

           /* Check if skip invalid */
           if ((ctx->cmd & 0x80) != 0 && ((ctx->cmd & 0x78) == 0 || (ctx->cmd & 0x78) > 0x60)) {
               ctx->sense = SENSE_CMDREJ;
               ctx->status = SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK;
               break;
           }
           /* Check if space invalid */
           if ((ctx->cmd & 0x80) == 0 && (ctx->cmd & 0x78) > 0x18) {
               ctx->sense = SENSE_CMDREJ;
               ctx->status = SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK;
               break;
           }

           /* Check if device not ready */
           if ((ctx->sense & SENSE_INTERV) != 0) {
               ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
               ctx->cmd_done = 1;
               break;
           }

           ctx->ch9 = 0;
           ctx->ch12 = 0;
           ctx->status = SNS_CHNEND;
           ctx->col = 0;
           ctx->cmd_done = 0;
           ctx->busy = 1;
           add_event(unit, done_callback, 2000 * print_line(ctx), NULL, 0);
           break;

    case 4:  /* Sense */
           if (cmd != 0x4) {
               ctx->sense = SENSE_CMDREJ;
               ctx->status = SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK;
               ctx->cmd_done = 1;
               break;
           }
           ctx->cmd = cmd;
           ctx->cmd_done = 0;
           ctx->busy = 1;
           log_device("Printer sense %02x\n", ctx->sense);
           break;

    default:
           ctx->sense = SENSE_CMDREJ;     /* Invalid command */
           ctx->status = SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK;
           break;
    }
}

/* Process channel */
void
model1443_dev(struct _device *unit, uint16_t *tags, uint16_t bus_out, uint16_t *bus_in)
{
    struct _1443_context *ctx = (struct _1443_context *)unit->dev;
    static   uint16_t last_tags = 0;

    if (last_tags != *tags) {
        print_tags("Printer", ctx->state, *tags, bus_out);
        last_tags = *tags;
    }

    /* Reset device if OPER OUT is dropped */
    if ((*tags & (CHAN_OPR_OUT|CHAN_SUP_OUT)) == 0) {
        if (unit->selected) {
           *tags &= ~(CHAN_OPR_IN|CHAN_ADR_IN|CHAN_SRV_IN|CHAN_STA_IN);
        }
        log_device("1443: %03x reset\n",unit->addr);
        unit->selected = 0;
        unit->request = 0;
        ctx->state = STATE_IDLE;
        ctx->status = 0;
        ctx->sense = 0;
        ctx->cmd = 0;
        ctx->cmd_done = 0;
        ctx->busy = 0;
        return;
    }

    switch (ctx->state) {
    /* Idle wait for CPU to talk to us */
    case STATE_IDLE:
            /* If operation out, reset device */
            if ((*tags & CHAN_OPR_OUT) == 0) {
                log_device("1443: %03x oper dropped\n",unit->addr);
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
                log_device("1443: %03x port request\n",unit->addr);
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
                        if (ctx->busy/* && ctx->data_end == 0*/) {
                            *bus_in = SNS_BSY | odd_parity[SNS_BSY];
                            *tags |= CHAN_STA_IN;             /* Put Busy flag on bus */
                            ctx->state = STATE_BUSY;
                            log_device("1443: %03x busy\n",unit->addr);
                            break;
                        }

                        /* Clear select in, and raise operation in */
                        *tags |= CHAN_OPR_IN;             /* Put our address on bus */
                        *bus_in = (unit->addr & 0xff) | odd_parity[unit->addr & 0xff];
                        ctx->state = STATE_INIT_SEL; /* Start initial select sequence */
                        unit->selected = 1;
                        log_device("1443: %03x selected\n",unit->addr);
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
                     log_device("1443: %03x polling\n",unit->addr);
                 }

             }
             break;

            /* Start of initial selection sequence */
    case STATE_INIT_SEL:
            *tags &= ~(CHAN_SEL_OUT);  /* Clear select in */
            *bus_in = (unit->addr & 0xff) | odd_parity[unit->addr & 0xff];
            log_device("1443: %03x address in\n",unit->addr);
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

            log_device("1443: %03x waiting command %02x\n",unit->addr, ctx->status);
            *bus_in = (unit->addr & 0xff) | odd_parity[unit->addr & 0xff];
            /* we get command out, process command */
            if ((*tags & (CHAN_CMD_OUT)) != 0) {
                *tags &= ~(CHAN_ADR_IN);        /* Command out, drop addressin */
                ctx->state = STATE_STATUS;
                /* Check if parity error on bus */
                if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0) {
                    ctx->cmd = 0;
                    ctx->cmd_done = 0;
                    ctx->busy = 0;
                    ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                    ctx->sense |= SENSE_BUSCHK;
                    break;
                }
                if (!unit->stacked && ctx->cmd == 0) {       /* If no stacked status, process command */
                    device_cmd(unit, bus_out & 0xff);
                }
                break;
            }

            /* If we get Address out again, we need to halt */
            if ((*tags & (CHAN_ADR_OUT)) != 0 && (*tags & CHAN_HLD_OUT) == 0) {
                *tags &= ~(CHAN_ADR_IN|CHAN_OPR_IN);  /* Clear select in */
                log_device("1443: Halt %03x device\n",unit->addr);
                device_stop(unit);
                ctx->status |= SNS_CHNEND;
                ctx->state = STATE_STATUS_WAIT;
                break;
            }

            break;

    /* Present initial status */
    case STATE_STATUS:
             /* Wait for Command out to drop */
             *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN);      /* Drop address in */

             *bus_in = ctx->status | odd_parity[ctx->status];
             log_device("1443: %03x initial status %02x\n",unit->addr, ctx->status);
             *tags |= (CHAN_STA_IN);
             ctx->state = STATE_STATUS_ACCEPT;    /* Wait for device to accept out status */
             break;

    /* Wait for CPU to either accept or stack status */
    case STATE_STATUS_ACCEPT:
             /* CPU will respond in a couple ways. */
             *tags &= ~(CHAN_SEL_OUT);
             *bus_in = ctx->status | odd_parity[ctx->status];
             if ((*tags & CHAN_CMD_OUT) != 0) {      /* CPU does not want status, stack it */
                 log_device("1443: %03x status stacked\n",unit->addr);
                 unit->stacked = 1;
                 ctx->state = STATE_STATUS_WAIT;
                 *tags &= ~(CHAN_OPR_IN|CHAN_STA_IN);
                 break;
             }
             if ((*tags & CHAN_SRV_OUT) != 0) {   /* CPU accepted the status, continue on */
                log_device("1443: %03x status accepted\n",unit->addr);

                *tags &= ~(CHAN_STA_IN);
                if ((ctx->status & SNS_CHNEND) != 0) {
                    ctx->status = 0;
                    ctx->state = STATE_STATUS_WAIT;
                    break;
                }
                ctx->status = 0;
                /* If end of command, and status accepted, all done */
                if (ctx->cmd_done || ctx->cmd == 0) {
                    *tags &= ~(CHAN_OPR_IN);
                    unit->stacked = 0;
                    ctx->cmd = 0;
                    ctx->cmd_done = 0;
                    ctx->busy = 0;
                    ctx->state = STATE_IDLE;
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
                 if ((*tags & CHAN_HLD_OUT) == 0) {
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
                     log_device("1443: %03x Halt IO\n",unit->addr);
                     device_stop(unit);
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

             log_device("1443: %03x %02x end status %d\n",unit->addr, ctx->status, unit->request);
             ctx->state = STATE_END_ACCEPT;    /* Wait for CPU to accept out status */
             break;

     /* Wait for CPU to accept or stack status */
     case STATE_END_ACCEPT:
             *tags &= ~(CHAN_SEL_OUT);

             *bus_in = ctx->status | odd_parity[ctx->status];
             /* CPU does not want status right now. stack it */
             if ((*tags & CHAN_CMD_OUT) != 0) {
                 log_device("1443: %03x status stacked %d\n",unit->addr, unit->request);
                 unit->stacked = 1;
                 ctx->state = STATE_STATUS_WAIT;
                 *tags &= ~(CHAN_STA_IN|CHAN_OPR_IN);
                 break;
             }

             /* CPU accepted status */
             if ((*tags & CHAN_SRV_OUT) != 0) {

                 log_device("1443: %03x status accepted %d\n",unit->addr, unit->request);
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

                 *tags &= ~(CHAN_STA_IN);
                 ctx->state = STATE_STATUS_WAIT;
             }
             break;

      /* Wait on device to finish, before posting status */
      case STATE_WAIT_DEVEND:
             log_device("1443: %03x wait end b=%d cd=%d %02x %02x\n",unit->addr,
                ctx->busy, ctx->cmd_done, ctx->cmd, ctx->status);
             *tags &= ~(CHAN_SEL_OUT);
             if (ctx->cmd_done) {
                 unit->request = 0;
                 ctx->state = STATE_STATUS;
             }
             break;

      /* Handle normal operations */
      case STATE_OPR:
             log_device("1443: %03x opr %d\n",unit->addr, unit->selected);
             unit->request = 0;
             *tags &= ~(CHAN_SEL_OUT);

             /* If address out, halt device */
             if ((*tags & CHAN_ADR_OUT) != 0) {
                 device_stop(unit);
                 break;
             }

             /* If sense command or write command, transfer  byte */
             if (ctx->cmd == 0x04 || ctx->col < 132) {
                 ctx->state = STATE_DATA_1;
                 break;
             }

             /* If at end of data or command, present status */
             add_event(unit, done_callback, 20 * print_line(ctx), NULL, 0);
             ctx->status |= SNS_CHNEND;
             ctx->state = STATE_END_STATUS;
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
             if (ctx->cmd == 0x04) {
                 *bus_in = ctx->sense | odd_parity[ctx->sense];
             } else {
                 *bus_in = 0x100;
             }
             ctx->state = STATE_DATA_2;
             break;

      case STATE_DATA_2:      /* Complete transfer */
             *tags &= ~CHAN_SEL_OUT;
             if (ctx->cmd == 0x04) {
                 *bus_in = ctx->sense | odd_parity[ctx->sense];
             } else {
                 *bus_in = 0x100;
             }
             if ((*tags & (CHAN_SRV_OUT|CHAN_CMD_OUT)) != 0) {
                 /* Wait for service out or command out */
                 *tags &= ~(CHAN_SRV_IN);   /* Clear service in request */
                 if ((*tags & CHAN_SRV_OUT) != 0) {  /* CPU is done sending data */
                     if ((ctx->cmd & 1) != 0) { /* Write command */
                          /* Device selected */
                          if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0) {
                              ctx->sense |= SENSE_BUSCHK;
                              ctx->status |= SNS_UNITCHK|SNS_CHNEND|SNS_DEVEND;
                              ctx->busy = 0;
                              ctx->cmd_done = 1;
                              ctx->state = STATE_END_STATUS;
                          } else {
                              ctx->buffer[ctx->col++] = (uint8_t)(bus_out & 0xff); /* Grab data */
                              ctx->state = STATE_OPR;   /* Go to process this data */
                          }
                     }
                     if (ctx->cmd == 0x4) {   /* Sense command */
                         ctx->status |= SNS_CHNEND|SNS_DEVEND;
                         ctx->cmd_done = 1;
                         ctx->state = STATE_END_STATUS;
                     }
                 }
                 if ((*tags & CHAN_CMD_OUT) != 0) {  /* CPU is done sending data */
                     /* If at end of data or command, present status */
                     add_event(unit, done_callback, 20 * print_line(ctx), NULL, 0);
                     ctx->status |= SNS_CHNEND;
                     ctx->state = STATE_END_STATUS;
                 }
             }
             break;
    }
}


/*
 * Print out a line and return number of lines being skipped
 */
int
print_line(struct _1443_context *ctx)
{

    char                out[150];       /* Temp conversion buffer */
    int                 i;
    int                 l = (ctx->cmd >> 3) & 0x1f;
    int                 f = 1;
    int                 r = 0;          /* Rows skipped */
    int                 time = 0;       /* Amount of time it took to print this line */
    uint16_t            mask;
    int                 ch9, ch12;

    ch9 = ch12 = 0;
    /* Dump buffer if full */
    if ((ctx->cmd & 0x7) == 01) {

        /* Try to convert to text */
        memset(out, ' ', sizeof(out));

        /* Scan each column */
        for (i = 0; i < ctx->col; i++) {
           int         ch = ctx->buffer[i];

           if (i < 120)
               ctx->output[14][i] = ebcdic_to_out[ch];
           ch = ebcdic_to_ascii[ch];
           if (!isprint(ch))
              ch = '.';
           out[i] = ch;
        }

        /* Trim trailing spaces */
        for (--i; i > 0 && out[i] == ' '; i--) ;
        out[++i] = '\0';

        /* Print out buffer */
        fwrite(&out, 1, i, ctx->file);
        log_device( " Printer: %s\n", out);
        memset(ctx->buffer, 0x40, sizeof(ctx->buffer));
        time = 11;
    }
    fflush(ctx->file);

    f = 1;  /* Indicate we need to output a new line */
    if (l < 4) {
        while(l != 0) {
            fwrite("\r\n", 1, 2, ctx->file);
            memcpy(&ctx->output[0][0], &ctx->output[1][0], 14 * 120);
            memset(&ctx->output[14][0], 0, 120);
            r++;
            f = 0;
            log_device( " Printer fcb: %04x\n", ctx->fcb[ctx->row]);
            if ((ctx->fcb[ctx->row] & (0x1000 >> 9)) != 0)
                ch9 = 1;
            if ((ctx->fcb[ctx->row] & (0x1000 >> 12)) != 0) {
        log_device( " Printer chan 12\n");
                ch12 = 1;
            }
            ctx->row++;
            if (ctx->row > ctx->lpp)
                break;
            l--;
        }
        if (ctx->row > ctx->lpp) {
           memcpy(&ctx->output[0][0], &ctx->output[1][0], 14 * 120);
           memset(&ctx->output[14][0], 0, 120);
           if (f)
               fwrite("\r\n", 1, 2, ctx->file);
           fwrite("\f", 1, 1, ctx->file);
           ctx->row = 0;
        }
        if (ch9 && (ctx->cmd & 0x3) == 0x1) {
           ctx->ch9 = 1;
           ctx->sense |= SENSE_CHAN9;
        }
        if (ch12 && (ctx->cmd & 0x3) == 0x1) {
        log_device( " Printer chan 12\n");
           ctx->ch12 = 1;
        }
        time += 20 + (5 * r);
        return time;
    }

    mask = 0x1000 >> (l & 0xf);  /* Mask which channel to stop at */
    f = 0;     /* Indicate if we started new form */
    l = 0;     /* Total lines skipped */
    r = 0;     /* What row we should be on */
    for (i = ctx->row + 1; (ctx->fcb[i] & mask) == 0 && ctx->row != i; i++) {
         l++;
         r++;
         if (i > ctx->lpp) {
             log_device("printer skip2 %d > %d\n", i, ctx->lpp);
             fwrite("\r\n\f", 1, 3, ctx->file);
             memcpy(&ctx->output[0][0], &ctx->output[1][0], 14 * 120);
             memset(&ctx->output[14][0], 0, 120);
             f = 1;
             r = 0;
         }
    }

    /* If we passed over form, clear row */
    if (f) {
       ctx->row = 0;
    }

    if (ctx->fcb[i] & mask) {
        while (r-- > 0) {
           fwrite("\r\n", 1, 2, ctx->file);
           memcpy(&ctx->output[0][0], &ctx->output[1][0], 14 * 120);
           memset(&ctx->output[14][0], 0, 120);
           ctx->row++;
           if (ctx->row > ctx->lpp) {
               log_device("printer skip %d > %d\n", ctx->row, ctx->lpp);
               fwrite("\f", 1, 1, ctx->file);
               ctx->row = 0;
           }
        }
    }
    time += 20 + (5 * l);
    return l;
}

int
model1443_create(struct _option *opt)
{
     struct _device       *dev1443;
     struct _1443_context *lpr;
     struct _option        opts;

     if ((dev1443 = calloc(1, sizeof(struct _device))) == NULL)
         return 0;
     if ((lpr = calloc(1, sizeof(struct _1443_context))) == NULL) {
         free(dev1443);
         return 0;
     }

     dev1443->bus_func = &model1443_dev;
     dev1443->dev = (void *)lpr;
     dev1443->draw_model = (void *)&model1443_draw;
     dev1443->create_ctrl = (void *)&model1443_control;
     dev1443->init_device = (void *)&model1443_init;
     dev1443->type_name = "1443";
     dev1443->rect[0].x = 305;
     dev1443->rect[0].y = 0;
     dev1443->rect[0].w = 280;
     dev1443->rect[0].h = 200;
     dev1443->n_units = 1;
     dev1443->addr = opt->addr;
     lpr->addr = (opt->addr & 0xff);
     lpr->chan = (opt->addr >> 8) & 0xf;
     lpr->state = STATE_IDLE;
     lpr->selected = 0;
     lpr->sense = 0;
     lpr->file_name = NULL;
     lpr->form = 1;
     lpr->fcb = &cctape_legacy[0];
     lpr->lpp = 66;
     add_chan(dev1443, opt->addr);

     /* Parse options given on definition */
     while (get_option(&opts)) {
           if (strcmp(opts.opt, "START") == 0 && lpr->file != NULL) {
               lpr->ready = 1;
           } else if (strcmp(opts.opt, "FILE") == 0 && opts.flags == 1) {
               if (lpr->file != NULL) {
                   fclose(lpr->file);
                   lpr->form = 1;
               }
               free(lpr->file_name);
               lpr->file_name = NULL;
               lpr->file = fopen(opts.string, "a");
               if (lpr->file != NULL) {
                  lpr->row = 0;
                  if ((lpr->file_name = (char *)malloc(strlen(opts.string)+1)) == NULL) {
                      fclose(lpr->file);
                      free(lpr);
                      free(dev1443);
                      return 0;
                  }
                  strcpy(lpr->file_name, opts.string);
                  lpr->form = 0;
               }
           } else {
               fprintf(stderr, "Invalid option %s to 1443\n", opts.opt);
               free(lpr);
               free(dev1443);
               return 0;
           }
     }

     return 1;
}

