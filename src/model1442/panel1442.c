/*
 * microsim360 - Model 1442 card reader/punch. Display
 *
 * Copyright 2025, Richard Cornwell
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
#include <string.h>
#include "logger.h"
#include "event.h"
#include "widgets.h"
#include "light.h"
#include "model1442.h"
#include "card.h"
#include "model1442.xpm"
#include "xlat.h"

static SDL_Texture *model1442_img = NULL;

/*
 * Initialize device graphics.
 */
void
model1442_init(struct _device *unit, void *rend)
{
     SDL_Renderer *render = (SDL_Renderer *)rend;
     SDL_Surface *text;


    /* Create the image on first run of draw function */
    if (model1442_img == NULL) {
        SDL_Surface *text;
        text = IMG_ReadXPMFromArray(model1442_xpm);
        model1442_img = SDL_CreateTextureFromSurface(render, text);
        SDL_SetTextureBlendMode(model1442_img, SDL_BLENDMODE_BLEND);
        SDL_FreeSurface(text);
    }
}

/*
 * Draw device in peripheral window.
 */
void
model1442_draw(struct _device *unit, void *rend)
{
    struct _1442_context *ctx = (struct _1442_context *)unit->dev;
    SDL_Renderer *render = (SDL_Renderer *)rend;
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
    rect.y = y;
    rect.h = 142;
    rect.w = 305;
    rect2.x = 0;
    rect2.y = 0;
    rect2.h = 142;
    rect2.w = 305;
    SDL_RenderCopy(render, model1442_img, &rect2, &rect);
    /* Draw device number */
    sprintf(buf, "%03X", unit->addr);
    text = TTF_RenderText_Solid(font14, buf, c1);
    txt = SDL_CreateTextureFromSurface(render, text);
    SDL_FreeSurface(text);
    SDL_QueryTexture(txt, &i, &j, &rect2.w, &rect2.h);
    rect2.x = x + 20;
    rect2.y = y + 20;
    SDL_RenderCopy(render, txt, NULL, & rect2);
    SDL_DestroyTexture(txt);

    /* Draw stacked cards */
    rect2.x = x + 351;
    rect2.w = 399 - rect2.x;
    rect2.y = y + 10;
    rect2.h = hopper_size(ctx->feed) / 30;
    rect2.y = 40 - rect2.h;
    rect.x = x + 184;
    rect.y = y + 56 - rect2.h;
    rect.h = rect2.h;
    rect.w = rect2.w;
    SDL_RenderCopy(render, model1442_img, &rect2, &rect);
    /* Draw weight */
    rect2.x = x + 351;
    rect2.y = y + 0;
    rect2.h = 10;
    rect.x = x + 184;
    rect.y -= 8;
    rect.h = rect2.h;
    rect.w = rect2.w;
    SDL_RenderCopy(render, model1442_img, &rect2, &rect);
    /* Draw front */
    rect2.x = x + 351;
    rect2.y = y + 51;
    rect2.w++;
    rect2.h = 15;
    rect.x = x + 182;
    rect.y = y + 60 - rect2.h;
    rect.h = rect2.h;
    rect.w = rect2.w;
    SDL_RenderCopy(render, model1442_img, &rect2, &rect);
    /* Draw Stacker 2 */
    rect2.x = x + 344;
    rect2.y = y + 104;
    rect2.w = stack_size(ctx->stack[1]) / 30;
    rect2.h = 30;
    rect.x = x + 122;
    rect.y = y + 75;
    rect.h = rect2.h;
    rect.w = rect2.w;
    SDL_RenderCopy(render, model1442_img, &rect2, &rect);
    /* Draw Stacker 1 */
    rect2.x = x + 344;
    rect2.y = y + 104;
    rect2.w = stack_size(ctx->stack[0]) / 30;
    rect2.h = 30;
    rect.x = x + 122;
    rect.y = y + 75;
    rect.h = rect2.h;
    rect.w = rect2.w;
    SDL_RenderCopy(render, model1442_img, &rect2, &rect);
    /* Overlay */
    rect2.x = x + 343;
    rect2.y = y + 69;
    rect2.w = 400 - 343;
    rect2.h = 101 - 69;
    rect.x = x + 122;
    rect.y = y + 97;
    rect.h = rect2.h;
    rect.w = rect2.w;
    SDL_RenderCopy(render, model1442_img, &rect2, &rect);
}

