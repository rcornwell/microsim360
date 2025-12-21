/*
 * microsim360 - Model 2415 tape controller.
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
#include "tape.h"
#include "model2415.h"

#include "model2415.xpm"
#include "tape_images.xpm"

SDL_Texture *model2415_img = NULL;
SDL_Texture *tape_images_img = NULL;
static int supply_color[8] = {1, 1, 1, 1, 1, 1, 1, 1};
static int takeup_color[8] = {2, 2, 2, 2, 2, 2, 2, 2};
static int supply_label[8] = {1, 1, 1, 1, 1, 1, 1, 1};
static int takeup_label[8] = {0, 0, 0, 0, 0, 0, 0, 0};

void
model2415_draw(struct _device *unit, void *rend)
{
    struct _2415_context *ctx = (struct _2415_context *)unit->dev;
    SDL_Renderer *render = (SDL_Renderer *)rend;
    struct _tape_image   *reel;
    SDL_Rect     rect;
    SDL_Rect     rect2;
    SDL_Surface *text;
    SDL_Texture *txt;
    int          i, j, k;
    int          t1, t2;
    int          x;
    int          y;
    char         buf[100];
    float        p;

#if 0
    if (model2415_img == NULL) {
        SDL_Surface *text;

        text = IMG_ReadXPMFromArray(model2415_xpm);
        model2415_img = SDL_CreateTextureFromSurface(render, text);
        SDL_SetTextureBlendMode(model2415_img, SDL_BLENDMODE_BLEND);
        SDL_FreeSurface(text);
        text = IMG_ReadXPMFromArray(tape_images_xpm);
        tape_images_img = SDL_CreateTextureFromSurface(render, text);
        SDL_SetTextureBlendMode(tape_images_img, SDL_BLENDMODE_BLEND);
        SDL_FreeSurface(text);
    }
#endif

    for (i = 0; i < unit->n_units; i++) {
        x = unit->rect[i].x;
        y = unit->rect[i].y;

        if (ctx->tape[i]->file_name != NULL) {
            rect2.x = 0;
            rect2.y = 0;
            rect2.w = unit->rect[i].w;
            rect2.h = unit->rect[i].h;
            rect.x = x;
            rect.y = y;
            rect.w = unit->rect[i].w;
            rect.h = unit->rect[i].h;
            SDL_RenderCopy(render, model2415_img, &rect2, &rect);
            sprintf(buf, "%1X%02X", ctx->chan, ctx->addr + i);
            text = TTF_RenderText_Solid(font14, buf, c1);
            txt = SDL_CreateTextureFromSurface(render, text);
            SDL_FreeSurface(text);
            SDL_QueryTexture(txt, &t1, &t2, &rect2.w, &rect2.h);
            rect2.x = x + 52;
            rect2.y = y + 20;
            SDL_RenderCopy(render, txt, NULL, &rect2);
            SDL_DestroyTexture(txt);

            /* Do indicators */
            rect2.x = x + 85;
            rect2.y = y + 14;
            rect2.w = 9;
            rect2.h = 7;
            rect.x = 17;
            rect.y = 230;
            rect.w = 9;
            rect.h = 7;
            /* Draw select */
            if (tape_is_selected(ctx->tape[i])) {
                SDL_RenderCopy(render, model2415_img, &rect, &rect2);
            }
            rect.x += rect.w;
            rect2.x += rect.w;
            /* Draw ready */
            if (tape_ready(ctx->tape[i])) {
                SDL_RenderCopy(render, model2415_img, &rect, &rect2);
            }
            rect.x += rect.w;
            rect2.x += rect.w;
            /* Draw file protect */
            if (tape_ring(ctx->tape[i]) == 0) {
               SDL_RenderCopy(render, model2415_img, &rect, &rect2);
            }
            /* Draw supply reel */
            reel = tape_supply_image(ctx->tape[i], &j);
            rect2.x = x + 34;
            rect2.y = y + 131;
            rect2.w = 69;
            rect2.h = 69;
            rect.x = reel->x+4;
            rect.y = reel->y+4;
            rect.w = 69;
            rect.h = 69;
            SDL_RenderCopy(render, tape_images_img, &rect, &rect2);
            /* Draw tape to vacuum column */
            SDL_SetRenderDrawColor(render, 0x37, 0x37, 0x37, 255);
            SDL_RenderDrawLine(render, x+32, y+127,
                    x + (69 - reel->radius), y + 164 + (reel->radius / 2));
            SDL_RenderDrawLine(render, x+32, y+99, x + 119, y + 77);
            SDL_RenderDrawLine(render, x+177, y+99, x + 167, y + 85);
            /* Overlay reel */
            j = 35 - j;
            switch (supply_color[i]) {
            case 0: rect.x = (75 * 15) + 4; break;
            case 1: rect.x = (75 * 3) + 4; break;
            case 2: rect.x = (75 * 7) + 4; break;
            }
            if (j > 17) {
                rect.x += 75;
                rect.y = (75 * (j - 18)) + 4;
            } else {
                rect.y = (75 * (j)) + 4;
            }
            SDL_RenderCopy(render, tape_images_img, &rect, &rect2);
            if (supply_label[i]) {
                if (rect.x > 1125)
                    rect.x -= (75 * 2);
                else
                    rect.x += (75 * 2);
                SDL_RenderCopy(render, tape_images_img, &rect, &rect2);
            }
            /* Draw take up reel */
            reel = tape_takeup_image(ctx->tape[i], &k);
            rect2.x = x + 107;
            rect.x = reel->x+4;
            rect.y = reel->y+4;
            SDL_RenderCopy(render, tape_images_img, &rect, &rect2);
            switch (takeup_color[i]) {
            case 0: rect.x = (75 * 15) + 4; break;
            case 1: rect.x = (75 * 3) + 4; break;
            case 2: rect.x = (75 * 7) + 4; break;
            }
            if (k > 17) {
                rect.x += 75;
                rect.y = (75 * (k - 18) + 4);
            } else {
                rect.y = (75 * (k)) + 4;
            }
            SDL_RenderDrawLine(render, x + 177, y + 127,
                 x + (141 + reel->radius), y + 164 + (reel->radius / 2));
            SDL_RenderCopy(render, tape_images_img, &rect, &rect2);
            if (takeup_label[i]) {
                if (rect.x > 1125)
                    rect.x -= (75 * 2);
                else
                    rect.x += (75 * 2);
                SDL_RenderCopy(render, tape_images_img, &rect, &rect2);
            }
        } else {
            rect2.x = 250;
            rect2.y = 0;
            rect2.w = unit->rect[i].w;
            rect2.h = unit->rect[i].h;
            rect.x = x;
            rect.y = y;
            rect.w = unit->rect[i].w;
            rect.h = unit->rect[i].h;
            SDL_RenderCopy(render, model2415_img, &rect2, &rect);
            sprintf(buf, "%1X%02X", ctx->chan, ctx->addr + i);
            text = TTF_RenderText_Solid(font14, buf, c1);
            txt = SDL_CreateTextureFromSurface(render, text);
            SDL_FreeSurface(text);
            SDL_QueryTexture(txt, &t1, &t2, &rect2.w, &rect2.h);
            rect2.x = x + 52;
            rect2.y = y + 20;
            SDL_RenderCopy(render, txt, NULL, &rect2);
            SDL_DestroyTexture(txt);
            /* Draw supply reel */
            rect2.x = x + 34;
            rect2.y = y + 131;
            rect2.w = 69;
            rect2.h = 69;
            rect.x = 4;
            rect.y = 4;
            rect.w = 69;
            rect.h = 69;
            SDL_RenderCopy(render, tape_images_img, &rect, &rect2);
            /* Draw take up reel */
            rect2.x = x + 107;
            SDL_RenderCopy(render, tape_images_img, &rect, &rect2);
            switch (takeup_color[i]) {
            case 0: rect.x = (75 * 15) + 4; break;
            case 1: rect.x = (75 * 3) + 4; break;
            case 2: rect.x = (75 * 7) + 4; break;
            }
            rect.y = 4;
            SDL_RenderCopy(render, tape_images_img, &rect, &rect2);
            if (takeup_label[i]) {
                if (rect.x > 1125)
                    rect.x -= (75 * 2);
                else
                    rect.x += (75 * 2);
                SDL_RenderCopy(render, tape_images_img, &rect, &rect2);
            }
            /* Draw empty hub */
            rect2.x = x + 34 + 15;
            rect2.y = y + 131 + 15;
            rect2.w = 40;
            rect2.h = 40;
            rect.x = 4 + 15;
            rect.y = 4 + 15;
            rect.w = 40;
            rect.h = 40;
            rect.x = (75 * 15) + 4 + 15;
            rect.y = 4 + 15;
            rect.w = 40;
            rect.h = 40;
            SDL_RenderCopy(render, tape_images_img, &rect, &rect2);
        }
    }
}

