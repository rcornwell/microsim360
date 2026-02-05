/*
 * microsim360 - Model 2540 card reader/punch. Display
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

#include <SDL.h>
#include <SDL_timer.h>
#include <SDL_thread.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <string.h>
#include "logger.h"
#include "event.h"
#include "conf.h"
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
#include "model2821/model2821.h"
#include "model2540.h"
#include "model2540.xpm"
#include "xlat.h"

static SDL_Texture *model2540_img = NULL;

DEV_LIST_STRUCT(2540, DEV_TYPE, CHAR_OPT);

/*
 * Initialize device graphics.
 */
void
model2540_init_graphics(struct _device *unit, void *rend)
{
    /* Create the image on first run of draw function */
    if (model2540_img == NULL) {
        SDL_Renderer *render = (SDL_Renderer *)rend;
        SDL_Surface *text;
        text = IMG_ReadXPMFromArray(model2540_xpm);
        model2540_img = SDL_CreateTextureFromSurface(render, text);
        SDL_SetTextureBlendMode(model2540_img, SDL_BLENDMODE_BLEND);
        SDL_FreeSurface(text);
    }
}

static SDL_Color power_on = {0x96, 0x8f, 0x85};    /* White */
static SDL_Color power_off = {0x85, 0x81, 0x73};
static SDL_Color ready = {0x7f,0xc0, 0x86};
static SDL_Color not_ready = {0x0c, 0x2e, 0x30};
static SDL_Color eof_color = {0x32, 0x97, 0xcd};   /* Blue */
static SDL_Color eof_color_off =  {0xa6, 0xa6, 0x4a};  /* Yellow */
static SDL_Color eof_color_on =  {0xf3, 0xee, 0x8e};
static SDL_Color chk_on = {0xbb, 0x18, 0x0f};   /* Red */
static SDL_Color chk_off =  {0x8d, 0x09, 0x01};
static SDL_Color start_col = {0x00, 0x5a, 0x41}; /* Green */
static SDL_Color start_on = {0x71, 0xcc, 0xb0}; /* Light Green */
static SDL_Color npro_col =  {0x0a, 0x52, 0x9a};
static SDL_Color stop_col =  {0xbf, 0x26, 0x21}; /* Red */
static SDL_Color button_col = {0x80, 0x80, 0x80};

/*
 * Draw device in peripheral window.
 */