/*
 * Handle control functions for device.
 */
static void model1442_update(struct _popup *popup, void *device, int index)
{
    struct _device *unit = (struct _device *)device;
    struct _1442_context *ctx = (struct _1442_context *)unit->dev;
    int r;
    int cards;

    switch (index) {
    case 0:  /* End of file key */
          ctx->eof_flag = !ctx->eof_flag;
          break;

    case 1:  /* Start Key */
          log_device("Start key\n");
          if (ctx->state == STATE_IDLE) {
              log_device("Start reader\n");
              if (ctx->rdr_full == 0) {
                  model1442_feed(ctx);
              }
              if (ctx->rdr_full) {
//                  ctx->state = STATE_END;
                  ctx->status = SNS_DEVEND;
                  ctx->data_end = 1;
                  ctx->feed_done = 1;
              }
          }
          break;

    case 2:  /* NPRO */
          if (ctx->selected == 0)
              model1442_feed(ctx);
          ctx->eof_flag = 0;
          break;

    case 3:  /* STOP */
          ctx->stop_flag = 1;
          break;

    case 4: /* Empty hopper */
          empty_cards(ctx->feed);
          break;

    case 5: /* Load hopper */
          r = read_deck(ctx->feed, popup->text[0].text);
          break;

    case 6: /* Load hopper blanks */
          cards = atoi(popup->text[0].text);
          if (cards > 0) {
              blank_deck(ctx->feed, cards);
          } else {
              blank_deck(ctx->feed, 1);
          }
          break;

    case 7: /* Empty Stacker 1 */
          empty_cards(ctx->stack[0]);
          break;

    case 8: /* Save Stacker 1 */
          r = save_deck(ctx->stack[0], popup->text[1].text);
          break;

    case 9: /* Empty Stacker 2 */
          empty_cards(ctx->stack[1]);
          break;

    case 10: /* Save Stacker 2 */
          r = save_deck(ctx->stack[1], popup->text[2].text);
          break;
    }
    ctx->hop_cnt = hopper_size(ctx->feed);
    ctx->stk_cnt[0] = stack_size(ctx->stack[0]);
    ctx->stk_cnt[1] = stack_size(ctx->stack[1]);
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
     {"POWER", "ON", 1, 0, 0, {0, 0, 0}, {0x96, 0x8f, 0x85}, {0xfd, 0xfd, 0xfd}},
     {"READY", NULL, 1, 1, 0, {0xff, 0xff, 0xff}, {0x7f,0xc0, 0x86}, {0x0c, 0x2e, 0x30}},
     {"END OF", "FILE", 0, 2, 0, {0xff, 0xff, 0xff}, {0x0c, 0x2e, 0x30}, {0,0,0}},
     {"CHECK", NULL, 1, 0, 1, {0, 0, 0}, {0xff, 0xfd, 0x5e}, {0xdd, 0xdc, 0x8a}},
     {"CHIP BOX", NULL, 1, 1, 1, {0, 0, 0}, {0xff, 0xfd, 0x5e}, {0xdd, 0xdc, 0x8a}},
     {"END OF", "FILE", 1, 2, 1, {0, 0, 0}, {0xff, 0xfd, 0x5e}, {0xdd, 0xdc, 0x8a} },
     {"START", NULL,  0, 0, 2, {0xff, 0xff, 0xff}, {0x0c, 0x2e, 0x30}, {0, 0, 0}},
     {"NPRO", NULL, 0, 1, 2, {0xff, 0xff, 0xff}, {0x0a, 0x52, 0x9a}, {0, 0, 0}},
     {"STOP", NULL, 0, 2, 2, {0xff, 0xff, 0xff}, {0xc8, 0x3a, 0x30}, {0, 0, 0}},
     {"EMPTY", NULL, 0, 8, 0, {0, 0, 0}, {0x80, 0x80, 0x80}, {0, 0, 0}},
     {"LOAD", NULL, 0, 9, 0, {0, 0, 0}, {0x80, 0x80, 0x80}, {0, 0, 0}},
     {"BLANK", NULL, 0, 10, 0, {0, 0, 0}, {0x80, 0x80, 0x80}, {0, 0, 0}},
     {"EMPTY", NULL, 0, 8, 1, {0, 0, 0}, {0x80, 0x80, 0x80}, {0, 0, 0}},
     {"SAVE", NULL, 0, 9, 1, {0, 0, 0}, {0x80, 0x80, 0x80}, {0, 0, 0}},
     {"EMPTY", NULL, 0, 8, 2, {0, 0, 0}, {0x80, 0x80, 0x80}, {0, 0, 0}},
     {"SAVE", NULL, 0, 9, 2, {0, 0, 0}, {0x80, 0x80, 0x80}, {0, 0, 0}},
     {NULL, NULL, 0},
};