static void model2415_update(struct _popup *popup, void *device, int index)
{
    struct _device *unit = (struct _device *)device;
    struct _2415_context *ctx = (struct _2415_context *)unit->dev;

fprintf (stderr, "Tape key %d\n", index);
    switch (index) {
    case 0:  /* Load Rewind */
          if ((ctx->tape[popup->unit_num]->format & ONLINE) == 0) {
              if (ctx->tape[popup->unit_num]->file_name == NULL ||
                  strcmp(ctx->tape[popup->unit_num]->file_name, popup->text[0].text) != 0) {
                  tape_detach(ctx->tape[popup->unit_num]);
                  tape_attach(ctx->tape[popup->unit_num],
                                  popup->text[0].text, popup->temp[0],
                                  popup->temp[3], popup->temp[1]);
              }
              tape_start_rewind(ctx->tape[popup->unit_num]);
              if (ctx->rew_delay == 0)
                  ctx->rew_delay = REWIND_DELAY;
              ctx->rew_flags |= 1 << popup->unit_num;
              add_event(unit, model2415_rewind_callback, REWIND_DELAY,
                        (void *)ctx->tape[popup->unit_num], popup->unit_num);
          }
          break;
    case 1:  /* Start */
          if ((ctx->tape[popup->unit_num]->format & ONLINE) == 0 &&
              (ctx->tape[popup->unit_num]->fd >= 0)) {
              ctx->tape[popup->unit_num]->format |= ONLINE;
              ctx->rdy_flags |= 1 << popup->unit_num;
          }
          break;
    case 2:  /* Unload */
          if ((ctx->tape[popup->unit_num]->format & ONLINE) == 0) {
              tape_start_rewind(ctx->tape[popup->unit_num]);
              if (ctx->rew_delay == 0)
                  ctx->rew_delay = REWIND_DELAY;
              ctx->rew_flags |= 1 << popup->unit_num;
              ctx->run_flags |= 1 << popup->unit_num;
              add_event(unit, model2415_rewind_callback, REWIND_DELAY,
                        (void *)ctx->tape[popup->unit_num], popup->unit_num);
          }
          break;
    case 3:  /* Reset */
          ctx->tape[popup->unit_num]->format &= ~ONLINE;
          break;

    case 4:  /* end */
          ctx->tape[popup->unit_num]->pos_frame = max_tape_length - 1;
          break;
    }
}