void
model2540_draw(struct _device *unit, void *rend, int u)
{
    struct _2821_dev_context *ctx = (struct _2821_dev_context *)unit->dev;
    struct _2540_context *cdr = (struct _2540_context *)ctx->ctx;
    SDL_Renderer *render = (SDL_Renderer *)rend;
    int          i, j;
    int          h, w;
    SDL_Rect     rect;
    SDL_Rect     rect2;
    SDL_Rect     rect3;
    SDL_Surface *text;
    SDL_Texture *txt;
    int           x = unit->rect[u].x;
    int           y = unit->rect[u].y;
    char          buf[100];

    /* Draw basic device */
    rect.x = x + 5;
    rect.y = y;
    rect.w = 205;
    rect.h = 144;
    /* Draw punch */
    rect2.x = 0;
    rect2.y = 0;
    rect2.w = 205;
    rect2.h = 144;
    SDL_RenderCopy(render, model2540_img, &rect2, &rect);
    /* Draw stacked cards */
    h = hopper_size(cdr->rdr_feed) / 50;
    if (h > 50) {
        h = 50;
    }
    w = (int)((float)h * 0.4);
    if (w > 20) {
        w = 20;
    }
    rect2.x = 265;
    rect2.w = 34;
    rect2.h = h; // 54 hopper_size(cdr->rdr_feed) / 60;
    rect2.y = 5; //200 + (50 - rect2.h);
    rect3.x = 157 + x + (20 - w);
    rect3.y = y + 2 + (50 - h);
    rect3.w = 34;
    rect3.h = h;
    SDL_RenderCopy(render, model2540_img, &rect2, &rect3);
    i = hopper_size(cdr->stack[0]);
    if (i != 0) {
        h = i / 100;
        if (h > 11) {
            h = 11;
        }

        rect2.x = 243;
        rect2.w = 14;
        rect2.h = 23 + h; // 54 hopper_size(cdr->rdr_feed) / 60;
        rect2.y = 20 + (11 - h); //200 + (50 - rect2.h);
        rect3.x = 126 + x;
        rect3.y = 92 + y;
        rect3.w = 14;
        rect3.h = rect2.h;
        SDL_RenderCopy(render, model2540_img, &rect2, &rect3);
    }
    i = hopper_size(cdr->stack[1]);
    if (i != 0) {
        h = i / 100;
        if (h > 11) {
            h = 11;
        }

        rect2.x = 243;
        rect2.w = 14;
        rect2.h = 23 + h; // 54 hopper_size(cdr->rdr_feed) / 60;
        rect2.y = 20 + (11 - h); //200 + (50 - rect2.h);
        rect3.x = 115 + x;
        rect3.y = 92 + y;
        rect3.w = 14;
        rect3.h = rect2.h;
        SDL_RenderCopy(render, model2540_img, &rect2, &rect3);
    }
    i = hopper_size(cdr->stack[2]);
    if (i != 0) {
        h = i / 100;
        if (h > 11) {
            h = 11;
        }

        rect2.x = 243;
        rect2.w = 14;
        rect2.h = 23 + h; // 54 hopper_size(cdr->rdr_feed) / 60;
        rect2.y = 20 + (11 - h); //200 + (50 - rect2.h);
        rect3.x = 100 + x;
        rect3.y = 92 + y;
        rect3.w = 14;
        rect3.h = rect2.h;
        SDL_RenderCopy(render, model2540_img, &rect2, &rect3);
    }
    i = hopper_size(cdr->stack[3]);
    if (i != 0) {
        h = i / 100;
        if (h > 11) {
            h = 11;
        }

        rect2.x = 226;
        rect2.w = 14;
        rect2.h = 23 + h; // 54 hopper_size(cdr->rdr_feed) / 60;
        rect2.y = 20 + (11 - h); //200 + (50 - rect2.h);
        rect3.x = 92 + x;
        rect3.y = 92 + y;
        rect3.w = 14;
        rect3.h = rect2.h;
        SDL_RenderCopy(render, model2540_img, &rect2, &rect3);
    }
    i = hopper_size(cdr->stack[4]);
    if (i != 0) {
        h = i / 100;
        if (h > 11) {
            h = 11;
        }

        rect2.x = 226;
        rect2.w = 14;
        rect2.h = 23 + h; // 54 hopper_size(cdr->rdr_feed) / 60;
        rect2.y = 20 + (11 - h); //200 + (50 - rect2.h);
        rect3.x = 83 + x;
        rect3.y = 92 + y;
        rect3.w = 14;
        rect3.h = rect2.h;
        SDL_RenderCopy(render, model2540_img, &rect2, &rect3);
    }
    /* Draw device number(s) */
    if (cdr->rdr_ctx != NULL) {
        sprintf(buf, "%03X", cdr->rdr_ctx->addr);
        text = TTF_RenderText_Solid(font14, buf, c_black);
        txt = SDL_CreateTextureFromSurface(render, text);
        SDL_FreeSurface(text);
        SDL_QueryTexture(txt, &i, &j, &rect2.w, &rect2.h);
        rect2.x = x + 180;
        rect2.y = y + 100;
        SDL_RenderCopy(render, txt, NULL, & rect2);
        SDL_DestroyTexture(txt);
    }

    if (cdr->pch_ctx != NULL) {
        sprintf(buf, "%03X", cdr->pch_ctx->addr);
        text = TTF_RenderText_Solid(font14, buf, c_black);
        txt = SDL_CreateTextureFromSurface(render, text);
        SDL_FreeSurface(text);
        SDL_QueryTexture(txt, &i, &j, &rect2.w, &rect2.h);
        rect2.x = x + 20;
        rect2.y = y + 100;
        SDL_RenderCopy(render, txt, NULL, & rect2);
        SDL_DestroyTexture(txt);
    }

    /* Draw reader panels. */
    rect.x = 145 + x;
    rect.y = 61 + y;
    rect.w = 4;
    rect.h = 3;
    SDL_SetRenderDrawColor(render, start_col.r, start_col.g, start_col.b, 0xff);
    SDL_RenderFillRect( render, &rect);
    rect.x += 5;
    if (cdr->rdr_ready) {
        SDL_SetRenderDrawColor(render, start_on.r, start_on.g, start_on.b, 0xff);
    } else {
        SDL_SetRenderDrawColor(render, start_col.r, start_col.g, start_col.b, 0xff);
    }
    SDL_RenderFillRect( render, &rect);
    rect.x += 5;
    SDL_SetRenderDrawColor(render, eof_color_off.r, eof_color_off.g, eof_color_off.b, 0xff);
    SDL_RenderFillRect( render, &rect);
    rect.x += 5;
    SDL_SetRenderDrawColor(render, chk_off.r, chk_off.g, chk_off.b, 0xff);
    SDL_RenderFillRect( render, &rect);
    rect.x = 145 + x;
    rect.y = 65 + y;
    rect.w = 4;
    rect.h = 3;
    SDL_SetRenderDrawColor(render, stop_col.r, stop_col.g, stop_col.b, 0xff);
    SDL_RenderFillRect( render, &rect);
    rect.x += 5;
    SDL_SetRenderDrawColor(render, eof_color_off.r, eof_color_off.g, eof_color_off.b, 0xff);
    SDL_RenderFillRect( render, &rect);
    rect.x += 5;
    SDL_SetRenderDrawColor(render, chk_off.r, chk_off.g, chk_off.b, 0xff);
    SDL_RenderFillRect( render, &rect);
    rect.x += 5;
    SDL_SetRenderDrawColor(render, chk_off.r, chk_off.g, chk_off.b, 0xff);
    SDL_RenderFillRect( render, &rect);
    rect.x = 145 + x;
    rect.y = 69 + y;
    rect.w = 4;
    rect.h = 3;
    SDL_SetRenderDrawColor(render, eof_color.r, eof_color.g, eof_color.b, 0xff);
    SDL_RenderFillRect( render, &rect);
    rect.x += 5;
    SDL_SetRenderDrawColor(render, power_on.r, power_on.g, power_on.b, 0xff);
    SDL_RenderFillRect( render, &rect);
    rect.x += 5;
    SDL_SetRenderDrawColor(render, eof_color_off.r, eof_color_off.g, eof_color_off.b, 0xff);
    SDL_RenderFillRect( render, &rect);
    rect.x += 5;
    if (cdr->eof_flag) {
        SDL_SetRenderDrawColor(render, eof_color_on.r, eof_color_on.g, eof_color_on.b, 0xff);
    } else {
        SDL_SetRenderDrawColor(render, eof_color_off.r, eof_color_off.g, eof_color_off.b, 0xff);
    }
    SDL_RenderFillRect( render, &rect);
}

