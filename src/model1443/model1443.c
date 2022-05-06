/*
 * microsim360 - Model 1443 printer
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
#include <stdio.h>
#include <string.h>
#include "logger.h"
#include "xlat.h"
#include "model1443.h"
#include "model1443.xpm"

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


#define SENSE_CMDREJ    BIT0  /* Invalid command */
#define SENSE_INTERV    BIT1  /* Operator intervention, stop key pressed */
                              /* No paper */
#define SENSE_BUSCHK    BIT2  /* Bus parity error */
#define SENSE_EQUCHK    BIT3  /* Equipment check, not implemented */
#define SENSE_DATCHK    BIT5  /* Character send not on print train */
#define SENSE_CHAN9     BIT7  /* Channel 9 skipped over */


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
#define STATE_STACK_CMD 13    /* Stack status select */
#define STATE_STACK_HLD 14    /* Stack status select */
#define STATE_WAIT      15    /* After data transfer wait motion */

struct _1443_context {
    int                    addr;         /* Device address */
    int                    chan;         /* Channel address */
    int                    state;        /* Current channel state */
    int                    selected;     /* Device currently selected */
    int                    sense;        /* Current sense value */
    int                    cmd;          /* Current command */
    int                    status;       /* Current bus status */
    int                    data;         /* Current byte to send/recieve */
    int                    data_rdy;     /* Data is valid */
    int                    data_end;     /* End of data transfer */
    int                    delay;        /* Delay time till operation done */
    FILE                  *file;         /* Output file. */
    char                  *file_name;    /* Attached file name */
    int                    buf[144];     /* Line buffer */
    int                    col;          /* Current transfer column */
    int                    row;          /* Current print row */
    int                    lpp;          /* Number of lines per page */
    uint16_t               ready;        /* Printer in ready status */
    uint16_t               start;        /* Start printer */
    uint16_t               stop;         /* Stop printer after next cycle */
    uint16_t               single;       /* Single cycle printer */
    uint16_t               form;         /* Forms ready */
    int                    cnt;          /* Transfer count */
    int                    fcb_num;      /* Forms control number */
    const uint16_t        *fcb;          /* Form control block */
    uint8_t                output[15][120];
};

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

void
model1443_dev(struct _device *unit, uint16_t *tags, uint16_t bus_out, uint16_t *bus_in)
{
    struct _1443_context *ctx = (struct _1443_context *)unit->dev;
    int      i;
    static   uint16_t last_tags = 0;

    if (last_tags != *tags) {
        print_tags("Printer", ctx->state, *tags, bus_out);
        last_tags = *tags;
    }

    /* Reset device if OPER OUT is dropped */
    if ((*tags & (CHAN_OPR_OUT|CHAN_SUP_OUT)) == 0) {
        log_device("Reset printer\n");
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
                 log_device("printer selected\n");
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
                 *tags &= ~(CHAN_SEL_OUT);             /* Clear select out and in */
                 *tags |= CHAN_ADR_IN;      /* Return address until accepted */
                 *bus_in = ctx->addr | odd_parity[ctx->addr];
                  log_device("printer address\n");
            }

            /* Wait for Command out to raise */
            /* Can now drop address in */
            if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                log_device("printer command %02x\n", bus_out);
                 ctx->cmd = bus_out & 0xff;
                 ctx->data_rdy = 0;
                 ctx->data_end = 0;
                 ctx->col = 0;
                 ctx->cnt = 0;
                 ctx->status = 0;
                 ctx->state = STATE_CMD;
                 ctx->delay = 0;
                 *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN);                 /* Clear address in */
                 if (ctx->ready == 0 || ctx->file == 0) {
                     ctx->sense = SENSE_INTERV;
                     ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                     break;
                 }
                 switch (ctx->cmd & 07) {
                 case 0: /* Test I/O */
                        break;
                 case 1: /* Write */
                        ctx->sense = 0;
                        /* Check if skip invalid */
                        if ((ctx->cmd & 0x80) != 0 && ((ctx->cmd & 0x78) == 0 || (ctx->cmd & 0x78) > 0x60)) {
                            ctx->cmd = 0;
                            ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                            ctx->sense = SENSE_CMDREJ;
                            break;
                        }
                        /* Check if space invalid */
                        if ((ctx->cmd & 0x80) == 0 && (ctx->cmd & 0x78) > 0x18) {
                            ctx->cmd = 0;
                            ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                            ctx->sense = SENSE_CMDREJ;
                            break;
                        }
                        break;
                 case 2: /* Read */
                        ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                        ctx->sense = SENSE_CMDREJ;
                        break;
                 case 3: /* Feed */
                        ctx->sense = 0;
                        /* Check if skip invalid */
                        if ((ctx->cmd & 0x80) != 0 && ((ctx->cmd & 0x78) == 0 || (ctx->cmd & 0x78) > 0x60)) {
                            ctx->cmd = 0;
                            ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                            ctx->sense = SENSE_CMDREJ;
                            break;
                        }
                        /* Check if space invalid */
                        if ((ctx->cmd & 0x80) == 0 && (ctx->cmd & 0x78) > 0x18) {
                            ctx->cmd = 0;
                            ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                            ctx->sense = SENSE_CMDREJ;
                            break;
                        }
                        ctx->status = (SNS_CHNEND);
                        ctx->data_end = 1;
                        break;
                 case 4:  /* Sense */
                        if (ctx->cmd != 0x4) {
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
             break;

    case STATE_CMD:
             /* Wait for Command out to drop */
             /* On MPX channel select out will drop, along with command */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_SUP_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SUP_OUT|CHAN_OPR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_OPR_IN)) {
                  *tags |= CHAN_OPR_IN|CHAN_STA_IN;     /* Wait for acceptance of status */
                 log_device("printer init stat\n");
             }

             /* When we get acknowlegment, go wait for it to go away */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_SUP_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~CHAN_STA_IN;
                 ctx->state = STATE_INIT_STAT;
                 log_device("printer init stat\n");
             }
             *bus_in = ctx->status | odd_parity[ctx->status];   /* Set initial status */
             /* Device selected */
             *tags &= ~(CHAN_SEL_OUT);                  /* Clear select out and in */
             break;

    case STATE_INIT_STAT:
             /* Wait for Service out to drop */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) || 
                 *tags == (CHAN_OPR_OUT|CHAN_OPR_IN)) {
                 if (ctx->cmd == 0 || (ctx->status & (SNS_UNITCHK|SNS_UNITEXP)) != 0) {
                     ctx->state = STATE_IDLE;
                     ctx->selected = 0;
                     *tags &= ~CHAN_OPR_IN;
                 } else {
                     ctx->state = STATE_OPR;
                     log_device("printer state done\n");
                 }
                 if (ctx->data_end && (*tags & CHAN_SEL_OUT) == 0) {
                     *tags &= ~CHAN_OPR_IN;
                     ctx->selected = 0;
                 }
                 break;
             }
             *tags &= ~(CHAN_SEL_OUT);                 /* Clear select out and in */
             break;

    case STATE_OPR:
              log_device("printer opr %d %d\n", ctx->selected, ctx->delay);