extern int textpos(struct _text *text, int pos);

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
     {"SELECT", NULL,    1, 0, 0, {0, 0, 0}, {0x96, 0x8f, 0x85}, {0xfd, 0xfd, 0xfd}},  /* White */
     {"READY", NULL,     1, 1, 0, {0xff, 0xff, 0xff}, {0x7f,0xc0, 0x86}, {0x0c, 0x2e, 0x30}}, /* Green */
     {"FILE", "PROTECT", 1, 2, 0, {0xff, 0xff, 0xff}, {0xd0, 0x08, 0x42}, {0xff, 0x00, 0x4a}},  /* Red */
     {"TAPE", "INDICATOR", 1, 3, 0, {0, 0, 0}, {0xff, 0xfd, 0x5e}, {0xdd, 0xdc, 0x8a}}, /* White */
     {"LOAD", "REWIND",   0, 0, 1, {0xff, 0xff, 0xff}, {0x0a, 0x52, 0x9a}, {0, 0, 0}}, /* Blue */
     {"START", NULL,      0, 1, 1, {0xff, 0xff, 0xff}, {0x0c, 0x2e, 0x30}, {0, 0, 0}}, /* Green */
     {"UNLOAD", "REWIND", 0, 2, 1, {0xff, 0xff, 0xff}, {0x0a, 0x52, 0x9a}, {0, 0, 0}}, /* Blue */
     {"RESET", NULL,      0, 3, 1, {0xff, 0xff, 0xff}, {0xc8, 0x3a, 0x30}, {0, 0, 0}}, /* Blue */
     {"EOM", NULL,         0, 0, 4, {0xff, 0xff, 0xff}, {0x0a, 0x052, 0x9a}, {0, 0, 0,}},
     { NULL, NULL, 0}
};