/*
 * Create a popup control window for device.
 */
struct _popup *
model1442_control(struct _device *unit, int hd, int wd, int u)
{
    struct _popup  *popup;
    struct _1442_context *ctx = (struct _1442_context *)unit->dev;
    SDL_Surface *text;
    int    i, j;
    int    w, h;
    char   buffer[100];

    if ((popup = (struct _popup *)calloc(1, sizeof(struct _popup))) == NULL)
        return popup;
    sprintf(buffer, "IBM1442 Dev 0x'%03X'", ctx->addr);
    popup->screen = SDL_CreateWindow(buffer, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        1000, 200, SDL_WINDOW_RESIZABLE );
    popup->render = SDL_CreateRenderer( popup->screen, -1, SDL_RENDERER_ACCELERATED);
    popup->device = unit;
    popup->areas[popup->area_ptr].rect.x = 20 + (12 * wd) * 3;
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

    popup->ind[1].value = &ctx->rdr_full;
    popup->ind[4].value = &ctx->eof_flag;

    text = TTF_RenderText_Solid(font14, "Hopper: ", c1);

    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 25 + (12 * wd) * 3;
    popup->ctl_label[popup->ctl_ptr].rect.y = 20;
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;
    popup->text[popup->txt_ptr].rect.x = 25 + (12 * wd) * 3;
    popup->text[popup->txt_ptr].rect.y = 20;
    popup->text[popup->txt_ptr].rect.w = 45 * wd;
    popup->text[popup->txt_ptr].rect.h = h + 5;
    if (ctx->feed->file_name != NULL)
        strncpy(popup->text[popup->txt_ptr].text, ctx->feed->file_name, 256);
    else
        popup->text[popup->txt_ptr].text[0] = '\0';
    popup->text[popup->txt_ptr].len = strlen(popup->text[popup->txt_ptr].text);
    popup->text[popup->txt_ptr].pos = popup->text[popup->txt_ptr].len;
    popup->text[popup->txt_ptr].cpos = textpos(&popup->text[popup->txt_ptr],
                                        popup->text[popup->txt_ptr].pos);
    popup->txt_ptr++;
    popup->number[popup->num_ptr].rect.x = 25 + (12 * wd) * 13;
    popup->number[popup->num_ptr].rect.y = 20;
    popup->number[popup->num_ptr].rect.w = 5 * wd;
    popup->number[popup->num_ptr].rect.h = h;
    popup->number[popup->num_ptr].value = &ctx->hop_cnt;
    popup->number[popup->num_ptr].c = &c;
    popup->num_ptr++;

    popup->combo[popup->cmb_ptr].rect.x = 25 + (12 * wd) * 11;
    popup->combo[popup->cmb_ptr].rect.y = 20;
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
    popup->combo[popup->cmb_ptr].num = ctx->feed->mode;
    popup->combo[popup->cmb_ptr].value = &ctx->feed->mode;
    popup->combo[popup->cmb_ptr].max = i - 1;
    popup->cmb_ptr++;

    text = TTF_RenderText_Solid(font14, "Stack 1: ", c1);

    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 25 + (12 * wd) * 3;
    popup->ctl_label[popup->ctl_ptr].rect.y = 20 + (hd * 3);
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;
    popup->text[popup->txt_ptr].rect.x = 25 + (12 * wd) * 3;
    popup->text[popup->txt_ptr].rect.y = 20 + (hd * 3);
    popup->text[popup->txt_ptr].rect.w = 45 * wd;
    popup->text[popup->txt_ptr].rect.h = h + 5;
    if (ctx->stack[0]->file_name != NULL)
        strncpy(popup->text[popup->txt_ptr].text, ctx->stack[0]->file_name, 256);
    else
        popup->text[popup->txt_ptr].text[0] = '\0';
    popup->text[popup->txt_ptr].len = strlen(popup->text[popup->txt_ptr].text);
    popup->text[popup->txt_ptr].pos = popup->text[popup->txt_ptr].len;
    popup->text[popup->txt_ptr].cpos = textpos(&popup->text[popup->txt_ptr],
                                        popup->text[popup->txt_ptr].pos);
    popup->txt_ptr++;
    popup->number[popup->num_ptr].rect.x = 25 + (12 * wd) * 13;
    popup->number[popup->num_ptr].rect.y = 20 + (hd * 3);
    popup->number[popup->num_ptr].rect.w = 5 * wd;
    popup->number[popup->num_ptr].rect.h = h;
    popup->number[popup->num_ptr].value = &ctx->stk_cnt[0];
    popup->number[popup->num_ptr].c = &c;
    popup->num_ptr++;
    popup->combo[popup->cmb_ptr].rect.x = 25 + (12 * wd) * 11;
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
    popup->combo[popup->cmb_ptr].num = ctx->stack[0]->mode;
    popup->combo[popup->cmb_ptr].value = &ctx->stack[0]->mode;
    popup->combo[popup->cmb_ptr].max = i - 1;
    popup->cmb_ptr++;

    text = TTF_RenderText_Solid(font14, "Stack 2: ", c1);

    popup->ctl_label[popup->ctl_ptr].text = SDL_CreateTextureFromSurface( popup->render, text);
    SDL_QueryTexture(popup->ctl_label[popup->ctl_ptr].text, NULL, NULL, &w, &h);
    SDL_FreeSurface(text);
    popup->ctl_label[popup->ctl_ptr].rect.x = 25 + (12 * wd) * 3;
    popup->ctl_label[popup->ctl_ptr].rect.y = 20 + (hd * 6);
    popup->ctl_label[popup->ctl_ptr].rect.w = w;
    popup->ctl_label[popup->ctl_ptr].rect.h = h;
    popup->ctl_ptr++;
    popup->text[popup->txt_ptr].rect.x = 25 + (12 * wd) * 3;
    popup->text[popup->txt_ptr].rect.y = 20 + (hd * 6);
    popup->text[popup->txt_ptr].rect.w = 45 * wd;
    popup->text[popup->txt_ptr].rect.h = h;
    if (ctx->stack[1]->file_name != NULL)
        strncpy(popup->text[popup->txt_ptr].text, ctx->stack[1]->file_name, 256);
    else
        popup->text[popup->txt_ptr].text[0] = '\0';
    popup->text[popup->txt_ptr].len = strlen(popup->text[popup->txt_ptr].text);
    popup->text[popup->txt_ptr].pos = popup->text[popup->txt_ptr].len;
    popup->text[popup->txt_ptr].cpos = textpos(&popup->text[popup->txt_ptr],
                                        popup->text[popup->txt_ptr].pos);
    popup->txt_ptr++;
    popup->number[popup->num_ptr].rect.x = 25 + (12 * wd) * 13;
    popup->number[popup->num_ptr].rect.y = 20 + (hd * 6);
    popup->number[popup->num_ptr].rect.w = 5 * wd;
    popup->number[popup->num_ptr].rect.h = h;
    popup->number[popup->num_ptr].value = &ctx->stk_cnt[1];
    popup->number[popup->num_ptr].c = &c;
    popup->num_ptr++;

    popup->combo[popup->cmb_ptr].rect.x = 25 + (12 * wd) * 11;
    popup->combo[popup->cmb_ptr].rect.y = 20 + (hd * 6);
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
    popup->combo[popup->cmb_ptr].num = ctx->stack[1]->mode;
    popup->combo[popup->cmb_ptr].value = &ctx->stack[1]->mode;
    popup->combo[popup->cmb_ptr].max = i - 1;
    popup->cmb_ptr++;

    w = 0;
    for (i = 0; i < popup->ctl_ptr; i++) {
        if (popup->ctl_label[i].rect.w > w)
            w = popup->ctl_label[i].rect.w;
    }
    for (i = 0; i < popup->txt_ptr; i++) {
        popup->text[i].rect.x += w;
    }

    popup->update = &model1442_update;
    return popup;
}