#if 0
             /* Wait for Command out to drop */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN) || 
                 *tags == (CHAN_CMD_OUT|CHAN_OPR_OUT|CHAN_OPR_IN)) {
                  log_device("Opr wait\n");
                 break;
             }
#endif

             /* If we are selected clear select in */
             if (ctx->selected)
                 *tags &= ~CHAN_SEL_OUT;

             if (ctx->data_end) {
                log_device("Oper Data end\n");
                if (ctx->cmd == 0x4) {   /* If sense, end command */
                    ctx->status = SNS_CHNEND|SNS_DEVEND;
                    ctx->state = STATE_END;
                    break;
                }
                ctx->delay = 100 * print_line(ctx);
                if (ctx->stop || ctx->single) {
                    ctx->stop = ctx->single = ctx->ready = 0;
                }
                if ((ctx->cmd & 0x3) == 3) {
                    if (ctx->selected) {
                       *tags &= ~CHAN_OPR_IN;
                       ctx->selected = 0;
                    }
                    ctx->status &= ~SNS_CHNEND;
                    ctx->state = STATE_WAIT;
                } else {
                    ctx->status |= SNS_CHNEND;
                    ctx->state = STATE_DATA_END;
                }
                break;
             }

             if (ctx->delay > 0) {
                 break;
             }

             log_device("Oper comand %02x\n", ctx->cmd);
             switch (ctx->cmd & 0x7) {
             case 4:  /* Sense */
                    ctx->data = ctx->sense;
                    ctx->data_rdy = 1;
                    ctx->data_end = 1;
                    ctx->state = STATE_DATA_O;
                    log_device("printer Sense %02x\n", ctx->sense);
                    break;
             case 1: /* Write */
                    /* Wait for data to be available */
                    if (ctx->data_rdy) {
                        log_device("printer get %02x\n", ctx->data);
                        if (ctx->cnt == 4) {
                           ctx->delay = 10;
                           ctx->cnt = 0;
                           *tags &= ~(CHAN_OPR_IN|CHAN_SEL_IN);/* Clear operator in */
                           ctx->selected = 0;                  /* Drop selected flag */
                           break;
                        }
                        if (ctx->col < 120) {
                           ctx->buf[ctx->col] = ctx->data & 0xff;
                           ctx->col++;
                           ctx->cnt++;
                        } else {
                           ctx->data_end = 1;
                           break;
                        }
                    }
                    ctx->data_rdy = 0;
                    ctx->state = STATE_DATA_I;
                    log_device("printer Write get1\n");
                    break;
             default: 
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
                log_device("printer reselect\n");
                break;
             }
            
             /* On Select channel, Select Out will not drop */
             /* Catch halt I/O */
             if (ctx->selected && *tags == (CHAN_OPR_OUT|CHAN_ADR_OUT|CHAN_OPR_IN) &&
                  (bus_out & 0xff) == ctx->addr)  {
                  /* Halt I/O */
                  /* Device selected */
                  *tags &= ~(CHAN_OPR_IN);             /* Clear select out and in */
                  ctx->data_end = 1; 
                  ctx->selected = 0;
                  log_device("printer Halt i/o\n");
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
                  log_device("printer deselected\n");
                  break;
             }
             break;

    case STATE_REQ:   /* Data available and we are not talking on channel */
             log_device("printer Request\n");
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_REQ_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_REQ_IN)) {
                 /* Put our address on the bus */
                 *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);        /* Clear select out and in */
                 *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                 *bus_in = ctx->addr | odd_parity[ctx->addr]; /* Put out our address */
                  log_device("printer Reselect\n");
                 break;
             }

             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                 /* Put our address on the bus */
                 *tags &= ~(CHAN_SEL_OUT);                    /* Clear select out and in */
                 *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                 *bus_in = ctx->addr | odd_parity[ctx->addr]; /* Put out our address */
                 log_device("printer Address\n");
                 break;
             }

             /* If we got bus, go and transfer */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                  *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN);  /* Clear select out and in */
                  ctx->selected = 1;
                  ctx->state = (ctx->cmd & 1) ? STATE_DATA_I : STATE_DATA_O;
                  log_device("printer selected\n");
                  break;
              }

              /* See if another device got it. */
              if ((*tags & (CHAN_OPR_IN|CHAN_STA_IN)) != 0) {
                  /* Drop request out until channel free again */
                  log_device("printer Other device\n");
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
              log_device("printer data in\n");
              /* Wait for command out to drop */
              if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_SRV_IN) || 
                  *tags == (CHAN_OPR_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_SRV_IN)) {
                   *tags &= ~(CHAN_SEL_OUT|CHAN_SRV_IN);  /* Clear select out and in */
                   /* Device selected */
                   log_device("Data %02x\n", bus_out);
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
                    log_device("printer Data End\n");
                   ctx->data_end = 1;
                   ctx->state = STATE_INIT_STAT;           /* Back to operation. */
                   break;
              }
              /* Put request in up */
              *tags |= CHAN_OPR_IN|CHAN_SRV_IN;
             /* If we are selected clear select in */
             if (ctx->selected)
                 *tags &= ~CHAN_SEL_OUT;

              break;

    case STATE_DATA_O:    /* Request to send data to channel */
               log_device("Printer Data output %02x %d\n", ctx->data, ctx->selected);
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
                   log_device("Printer Data sent\n");
              }
              if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_SRV_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_SRV_IN)) {
                   *tags &= ~(CHAN_SEL_OUT|CHAN_SRV_IN);  /* Count reached zero, no more accepted */
                   ctx->data_end = 1;
                   ctx->state = STATE_INIT_STAT;           /* Back to operation. */
                   log_device("printer Data End\n");
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
                         log_device("printer selected data_end\n");
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
                     log_device("printer Reselect data_end\n");
                     break;
                 }
                
                 if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                     *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                     /* Put our address on the bus */
                     *tags &= ~(CHAN_SEL_OUT);                    /* Clear select out and in */
                     *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                     *bus_in = ctx->addr | odd_parity[ctx->addr]; /* Put out our address */
                     log_device("printer Address data_end\n");
                     break;
                 }
                
                 /* If we got bus, go and transfer */
                 if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                     *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                      *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN);  /* Clear select out and in */
                      ctx->selected = 1;
                      log_device("printer selected data_end\n");
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
                 *tags |= CHAN_OPR_IN|CHAN_STA_IN;
                 *bus_in = ctx->status & SNS_CHNEND;
                 *bus_in |= odd_parity[*bus_in];
                  log_device("printer End channel status %02x %02x %03x\n", ctx->status, ctx->cmd, *bus_in);
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
                 log_device("printer Accepted\n");
                  if ((*tags & CHAN_SUP_OUT) == 0) {
                      ctx->status &= ~SNS_CHNEND;
                  }