#if 0
static char *format_type[] = { "SIMH", "E11", "P7B", NULL };
static char *density_type[] = { "1600", "800", NULL };
static char *tracks[] = { "9 track", "7 track", NULL };
static char *ring_mode[] = { "Ring", "No Ring", NULL };
static char *reel_color[] = { "Clear", "Red", "Blue" };
static char *label_mode[] = { "No", "Yes", NULL };
#endif

//static SDL_Renderer *render;
struct _popup *
model2415_control(struct _device *unit, int hd, int wd, int u)
{
    struct _popup  *popup;
    struct _2415_context *ctx = (struct _2415_context *)unit->dev;
    SDL_Surface *text;
    int    i, j;
    int    w, h;
    char   buffer[100];

    if ((popup = (struct _popup *)calloc(1, sizeof(struct _popup))) == NULL)
        return popup;
    sprintf(buffer, "IBM2415 Dev 0x'%03X'", ctx->addr + u);
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
    for (i = 0; labels[i].top != NULL; i++) {
        if (labels[i].ind) {
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
            popup->ind[popup->ind_ptr].rect.y = 20 + ((3 *hd) * labels[i].y);
            popup->ind[popup->ind_ptr].rect.h = 2 * hd;
            popup->ind[popup->ind_ptr].rect.w = 10 * wd;
            popup->ind_ptr++;
        } else {
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
            popup->sws[popup->sws_ptr].rect.y = 20 + ((3 *hd) * labels[i].y);
            popup->sws[popup->sws_ptr].rect.h = 2 * hd;
            popup->sws[popup->sws_ptr].rect.w = 10 * wd;
            popup->sws_ptr++;
        }
    }

    popup->ind[0].value = &ctx->tape[u]->format;  /* Select */
    popup->ind[0].shift = 8;
    popup->ind[1].value = &ctx->tape[u]->format;  /* Ready */
    popup->ind[1].shift = 9;
    popup->ind[2].value = &ctx->tape[u]->format;  /* File protect */
    popup->ind[2].shift = 2;
    popup->ind[3].value = &ctx->tape[u]->format;  /* Tape indicator */
    popup->ind[3].shift = 3;

    text = TTF_RenderText_Solid(font14, "Tape: ", c1);

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
    if (ctx->tape[u]->file_name != NULL)
        strncpy(popup->text[popup->txt_ptr].text, ctx->tape[u]->file_name, 256);
    else
        popup->text[popup->txt_ptr].text[0] = '\0';
    popup->text[popup->txt_ptr].len = strlen(popup->text[popup->txt_ptr].text);
    popup->text[popup->txt_ptr].pos = popup->text[popup->txt_ptr].len;
    popup->text[popup->txt_ptr].cpos = textpos(&popup->text[popup->txt_ptr],
                                        popup->text[popup->txt_ptr].pos);
    popup->txt_ptr++;

    text = TTF_RenderText_Solid(font14, "Type: ", c1);

    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 25 + (12 * wd) * 4;
    popup->ctl_label[popup->ctl_ptr].rect.y = 20 + (2 * hd);
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;
    popup->combo[popup->cmb_ptr].rect.x = 25 + (12 * wd) * 5;
    popup->combo[popup->cmb_ptr].rect.y = 20 + (2 * hd);
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
    for (i = 0; format_type[i] != NULL; i++) {
        text = TTF_RenderText_Solid(font14, format_type[i], c1);
        popup->combo[popup->cmb_ptr].label[i] = SDL_CreateTextureFromSurface( popup->render,
                                      text);
        SDL_QueryTexture(popup->combo[popup->cmb_ptr].label[i], NULL, NULL,
             &popup->combo[popup->cmb_ptr].lw[i], &popup->combo[popup->cmb_ptr].lh[i]);
    }
    popup->temp[0] = ctx->tape[u]->format & TAPE_FMT;
    popup->combo[popup->cmb_ptr].num = popup->temp[0];
    popup->combo[popup->cmb_ptr].value = &popup->temp[0];
    popup->combo[popup->cmb_ptr].max = i - 1;
    popup->cmb_ptr++;

    text = TTF_RenderText_Solid(font14, "Density: ", c1);

    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 25 + (12 * wd) * 4;
    popup->ctl_label[popup->ctl_ptr].rect.y = 20 + (4 * hd);
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;
    popup->combo[popup->cmb_ptr].rect.x = 25 + (12 * wd) * 5;
    popup->combo[popup->cmb_ptr].rect.y = 20 + (4 * hd);
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
    for (i = 0; density_type[i] != NULL; i++) {
        text = TTF_RenderText_Solid(font14, density_type[i], c1);
        popup->combo[popup->cmb_ptr].label[i] = SDL_CreateTextureFromSurface( popup->render,
                                      text);
        SDL_QueryTexture(popup->combo[popup->cmb_ptr].label[i], NULL, NULL,
             &popup->combo[popup->cmb_ptr].lw[i], &popup->combo[popup->cmb_ptr].lh[i]);
    }
    popup->temp[1] = (ctx->tape[u]->format & DEN_MASK) == DEN_800;
    popup->combo[popup->cmb_ptr].num = popup->temp[1];
    popup->combo[popup->cmb_ptr].value = &popup->temp[1];
    popup->combo[popup->cmb_ptr].max = i - 1;
    popup->cmb_ptr++;

    text = TTF_RenderText_Solid(font14, "Tracks: ", c1);

    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 25 + (12 * wd) * 4;
    popup->ctl_label[popup->ctl_ptr].rect.y = 20 + (6 * hd);
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;
    popup->combo[popup->cmb_ptr].rect.x = 25 + (12 * wd) * 5;
    popup->combo[popup->cmb_ptr].rect.y = 20 + (6 * hd);
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
    for (i = 0; tracks[i] != NULL; i++) {
        text = TTF_RenderText_Solid(font14, tracks[i], c1);
        popup->combo[popup->cmb_ptr].label[i] = SDL_CreateTextureFromSurface( popup->render,
                                      text);
        SDL_QueryTexture(popup->combo[popup->cmb_ptr].label[i], NULL, NULL,
             &popup->combo[popup->cmb_ptr].lw[i], &popup->combo[popup->cmb_ptr].lh[i]);
    }
    popup->temp[2] = (ctx->tape[u]->format & TRACK9) == 0;
    popup->combo[popup->cmb_ptr].num = popup->temp[2];
    popup->combo[popup->cmb_ptr].value = &popup->temp[2];
    popup->combo[popup->cmb_ptr].max = i - 1;
    popup->cmb_ptr++;

    text = TTF_RenderText_Solid(font14, "Write: ", c1);

    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 25 + (12 * wd) * 4;
    popup->ctl_label[popup->ctl_ptr].rect.y = 20 + (8 * hd);
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;
    popup->combo[popup->cmb_ptr].rect.x = 25 + (12 * wd) * 5;
    popup->combo[popup->cmb_ptr].rect.y = 20 + (8 * hd);
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
    for (i = 0; ring_mode[i] != NULL; i++) {
        text = TTF_RenderText_Solid(font14, ring_mode[i], c1);
        popup->combo[popup->cmb_ptr].label[i] = SDL_CreateTextureFromSurface( popup->render,
                                      text);
        SDL_QueryTexture(popup->combo[popup->cmb_ptr].label[i], NULL, NULL,
             &popup->combo[popup->cmb_ptr].lw[i], &popup->combo[popup->cmb_ptr].lh[i]);
    }
    popup->temp[3] = (ctx->tape[u]->format & WRITE_RING) == 0;
    popup->combo[popup->cmb_ptr].num = popup->temp[3];
    popup->combo[popup->cmb_ptr].value = &popup->temp[3];
    popup->combo[popup->cmb_ptr].max = i - 1;
    popup->cmb_ptr++;
    text = TTF_RenderText_Solid(font14, "Color: ", c1);

    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 25 + (12 * wd) * 4;
    popup->ctl_label[popup->ctl_ptr].rect.y = 20 + (10 * hd);
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;
    popup->combo[popup->cmb_ptr].rect.x = 25 + (12 * wd) * 5;
    popup->combo[popup->cmb_ptr].rect.y = 20 + (10 * hd);
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
    for (i = 0; reel_color[i] != NULL; i++) {
        text = TTF_RenderText_Solid(font14, reel_color[i], c1);
        popup->combo[popup->cmb_ptr].label[i] = SDL_CreateTextureFromSurface( popup->render,
                                      text);
        SDL_QueryTexture(popup->combo[popup->cmb_ptr].label[i], NULL, NULL,
             &popup->combo[popup->cmb_ptr].lw[i], &popup->combo[popup->cmb_ptr].lh[i]);
    }
    popup->combo[popup->cmb_ptr].num = supply_color[u];
    popup->combo[popup->cmb_ptr].value = &supply_color[u];
    popup->combo[popup->cmb_ptr].max = i - 1;
    popup->cmb_ptr++;
    text = TTF_RenderText_Solid(font14, "Label: ", c1);

    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 25 + (12 * wd) * 4;
    popup->ctl_label[popup->ctl_ptr].rect.y = 20 + (12 * hd);
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;
    popup->combo[popup->cmb_ptr].rect.x = 25 + (12 * wd) * 5;
    popup->combo[popup->cmb_ptr].rect.y = 20 + (12 * hd);
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
    for (i = 0; label_mode[i] != NULL; i++) {
        text = TTF_RenderText_Solid(font14, label_mode[i], c1);
        popup->combo[popup->cmb_ptr].label[i] = SDL_CreateTextureFromSurface( popup->render,
                                      text);
        SDL_QueryTexture(popup->combo[popup->cmb_ptr].label[i], NULL, NULL,
             &popup->combo[popup->cmb_ptr].lw[i], &popup->combo[popup->cmb_ptr].lh[i]);
    }
    popup->combo[popup->cmb_ptr].num = supply_label[u];
    popup->combo[popup->cmb_ptr].value = &supply_label[u];
    popup->combo[popup->cmb_ptr].max = i - 1;
    popup->cmb_ptr++;
    text = TTF_RenderText_Solid(font14, "Take Up", c1);

    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 25 + (20 * wd) * 4;
    popup->ctl_label[popup->ctl_ptr].rect.y = 20 + (8 * hd);
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;
    text = TTF_RenderText_Solid(font14, "Color: ", c1);

    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 25 + (20 * wd) * 4;
    popup->ctl_label[popup->ctl_ptr].rect.y = 20 + (10 * hd);
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;
    popup->combo[popup->cmb_ptr].rect.x = 25 + (20 * wd) * 5;
    popup->combo[popup->cmb_ptr].rect.y = 20 + (10 * hd);
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
    for (i = 0; reel_color[i] != NULL; i++) {
        text = TTF_RenderText_Solid(font14, reel_color[i], c1);
        popup->combo[popup->cmb_ptr].label[i] = SDL_CreateTextureFromSurface( popup->render,
                                      text);
        SDL_QueryTexture(popup->combo[popup->cmb_ptr].label[i], NULL, NULL,
             &popup->combo[popup->cmb_ptr].lw[i], &popup->combo[popup->cmb_ptr].lh[i]);
    }
    popup->combo[popup->cmb_ptr].num = takeup_color[u];
    popup->combo[popup->cmb_ptr].value = &takeup_color[u];
    popup->combo[popup->cmb_ptr].max = i - 1;
    popup->cmb_ptr++;
    text = TTF_RenderText_Solid(font14, "Label: ", c1);

    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 25 + (20 * wd) * 4;
    popup->ctl_label[popup->ctl_ptr].rect.y = 20 + (12 * hd);
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;
    popup->combo[popup->cmb_ptr].rect.x = 25 + (20 * wd) * 5;
    popup->combo[popup->cmb_ptr].rect.y = 20 + (12 * hd);
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
    for (i = 0; label_mode[i] != NULL; i++) {
        text = TTF_RenderText_Solid(font14, label_mode[i], c1);
        popup->combo[popup->cmb_ptr].label[i] = SDL_CreateTextureFromSurface( popup->render,
                                      text);
        SDL_QueryTexture(popup->combo[popup->cmb_ptr].label[i], NULL, NULL,
             &popup->combo[popup->cmb_ptr].lw[i], &popup->combo[popup->cmb_ptr].lh[i]);
    }
    popup->combo[popup->cmb_ptr].num = takeup_label[u];
    popup->combo[popup->cmb_ptr].value = &takeup_label[u];
    popup->combo[popup->cmb_ptr].max = i - 1;
    popup->cmb_ptr++;
    popup->update = &model2415_update;
    return popup;
}

