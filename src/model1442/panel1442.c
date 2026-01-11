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
#include "label.h"
#include "indicator.h"
#include "button.h"
#include "number.h"
#include "area.h"
#include "light.h"
#include "text.h"
#include "combo.h"
#include "cpu.h"
#include "card.h"
#include "model1442.h"
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
static void model1442_update(void *arg, int iarg)
{
    struct _1442_context *ctx = (struct _1442_context *)arg;
    int r;
    int cards;

printf("1442 click %d\n", iarg);
    switch (iarg) {
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
          r = read_deck(ctx->feed, get_textbuffer(ctx->input[0]));
          break;

    case 6: /* Load hopper blanks */
          cards = atoi(get_textbuffer(ctx->input[0]));
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
          r = save_deck(ctx->stack[0], get_textbuffer(ctx->input[1]));
          break;

    case 9: /* Empty Stacker 2 */
          empty_cards(ctx->stack[1]);
          break;

    case 10: /* Save Stacker 2 */
          r = save_deck(ctx->stack[0], get_textbuffer(ctx->input[2]));
          break;
    }
    ctx->hop_cnt = hopper_size(ctx->feed);
    ctx->stk_cnt[0] = stack_size(ctx->stack[0]);
    ctx->stk_cnt[1] = stack_size(ctx->stack[1]);
}


static SDL_Color power_on = {0x96, 0x8f, 0x85};
static SDL_Color power_off = {0xfd, 0xfd, 0xfd};
static SDL_Color ready = {0x7f,0xc0, 0x86};
static SDL_Color not_ready = {0x0c, 0x2e, 0x30};
static SDL_Color eof_color =  {0x0c, 0x2e, 0x30};
static SDL_Color chk_on = {0xff, 0xfd, 0x5e};
static SDL_Color chk_off =  {0xdd, 0xdc, 0x8a};
static SDL_Color start_col = {0x0c, 0x2e, 0x30};
static SDL_Color npro_col =  {0x0a, 0x52, 0x9a};
static SDL_Color stop_col =  {0xc8, 0x3a, 0x30};
static SDL_Color button_col = {0x80, 0x80, 0x80};
/*
 * Create a popup control window for device.
 */
