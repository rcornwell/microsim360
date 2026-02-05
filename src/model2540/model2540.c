/*
 * microsim360 - Model 2540 card reader/punch.
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
#include "model2821/model2821.h"
#include "model2540.h"

//DEV_LIST_STRUCT(2540, DEV_TYPE, CHAR_OPT);

static char *model2540r = "2540R";
static char *model2540p = "2540P";

struct _device *
model2540_init(uint16_t addr, uint16_t addr_2821, int dev_type)
{
     struct _device           *dev2540;
     struct _device           *unit;
     struct _2821_dev_context *ctx2821 = NULL;
     struct _2821_dev_context *ctx2540 = NULL;
     struct _2540_context     *dev;
     int                       i;
     int                       init = 0;
     char                     *type;

     /* Find 2821 to attach too */
     if ((unit = find_chan_name("2821", addr_2821)) == NULL) {
         printf("Unable to find a 2821 device\n");
         return NULL;
     }

    /* See if we can find an existing device */
    switch(dev_type) {
    case DEVICE_2821_READER:
         type = model2540r;
         ctx2821 = model2821_get_type(unit, DEVICE_2821_PUNCH);
         break;

    case DEVICE_2821_PUNCH:
         type = model2540p;
         ctx2821 = model2821_get_type(unit, DEVICE_2821_READER);
         break;

    default:
         log_error("Invalid device type for 2450 %03x\n", addr);
         return NULL;
    }

    /* Create device */
    if ((dev2540 = calloc(1, sizeof(struct _device))) == NULL) {
        return NULL;
    }

    if ((ctx2540 = calloc(1, sizeof(struct _2821_dev_context))) == NULL) {
        free(dev2540);
        return NULL;
    }

    dev2540->type_name = type;
    dev2540->addr = addr;
    ctx2540->addr = addr;
    ctx2540->device = dev2540;
    ctx2540->blen = 80;
    ctx2540->mpx_count = 2;
    ctx2540->sense = 0;
    if (dev_type == DEVICE_2821_READER) {
        ctx2540->device_cmd = &model2540r_cmd;
        ctx2540->device_xfer_done = &model2540r_xfer_done;
    } else {
        ctx2540->device_cmd = &model2540p_cmd;
        ctx2540->device_xfer_done = &model2540p_xfer_done;
    }
    dev2540->dev = (void *)ctx2540;

    /* See if 2540 context is available */
    if(ctx2821 == NULL) {
        init = 1;
        /* Create one if not */
        if ((dev = calloc(1, sizeof(struct _2540_context))) == NULL) {
            return NULL;
        }

        for(i = 0; i < 5; i++) {
            dev->stack[i] = init_card_context();
            dev->stk_cnt[i] = stack_size(dev->stack[i]);
        }

        /* This device will draw device */
        dev2540->draw_model = (void *)&model2540_draw;
        dev2540->create_ctrl = (void *)&model2540_control;
        dev2540->init_device = (void *)&model2540_init_graphics;
        dev2540->rect[0].x = 0;
        dev2540->rect[0].y = 0;
        dev2540->rect[0].w = 210;
        dev2540->rect[0].h = 150;
        dev2540->rect[0].u_offset_x = 105;
        dev2540->n_units = 1;
    } else {
        dev = (struct _2540_context *)ctx2821->ctx;
    }

    if (dev_type == DEVICE_2821_READER) {
        dev->rdr_feed = init_card_context();
        dev->rdr_ctx = ctx2540;
    } else {
        dev->pch_feed = init_card_context();
        dev->pch_ctx = ctx2540;
    }

    ctx2540->ctx = (void *)dev;
    add_chan(dev2540, dev2540->addr);

     switch(model2821_register(unit, ctx2540, dev_type)) {
     case DEVICE_2821_OK:
               log_info("%03x registered to 2821 device %03x\n", addr, addr_2821);
               break;
     default:
               log_error("2821 %03x already has too many printers\n", addr_2821);
               del_chan(dev2540, dev2540->addr);
               if (init) {
                   free(dev);
               }
               free(ctx2540);
               free(dev2540);
               return NULL;
     }
     return dev2540;
}

/*
 * Create a new 2540 device.
 */