void
model2415_init(struct _device *unit, void *rend)
{
     SDL_Renderer *render = (SDL_Renderer *)rend;
     SDL_Surface *text;

     if (model2415_img == NULL) {
         text = IMG_ReadXPMFromArray(model2415_xpm);
         model2415_img = SDL_CreateTextureFromSurface(render, text);
         SDL_SetTextureBlendMode(model2415_img, SDL_BLENDMODE_BLEND);
         SDL_FreeSurface(text);
     }
     if (tape_images_img == NULL) {
         text = IMG_ReadXPMFromArray(tape_images_xpm);
         tape_images_img = SDL_CreateTextureFromSurface(render, text);
         SDL_SetTextureBlendMode(tape_images_img, SDL_BLENDMODE_BLEND);
         SDL_FreeSurface(text);
     }
}

#if 0
struct _device *
model2415_init(void *rend, uint16_t addr) {
     SDL_Renderer *render = (SDL_Renderer *)rend;
     SDL_Surface *text;
     struct _device *dev2415;
     struct _2415_context *tape;
     int     i;

     if ((dev2415 = calloc(1, sizeof(struct _device))) == NULL)
         return NULL;
     if ((tape = calloc(1, sizeof(struct _2415_context))) == NULL) {
         free(dev2415);
         return NULL;
     }

     tape_init();
     text = IMG_ReadXPMFromArray(model2415_xpm);
     model2415_img = SDL_CreateTextureFromSurface(render, text);
     SDL_SetTextureBlendMode(model2415_img, SDL_BLENDMODE_BLEND);
     SDL_FreeSurface(text);
     text = IMG_ReadXPMFromArray(tape_images_xpm);
     tape_images_img = SDL_CreateTextureFromSurface(render, text);
     SDL_SetTextureBlendMode(tape_images_img, SDL_BLENDMODE_BLEND);
     SDL_FreeSurface(text);

     dev2415->bus_func = &model2415_dev;
     dev2415->dev = (void *)tape;
     dev2415->draw_model = (void *)&model2415_draw;
     dev2415->create_ctrl = (void *)&model2415_control;
     dev2415->n_units = 6;
     for (i = 0; i < dev2415->n_units; i++) {
         dev2415->rect[i].x = 210 * i;
         dev2415->rect[i].y = 200;
         dev2415->rect[i].w = 210;
         dev2415->rect[i].h = 220;
         if (dev2415->rect[i].x > 800) {
            dev2415->rect[i].y += dev2415->rect[i].h;
            dev2415->rect[i].x = 210 * (i - 4);
         }
     }
     tape->addr = (addr & 0xf8);
     tape->chan = (addr >> 8) & 0xf;
     tape->state = STATE_IDLE;
     tape->selected = 0;
     for (i = 0; i < dev2415->n_units; i++) {
        tape->sense[i] = 0;
        tape->tape[i] = (struct _tape_buffer *)calloc(1, sizeof(struct _tape_buffer));
        tape->tape[i]->format = TRACK9;
     }
     tape->nunits = dev2415->n_units;
//     tape_attach(tape->tape[0], "../test_progs/bos.tap", TYPE_E11, 0, 1);
     tape_attach(tape->tape[0], "../test_progs/sysres.tap", TYPE_E11, 0, 1);
     tape_attach(tape->tape[1], "sys001.tap", TYPE_E11, 1, 1);
     tape_attach(tape->tape[2], "sys002.tap", TYPE_E11, 1, 1);
     tape_attach(tape->tape[3], "sys003.tap", TYPE_E11, 1, 1);
     tape_attach(tape->tape[4], "sys004.tap", TYPE_E11, 1, 1);
     tape_attach(tape->tape[5], "sys005.tap", TYPE_E11, 1, 1);
     for (i = 0; i < 6; i++)
         tape->tape[i]->format |= ONLINE;
     add_chan(dev2415, addr);
     return dev2415;
}