/*
 * Handle control functions for device.
 */
static void model2540_update(void *args, int iarg)
{
    struct _2540_context *ctx = (struct _2540_context *)args;
    int     r;
    int     i;
    int cards;

    switch (iarg) {
    case 1:  /* Reader Start Key */
          log_device("Reader Start key\n");
          model2540r_start(ctx, 1);
          break;

    case 2:  /* STOP */
          if (ctx->rdr_ctx->busy) {
              ctx->rdr_stop_flag = 1;
          } else {
              ctx->rdr_ctx->sense = BIT1;
              ctx->rdr_ready = 0;
          }
          break;

    case 3: /* Empty reader hopper */
          empty_cards(ctx->rdr_feed);
          break;

    case 4: /* Load hopper */
          r = read_deck(ctx->rdr_feed, get_textbuffer(ctx->rdr_input));
          break;

    case 5: /* Load hopper blanks */
          cards = atoi(get_textbuffer(ctx->rdr_input));
          if (cards > 0) {
              blank_deck(ctx->rdr_feed, cards);
          } else {
              blank_deck(ctx->rdr_feed, 1);
          }
          break;

    case 6:  /* Punch Start Key */
          log_device("Punch Start key\n");
          model2540p_start(ctx, 1);
          break;

    case 7:  /* STOP */
          if (ctx->pch_ctx->busy) {
              ctx->pch_stop_flag = 1;
          } else {
              ctx->pch_ctx->sense = BIT1;
              ctx->pch_ready = 0;
          }
          break;

    case 8: /* Empty reader hopper */
          empty_cards(ctx->pch_feed);
          break;

    case 9: /* Load hopper */
          r = read_deck(ctx->pch_feed, get_textbuffer(ctx->pch_input));
          break;

    case 10: /* Load hopper blanks */
          cards = atoi(get_textbuffer(ctx->pch_input));
          if (cards > 0) {
              blank_deck(ctx->pch_feed, cards);
          } else {
              blank_deck(ctx->pch_feed, 1);
          }
          break;

    case 11: /* Empty Stacker R1 */
          empty_cards(ctx->stack[0]);
          break;

    case 12: /* Save Stacker R1 */
          r = save_deck(ctx->stack[0], get_textbuffer(ctx->stack_input[0]));
          break;

    case 13: /* Empty Stacker R2 */
          empty_cards(ctx->stack[1]);
          break;

    case 14: /* Save Stacker R2 */
          r = save_deck(ctx->stack[1], get_textbuffer(ctx->stack_input[1]));
          break;

    case 15: /* Empty Stacker RP3 */
          empty_cards(ctx->stack[2]);
          break;

    case 16: /* Save Stacker RP3 */
          r = save_deck(ctx->stack[2], get_textbuffer(ctx->stack_input[2]));
          break;

    case 17: /* Empty Stacker P2 */
          empty_cards(ctx->stack[3]);
          break;

    case 18: /* Save Stacker P2 */
          r = save_deck(ctx->stack[3], get_textbuffer(ctx->stack_input[3]));
          break;

    case 19: /* Empty Stacker P2 */
          empty_cards(ctx->stack[4]);
          break;

    case 20: /* Save Stacker P2 */
          r = save_deck(ctx->stack[4], get_textbuffer(ctx->stack_input[4]));
          break;
    }
    ctx->rdr_hop_cnt = hopper_size(ctx->rdr_feed);
    ctx->pch_hop_cnt = hopper_size(ctx->pch_feed);
    for (i = 0; i < 5; i++) {
        ctx->stk_cnt[i] = stack_size(ctx->stack[i]);
    }
}


