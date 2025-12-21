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
#include "logger.h"
#include "event.h"
#include "device.h"
#include "xlat.h"
#include "model2844.h"

#include "model2314.xpm"


SDL_Texture *model2314_img = NULL;

void
model2314_draw(struct _device *unit, void *rend)
{
    struct _2844_context *ctx = (struct _2844_context *)unit->dev;
    SDL_Renderer *render = (SDL_Renderer *)rend;
    SDL_Rect     rect;
    SDL_Rect     rect2;
    SDL_Surface *text;
    SDL_Texture *txt;
    int          i, j, k;
    int          t1, t2;
    int          x;
    int          y;
    char         buf[100];

    if (model2314_img == NULL) {
        SDL_Surface *text;

        text = IMG_ReadXPMFromArray(model2314_xpm);
        model2314_img = SDL_CreateTextureFromSurface(render, text);
        SDL_SetTextureBlendMode(model2314_img, SDL_BLENDMODE_BLEND);
        SDL_FreeSurface(text);
    }

    for (i = 0; i < unit->n_units; i++) {
        x = unit->rect[i].x;
        y = unit->rect[i].y;

        if (ctx->disk[i] == NULL)
            continue;

        rect2.x = 0;
        rect2.y = 0;
        rect2.w = unit->rect[i].w;
        rect2.h = unit->rect[i].h;
        rect.x = x;
        rect.y = y;
        rect.w = unit->rect[i].w;
        rect.h = unit->rect[i].h;
        SDL_RenderCopy(render, model2314_img, &rect2, &rect);
        sprintf(buf, "%1X%02X", ctx->chan, ctx->addr + i);
        text = TTF_RenderText_Solid(font14, buf, c1);
        txt = SDL_CreateTextureFromSurface(render, text);
        SDL_FreeSurface(text);
        SDL_QueryTexture(txt, &t1, &t2, &rect2.w, &rect2.h);
        rect2.x = x + 52;
        rect2.y = y + 20;
        SDL_RenderCopy(render, txt, NULL, &rect2);
        SDL_DestroyTexture(txt);
    }
}

static void model2314_update(struct _popup *popup, void *device, int index)
{
    struct _device *unit = (struct _device *)device;
    struct _2844_context *ctx = (struct _2844_context *)unit->dev;

fprintf (stderr, "Disk key %d\n", index);
    switch (index) {
    case 0:  /* Start */
          if ((ctx->disk[popup->unit_num]->status & ONLINE) == 0) {
              if (strcmp(ctx->disk[popup->unit_num]->vol_label, popup->text[1].text) != 0) {
                  dasd_setvolid(ctx->disk[popup->unit_num], popup->text[1].text);
              }
              if (ctx->disk[popup->unit_num]->file_name == NULL ||
                  strcmp(ctx->disk[popup->unit_num]->file_name, popup->text[0].text) != 0) {
                  if (ctx->disk[popup->unit_num]->file_name != NULL)
                      dasd_detach(ctx->disk[popup->unit_num]);
                  dasd_attach(ctx->disk[popup->unit_num], popup->text[0].text, ctx->disk[popup->unit_num]->fmt);
              }
          }
          break;
    case 1:  /* Stop */
          dasd_detach(ctx->disk[popup->unit_num]);
          break;
    }
}

extern int textpos(struct _text *text, int pos);

static SDL_Color   col_green_on = { 0x7f, 0xc0, 0x86 };
static SDL_Color   col_green_off = { 0x0c, 0x2e, 0x30 };
static SDL_Color   col_red_on = { 0xd0, 0x08, 0x42 };
static SDL_Color   col_red_off = { 0xff, 0x00, 0x4a };

static char *format_mode[] = {"No", "Yes", NULL };