struct _popup *
model1442_control(struct _device *unit, int hd, int wd, int u)
{
    struct _popup  *popup;
    struct _1442_context *ctx = (struct _1442_context *)unit->dev;
    SDL_Surface *text;
    int    row;
    int    i, j;
    int    w, h;
    int    wx, hx;
    char   buffer[100];
    Panel  panel;
    int    *value;
    int    turnoff;

    if (TTF_SizeText(font10, "M", &wx, &hx) != 0) {
        return NULL;
    }
    if (TTF_SizeText(font14, "M", NULL, &h) != 0) {
        return NULL;
    }
    if ((popup = (struct _popup *)calloc(1, sizeof(struct _popup))) == NULL)
        return NULL;
    if ((panel = (struct _panel_t *)calloc(1, sizeof(struct _panel_t))) == NULL) {
        free(popup);
        return NULL;
    }
    popup->panel = panel;
    sprintf(buffer, "IBM1442 Dev 0x'%03X'", ctx->addr);
    popup->screen = SDL_CreateWindow(buffer, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        1000, 200, SDL_WINDOW_RESIZABLE );
    popup->render = SDL_CreateRenderer( popup->screen, -1, SDL_RENDERER_ACCELERATED);
    popup->device = unit;
    panel->screen = popup->screen;
    panel->render = popup->render;
    panel->windowID = SDL_GetWindowID(panel->screen);
    add_area(panel, 20+(12*wx) * 3, 0, 200, 800, &c_white);
    add_indicator(panel, 20 + ((12*wx) * 0), 20 + ((3*hx) * 0),
                    2 * hx, 10 * wx, "POWER", "ON", &POWER, 0, font10,
                          &c_white, &power_on, &power_off);
    add_indicator(panel, 20 + ((12*wx) * 1), 20 + ((3*hx) * 0),
                    2 * hx, 10 * wx, "READY", NULL, &ctx->rdy_flag, 0, font10,
                          &c_black, &ready, &not_ready);
    add_button(panel, 20 + ((12*wx) * 2), 20 + ((3*hx) * 0),
                    2 * hx, 10 * wx, "END OF", "FILE", &ctx->eof_flag, font10,
                          &c_white, &eof_color, 0);
    add_indicator(panel, 20 + ((12*wx) * 0), 20 + ((3*hx) * 1),
                    2 * hx, 10 * wx, "CHECK", NULL, NULL, 0, font10,
                          &c_black, &chk_on, &chk_off);
    add_indicator(panel, 20 + ((12*wx) * 1), 20 + ((3*hx) * 1),
                    2 * hx, 10 * wx, "CHIP BOX", NULL, &ctx->rdy_flag, 0, font10,
                          &c_black, &chk_on, &chk_off);
    add_indicator(panel, 20 + ((12*wx) * 2), 20 + ((3*hx) * 1),
                    2 * hx, 10 * wx, "END OF", "FILE", &ctx->eof_flag, 0, font10,
                          &c_black, &chk_on, &chk_off);
    add_button_callback(panel, 20 + ((12*wx) * 0), 20 + ((3*hx) * 2),
                    2 * hx, 10 * wx, "START", NULL, &model1442_update, (void *)ctx, 1,
                    font10, &c_black, &start_col);
    add_button_callback(panel, 20 + ((12*wx) * 1), 20 + ((3*hx) * 2),
                    2 * hx, 10 * wx, "NPRO", NULL, &model1442_update, (void *)ctx, 2,
                    font10, &c_black, &npro_col);
    add_button_callback(panel, 20 + ((12*wx) * 2), 20 + ((3*hx) * 2),
                    2 * hx, 10 * wx, "STOP", NULL, &model1442_update, (void *)ctx, 3,
                    font10, &c_black, &stop_col);

    h = hx;
    w = wx * 15;
    row = 20;
    add_label(panel, 25 + (12 * wx) * 3, row, "Hopper:", font10, &c1);
    ctx->input[0] = (void *)add_textinput(panel, 25 + (12*wx) * 4, row, h+2, 40*wx,
                    ctx->feed->file_name);
    add_button_callback(panel, 20 + ((12*wx) * 8), row,
                    2 * hx, 10 * wx, "EMPTY", NULL, &model1442_update, (void *)ctx, 4,
                    font10, &c_white, &button_col);
    add_button_callback(panel, 20 + ((12*wx) * 9), row,
                    2 * hx, 10 * wx, "LOAD", NULL, &model1442_update, (void *)ctx, 5,
                    font10, &c_white, &button_col);
    add_button_callback(panel, 20 + ((12*wx) * 10), row,
                    2 * hx, 10 * wx, "BLANK", NULL, &model1442_update, (void *)ctx, 6,
                    font10, &c_white, &button_col);
    add_combo(panel, 25 + (12 * wx) * 11, row, h+2, 10 * wx, card_fmt_type, &ctx->feed->mode,
                        font14, &c_black, &c_white);
    add_number(panel, 25 + (12*wx) * 12, row, h+2, 5 * wx, &ctx->hop_cnt, font14, &c_black, &c_white);
    row += 3*hx;
    add_label(panel, 25 + (12 * wx) * 3, row, "Stack 1:", font10, &c1);
    ctx->input[1] = (void *)add_textinput(panel, 25 + (12*wx) * 4, row, hx+5, 40*wx,
                   ctx->stack[0]->file_name);
    add_button_callback(panel, 20 + ((12*wx) * 8), row,
                    2 * hx, 10 * wx, "EMPTY", NULL, &model1442_update, (void *)ctx, 7,
                    font10, &c_white, &button_col);
    add_button_callback(panel, 20 + ((12*wx) * 9), row,
                    2 * hx, 10 * wx, "SAVE", NULL, &model1442_update, (void *)ctx, 8,
                    font10, &c_white, &button_col);
    add_combo(panel, 25 + (12 * wx) * 11, row, h+2, 10 * wx, card_fmt_type, &ctx->stack[0]->mode,
                        font14, &c_black, &c_white);
    add_number(panel, 25 + (12*wx) * 12, row, h+2, 5 * wx, &ctx->stk_cnt[0], font14,
                            &c_black, &c_white);
    row += 3*hx;
    add_label(panel, 25 + (12 * wx) * 3, row, "Stack 2:", font10, &c1);
    ctx->input[2] = (void *)add_textinput(panel, 25 + (12*wx) * 4, row, hx+5, 40*wx,
                   ctx->stack[1]->file_name);
    add_button_callback(panel, 20 + ((12*wx) * 8), row,
                    2 * hx, 10 * wx, "EMPTY", NULL, &model1442_update, (void *)ctx, 9,
                    font10, &c_white, &button_col);
    add_button_callback(panel, 20 + ((12*wx) * 9), row,
                    2 * hx, 10 * wx, "SAVE", NULL, &model1442_update, (void *)ctx, 10,
                    font10, &c_white, &button_col);
    add_combo(panel, 25 + (12 * wx) * 11, row, h+2, 10 * wx, card_fmt_type, &ctx->stack[1]->mode,
                        font14, &c_black, &c_white);
    add_number(panel, 25 + (12*wx) * 12, row, h+2, 5 * wx, &ctx->stk_cnt[1], font14,
                            &c_black, &c_white);
    return popup;
}

