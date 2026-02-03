/*
 * microsim360 - Model 2821 controller
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "xlat.h"
#include "logger.h"
#include "event.h"
#include "device.h"
#include "model2821.h"

/*
 * The 2821 is a controller that connects to a 2540 and up to 3 1403 printers.
 * This controller handles channel and buffer control for all devices.
 */

#define SENSE_CMDREJ    BIT0  /* Invalid command */
#define SENSE_INTERV    BIT1  /* Operator intervention, reader empty */
#define SENSE_BUSCHK    BIT2  /* Bus parity error */
#define SENSE_EQUCHK    BIT3  /* Equipment check, not implemented */
#define SENSE_DATCHK    BIT4  /* Data format error */
#define SENSE_OVRRUN    BIT5  /* Data missed */


DEV_LIST_STRUCT(2821, CTRL_TYPE, 0);

/* Process channel operations */
void
model2821_dev(struct _device *unit, uint16_t *tags, uint16_t bus_out, uint16_t *bus_in)
{
    struct _2821_context *ctx = (struct _2821_context *)unit->dev;
    struct _2821_dev_context *dev = ctx->sel_device;
    static   uint16_t last_tags = 0;
    int               i;

    if (last_tags != *tags || unit->selected) {
        print_tags("Test", ctx->state, *tags, bus_out);
        last_tags = 0; // *tags;
    }

    /* Reset device if OPER OUT is dropped */
    if ((*tags & (CHAN_OPR_OUT|CHAN_SUP_OUT)) == 0) {
        if (unit->selected) {
           *tags &= ~(CHAN_OPR_IN|CHAN_ADR_IN|CHAN_SRV_IN|CHAN_STA_IN);
        }
        log_device("2821: %03x reset\n",unit->addr);
        unit->selected = 0;
        unit->request = 0;
        ctx->state = STATE_IDLE;
        return;
    }

    switch (ctx->state) {
    /* Idle wait for CPU to talk to us */
    case STATE_IDLE:
            ctx->disconnect = 0;
            /* If operation out, reset device */
            if ((*tags & CHAN_OPR_OUT) == 0) {
                break;
            }

            /* If operation in, another device has channel */
            if ((*tags & CHAN_OPR_IN) != 0) {
                for (i = 0; i < 5; i++) {
                    if (ctx->device[i] != NULL &&
                      (ctx->device[i]->request || ctx->device[i]->stacked)) {
                        *tags &= ~(CHAN_REQ_IN);
                        break;
                    }
                }
                break;
            }

            /* If we have request and suppress out is down, post request */
            for (i = 0; i < 5; i++) {
                if (ctx->device[i] != NULL &&
                  (ctx->device[i]->request || ctx->device[i]->stacked)) {
                   log_device("2821: %03x port request\n",ctx->device[i]->addr);
                   if ((*tags & (CHAN_SUP_OUT|CHAN_ADR_OUT)) == 0) {
                       *tags |= (CHAN_REQ_IN);
                       break;
                   }
                }
            }

            /* If select out check if channel is asking for us or we have status */
            if ((*tags & CHAN_SEL_OUT) != 0) {
                 dev = NULL;
                 /* Check if looking for this device */
                 if ((*tags & CHAN_ADR_OUT) != 0) {
                     for (i = 0; i < 5; i++) {
                         if (ctx->device[i] != NULL &&
                                (bus_out & 0xff) == (ctx->device[i]->addr & 0xff)) {
                             *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);
                             dev = ctx->device[i];
                             ctx->sel_device = dev;
                             /* Check if parity error on bus */
                             if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0) {
                                 dev->sense |= SENSE_BUSCHK;
                             }
                             /* If device in operation, respond with busy status */
                             if (dev->busy/* && ctx->data_end == 0*/) {
                                 *bus_in = SNS_BSY | odd_parity[SNS_BSY];
                                 *tags |= CHAN_STA_IN;             /* Put Busy flag on bus */
                                 ctx->state = STATE_BUSY;
                                 log_device("2821: %03x busy\n",dev->addr);
                                 break;
                             }

                             /* Clear select in, and raise operation in */
                             *tags |= CHAN_OPR_IN;             /* Put our address on bus */
                             *bus_in = (dev->addr && 0xff) | odd_parity[dev->addr && 0xff];
                             ctx->state = STATE_INIT_SEL; /* Start initial select sequence */
                             unit->selected = 1;
                             log_device("2821: %03x selected\n",dev->addr);
                             break;
                          }
                      }
                 }

                 /* If no address out, see if we have request or stacked status */
                 if ((*tags & CHAN_SUP_OUT) == 0) {
                     for (i = 0; i < 5; i++) {
                         if (ctx->device[i] != NULL &&
                            (ctx->device[i]->request || ctx->device[i]->stacked)) {
                             dev = ctx->device[i];
                             ctx->sel_device = dev;
                            *tags &= ~(CHAN_SEL_OUT|CHAN_REQ_IN);
                            *tags |= CHAN_OPR_IN;      /* Put our address on bus */
                            *bus_in = (dev->addr & 0xff) | odd_parity[dev->addr & 0xff];
                            unit->selected = 1;
                            ctx->state = STATE_INIT_SEL;
                            log_device("2821: %03x polling\n",dev->addr);
                            break;
                         }
                     }
                 }

             }
             break;

            /* Start of initial selection sequence */
    case STATE_INIT_SEL:
            *tags &= ~(CHAN_SEL_OUT);  /* Clear select in */
            *bus_in = (dev->addr & 0xff) | odd_parity[dev->addr & 0xff];
            log_device("2821: %03x address in\n", dev->addr);
            /* Wait for Address out to drop */
            if ((*tags & (CHAN_ADR_OUT)) == 0) {
                 *tags |= CHAN_ADR_IN;
                 ctx->state = STATE_COMMAND;
            }
            break;

     case STATE_COMMAND:
            /* Wait for command or address out */
            *tags &= ~(CHAN_SEL_OUT);
            dev->request = 0;

            log_device("2821: %03x waiting command %02x\n",dev->addr, dev->status);
            *bus_in = (dev->addr & 0xff) | odd_parity[dev->addr & 0xff];
            /* we get command out, process command */
            if ((*tags & (CHAN_CMD_OUT)) != 0) {
                *tags &= ~(CHAN_ADR_IN);        /* Command out, drop addressin */
                /* Check if parity error on bus */
                if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0) {
                    dev->cmd = 0;
                    dev->cmd_done = 0;
                    dev->busy = 0;
                    dev->bptr = dev->blen;
                    dev->status = (SNS_CHNEND|SNS_DEVEND|SNS_UNITCHK);
                    dev->sense |= SENSE_BUSCHK;
                    ctx->state = STATE_STATUS;
                    break;
                }
                ctx->state = STATE_STATUS; /* Present status out */

                /* Present command end if command done */
                if (dev->cmd_done) {
                    dev->status |= SNS_DEVEND;
                    break;
                }

                if (dev->busy) {
                    if (dev->bptr < dev->blen) {
                        ctx->data = dev->buffer[dev->bptr];
                        ctx->state = STATE_DATA_1;
                        break;
                    }
                    if (dev->status == 0) {
                        dev->status = SNS_BSY;
                    }
                    break;
                }
                if (!dev->stacked && dev->status == 0) {       /* If no stacked status, process command */
                    dev->device_cmd(dev, bus_out);
                    if (ctx->mode) {
                        dev->xfer_count = dev->blen;
                    } else {
                        dev->xfer_count = dev->mpx_count;
                    }
                }
                break;
            }

            /* If we get Address out again, we need to halt */
            if ((*tags & (CHAN_ADR_OUT)) != 0 && (*tags & CHAN_HLD_OUT) == 0) {
                *tags &= ~(CHAN_ADR_IN|CHAN_OPR_IN);  /* Clear select in */
                log_device("Halt: %03x device\n",dev->addr);
                if (dev->bptr < dev->blen) {
                    if (dev->device_halt != NULL) {
                        dev->device_halt(dev);
                    }
                    dev->status |= SNS_CHNEND;
                }
                ctx->state = STATE_STATUS_WAIT;
                break;
            }

            break;

    /* Present initial status */
    case STATE_STATUS:
             /* Wait for Command out to drop */
             *tags &= ~(CHAN_SEL_OUT|CHAN_ADR_IN);      /* Drop address in */

             *bus_in = dev->status | odd_parity[dev->status];
             log_device("2821: %03x initial status %02x\n",dev->addr, dev->status);
             *tags |= (CHAN_STA_IN);
             ctx->state = STATE_STATUS_ACCEPT;    /* Wait for device to accept out status */
             break;

    /* Wait for CPU to either accept or stack status */
    case STATE_STATUS_ACCEPT:
             /* CPU will respond in a couple ways. */
             *tags &= ~(CHAN_SEL_OUT);
             *bus_in = dev->status | odd_parity[dev->status];
             if ((*tags & CHAN_CMD_OUT) != 0) {      /* CPU does not want status, stack it */
                 log_device("2821: %03x status stacked\n",dev->addr);
                 dev->stacked = 1;
                 ctx->state = STATE_STATUS_WAIT;
                 *tags &= ~(CHAN_OPR_IN|CHAN_STA_IN);
                 break;
             }
             if ((*tags & CHAN_SRV_OUT) != 0) {   /* CPU accepted the status, continue on */
                 log_device("2821: %03x status accepted\n",dev->addr);

                dev->status = 0;
                *tags &= ~(CHAN_STA_IN);
                /* If end of command, and status accepted, all done */
                if (dev->cmd_done || dev->cmd == 0) {
                    *tags &= ~(CHAN_OPR_IN);
                    dev->stacked = 0;
                    dev->bptr = dev->blen;
                    dev->cmd_done = 0;
                    dev->cmd = 0;
                    dev->busy = 0;
                    ctx->state = STATE_STATUS_WAIT;
                    break;
                }

                /* Handle sense command */
                if (dev->cmd == 0x04) {
                    ctx->data = dev->sense;
                    ctx->state = STATE_DATA_1;
                    break;
                }

                if (dev->bptr == dev->blen) {
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
                 if ((*tags & CHAN_HLD_OUT) == 0 || dev->busy == 0) {
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
                     log_device("2821: %03x Halt IO\n",unit->addr);
                     if (dev->device_halt != NULL) {
                         dev->device_halt(dev);
                     }
                     dev->bptr = dev->blen;
                     dev->status |= SNS_CHNEND|SNS_DEVEND;
                     dev->cmd_done = 1;
                     dev->request = 1;
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

             *bus_in = dev->status | odd_parity[dev->status];
             *tags |= (CHAN_STA_IN);

             log_device("2821: %03x %02x end status %d\n",unit->addr, dev->status, unit->request);
             ctx->state = STATE_END_ACCEPT;    /* Wait for CPU to accept out status */
             break;

     /* Wait for CPU to accept or stack status */
     case STATE_END_ACCEPT:
             *tags &= ~(CHAN_SEL_OUT);

             *bus_in = dev->status | odd_parity[dev->status];
             /* CPU does not want status right now. stack it */
             if ((*tags & CHAN_CMD_OUT) != 0) {
                 log_device("2821: %03x status stacked %d\n",unit->addr, unit->request);
                 dev->stacked = 1;
                 ctx->state = STATE_STATUS_WAIT;
                 *tags &= ~(CHAN_STA_IN|CHAN_OPR_IN);
                 break;
             }

             /* CPU accepted status */
             if ((*tags & CHAN_SRV_OUT) != 0) {

                 log_device("2821: %03x status accepted %d\n",unit->addr, unit->request);
                 dev->status = 0;
                 /* If end of command, and status accepted, all done */
                 if (dev->cmd_done) {
                     *tags &= ~(CHAN_STA_IN|CHAN_OPR_IN);
                     dev->stacked = 0;
                     dev->cmd = 0;
                     dev->cmd_done = 0;
                     dev->busy = 0;
                     ctx->state = STATE_STATUS_WAIT;
                     break;
                 }

                 if (dev->bptr == dev->blen) {
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
                     if (dev->xfer_count == 0) {
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
            log_device("2821: %03x wait end b=%d cd=%d %02x %02x\n",dev->addr,
                dev->busy, dev->cmd_done, dev->cmd, dev->status);
             *tags &= ~(CHAN_SEL_OUT);
             if (dev->cmd_done) {
                 dev->request = 0;
                 ctx->state = STATE_STATUS;
             }
             break;

      /* Handle normal operations */
      case STATE_OPR:
             log_device("2821: %03x opr\n",dev->addr, unit->selected);
             dev->request = 0;
             *tags &= ~(CHAN_SEL_OUT);

             /* If address out, halt device */
             if ((*tags & CHAN_ADR_OUT) != 0) {
                 if (dev->device_halt != NULL) {
                     dev->device_halt(dev);
                 }
                 dev->bptr = dev->blen;
                 dev->xfer_count = 0;
                 dev->status |= SNS_CHNEND;
                 *tags &= ~(CHAN_OPR_IN);
                 ctx->selected = 0;
                 ctx->state = STATE_IDLE;
                 break;
             }

             /* If device still needs data */
             if (dev->bptr == dev->blen) {
                 dev->status = SNS_CHNEND;
                 if (dev->device_xfer_done != NULL) {
                     dev->device_xfer_done(dev);
                 }
                 ctx->state = STATE_END_STATUS;
                 break;
             }

             /* If disconnect requested, and not on selector channel disconnect */
             if (ctx->disconnect) {
                 ctx->disconnect = 0;
                 if ((*tags & CHAN_HLD_OUT) == 0) {
                     *tags &= ~(CHAN_OPR_IN);
                     unit->selected = 0;
                     dev->request = 1;
                     ctx->state = STATE_IDLE;
                     break;
                 }
             }

             /* If device still needs data */
             if (dev->bptr < dev->blen) {
                 ctx->data = dev->buffer[dev->bptr];
                 ctx->state = STATE_DATA_1;
                 break;
             }


             /* If at end of data or command, present status */
             if (dev->cmd_done) {
                 ctx->state = STATE_END_STATUS;
                 break;
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
             if ((*tags & (CHAN_SRV_OUT)) != 0) {
                 *tags &= ~(CHAN_SRV_IN);   /* Clear service in request */
                 /* Wait for service out or command out */
                 if ((dev->cmd & 1) != 0) { /* Write command */
                      /* Device selected */
                      if (((bus_out ^ odd_parity[bus_out & 0xff]) & 0x100) != 0) {
                          dev->sense |= SENSE_BUSCHK;
                          dev->status |= (SNS_UNITCHK);
                          if (dev->device_xfer_done != NULL) {
                              dev->device_xfer_done(dev);
                          }
                          dev->status |= SNS_CHNEND|SNS_DEVEND;
                          dev->busy = 0;
                          dev->cmd_done = 1;
                      }
                      dev->buffer[dev->bptr] = (uint8_t)(bus_out & 0xff); /* Grab data */
                 }
                 if (dev->cmd == 0x04) {
                     dev->status |= SNS_CHNEND|SNS_DEVEND;
                     dev->busy = 0;
                     dev->cmd_done = 1;
                     dev->cmd = 0;
                     ctx->state = STATE_END_STATUS;
                     break;
                 }
                 dev->bptr++;
                 ctx->state = STATE_OPR;   /* Go to process this data */
                 if (ctx->mode == 0) {
                     if (dev->xfer_count == 0) {
                         dev->xfer_count = dev->mpx_count;
                         ctx->disconnect = 1;
                     } else {
                         dev->xfer_count--;
                     }
                 }
             }
             if ((*tags & CHAN_CMD_OUT) != 0) {  /* CPU is done sending data */
                 *tags &= ~(CHAN_SRV_IN);   /* Clear service in request */
                 dev->bptr = dev->blen;
                 if (dev->device_xfer_done != NULL) {
                     dev->device_xfer_done(dev);
                 }
                 dev->status |= SNS_CHNEND;
                 ctx->state = STATE_END_STATUS;
             }
             break;
    }
}

static void
null_dev(struct _device *unit, uint16_t *tags, uint16_t bus_out, uint16_t *bus_in)
{
}

struct _2821_dev_context *
model2821_get_type(struct _device *unit, int dev_type)
{
    struct _2821_context *ctx = (struct _2821_context *)unit->dev;
    int     u;

    switch(dev_type) {
    case DEVICE_2821_READER:
         return ctx->device[0];

    case DEVICE_2821_PUNCH:
         return ctx->device[1];

    case DEVICE_2821_PRINTER:
         for (u = 2; u < 5; u++) {
             if (ctx->device[u] != NULL) {
                return ctx->device[u];
             }
         }
    }
    return NULL;
}

int
model2821_register(struct _device *unit, struct _2821_dev_context *dev, int dev_type)
{
    struct _2821_context *ctx = (struct _2821_context *)unit->dev;
    int       u = -1;
    int       i;

    switch(dev_type) {
    case DEVICE_2821_READER: u = 0; break;
    case DEVICE_2821_PUNCH: u = 1; break;
    case DEVICE_2821_PRINTER:
         for (i = 2; i < 5; i++) {
             if (ctx->device[i] == NULL) {
                u = i;
                break;
             }
         }
         break;
    default:
         return DEVICE_2821_UNKN;
    }

    if (u == -1) {
        return DEVICE_2821_FULL;
    }

    if (ctx->device[u] != NULL) {
        return DEVICE_2821_DUP;
    }
    ctx->device[u] = dev;
    dev->unit = ctx;
    dev->device->bus_func = &null_dev;
    return DEVICE_2821_OK;
}

struct _device *
model2821_init(uint16_t addr)
{
     struct _device       *dev2821;
     struct _2821_context *ctrl;
     int                   i;

     /* Allocate structures to hold device information */
     if ((dev2821 = (struct _device *)calloc(1, sizeof(struct _device))) == NULL)
         return 0;
     if ((ctrl = (struct _2821_context *)calloc(1, sizeof(struct _2821_context))) == NULL) {
         free(dev2821);
         return 0;
     }

     /* Fill in structures */
     dev2821->bus_func = &model2821_dev;
     dev2821->dev = (void *)ctrl;
     dev2821->draw_model = NULL;
     dev2821->create_ctrl = NULL;
     dev2821->init_device = NULL;
     dev2821->type_name = "2821";
     for (i = 0; i < 5; i++) {
          dev2821->rect[i].x = 0;
          dev2821->rect[i].y = 0;
          dev2821->rect[i].w = 0;
          dev2821->rect[i].h = 0;
          ctrl->device[i] = NULL;
     }
     dev2821->n_units = 0;
     dev2821->addr = addr;
     ctrl->state = STATE_IDLE;
     ctrl->selected = 0;
     add_chan(dev2821, addr);
     return dev2821;
}

/*
 * Create a new 2821 device.
 */
int
model2821_create(struct _option *opt)
{
     struct _device       *dev2821;
     struct _2821_context *ctx;
     struct _option       opts;

     /* Check for valid address */
     if (opt->addr == 0) {
         fprintf(stderr, "Missing address on 2540 device\n");
         return 0;
     }

     /* Allocate structures to hold device information */
     dev2821 = model2821_init(opt->addr);
     ctx = (struct _2821_context *)dev2821->dev;

     /* Parse options given on definition */
     while (get_option(&opts)) {
           if (strcmp(opts.opt, "MODE") == 0) {
               if (get_string(&opts)) {
                   if (strcmp(opts.string, "BYTE") == 0) {
                       ctx->mode = 0;
                   } else if (strcmp(opts.string, "BURST") == 0) {
                       ctx->mode = 1;
                   } else {
                       log_error("Invalid mode %s\n", opts.string);
                       del_chan(dev2821, opt->addr);
                       free(dev2821);
                       return 0;
                   }
               }
           } else {
               fprintf(stderr, "Invalid option %s to 2821\n", opts.opt);
               del_chan(dev2821, opt->addr);
               free(dev2821);
               return 0;
           }
     }

     return 1;
}