struct _popup *
model2314_control(struct _device *unit, int hd, int wd, int u)
{
    struct _popup  *popup;
    struct _2844_context *ctx = (struct _2844_context *)unit->dev;
    SDL_Surface *text;
    int    i, j;
    int    w, h;
    char   buffer[100];
    char   lab[2];

    if ((popup = (struct _popup *)calloc(1, sizeof(struct _popup))) == NULL)
        return popup;
    sprintf(buffer, "IBM2314 Dev 0x'%03X'", ctx->addr + u);
    popup->screen = SDL_CreateWindow(buffer, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        800, 200, SDL_WINDOW_RESIZABLE );
    popup->render = SDL_CreateRenderer( popup->screen, -1, SDL_RENDERER_ACCELERATED);
    popup->device = unit;
    popup->unit_num = u;
    popup->areas[popup->area_ptr].rect.x = 0;
    popup->areas[popup->area_ptr].rect.y = 0;
    popup->areas[popup->area_ptr].rect.h = 200;
    popup->areas[popup->area_ptr].rect.w = 800;
    popup->areas[popup->area_ptr].c = &c;
    popup->area_ptr++;
    lab[0] = u + '0';
    lab[1] = '\0';
    popup->ind[popup->ind_ptr].lab = NULL;
    popup->ind[popup->ind_ptr].c[0] = &col_green_off;
    popup->ind[popup->ind_ptr].c[1] = &col_green_on;
    popup->ind[popup->ind_ptr].ct = &c;
    text = TTF_RenderText_Solid(font1, lab, c);
    popup->ind[popup->ind_ptr].top = SDL_CreateTextureFromSurface( popup->render, text);
    popup->ind[popup->ind_ptr].top_len = 1;
    SDL_FreeSurface(text);
    popup->ind[popup->ind_ptr].rect.x = 20;
    popup->ind[popup->ind_ptr].rect.y = 20;
    popup->ind[popup->ind_ptr].rect.h = 2 * hd;
    popup->ind[popup->ind_ptr].rect.w = 10 * wd;
    popup->ind[popup->ind_ptr].value = &ctx->disk[u]->status;
    popup->ind[popup->ind_ptr].shift = 5;
    popup->ind_ptr++;
    popup->ind[popup->ind_ptr].lab = "SELECT";
    popup->ind[popup->ind_ptr].c[0] = &col_green_off;
    popup->ind[popup->ind_ptr].c[1] = &col_green_on;
    popup->ind[popup->ind_ptr].ct = &c;
    text = TTF_RenderText_Solid(font1, "SELECT", c);
    popup->ind[popup->ind_ptr].top = SDL_CreateTextureFromSurface( popup->render, text);
    popup->ind[popup->ind_ptr].top_len = 6;
    SDL_FreeSurface(text);
    text = TTF_RenderText_Solid(font1, "LOCK", c);
    popup->ind[popup->ind_ptr].bot = SDL_CreateTextureFromSurface( popup->render, text);
    popup->ind[popup->ind_ptr].bot_len = 4;
    SDL_FreeSurface(text);
    popup->ind[popup->ind_ptr].rect.x = 20 + (12 * wd);
    popup->ind[popup->ind_ptr].rect.y = 20;
    popup->ind[popup->ind_ptr].rect.h = 2 * hd;
    popup->ind[popup->ind_ptr].rect.w = 10 * wd;
    popup->ind[popup->ind_ptr].value = &ctx->disk[u]->status;
    popup->ind[popup->ind_ptr].shift = 6;
    popup->ind_ptr++;
    popup->sws[popup->sws_ptr].lab = "START";
    popup->sws[popup->sws_ptr].c[0] = &col_green_on;
    text = TTF_RenderText_Solid(font1, "START", c);
    popup->sws[popup->sws_ptr].top = SDL_CreateTextureFromSurface( popup->render, text);
    popup->sws[popup->sws_ptr].top_len = 5;
    SDL_FreeSurface(text);
    popup->sws[popup->sws_ptr].rect.x = 20 + ((12 *wd) * 2);
    popup->sws[popup->sws_ptr].rect.y = 20;
    popup->sws[popup->sws_ptr].rect.h = 2 * hd;
    popup->sws[popup->sws_ptr].rect.w = 10 * wd;
    popup->sws_ptr++;
    popup->sws[popup->sws_ptr].lab = "STOP";
    popup->sws[popup->sws_ptr].c[0] = &col_green_on;
    text = TTF_RenderText_Solid(font1, "STOP", c);
    popup->sws[popup->sws_ptr].top = SDL_CreateTextureFromSurface( popup->render, text);
    popup->sws[popup->sws_ptr].top_len = 5;
    SDL_FreeSurface(text);
    popup->sws[popup->sws_ptr].rect.x = 20 + ((12 *wd) * 3);
    popup->sws[popup->sws_ptr].rect.y = 20;
    popup->sws[popup->sws_ptr].rect.h = 2 * hd;
    popup->sws[popup->sws_ptr].rect.w = 10 * wd;
    popup->sws_ptr++;

    text = TTF_RenderText_Solid(font14, "Disk: ", c1);

    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 25 + (12 * wd) * 4;
    popup->ctl_label[popup->ctl_ptr].rect.y = 20;
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;
    popup->text[popup->txt_ptr].rect.x = 25 + (12 * wd) * 5;
    popup->text[popup->txt_ptr].rect.y = 20;
    popup->text[popup->txt_ptr].rect.w = 45 * wd;
    popup->text[popup->txt_ptr].rect.h = h + 5;
    if (ctx->disk[u]->file_name != NULL)
        strncpy(popup->text[popup->txt_ptr].text, ctx->disk[u]->file_name, 256);
    else
        popup->text[popup->txt_ptr].text[0] = '\0';
    popup->text[popup->txt_ptr].len = strlen(popup->text[popup->txt_ptr].text);
    popup->text[popup->txt_ptr].pos = popup->text[popup->txt_ptr].len;
    popup->text[popup->txt_ptr].cpos = textpos(&popup->text[popup->txt_ptr],
                                        popup->text[popup->txt_ptr].pos);
    popup->txt_ptr++;

    text = TTF_RenderText_Solid(font14, "Vol Id: ", c1);

    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 25 + (12 * wd) * 4;
    popup->ctl_label[popup->ctl_ptr].rect.y = 40;
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;
    popup->text[popup->txt_ptr].rect.x = 25 + (12 * wd) * 5;
    popup->text[popup->txt_ptr].rect.y = 40;
    popup->text[popup->txt_ptr].rect.w = 45 * wd;
    popup->text[popup->txt_ptr].rect.h = h + 5;
    if (ctx->disk[u]->vol_label[0] != '\0')
        strncpy(popup->text[popup->txt_ptr].text, ctx->disk[u]->vol_label, 256);
    else
        popup->text[popup->txt_ptr].text[0] = '\0';
    popup->text[popup->txt_ptr].len = strlen(popup->text[popup->txt_ptr].text);
    popup->text[popup->txt_ptr].pos = popup->text[popup->txt_ptr].len;
    popup->text[popup->txt_ptr].cpos = textpos(&popup->text[popup->txt_ptr],
                                        popup->text[popup->txt_ptr].pos);
    popup->txt_ptr++;
    text = TTF_RenderText_Solid(font14, "Format: ", c1);

    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 25 + (12 * wd) * 4;
    popup->ctl_label[popup->ctl_ptr].rect.y = 60;
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;
    popup->combo[popup->cmb_ptr].rect.x = 25 + (12 * wd) * 5;
    popup->combo[popup->cmb_ptr].rect.y = 60;
    popup->combo[popup->cmb_ptr].rect.w = 16 * wd;
    popup->combo[popup->cmb_ptr].rect.h = h;
    popup->combo[popup->cmb_ptr].urect.x = popup->combo[popup->cmb_ptr].rect.x;
    popup->combo[popup->cmb_ptr].urect.y = popup->combo[popup->cmb_ptr].rect.y;
    popup->combo[popup->cmb_ptr].urect.w = 2 * wd;
    popup->combo[popup->cmb_ptr].urect.h = h;
    popup->combo[popup->cmb_ptr].drect.x = popup->combo[popup->cmb_ptr].rect.x + (14 * wd) - 1;
    popup->combo[popup->cmb_ptr].drect.y = popup->combo[popup->cmb_ptr].rect.y;
    popup->combo[popup->cmb_ptr].drect.w = 2 * wd;
    popup->combo[popup->cmb_ptr].drect.h = h;
    for (i = 0; format_mode[i] != NULL; i++) {
        text = TTF_RenderText_Solid(font14, format_mode[i], c1);
        popup->combo[popup->cmb_ptr].label[i] = SDL_CreateTextureFromSurface( popup->render,
                                      text);
        SDL_QueryTexture(popup->combo[popup->cmb_ptr].label[i], NULL, NULL,
             &popup->combo[popup->cmb_ptr].lw[i], &popup->combo[popup->cmb_ptr].lh[i]);
    }
    popup->combo[popup->cmb_ptr].num = 0;
    popup->combo[popup->cmb_ptr].value = &ctx->disk[u]->fmt;
    popup->combo[popup->cmb_ptr].max = i - 1;
    popup->cmb_ptr++;

    popup->update = &model2314_update;
    return popup;
}

DEV_LIST_STRUCT(2314, UNIT_TYPE, 0);
DEV_LIST_STRUCT(2844, CTRL_TYPE, 0);