//                  ctx->status |= SNS_DEVEND;
                  ctx->delay = 150;
                  ctx->state = STATE_WAIT;                    /* All done, back to idle state */
                  break;
             }

             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~(CHAN_SEL_OUT|CHAN_OPR_IN|CHAN_STA_IN);  /* Clear select out and in */
                  log_device("printer Stacked\n");
                  ctx->selected = 0;
                  if ((*tags & CHAN_SUP_OUT) == 0) {
                      ctx->status &= ~SNS_CHNEND;
                  }
//                  ctx->status |= SNS_DEVEND;
                  ctx->state = STATE_WAIT;                         /* Stack status */
                  break;
             }

             *bus_in = ctx->status & SNS_CHNEND;
             *bus_in |= odd_parity[*bus_in];
             /* Mark channel still in use */
             *tags &= ~(CHAN_SEL_OUT);  /* Clear select out and in */
             *tags |= CHAN_OPR_IN|CHAN_STA_IN;
             break;

    case STATE_END:
             if (ctx->selected == 0) {
                if ((*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_REQ_IN) ||
                    *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT|CHAN_SUP_OUT|CHAN_REQ_IN)) &&
                     (bus_out & 0xff) == ctx->addr) {
                     /* Device selected */
                     if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0)
                         ctx->sense |= SENSE_BUSCHK;
                     *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);      /* Clear select out and in */
                     *tags |= CHAN_OPR_IN;
                     log_device("printer selected end\n");
                     break;
                 }
                 if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                     *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN) ||
                     *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                     *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                     /* Put our address on the bus */
                     *tags &= ~(CHAN_SEL_OUT);                    /* Clear select out and in */
                     *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                     *bus_in = ctx->addr | odd_parity[ctx->addr]; /* Put out our address */
                     log_device("printer Reselect end\n");
                     break;
                 }
                 if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SUP_OUT|CHAN_REQ_IN) ||
                     *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_REQ_IN)) {
                     /* Put our address on the bus */
                     *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);        /* Clear select out and in */
                     *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                     *bus_in = ctx->addr | odd_parity[ctx->addr]; /* Put out our address */
                     log_device("printer Reselect end\n");
                     break;
                 }
                
                 /* If we got bus, go and transfer */
                 if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                     *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                      *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN);  /* Clear select out and in */
                      ctx->selected = 1;
                      log_device("printer selected end\n");
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
                 log_device("printer End status %02x %02x\n", ctx->status, ctx->cmd);
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
                 log_device("printer Accepted end\n");
                  ctx->selected = 0;
                  ctx->state = STATE_IDLE;                          /* All done, back to idle state */
                  break;
             }

             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_SUP_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~(CHAN_SEL_OUT|CHAN_OPR_IN|CHAN_STA_IN);  /* Clear select out and in */
                  log_device("printer Stacked end\n");
                  ctx->selected = 0;
                  ctx->state = STATE_STACK;                         /* Stack status */
                  break;
             }

             *bus_in = ctx->status | odd_parity[ctx->status];
             /* Mark channel still in use */
             *tags &= ~(CHAN_SEL_OUT);  /* Clear select out and in */
             *tags |= CHAN_OPR_IN;
             log_device("printer End status ready\n");
             break;

    case STATE_STACK:
            /* If channel drops select out */
            if (*tags == (CHAN_OPR_OUT)) {
                *tags |= CHAN_REQ_IN;
                break;
            }
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_REQ_IN)) {
                 /* Put our address on the bus */
                 *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);        /* Clear select out and in */
                 *tags |= CHAN_OPR_IN|CHAN_ADR_IN;            /* Put out device on request */
                 *bus_in = ctx->addr | odd_parity[ctx->addr]; /* Put out our address */
                 log_device("printer stack Reselect\n");
                 ctx->state = STATE_STACK_SEL;
                 break;
             }

             /* See if another device got it. */
             if ((*tags & (CHAN_OPR_IN|CHAN_STA_IN)) != 0) {
                 /* Drop request out until channel free again */
                 log_device("printer Other device\n");
                 break;
             }
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
                  log_device("printer stack selected\n");
             }
             break;

    case STATE_STACK_SEL:
            /* Wait until address out drops to put our address on bus */
            *tags |= CHAN_OPR_IN;
            /* When address out drops put our address on bus in */
            if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN|CHAN_SUP_OUT) ||
                *tags == (CHAN_OPR_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_OPR_IN|CHAN_ADR_IN|CHAN_SUP_OUT)) {
                 *tags |= CHAN_ADR_IN;      /* Return address until accepted */
                 log_device("printer stack address\n");
            }

            /* Wait for Command out to raise */
            /* Can now drop address in */
            if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN) ||
                *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_ADR_IN)) {
                log_device("printer stack command %02x\n", bus_out);
                 ctx->state = STATE_STACK_CMD;
                 *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN);                 /* Clear address in */
                 if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0) {
                     ctx->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                     ctx->sense |= SENSE_BUSCHK;
                 }
             }
             *tags &= ~(CHAN_SEL_OUT);             /* Clear select out and in */
             *bus_in = ctx->addr | odd_parity[ctx->addr];
             break;

    case STATE_STACK_CMD:
             /* Wait for Command out to drop */
             /* On MPX channel select out will drop, along with command */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_OPR_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_OPR_IN)) {
                  *tags |= CHAN_OPR_IN|CHAN_STA_IN;     /* Wait for acceptance of status */
                  log_device("printer stack init stat\n");
             }

             /* When we get acknowlegment, go wait for it to go away */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_SUP_OUT|CHAN_HLD_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SRV_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~(CHAN_STA_IN|CHAN_OPR_IN);
                 ctx->state = STATE_STACK_HLD;
                 log_device("printer stack init stat accept\n");
                 ctx->selected = 0;
             }
             /* When we get acknowlegment, go wait for it to go away */
             if (*tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_SUP_OUT|CHAN_HLD_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN) ||
                 *tags == (CHAN_OPR_OUT|CHAN_CMD_OUT|CHAN_OPR_IN|CHAN_STA_IN)) {
                 *tags &= ~(CHAN_STA_IN|CHAN_OPR_IN);
                 ctx->state = STATE_STACK_HLD;
                 log_device("printer stack init stat stacked\n");
             }
             *bus_in = ctx->status | odd_parity[ctx->status];   /* Set initial status */
             /* Device selected */
             *tags &= ~(CHAN_SEL_OUT);                  /* Clear select out and in */
             break;

    case STATE_STACK_HLD:
             /* Wait for Service out to drop */
             if (*tags == (CHAN_OPR_OUT) ||
                 *tags == (CHAN_OPR_OUT|CHAN_SUP_OUT) ||
                 *tags == (CHAN_OPR_OUT|CHAN_REQ_IN)) {
                 ctx->state = ctx->selected ? STATE_STACK : STATE_IDLE;
                 log_device("printer state done\n");
                 *tags &= ~CHAN_OPR_IN;
                 ctx->selected = 0;
             }
             break;

    case STATE_WAIT:
