/*
 * microsim360 - Model 2844 disk controller.
 *
 * Copyright 2023, Richard Cornwell
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
#include <SDL.h>
#include <SDL_timer.h>
#include <SDL_thread.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <string.h>
#include "widgets.h"
#include "button.h"
#include "area.h"
#include "indicator.h"
#include "label.h"
#include "text.h"
#include "checkbox.h"
#include "logger.h"
#include "event.h"
#include "device.h"
#include "xlat.h"
#include "model2844.h"

#include "model2314.xpm"


SDL_Texture *model2314_img = NULL;

void
model2314_init_graphics(struct _device *unit, void *rend)
{
    if (model2314_img == NULL) {
        SDL_Renderer *render = (SDL_Renderer *)rend;
        SDL_Surface *text;

        text = IMG_ReadXPMFromArray(model2314_xpm);
        model2314_img = SDL_CreateTextureFromSurface(render, text);
        SDL_SetTextureBlendMode(model2314_img, SDL_BLENDMODE_BLEND);
        SDL_FreeSurface(text);
    }
}


/*
 * microsim360 - Model 2844 microcode simulator.
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
#include <string.h>
#include <stdlib.h>
#include "device.h"
#include "logger.h"
#include "conf.h"
#include "model2844.h"

//DEV_LIST_STRUCT(2314, UNIT_TYPE, 0);
//DEV_LIST_STRUCT(2844, CTRL_TYPE, 0);


int
model2844_create(struct _option *opt)
{
     struct  _device *dev2844;

     /* Check for valid address */
     if (opt->addr == 0) {
         fprintf(stderr, "Missing address on 2844 device\n");
         return 0;
     }

     dev2844 = model2844_init(opt->addr);

     return 1;
}

/*
 * Create a new 2314 disk drive.
 */
int
model2314_create(struct _option *opt)
{
     struct  _device *dev2844;
     struct  _2844_context *ctx;
     struct _option   opts;
     int              i;
     char            *file;
     int              fmt;
     char            *vol;
     int              t;

     dev2844 = find_chan(opt->addr, 0xf8);
     if (dev2844 == NULL) {
         fprintf(stderr, "Device not found %s %03x\n", opt->opt, opt->addr);
         return 0;
     }
     i = opt->addr & 0x7;
     ctx = (struct _2844_context *)dev2844->dev;
     if (ctx->disk[i] != NULL) {
         fprintf(stderr, "Duplicate device %s %03x\n", opt->opt, opt->addr);
         return 0;
     }
     ctx->disk[i] = (struct _dasd_t *)calloc(1, sizeof(struct _dasd_t));
     if (ctx->disk[i] == NULL) {
         fprintf(stderr, "Unable to create device %s %03x\n", opt->opt, opt->addr);
         return 0;
     }
     if (dasd_settype(ctx->disk[i], "2314") == 0) {
         fprintf(stderr, "Unknown type %s %03x\n", opt->opt, opt->addr);
         free(ctx->disk[i]);
         ctx->disk[i] = NULL;
         return 0;
     }
     file = NULL;
     vol = NULL;
     fmt = 0;
     while (get_option(&opts)) {
         if (strcmp(opts.opt, "FILE") == 0 && opts.flags == 1) {
             file = strdup(opts.string);
         } else if (strcmp(opts.opt, "FORMAT") == 0) {
             fmt = 1;
         } else if (strcmp(opts.opt, "VOLID") == 0) {
             vol = strdup(opts.string);
         } else {
             fprintf(stderr, "Invalid option %s to 2314 Unit\n", opts.opt);
             free(ctx->disk[i]);
             ctx->disk[i] = NULL;
             return 0;
         }
     }
     dev2844->rect[i&6].x = 0;
     dev2844->rect[i&6].y = 0;
     dev2844->rect[i&6].w = 180;
     dev2844->rect[i&6].h = 400;
     dev2844->rect[i&6].u_offset_y = 200;
     if (vol != NULL) {
         for (t = 0; t < 8; t++) {
             if (vol[t] == '\0')
                 break;
             ctx->disk[i]->vol_label[t] = vol[t];
         }
         for (;t < 8; t++) {
             ctx->disk[i]->vol_label[t] = ' ';
         }
         ctx->disk[i]->vol_label[t] = '\0';
         free(vol);
     }
     if (file != NULL) {
         if (dasd_attach(ctx->disk[i], file, fmt) == 0) {
             log_warn("Unable to open file %s\n", file);
         }
         free(file);
     }
     return 1;
}


void
model2314_draw(struct _device *unit, void *rend, int u)
{
    struct _2844_context *ctx = (struct _2844_context *)unit->dev;
    SDL_Renderer *render = (SDL_Renderer *)rend;
    SDL_Rect     rect;
    SDL_Rect     rect2;
    SDL_Surface *text;
    SDL_Texture *txt;
    int          i, j, k;
    int          t1, t2;
    int          x = unit->rect[u].x;
    int          y = unit->rect[u].y;
    char         buf[100];


    if (ctx->disk[u] == NULL)
        return;

    rect2.x = 0;
    rect2.y = 0;
    rect2.w = unit->rect[u].w;
    rect2.h = unit->rect[u].h;
    rect.x = x;
    rect.y = y;
    rect.w = unit->rect[u].w;
    rect.h = unit->rect[u].h;
    SDL_RenderCopy(render, model2314_img, &rect2, &rect);
    sprintf(buf, "%1X%02X", ctx->chan, ctx->addr + u);
    text = TTF_RenderText_Solid(font14, buf, c_black);
    txt = SDL_CreateTextureFromSurface(render, text);
    SDL_FreeSurface(text);
    SDL_QueryTexture(txt, &t1, &t2, &rect2.w, &rect2.h);
    rect2.x = x + 52;
    rect2.y = y + 20;
    SDL_RenderCopy(render, txt, NULL, &rect2);

    sprintf(buf, "%1X%02X", ctx->chan, ctx->addr + u + 1);
    text = TTF_RenderText_Solid(font14, buf, c_black);
    txt = SDL_CreateTextureFromSurface(render, text);
    SDL_FreeSurface(text);
    SDL_QueryTexture(txt, &t1, &t2, &rect2.w, &rect2.h);
    rect2.x = x + 52;
    rect2.y = y + 80;
    SDL_RenderCopy(render, txt, NULL, &rect2);
    SDL_DestroyTexture(txt);
}