/*
 * Create a popup control window for device.
 */
void *
model2540_control(struct _device *unit, int u, int x, int y)
{
    struct _2821_dev_context *ctx = (struct _2821_dev_context *)unit->dev;
    struct _2540_context *dev = (struct _2540_context *)ctx->ctx;
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

    printf("2540 %d, %d\n", x, y);
    if (u != 0) {
        ctx = dev->rdr_ctx;
    } else {
        ctx = dev->pch_ctx;
    }

    /* Create device panel. */
    sprintf(buffer, "IBM2540 Dev 0x'%03X'", ctx->addr);
    if ((panel = create_window(buffer, 1000, 200, 1)) == NULL) {
        return NULL;
    }

    if (u != 0) {
        add_area(panel, 20+(12*wx) * 4, 0, 200, 800, &c_white);
        add_button_callback(panel, 20 + ((12*wx) * 0), 20 + ((3*hx) * 0),
                        2 * hx, 10 * wx, "START", NULL, &model2540_update, (void *)dev, 1,
                        font10, &c_white, &start_col);
        add_indicator(panel, 20 + ((12*wx) * 1), 20 + ((3*hx) * 0),
                        2 * hx, 10 * wx, "READY", NULL, &dev->rdr_ready, 0, font10,
                              &c_black, &start_on, &start_col);
        add_indicator(panel, 20 + ((12*wx) * 2), 20 + ((3*hx) * 0),
                        2 * hx, 10 * wx, "FEED", "STOP", NULL, 0, font10,
                              &c_black, &eof_color_on, &eof_color_off);
        add_indicator(panel, 20 + ((12*wx) * 3), 20 + ((3*hx) * 0),
                        2 * hx, 10 * wx, "FUSE", NULL, NULL, 0, font10,
                              &c_black, &chk_on, &chk_off);
        add_button_callback(panel, 20 + ((12*wx) * 0), 20 + ((3*hx) * 1),
                        2 * hx, 10 * wx, "STOP", NULL, &model2540_update, (void *)dev, 2,
                        font10, &c_white, &stop_col);
        add_indicator(panel, 20 + ((12*wx) * 1), 20 + ((3*hx) * 1),
                        2 * hx, 10 * wx, "VALIDITY", "CHECK", NULL, 0, font10,
                              &c_black, &eof_color_on, &eof_color_off);
        add_indicator(panel, 20 + ((12*wx) * 2), 20 + ((3*hx) * 1),
                        2 * hx, 10 * wx, "READ", "CHECK", NULL, 0, font10,
                              &c_black, &eof_color_on, &chk_off);
        add_indicator(panel, 20 + ((12*wx) * 3), 20 + ((3*hx) * 1),
                        2 * hx, 10 * wx, "TRANSPORT", NULL, NULL, 0, font10,
                              &c_black, &eof_color_on, &chk_off);
        add_button(panel, 20 + ((12*wx) * 0), 20 + ((3*hx) * 2),
                        2 * hx, 10 * wx, "END OF", "FILE", &dev->eof_flag, font10,
                              &c_white, &eof_color, 0);
        add_indicator(panel, 20 + ((12*wx) * 1), 20 + ((3*hx) * 2),
                        2 * hx, 10 * wx, "POWER", "ON", &POWER, 0, font10,
                              &c_white, &power_on, &power_off);
        add_indicator(panel, 20 + ((12*wx) * 2), 20 + ((3*hx) * 2),
                        2 * hx, 10 * wx, "STACKER", NULL, NULL, 0, font10,
                              &c_black, &eof_color_on, &eof_color_off);
        add_indicator(panel, 20 + ((12*wx) * 3), 20 + ((3*hx) * 2),
                        2 * hx, 10 * wx, "END OF", "FILE", &dev->eof_flag, 0, font10,
                              &c_black, &eof_color_on, &eof_color_off);
        h = hx;
        w = wx * 15;
        row = 20;
        add_label(panel, 25 + (12 * wx) * 4, row, "Hopper:", font10, &c_black);
        dev->rdr_input = (void *)add_textinput(panel, 25 + (12*wx) * 5, row, h+2, 40*wx,
                        dev->rdr_feed->file_name);
        add_button_callback(panel, ((12*wx) * 9), row,
                        2 * hx, 10 * wx, "EMPTY", NULL, &model2540_update, (void *)dev, 3,
                        font10, &c_white, &button_col);
        add_button_callback(panel, ((12*wx) * 10), row,
                        2 * hx, 10 * wx, "LOAD", NULL, &model2540_update, (void *)dev, 4,
                        font10, &c_white, &button_col);
        add_button_callback(panel, ((12*wx) * 11), row,
                        2 * hx, 10 * wx, "BLANK", NULL, &model2540_update, (void *)dev, 5,
                        font10, &c_white, &button_col);
        add_combo(panel, 25 + (12 * wx) * 12, row, h+2, 10 * wx, card_fmt_type, &dev->rdr_feed->mode,
                            font14, &c_black, &c_white);
        add_number(panel, 25 + (12*wx) * 13, row, h+2, 5 * wx, &dev->rdr_hop_cnt, font14, &c_black, &c_white);
        row += 3*hx;
        add_label(panel, 25 + (12 * wx) * 4, row, "R1:", font10, &c_black);
        dev->stack_input[0] = (void *)add_textinput(panel, 25 + (12*wx) * 5, row, hx+5, 40*wx,
                       dev->stack[0]->file_name);
        add_button_callback(panel, ((12*wx) * 9), row,
                        2 * hx, 10 * wx, "EMPTY", NULL, &model2540_update, (void *)dev, 11,
                        font10, &c_white, &button_col);
        add_button_callback(panel, ((12*wx) * 10), row,
                        2 * hx, 10 * wx, "SAVE", NULL, &model2540_update, (void *)dev, 12,
                        font10, &c_white, &button_col);
        add_combo(panel, 25 + (12 * wx) * 12, row, h+2, 10 * wx, card_fmt_type, &dev->stack[0]->mode,
                            font14, &c_black, &c_white);
        add_number(panel, 25 + (12*wx) * 13, row, h+2, 5 * wx, &dev->stk_cnt[0], font14,
                                &c_black, &c_white);
        row += 3*hx;
        add_label(panel, 25 + (12 * wx) * 4, row, "R2:", font10, &c_black);
        dev->stack_input[1] = (void *)add_textinput(panel, 25 + (12*wx) * 5, row, hx+5, 40*wx,
                       dev->stack[1]->file_name);
        add_button_callback(panel, ((12*wx) * 9), row,
                        2 * hx, 10 * wx, "EMPTY", NULL, &model2540_update, (void *)dev, 13,
                        font10, &c_white, &button_col);
        add_button_callback(panel, ((12*wx) * 10), row,
                        2 * hx, 10 * wx, "SAVE", NULL, &model2540_update, (void *)dev, 14,
                        font10, &c_white, &button_col);
        add_combo(panel, 25 + (12 * wx) * 12, row, h+2, 10 * wx, card_fmt_type, &dev->stack[1]->mode,
                            font14, &c_black, &c_white);
        add_number(panel, 25 + (12*wx) * 13, row, h+2, 5 * wx, &dev->stk_cnt[1], font14,
                                &c_black, &c_white);
        row += 3*hx;
        add_label(panel, 25 + (12 * wx) * 4, row, "RP3:", font10, &c_black);
        dev->stack_input[2] = (void *)add_textinput(panel, 25 + (12*wx) * 5, row, hx+5, 40*wx,
                       dev->stack[2]->file_name);
        add_button_callback(panel, ((12*wx) * 9), row,
                        2 * hx, 10 * wx, "EMPTY", NULL, &model2540_update, (void *)dev, 15,
                        font10, &c_white, &button_col);
        add_button_callback(panel, ((12*wx) * 10), row,
                        2 * hx, 10 * wx, "SAVE", NULL, &model2540_update, (void *)dev, 16,
                        font10, &c_white, &button_col);
        add_combo(panel, 25 + (12 * wx) * 12, row, h+2, 10 * wx, card_fmt_type, &dev->stack[2]->mode,
                            font14, &c_black, &c_white);
        add_number(panel, 25 + (12*wx) * 13, row, h+2, 5 * wx, &dev->stk_cnt[2], font14,
                                &c_black, &c_white);
    } else {
        add_area(panel, 20+(12*wx) * 3, 0, 200, 800, &c_white);
        add_indicator(panel, 20 + ((12*wx) * 0), 20 + ((3*hx) * 0),
                        2 * hx, 10 * wx, "VALIDITY", "CHECK", NULL, 0, font10,
                              &c_black, &eof_color_on, &eof_color_off);
        add_indicator(panel, 20 + ((12*wx) * 1), 20 + ((3*hx) * 0),
                        2 * hx, 10 * wx, "CHIP BOX", NULL, NULL, 0, font10,
                              &c_black, &eof_color_on, &eof_color_off);
        add_button_callback(panel, 20 + ((12*wx) * 2), 20 + ((3*hx) * 0),
                        2 * hx, 10 * wx, "START", NULL, &model2540_update, (void *)dev, 6,
                        font10, &c_white, &start_col);
        add_indicator(panel, 20 + ((12*wx) * 0), 20 + ((3*hx) * 1),
                        2 * hx, 10 * wx, "PUNCH", "CHECK", NULL, 0, font10,
                              &c_black, &eof_color_on, &eof_color_off);
        add_indicator(panel, 20 + ((12*wx) * 1), 20 + ((3*hx) * 1),
                        2 * hx, 10 * wx, "READY", NULL, &dev->pch_ready, 0, font10,
                              &c_black, &start_on, &start_col);
        add_button_callback(panel, 20 + ((12*wx) * 2), 20 + ((3*hx) * 1),
                        2 * hx, 10 * wx, "STOP", NULL, &model2540_update, (void *)dev, 7,
                        font10, &c_white, &stop_col);
        add_indicator(panel, 20 + ((12*wx) * 0), 20 + ((3*hx) * 2),
                        2 * hx, 10 * wx, "FEED", "STOP", NULL, 0, font10,
                              &c_black, &eof_color_on, &eof_color_off);
        add_blank(panel, 20 + ((12*wx) * 2), 20 + ((3*hx) * 2),
                        2 * hx, 10 * wx, &eof_color_off);
        add_blank(panel, 20 + ((12*wx) * 1), 20 + ((3*hx) * 2),
                        2 * hx, 10 * wx, &eof_color);

        h = hx;
        w = wx * 15;
        row = 20;
        add_label(panel, 25 + (12 * wx) * 3, row, "Hopper:", font10, &c_black);
        dev->pch_input = (void *)add_textinput(panel, 25 + (12*wx) * 4, row, h+2, 40*wx,
                        dev->pch_feed->file_name);
        add_button_callback(panel, 20 + ((12*wx) * 8), row,
                        2 * hx, 10 * wx, "EMPTY", NULL, &model2540_update, (void *)dev, 8,
                        font10, &c_white, &button_col);
        add_button_callback(panel, 20 + ((12*wx) * 9), row,
                        2 * hx, 10 * wx, "LOAD", NULL, &model2540_update, (void *)dev, 9,
                        font10, &c_white, &button_col);
        add_button_callback(panel, 20 + ((12*wx) * 10), row,
                        2 * hx, 10 * wx, "BLANK", NULL, &model2540_update, (void *)dev, 10,
                        font10, &c_white, &button_col);
        add_combo(panel, 25 + (12 * wx) * 11, row, h+2, 10 * wx, card_fmt_type, &dev->pch_feed->mode,
                            font14, &c_black, &c_white);
        add_number(panel, 25 + (12*wx) * 12, row, h+2, 5 * wx, &dev->pch_hop_cnt, font14, &c_black, &c_white);
        row += 3*hx;
        add_label(panel, 25 + (12 * wx) * 3, row, "P1:", font10, &c_black);
        dev->stack_input[4] = (void *)add_textinput(panel, 25 + (12*wx) * 4, row, hx+5, 40*wx,
                       dev->stack[4]->file_name);
        add_button_callback(panel, 20 + ((12*wx) * 8), row,
                        2 * hx, 10 * wx, "EMPTY", NULL, &model2540_update, (void *)dev, 19,
                        font10, &c_white, &button_col);
        add_button_callback(panel, 20 + ((12*wx) * 9), row,
                        2 * hx, 10 * wx, "SAVE", NULL, &model2540_update, (void *)dev, 20,
                        font10, &c_white, &button_col);
        add_combo(panel, 25 + (12 * wx) * 11, row, h+2, 10 * wx, card_fmt_type, &dev->stack[4]->mode,
                            font14, &c_black, &c_white);
        add_number(panel, 25 + (12*wx) * 12, row, h+2, 5 * wx, &dev->stk_cnt[4], font14,
                                &c_black, &c_white);
        row += 3*hx;
        add_label(panel, 25 + (12 * wx) * 3, row, "P2:", font10, &c_black);
        dev->stack_input[3] = (void *)add_textinput(panel, 25 + (12*wx) * 4, row, hx+5, 40*wx,
                       dev->stack[3]->file_name);
        add_button_callback(panel, 20 + ((12*wx) * 8), row,
                        2 * hx, 10 * wx, "EMPTY", NULL, &model2540_update, (void *)dev, 17,
                        font10, &c_white, &button_col);
        add_button_callback(panel, 20 + ((12*wx) * 9), row,
                        2 * hx, 10 * wx, "SAVE", NULL, &model2540_update, (void *)dev, 18,
                        font10, &c_white, &button_col);
        add_combo(panel, 25 + (12 * wx) * 11, row, h+2, 10 * wx, card_fmt_type, &dev->stack[3]->mode,
                            font14, &c_black, &c_white);
        add_number(panel, 25 + (12*wx) * 12, row, h+2, 5 * wx, &dev->stk_cnt[3], font14,
                                &c_black, &c_white);
    }
    return (void *)panel;
}

