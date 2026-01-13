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
#include "widgets.h"
#include "label.h"
#include "indicator.h"
#include "button.h"
#include "number.h"
#include "area.h"
#include "light.h"
#include "text.h"
#include "combo.h"
#include "checkbox.h"
#include "model2415.h"

#include "model2415.xpm"
#include "tape_images.xpm"

SDL_Texture *model2415_img = NULL;
SDL_Texture *tape_images_img = NULL;

void
model2415_draw(struct _device *unit, void *rend, int u)
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
    int          x = unit->rect[u].x;
    int          y = unit->rect[u].y;
    char         buf[100];
    float        p;


        if (ctx->tape[u]->file_name != NULL) {
            rect2.x = 0;
            rect2.y = 0;
            rect2.w = unit->rect[u].w;
            rect2.h = unit->rect[u].h;
            rect.x = x;
            rect.y = y;
            rect.w = unit->rect[u].w;
            rect.h = unit->rect[u].h;
            SDL_RenderCopy(render, model2415_img, &rect2, &rect);
            sprintf(buf, "%1X%02X", ctx->chan, ctx->addr + u);
            text = TTF_RenderText_Solid(font14, buf, c_black);
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
            if (tape_is_selected(ctx->tape[u])) {
                SDL_RenderCopy(render, model2415_img, &rect, &rect2);
            }
            rect.x += rect.w;
            rect2.x += rect.w;
            /* Draw ready */
            if (tape_ready(ctx->tape[u])) {
                SDL_RenderCopy(render, model2415_img, &rect, &rect2);
            }
            rect.x += rect.w;
            rect2.x += rect.w;
            /* Draw file protect */
            if (tape_ring(ctx->tape[u]) == 0) {
               SDL_RenderCopy(render, model2415_img, &rect, &rect2);
            }
            /* Draw supply reel */
            reel = tape_supply_image(ctx->tape[u], &j);
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
            switch (ctx->supply_color[u]) {
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
            if (ctx->supply_label[i]) {
                if (rect.x > 1125)
                    rect.x -= (75 * 2);
                else
                    rect.x += (75 * 2);
                SDL_RenderCopy(render, tape_images_img, &rect, &rect2);
            }
            /* Draw take up reel */
            reel = tape_takeup_image(ctx->tape[u], &k);
            rect2.x = x + 107;
            rect.x = reel->x+4;
            rect.y = reel->y+4;
            SDL_RenderCopy(render, tape_images_img, &rect, &rect2);
            switch (ctx->takeup_color[u]) {
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
            if (ctx->takeup_label[u]) {
                if (rect.x > 1125)
                    rect.x -= (75 * 2);
                else
                    rect.x += (75 * 2);
                SDL_RenderCopy(render, tape_images_img, &rect, &rect2);
            }
        } else {
            rect2.x = 250;
            rect2.y = 0;
            rect2.w = unit->rect[u].w;
            rect2.h = unit->rect[u].h;
            rect.x = x;
            rect.y = y;
            rect.w = unit->rect[u].w;
            rect.h = unit->rect[u].h;
            SDL_RenderCopy(render, model2415_img, &rect2, &rect);
            sprintf(buf, "%1X%02X", ctx->chan, ctx->addr + u);
            text = TTF_RenderText_Solid(font14, buf, c_black);
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
            switch (ctx->takeup_color[u]) {
            case 0: rect.x = (75 * 15) + 4; break;
            case 1: rect.x = (75 * 3) + 4; break;
            case 2: rect.x = (75 * 7) + 4; break;
            }
            rect.y = 4;
            SDL_RenderCopy(render, tape_images_img, &rect, &rect2);
            if (ctx->takeup_label[u]) {
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

struct _2415_callback_args {
    struct _device        *unit;
    int                   unit_num;
    int                   format_type;
    int                   ring;
    int                   density;
    Widget                file_text;
};

static void
model2415_update(void *arg, int iarg)
{
    struct _2415_callback_args *data = (struct _2415_callback_args *)arg;
    struct _device *unit = (struct _device *)data->unit;
    struct _2415_context *ctx = (struct _2415_context *)unit->dev;
    char                 *file_name;

    file_name = get_textbuffer(data->file_text);
    switch (iarg) {
    case 0:  /* Load Rewind */
          /* If not offline, do nothing. */
          if ((ctx->tape[data->unit_num]->format & ONLINE) != 0) {
              break;
          }
          /* If different tape, detach current one */
          if ((ctx->tape[data->unit_num]->format & ATTACHED) != 0 &&
              strcmp(ctx->tape[data->unit_num]->file_name, file_name) != 0) {
                  tape_detach(ctx->tape[data->unit_num]);
          }

          /* If not attached load new one */
          if ((ctx->tape[data->unit_num]->format & ATTACHED) == 0) {
              tape_attach(ctx->tape[data->unit_num], file_name,
                                  data->format_type, data->ring, data->density);
          }

          /* Start off rewind. */
          tape_start_rewind(ctx->tape[data->unit_num]);
          if (ctx->rew_delay == 0)
              ctx->rew_delay = REWIND_DELAY;
          ctx->rew_flags |= 1 << data->unit_num;
          add_event(unit, model2415_rewind_callback, REWIND_DELAY,
                    (void *)ctx->tape[data->unit_num], data->unit_num);
          break;

    case 1:  /* Start */
          if ((ctx->tape[data->unit_num]->format & ONLINE) == 0 &&
              (ctx->tape[data->unit_num]->fd >= 0)) {
              ctx->tape[data->unit_num]->format |= ONLINE;
              ctx->rdy_flags |= 1 << data->unit_num;
          }
          break;

    case 2:  /* Unload */
          if ((ctx->tape[data->unit_num]->format & ONLINE) == 0) {
              tape_start_rewind(ctx->tape[data->unit_num]);
              if (ctx->rew_delay == 0)
                  ctx->rew_delay = REWIND_DELAY;
              ctx->rew_flags |= 1 << data->unit_num;
              ctx->run_flags |= 1 << data->unit_num;
              add_event(unit, model2415_rewind_callback, REWIND_DELAY,
                        (void *)ctx->tape[data->unit_num], data->unit_num);
          }
          break;

    case 3:  /* Reset */
          ctx->tape[data->unit_num]->format &= ~ONLINE;
          break;

    case 4:  /* end */
          ctx->tape[data->unit_num]->pos_frame = max_tape_length - 1;
          break;
    }
}

static SDL_Color select_on = {0x96, 0x8f, 0x85};   /* White */
static SDL_Color select_off = {0xfd, 0xfd, 0xfd};
static SDL_Color ready = {0x7f,0xc0, 0x86};        /* Green */
static SDL_Color not_ready = {0x0c, 0x2e, 0x30};
static SDL_Color ring_color =  {0xd0, 0x08, 0x42};   /* Red */
static SDL_Color noring_color =  {0xff, 0x00, 0x4a};
static SDL_Color tape_ind_on =  {0xff, 0xfd, 0x5e};  /* White */
static SDL_Color tape_ind_off =  {0xdd, 0xdc, 0x8a};
static SDL_Color load_color = {0x0a, 0x52, 0x9a};       /* Blue */
static SDL_Color start_color = {0x0c, 0x2e, 0x30};      /* Green */
static SDL_Color reset_color = {0xc8, 0x3a, 0x30};      /* Blue */
static SDL_Color eom_color = {0x0a, 0x52, 0x9a};        /* Blue */

void *
model2415_control(struct _device *unit, int u)
{
    struct _popup  *popup;
    struct _2415_context *ctx = (struct _2415_context *)unit->dev;
    struct _2415_callback_args *args;
    Panel   panel;
    SDL_Surface *text;
    int    i, j;
    int    w, h;
    int    wx, hx;
    int    row;
    char   buffer[100];

    if (TTF_SizeText(font10, "M", &wx, &hx) != 0) {
        return NULL;
    }
    if (TTF_SizeText(font14, "M", NULL, &h) != 0) {
        return NULL;
    }
    sprintf(buffer, "IBM2415 Dev 0x'%03X'", ctx->addr + u);
    if ((args = (struct _2415_callback_args *)calloc(1, sizeof(struct _2415_callback_args))) == NULL) {
        return NULL;
    }

    if ((panel = create_window(buffer, 900, h*15, 1)) == NULL) {
        free(args);
        return NULL;
    }

    args->unit = unit;
    args->unit_num = u;
    args->format_type = (ctx->tape[u]->format & TAPE_FMT) >> TAPE_FMT_V;
    args->density = ctx->tape[u]->format;

    add_area(panel, 0, 0, h*15, 800, &c_white);
    add_indicator(panel, 20 + ((12*wx) * 0), 20 + ((3*hx) * 0),
                    2 * hx, 10 * wx, "SELECT", NULL, &ctx->tape[u]->format, 8,
                    font10, &c_black, &select_on, &select_off);
    add_indicator(panel, 20 + ((12*wx) * 1), 20 + ((3*hx) * 0),
                    2 * hx, 10 * wx, "READY", NULL, &ctx->tape[u]->format, 9,
                    font10, &c_black, &ready, &not_ready);
    add_indicator(panel, 20 + ((12*wx) * 2), 20 + ((3*hx) * 0),
                    2 * hx, 10 * wx, "FILE", "PROTECT", &ctx->tape[u]->format, 2,
                    font10, &c_white, &ring_color, &noring_color);
    add_indicator(panel, 20 + ((12*wx) * 3), 20 + ((3*hx) * 0),
                    2 * hx, 10 * wx, "TAPE", "INDICATOR", &ctx->tape[u]->format, 3,
                    font10, &c_black, &tape_ind_on, &tape_ind_off);
    add_button_callback(panel, 20 + ((12*wx) * 0), 20 + ((3*hx) * 1),
                    2 * hx, 10 * wx, "LOAD", "REWIND", &model2415_update, (void *)args, 0,
                    font10, &c_black, &c_blue);
    add_button_callback(panel, 20 + ((12*wx) * 1), 20 + ((3*hx) * 1),
                    2 * hx, 10 * wx, "START", NULL, &model2415_update, (void *)args, 1,
                    font10, &c_black, &c_green);
    add_button_callback(panel, 20 + ((12*wx) * 2), 20 + ((3*hx) * 1),
                    2 * hx, 10 * wx, "UNLOAD", "REWIND", &model2415_update, (void *)args, 2,
                    font10, &c_black, &c_blue);
    add_button_callback(panel, 20 + ((12*wx) * 3), 20 + ((3*hx) * 1),
                    2 * hx, 10 * wx, "RESET", NULL, &model2415_update, (void *)args, 3,
                    font10, &c_black, &reset_color);
    add_button_callback(panel, 20 + ((12*wx) * 0), 20 + ((3*hx) * 2),
                    2 * hx, 10 * wx, "EOM", NULL, &model2415_update, (void *)args, 4,
                    font10, &c_black, &eom_color);

    row = 20;

    add_label(panel, 25 + (12 * wx) * 4, row, "Tape:", font14, &c_black);
    args->file_text = add_textinput(panel, 25 + (12*wx) * 5, row, h+2, 45*wx,
                    ctx->tape[u]->file_name);

    row += h+10;
    add_label(panel, 25 + (12 * wx) * 4, row, "Type:", font14, &c_black);
    args->format_type = (ctx->tape[u]->format & TAPE_FMT) >> TAPE_FMT_V;
    add_combo(panel, 25 + (12 * wx) * 5, row, h, 12 * wx, tape_format_type,
                        &args->format_type, font14, &c_black, &c_white);

    row += h+10;
    add_label(panel, 25 + (12 * wx) * 4, row, "Density:", font14, &c_black);
    args->density = (ctx->tape[u]->format & DENSITY_MASK) >> DENSITY_V;
    add_combo(panel, 25 + (12 * wx) * 5, row, h, 12 * wx, tape_density_type, &args->density,
                        font14, &c_black, &c_white);
    row += h+10;
    add_label(panel, 25 + (12 * wx) * 4, row, "7 Track:", font14, &c_black);
    add_checkbox(panel, 30 + (12 * wx) * 5, row, h, wx, NULL,
                       &ctx->tape[u]->format, TRACK9_V, 1, font10, &c_black, &c_white);

    row += h+10;
    args->ring = (ctx->tape[u]->format & WRITE_RING) == 0;
    add_label(panel, 25 + (12 * wx) * 4, row, "Ring:", font14, &c_black);
    add_checkbox(panel, 30 + (12 * wx) * 5, row, h,  wx, NULL, &args->ring,
                        0, 1, font10, &c_black, &c_white);
    row += h+10;
    add_label(panel, 25 + (12 * wx) * 4, row, "Supply:", font14, &c_black);
    add_combo(panel, 25 + (12 * wx) * 5, row, h, 12 * wx, tape_reel_color, &ctx->supply_color[u],
                        font14, &c_black, &c_white);
    add_checkbox(panel, 25 + (12 * wx) * 7, row, h, 12 * wx, "Label:", &ctx->supply_label[u],
                        0, 1, font14, &c_black, &c_white);
    row += h+10;
    add_label(panel, 25 + (12 * wx) * 4, row, "Takeup:", font14, &c_black);
    add_combo(panel, 25 + (12 * wx) * 5, row, h, 12 * wx, tape_reel_color, &ctx->takeup_color[u],
                        font14, &c_black, &c_white);
    add_checkbox(panel, 25 + (12 * wx) * 7, row, h, 12 * wx, "Label:", &ctx->takeup_label[u],
                        0, 1, font14, &c_black, &c_white);
    return (void *)panel;
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