struct _2844_callback_args {
     struct _device *unit;
     Widget         file_text;
     Widget         volid_text;
     int            unit_num;
     int            init_dsk;
};

static void
model2844_update(void *args, int iarg)
{
    struct _2844_callback_args *data = (struct _2844_callback_args *)args;
    struct _device *unit = (struct _device *)data->unit;
    struct _2844_context *ctx = (struct _2844_context *)unit->dev;
    char    *file_name;
    char    *volid;
    int      u;

    file_name = get_textbuffer(data->file_text);
    volid = get_textbuffer(data->volid_text);
    switch (iarg) {
    case 0:  /* Start */
          if ((ctx->disk[data->unit_num]->status & ONLINE) == 0) {
              if (strcmp(ctx->disk[data->unit_num]->vol_label, volid) != 0) {
                  dasd_setvolid(ctx->disk[data->unit_num], volid);
              }
              if (ctx->disk[data->unit_num]->file_name == NULL ||
                  strcmp(ctx->disk[data->unit_num]->file_name, file_name) != 0) {
                  if (ctx->disk[data->unit_num]->file_name != NULL)
                      dasd_detach(ctx->disk[data->unit_num]);
                  dasd_attach(ctx->disk[data->unit_num], file_name, data->init_dsk);
              }
          }
          break;
    case 1:  /* Stop */
          dasd_detach(ctx->disk[data->unit_num]);
          break;
    }
}

static SDL_Color   col_green_on = { 0x7f, 0xc0, 0x86 };
static SDL_Color   col_green_off = { 0x0c, 0x2e, 0x30 };
static SDL_Color   col_red_on = { 0xd0, 0x08, 0x42 };
static SDL_Color   col_red_off = { 0xff, 0x00, 0x4a };

void *
model2314_control(struct _device *unit, int u, int x, int y)
{
    struct _popup  *popup;
    struct _2844_context *ctx = (struct _2844_context *)unit->dev;
    Panel  panel;
    SDL_Surface *text;
    struct _2844_callback_args *args;
    int    i, j;
    int    w, h;
    int    wx, hx;
    int    row;
    char   buffer[100];
    char   lab[2];

    if (TTF_SizeText(font10, "M", &wx, &hx) != 0) {
        return NULL;
    }
    if (TTF_SizeText(font14, "M", NULL, &h) != 0) {
        return NULL;
    int    row;
    }

    if ((args = (struct _2844_callback_args *)calloc(1, sizeof(struct _2844_callback_args))) == NULL) {
        return NULL;
    }

    args->unit = unit;
    args->unit_num = u;

    sprintf(buffer, "IBM2314 Dev 0x'%03X'", ctx->addr + u);
    if ((panel = create_window(buffer, 900, h*10, 1)) == NULL) {
        free(args);
        return NULL;
    }

    add_area(panel, 0, 0, 200, 800, &c_white);
    lab[0] = u + '0';
    lab[1] = '\0';

    add_indicator(panel, 20, 20, 2 * hx, 10 * wx, lab, NULL,
                     &ctx->disk[u]->status, 7, font10, &c_white,
                     &col_green_on, &col_green_off);
    add_indicator(panel, 20 + (12 * wx), 20, 2 * hx, 10 * wx, "SELECT", "LOCK",
                     &ctx->disk[u]->status, 6, font10, &c_white,
                     &col_green_on, &col_green_off);
    add_button_callback(panel, 20 + ((12 * wx) * 2), 20, 2 * hx, 10 *wx,
               "START", NULL, &model2844_update, args, 0,
               font10, &c_black, &col_green_on);
    add_button_callback(panel, 20 + ((12 * wx) * 3), 20, 2 * hx, 10 *wx,
               "STOP", NULL, &model2844_update, args, 1,
               font10, &c_black, &col_green_on);
    row = 20;

    add_label(panel, 25 + (12 * wx) * 4, row, "Disk:", font14, &c_black);
    args->file_text = add_textinput(panel, 25 + (12*wx) * 5, row, h+2, 50*wx,
                    ctx->disk[u]->file_name);

    row += 20;

    add_label(panel, 25 + (12 * wx) * 4, row, "Vol ID:", font14, &c_black);
    args->volid_text = add_textinput(panel, 25 + (12*wx) * 5, row, h+2, 12*wx,
                    ctx->disk[u]->vol_label);

    row += h+10;
    add_label(panel, 25 + (12 * wx) * 4, row, "Format:", font14, &c_black);
    add_checkbox(panel, 30 + (12 * wx) * 5, row, h, wx, NULL,
                       &args->init_dsk, 0, 0, font10, &c_black, &c_white);
    return panel;
}