// log_device("wait %d %d\n", ctx->selected, ctx->delay);
             /* If we get select out with address out, reselection */
             if (!ctx->selected &&
                 *tags == (CHAN_OPR_OUT|CHAN_SEL_OUT|CHAN_HLD_OUT|CHAN_ADR_OUT) &&
                (bus_out & 0xff) == ctx->addr) {
                /* Device selected */
                *tags |= CHAN_STA_IN;
                *bus_in = SNS_BSY;
                ctx->selected = 1;
                log_device("printer wait select attempt\n");
             }

             /* If selected clear status in when select out drops */
             if (ctx->selected && *tags == (CHAN_OPR_OUT|CHAN_STA_IN)) {
                /* Device selected */
                *tags &= ~(CHAN_STA_IN);              /* Clear status in */
                ctx->selected = 0;
                log_device("printer wait deselect\n");
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
 * Print out a line and return number of lines being skipped 
 */
int
print_line(struct _1443_context *ctx)
{

    char                out[150];       /* Temp conversion buffer */
    int                 i;
    int                 l = (ctx->cmd >> 3) & 0x1f;
    int                 f = 1;
    int                 r = 0;     /* Rows skipped */
    uint16_t            mask;
    uint16_t            ch9, ch12;

    /* Dump buffer if full */
    if ((ctx->cmd & 0x7) == 01) {

        /* Try to convert to text */
        memset(out, ' ', sizeof(out));

        /* Scan each column */
        for (i = 0; i < ctx->col; i++) {
           int         ch = ctx->buf[i];

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
        memset(ctx->buf, 0x40, sizeof(ctx->buf));
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
            if (ctx->fcb[ctx->row] & (0x1000 >> 9) != 0)
                ch9 = 1;
            if (ctx->fcb[ctx->row] & (0x1000 >> 12) != 0)
                ch12 = 1;
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
           ctx->sense |= SENSE_CHAN9;
        }
        if (ch12 && (ctx->cmd & 0x3) == 0x1) {
           ctx->status |= SNS_UNITEXP;
        }
        return r;
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

    return l;
}


SDL_Texture *model1443_img;
static SDL_Color     cx = {0x10, 0x83, 0xd9};


void
draw_model1443(struct _device *unit, SDL_Renderer *render)
{
    struct _1443_context *ctx = (struct _1443_context *)unit->dev;
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
    rect.y = y + 42;
    rect.h = 100;
    rect.w = 305;
    rect2.x = 0;
    rect2.y = 0;
    rect2.h = 100;
    rect2.w = 305;
    SDL_RenderCopy(render, model1443_img, &rect2, &rect);
    /* Draw device number */
    sprintf(buf, "%01X%02X", ctx->chan, ctx->addr);
    text = TTF_RenderText_Solid(font14, buf, c1);
    txt = SDL_CreateTextureFromSurface(render, text);
    SDL_FreeSurface(text);
    SDL_QueryTexture(txt, &i, &j, &rect2.w, &rect2.h);
    rect2.x = rect.x+200;
    rect2.y = 20;
    SDL_RenderCopy(render, txt, NULL, & rect2);
    SDL_DestroyTexture(txt);
    /* Draw the paper.  */
    rect.x = x + 89;
    rect.y = y + 23 + 42;
    rect.w = 94;
    rect.h = 22;
    rect2.x = 0;
    rect2.y = 120 + ctx->row;
    rect2.w = 94;
    rect2.h = 22;
    SDL_RenderCopy(render, model1443_img, &rect2, &rect);
    /* Draw output */
    rect.h = 2;
    rect.w = 2;
    rect2.x = 118;
    rect2.h = 2;
    rect2.w = 2;
    for (i = 0; i < 10; i++) {
        rect.x = x + 89;
        rect.y = y + 23 + 42 + (2 * i) ;
        for (j = 0; j < 94; j++) {
            uint8_t  ch = ctx->output[i][j];
            if (ch != 0) {
                rect2.x = ch * 2;
                SDL_RenderCopy(render, model1443_img, &rect2, &rect);
            }
            rect.x++;
        }
    }
}

static void lpr_update(struct _popup *popup, void *device, int index)
{
    struct _device *unit = (struct _device *)device;
    struct _1443_context *ctx = (struct _1443_context *)unit->dev;
    int r;

    switch (index) {
    case 0:  /* Start Key */
          if (ctx->state == STATE_IDLE && ctx->file != NULL) {
              ctx->state = STATE_END;
              ctx->status |= SNS_DEVEND;
              ctx->stop = 0;
              ctx->single = 0;
              ctx->ready = 1;
          }
          break;

    case 1:  /* Check Reset */
          if (ctx->state == STATE_IDLE) {
              ctx->sense = 0;
          }
          break;

    case 2:  /* STOP */
          ctx->stop = 1;
          ctx->single = 0;
          ctx->ready = 0;
          break;

    case 3: /* Carriage Space */
          if (ctx->ready == 0) {
              ctx->cmd = 0x0b;
              (void)print_line(ctx);
              ctx->cmd = 0x0;
          }
          break;

    case 4: /* Carriage Restore */
          if (ctx->ready == 0) {
              ctx->cmd = 0x8b;
              (void)print_line(ctx);
              ctx->cmd = 0x0;
          }
          break;

    case 5: /* Single Cycle */
          if (ctx->state == STATE_IDLE && ctx->file != NULL) {
              ctx->single = 1;
              ctx->ready = 1;
          }
          break;

    case 6: /* Save paper */
          if (ctx->file != NULL) {
              fclose(ctx->file);
              ctx->form = 1;
          }
          free(ctx->file_name);
          ctx->file_name = NULL;
          ctx->file = fopen(popup->text[0].text, "a");
          if (ctx->file != NULL) {
             ctx->row = 0;
             if ((ctx->file_name = (char *)malloc(strlen(popup->text[0].text)+1)) == NULL) {
                 fclose(ctx->file);
                 ctx->file = NULL;
                 break;
             }
             strcpy(ctx->file_name, popup->text[0].text);
             ctx->form = 0;
          }
          break;
    }

    switch(ctx->fcb_num) {
    case 0:   ctx->fcb = &cctape_legacy[0]; ctx->lpp = 66; break;
    case 1:   ctx->fcb = &cctape_std1[0]; ctx->lpp = 66; break;
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
     {"START", NULL,  0, 0, 0, {0xff, 0xff, 0xff}, {0x0c, 0x2e, 0x30}, {0, 0, 0}},
     {"CHECK", "RESET", 0, 1, 0, {0, 0, 0},  {0xff, 0xfd, 0x5e}, {0xdd, 0xdc, 0x8a}},
     {"STOP", NULL, 0, 2, 0, {0xff, 0xff, 0xff}, {0xc8, 0x3a, 0x30}, {0, 0, 0}},
     {"CARRIAGE", "SPACE", 0, 0, 1, {0, 0, 0},  {0xdd, 0xdc, 0x8a}, {0xdd, 0xdc, 0x8a}},
     {"CARRIAGE", "RESTORE", 0, 1, 1, {0, 0, 0},  {0xdd, 0xdc, 0x8a}, {0xdd, 0xdc, 0x8a}},
     {"SINGLE", "CYCLE", 0, 2, 1, {0, 0, 0},  {0xdd, 0xdc, 0x8a}, {0xdd, 0xdc, 0x8a}},
     {"PRINT", "READY", 2, 3, 0, {0, 0, 0}, {0xd8, 0xcb, 0x72}, {0, 0, 0}},
     {"PRINT", "CHECK", 2, 4, 0, {0, 0, 0}, {0xd8, 0xcb, 0x72}, {0, 0, 0}},
     {"END OF", "FORMS", 2, 3, 1, {0, 0, 0}, {0xd8, 0xcb, 0x72}, {0, 0, 0}},
     {"FORMS", "CHECK", 2, 3, 2, {0, 0, 0}, {0xd8, 0xcb, 0x72}, {0, 0, 0}},
     {"SYNC", "CHECK", 2, 4, 2, {0, 0, 0}, {0xd8, 0xcb, 0x72}, {0, 0, 0}},
     {"SAVE", NULL, 0, 10, 0, {0, 0, 0}, {0xdd, 0xdc, 0x8a}, {0xdd, 0xdc, 0x8a}},
     {NULL, NULL, 0},
};

static char *type_label[] = { "LEGACY", "STD1", NULL};

static SDL_Renderer *render;
struct _popup 
*model1443_control(struct _device *unit, int hd, int wd, int u)
{
    struct _popup  *popup;
    struct _1443_context *ctx = (struct _1443_context *)unit->dev;
    SDL_Surface *text;
    int    i, j;
    int    w, h;
    int    tx;
    char   buffer[100];

    if ((popup = (struct _popup *)calloc(1, sizeof(struct _popup))) == NULL)
        return popup;
    sprintf(buffer, "IBM1443 Dev 0x'%01X%02X'", ctx->chan, ctx->addr);
    popup->screen = SDL_CreateWindow(buffer, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        900, 200, SDL_WINDOW_RESIZABLE );
    popup->render = SDL_CreateRenderer( popup->screen, -1, SDL_RENDERER_ACCELERATED);
    popup->device = unit; 
    popup->areas[popup->area_ptr].rect.x = 0;
    popup->areas[popup->area_ptr].rect.y = 0;
    popup->areas[popup->area_ptr].rect.h = 200;
    popup->areas[popup->area_ptr].rect.w = 360;
    popup->areas[popup->area_ptr].c = &cx;
    popup->area_ptr++;
    popup->areas[popup->area_ptr].rect.x = 360;
    popup->areas[popup->area_ptr].rect.y = 0;
    popup->areas[popup->area_ptr].rect.h = 200;
    popup->areas[popup->area_ptr].rect.w = 700;
    popup->areas[popup->area_ptr].c = &c;
    popup->area_ptr++;
    for (i = 0; labels[i].top != NULL; i++) {
        switch (labels[i].ind) {
        case 2:
            text = TTF_RenderText_Solid(font1, labels[i].top, labels[i].col_on);
            popup->led_bits[popup->led_ptr].digith_on = SDL_CreateTextureFromSurface( popup->render, text);
            SDL_FreeSurface(text);
            text = TTF_RenderText_Solid(font1, labels[i].top, labels[i].col_off);
            popup->led_bits[popup->led_ptr].digith_off = SDL_CreateTextureFromSurface( popup->render, text);
            SDL_FreeSurface(text);
            popup->led_bits[popup->led_ptr].recth.x = 20 + ((12* wd) * labels[i].x);
            popup->led_bits[popup->led_ptr].recth.y = 20 + ((2 * hd) * labels[i].y);
            popup->led_bits[popup->led_ptr].recth.h = hd;
            popup->led_bits[popup->led_ptr].recth.w = strlen(labels[i].top) * wd;
            if (labels[i].bot != NULL) {
                text = TTF_RenderText_Solid(font1, labels[i].bot, labels[i].col_on);
                popup->led_bits[popup->led_ptr].digitl_on = SDL_CreateTextureFromSurface( popup->render, text);
                SDL_FreeSurface(text);
                text = TTF_RenderText_Solid(font1, labels[i].bot, labels[i].col_off);
                popup->led_bits[popup->led_ptr].digitl_off = SDL_CreateTextureFromSurface( popup->render, text);
                SDL_FreeSurface(text);
                popup->led_bits[popup->led_ptr].rectl.x = popup->led_bits[popup->led_ptr].recth.x;
                popup->led_bits[popup->led_ptr].rectl.y = popup->led_bits[popup->led_ptr].recth.y + hd - 4;
                popup->led_bits[popup->led_ptr].rectl.h = hd;
                popup->led_bits[popup->led_ptr].rectl.w = strlen(labels[i].bot) * wd;
            }
            popup->led_ptr++;
            break;
        case 1:
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
            popup->ind[popup->ind_ptr].rect.y = 20 + ((4 *hd) * labels[i].y);
            popup->ind[popup->ind_ptr].rect.h = 2 * hd;
            popup->ind[popup->ind_ptr].rect.w = 10 * wd;
            popup->ind_ptr++;
            break;
        case 0:
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
            popup->sws[popup->sws_ptr].rect.y = 20 + ((4 *hd) * labels[i].y);
            popup->sws[popup->sws_ptr].rect.h = 2 * hd;
            popup->sws[popup->sws_ptr].rect.w = 10 * wd;
            popup->sws_ptr++;
            break;
        }
    }
    popup->led_bits[0].value = &ctx->ready;
    popup->led_bits[2].value = &ctx->form;

    text = TTF_RenderText_Solid(font14, "Paper: ", c1);
    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 380;
    popup->ctl_label[popup->ctl_ptr].rect.y = 20;
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;
    tx = 380 + w;
    popup->text[popup->txt_ptr].rect.x = tx;
    popup->text[popup->txt_ptr].rect.y = 20;
    popup->text[popup->txt_ptr].rect.w = 45 * wd;
    popup->text[popup->txt_ptr].rect.h = h + 5;
    if (ctx->file_name != NULL) 
        strncpy(popup->text[popup->txt_ptr].text, ctx->file_name, 256);
    else
        popup->text[popup->txt_ptr].text[0] = '\0';
    popup->text[popup->txt_ptr].len = strlen(popup->text[popup->txt_ptr].text);
    popup->text[popup->txt_ptr].pos = popup->text[popup->txt_ptr].len;
    popup->text[popup->txt_ptr].cpos = textpos(&popup->text[popup->txt_ptr], 
                                        popup->text[popup->txt_ptr].pos);
    popup->txt_ptr++;

    text = TTF_RenderText_Solid(font14, "Row: ", c1);
    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 380 + (60 * wd) - w;
    popup->ctl_label[popup->ctl_ptr].rect.y = 20 + (3 * hd);
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;
    popup->number[popup->num_ptr].rect.x = 380 + (60 * wd);
    popup->number[popup->num_ptr].rect.y = 20 + (3 * hd);
    popup->number[popup->num_ptr].rect.w = 5 * wd;
    popup->number[popup->num_ptr].rect.h = h;
    popup->number[popup->num_ptr].value = &ctx->row;
    popup->number[popup->num_ptr].c = &c;
    popup->num_ptr++;

    text = TTF_RenderText_Solid(font14, "FCB: ", c1);
    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 380;
    popup->ctl_label[popup->ctl_ptr].rect.y = 20 + (3 * hd);
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;

    popup->combo[popup->cmb_ptr].rect.x = tx;
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
    popup->combo[popup->cmb_ptr].num = ctx->fcb_num;
    popup->combo[popup->cmb_ptr].value = &ctx->fcb_num;
    popup->combo[popup->cmb_ptr].max = i - 1;
    popup->cmb_ptr++;

    popup->update = &lpr_update;
    return popup;
}

struct _device *
model1443_init(SDL_Renderer *render, uint16_t addr)
{
     SDL_Surface *text;
     struct _device *dev1443;
     struct _1443_context *lpr;

     if ((dev1443 = calloc(1, sizeof(struct _device))) == NULL)
         return NULL;
     if ((lpr = calloc(1, sizeof(struct _1443_context))) == NULL) {
         free(dev1443);
         return NULL;
     }

     text = IMG_ReadXPMFromArray(model1443_xpm);
     model1443_img = SDL_CreateTextureFromSurface(render, text);
     SDL_SetTextureBlendMode(model1443_img, SDL_BLENDMODE_BLEND);
     SDL_FreeSurface(text);

     dev1443->bus_func = &model1443_dev;
     dev1443->dev = (void *)lpr;
     dev1443->draw_model = (void *)&draw_model1443;
     dev1443->create_ctrl = (void *)&model1443_control;
     dev1443->rect[0].x = 305;
     dev1443->rect[0].y = 0;
     dev1443->rect[0].w = 280;
     dev1443->rect[0].h = 100;
     dev1443->n_units = 1;
     lpr->addr = (addr & 0xff);
     lpr->chan = (addr >> 8) & 0xf;
     lpr->state = STATE_IDLE;
     lpr->selected = 0;
     lpr->sense = 0;
     lpr->file_name = NULL;
     lpr->form = 1;
     add_chan(dev1443, addr);
     return dev1443;
}