int
model2540_create(struct _option *opt)
{
     struct _device       *dev2540;
     struct _2821_dev_context *ctx2821;
     struct _2540_context *ctx;
     struct _option       opts;
     int                  i;
     int                  addr2821 = -1;

     /* Check for valid address */
     if (opt->addr == 0) {
         log_error("Missing address on 2540%c device\n", opt->model);
         return 0;
     }

     /* Find 2821 device */
     while (get_option(&opts)) {
        if (strcmp(opts.opt, "2821") == 0) {
            if (get_integer(&opts, &addr2821) == 0) {
               addr2821 = -1;
               log_error("Invalid 2821 address for device %03x\n", opt->addr);
               return 0;
            }
            break;
        }
     }

     if (addr2821 < 0) {
         log_error("%03x missing 2821 address\n", opt->addr);
         return 0;
     }

     option_reset();

     /* Check whether reader or punch */
     switch (opt->model) {
     case 'R':
            /* Create device */
            dev2540 = model2540_init(opt->addr, (uint16_t)addr2821, DEVICE_2821_READER);

            if (dev2540 == NULL) {
               return 0;
            }

            /* Get pointer to shared context */
            ctx2821 = (struct _2821_dev_context *)dev2540->dev;
            ctx = (struct _2540_context *)ctx2821->ctx;

            /* Parse options given on definition */
            while (get_option(&opts)) {
                  if (strcmp(opts.opt, "FILE") == 0 && opts.flags == 1) {

                      if (read_deck(ctx->rdr_feed, opts.string) != 1) {
                         log_error("Unable to attach deck %s\n", opts.string);
                         return 0;
                      }
                  } else if (strcmp(opts.opt, "EMPTY") == 0) {
                      empty_cards(ctx->rdr_feed);
                  } else if (strcmp(opts.opt, "BLANK") == 0 && opts.flags == 1) {
                      int num;
                      if (get_integer(&opts, &num) != 0)
                          return 0;
                      blank_deck(ctx->rdr_feed, num);
                  } else if (strcmp(opts.opt, "FORMAT") == 0) {
                      i = get_index(&opts, card_fmt_type);
                      if (i >= 0)
                          ctx->rdr_feed->mode = i;
                  } else if (strcmp(opts.opt, "2821") == 0) {
                  } else {
                      fprintf(stderr, "Invalid option %s to 2540\n", opts.opt);
                      del_chan(dev2540, opt->addr);
                      free(dev2540);
                      return 0;
                  }
            }
            break;

     case 'P':
            /* Create device */
            dev2540 = model2540_init(opt->addr, (uint16_t)addr2821, DEVICE_2821_PUNCH);

            if (dev2540 == NULL) {
               return 0;
            }

            /* Get pointer to shared context */
            ctx2821 = (struct _2821_dev_context *)dev2540->dev;
            ctx = (struct _2540_context *)ctx2821->ctx;

            /* Parse options given on definition */
            while (get_option(&opts)) {
                  if (strcmp(opts.opt, "FILE") == 0 && opts.flags == 1) {
                      if (read_deck(ctx->pch_feed, opts.string) != 1) {
                         log_error("Unable to attach deck %s\n", opts.string);
                         return 0;
                      }
                  } else if (strcmp(opts.opt, "EMPTY") == 0) {
                      empty_cards(ctx->pch_feed);
                  } else if (strcmp(opts.opt, "BLANK") == 0 && opts.flags == 1) {
                      int num;
                      if (get_integer(&opts, &num) != 0)
                          return 0;
                      blank_deck(ctx->pch_feed, num);
                  } else if (strcmp(opts.opt, "FORMAT") == 0) {
                      i = get_index(&opts, card_fmt_type);
                      if (i >= 0)
                          ctx->pch_feed->mode = i;
                  } else if (strcmp(opts.opt, "2821") == 0) {
                  } else {
                      fprintf(stderr, "Invalid option %s to 2540\n", opts.opt);
                      del_chan(dev2540, opt->addr);
                      free(dev2540);
                      return 0;
                  }
            }
            break;

     default:
          log_error("Invalid model for 2540%c %03x\n", opt->model, opt->addr);
          return 0;
     }

     return 1;
}