int
model2415_create(struct _option *opt)
{
     struct _option  opts;
     struct _device  *dev2415;
     struct _2415_context *tape;
     char            *file;
     int             i;
     int             track7;
     int             ring;
     int             fmt;
     int             den;

     /* Check if dealing with unit */
     if (opt->model == 'U') {
         dev2415 = find_chan(opt->addr, 0xf8);
         if (dev2415 == NULL) {
             fprintf(stderr, "Device not found %s %03x\n", opt->opt, opt->addr);
             return 0;
         }
         i = opt->addr & 0x7;
         tape = (struct _2415_context *)dev2415->dev;
         tape->tape[i] = (struct _tape_buffer *)calloc(1, sizeof(struct _tape_buffer));
         tape->tape[i]->format = TRACK9;
         /* Parse options given on definition */
         file = NULL;
         track7 = 0;
         ring = 0;
         den = 1;
         fmt = TYPE_TAP;
         while (get_option(&opts)) {
               if (strcmp(opts.opt, "FILE") == 0 && opts.flags == 1) {
                   file = strdup(opts.string);
               } else if (strcmp(opts.opt, "7TRACK") == 0) {
                   track7 = 1;
               } else if (strcmp(opts.opt, "NORING") == 0) {
                   ring = 0;
               } else if (strcmp(opts.opt, "RING") == 0) {
                   ring = 1;
               } else if (strcmp(opts.opt, "FORMAT") == 0) {
                   fmt = get_index(&opts, format_type);
               } else if (strcmp(opts.opt, "800") == 0) {
                   den = 0;
               } else if (strcmp(opts.opt, "1600") == 0) {
                   den = 1;
               } else {
                   fprintf(stderr, "Invalid option %s to 2415 Unit\n", opts.opt);
                   free(tape->tape[i]);
                   tape->tape[i] = NULL;
                   return 0;
               }
         }
         if (track7) {
            tape->tape[i]->format &= ~TRACK9;
         }
         if (file != NULL) {
             if (tape_attach(tape->tape[i], file, fmt, ring, den) == 0) {
                log_warn("Unable to open file %s\n", file);
             } else {
                tape->tape[i]->format |= ONLINE;
             }
             free(file);
         }
     } else {
         if ((dev2415 = calloc(1, sizeof(struct _device))) == NULL)
             return 0;
         if ((tape = calloc(1, sizeof(struct _2415_context))) == NULL) {
             free(dev2415);
             return 0;
         }

         tape_init();

         dev2415->bus_func = &model2415_dev;
         dev2415->dev = (void *)tape;
         dev2415->draw_model = (void *)&model2415_draw;
         dev2415->create_ctrl = (void *)&model2415_control;
         dev2415->type_name = "2415";
         dev2415->n_units = 6;
         if (opt->flags & 1) {
             if (opt->dash_num >= 4)
                 dev2415->n_units = (opt->dash_num - 3) * 2;
             else
                 dev2415->n_units = (opt->dash_num) * 2;
         }
         for (i = 0; i < dev2415->n_units; i++) {
             dev2415->rect[i].x = 210 * i;
             dev2415->rect[i].y = 200;
             dev2415->rect[i].w = 210;
             dev2415->rect[i].h = 220;
             if (dev2415->rect[i].x > 800) {
                dev2415->rect[i].y += dev2415->rect[i].h;
                dev2415->rect[i].x = 210 * (i - 4);
             }
         }
         tape->addr = (opt->addr & 0xf8);
         tape->chan = (opt->addr >> 8) & 0xf;
         tape->state = STATE_IDLE;
         tape->selected = 0;
         tape->nunits = dev2415->n_units;
         /* Parse options given on definition */
         while (get_option(&opts)) {
               if (strcmp(opts.opt, "7TRACK") == 0) {
                   tape->track_7 = 1;
               } else {
                   fprintf(stderr, "Invalid option %s to 1442\n", opts.opt);
                   free(tape);
                   free(dev2415);
                   return 0;
               }
         }

         for (i = 0; i < 6; i++)
             tape->sense[i] = 0;
         add_chan(dev2415, opt->addr);
     }
     return 1;
}

DEV_LIST_STRUCT(2415, CTRL_TYPE, CHAR_OPT|NUM_MOD);
#endif

