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
#include "widgets.h"
#include "area.h"
#include "indicator.h"
#include "button.h"
#include "number.h"
#include "text.h"
#include "label.h"
#include "combo.h"
#include "light.h"
#include "logger.h"
#include "event.h"
#include "xlat.h"
#include "model1443.h"
#include "model1443.xpm"


SDL_Texture *model1443_img = NULL;
static SDL_Color     cx = {0x10, 0x83, 0xd9};

void
model1443_init(struct _device *unit, void *rend)
{
     if (model1443_img == NULL) {
         SDL_Renderer *render = (SDL_Renderer *)rend;
         SDL_Surface *text;

         text = IMG_ReadXPMFromArray(model1443_xpm);
         model1443_img = SDL_CreateTextureFromSurface(render, text);
         SDL_SetTextureBlendMode(model1443_img, SDL_BLENDMODE_BLEND);
         SDL_FreeSurface(text);
     }
}


void
model1443_draw(struct _device *unit, void *rend, int u)
{
    struct _1443_context *ctx = (struct _1443_context *)unit->dev;
    SDL_Renderer *render = (SDL_Renderer *)rend;
    int          i, j;
    SDL_Rect     rect;
    SDL_Rect     rect2;
    SDL_Surface *text;
    SDL_Texture *txt;
    int           x = unit->rect[u].x;
    int           y = unit->rect[u].y;
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
    text = TTF_RenderText_Solid(font14, buf, c_black);
    txt = SDL_CreateTextureFromSurface(render, text);
    SDL_FreeSurface(text);
    SDL_QueryTexture(txt, &i, &j, &rect2.w, &rect2.h);
    rect2.x = rect.x+200;
    rect2.y = y + 20;
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

struct _1443_callback_args {
    struct _device        *unit;
    Widget                file_text;
};


static void
 model1443_update(void *args, int iarg)
{
    struct _1443_callback_args *data = (struct _1443_callback_args *)args;
    struct _device *unit = (struct _device *)data->unit;
    struct _1443_context *ctx = (struct _1443_context *)unit->dev;
    char *name;
    int r;

    switch (iarg) {
    case 0:  /* Start Key */
          if (ctx->state == STATE_IDLE && ctx->file != NULL) {
              ctx->status |= SNS_DEVEND;
              ctx->data_end = 1;
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
          name = get_textbuffer(data->file_text);
          ctx->file = fopen(name, "a");
          if (ctx->file != NULL) {
             ctx->row = 0;
             if ((ctx->file_name = (char *)malloc(strlen(name)+1)) == NULL) {
                 fclose(ctx->file);
                 ctx->file = NULL;
                 break;
             }
             strcpy(ctx->file_name, name);
             ctx->form = 0;
          }
          break;
    }
}


static char *type_label[] = { "LEGACY", "STD1", NULL};

static SDL_Color start_col = {0x0c, 0x2e, 0x30};
static SDL_Color check_col = {0xff, 0xfd, 0x5e};
static SDL_Color stop_col = {0xc8, 0x3a, 0x30};
static SDL_Color space_col = {0xdd, 0xdc, 0x8a};
static SDL_Color ready_col = {0xd8, 0xcb, 0x72};

void *
model1443_control(struct _device *unit, int u, int x, int y)
{
    struct _popup *popup;
    struct _1443_context *ctx = (struct _1443_context *)unit->dev;
    struct _1443_callback_args *args;
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
    if ((args = (struct _1443_callback_args *)calloc(1, sizeof(struct _1443_callback_args))) == NULL) {
        return NULL;
    }
    args->unit = unit;
    sprintf(buffer, "IBM1443 Dev 0x'%03X'", ctx->addr);
    if ((panel = create_window(buffer, 900, h*10, 1)) == NULL) {
        free(args);
        return NULL;
    }

    add_area(panel, 0, 0, 200, 360, &cx);
    add_area(panel, 360, 0, 200, 760, &c_white);
    add_button_callback(panel, 20 + ((12*wx) * 0), 20 + ((hx*3) * 0),
                    2 * hx, 10 * wx, "START", NULL, &model1443_update, (void *)args, 0,
                    font10, &c_black, &start_col);
    add_button_callback(panel, 20 + ((12*wx) * 1), 20 + ((hx*3) * 0),
                    2 * hx, 10 * wx, "CHECK", "RESET", &model1443_update, (void *)args, 1,
                    font10, &c_black, &check_col);
    add_button_callback(panel, 20 + ((12*wx) * 2), 20 + ((hx*3) * 0),
                    2 * hx, 10 * wx, "STOP", NULL, &model1443_update, (void *)args, 2,
                    font10, &c_black, &stop_col);
    add_light(panel, 20 + ((12 * wx) * 3), 20 + ((hx*3)*0),
                  "PRINT", "READY", &ctx->ready, 0, font10, &ready_col, &c_off);
    add_light(panel, 20 + ((12 * wx) * 4), 20 + ((hx*3)*0),
                  "PRINT", "CHECK", NULL, 0, font10, &ready_col, &c_off);
    add_button_callback(panel, 20 + ((12*wx) * 0), 20 + ((hx*3) * 1),
                    2 * hx, 10 * wx, "CARRIAGE", "SPACE", &model1443_update, (void *)args, 3,
                    font10, &c_black, &space_col);
    add_button_callback(panel, 20 + ((12*wx) * 1), 20 + ((hx*3) * 1),
                    2 * hx, 10 * wx, "CARRIAGE", "RESTORE", &model1443_update, (void *)args, 4,
                    font10, &c_black, &space_col);

    add_button_callback(panel, 20 + ((12*wx) * 2), 20 + ((hx*3) * 1),
                    2 * hx, 10 * wx, "SINGLE", "CYCLE", &model1443_update, (void *)args, 5,
                    font10, &c_black, &space_col);
    add_light(panel, 20 + ((12*wx) * 3), 20 + ((hx*3) * 1), "END OF", "FORMS",
                    NULL, 0, font10, &ready_col, &c_off);
    add_light(panel, 20 + ((12*wx) * 3), 20 + ((hx*3) * 2), "FORMS", "CHECK",
                    NULL, 0, font10, &ready_col, &c_off);
    add_light(panel, 20 + ((12*wx) * 4), 20 + ((hx*3) * 2), "SYNC", "CHECK",
                    NULL, 0,
                    font10, &ready_col, &c_off);
    add_button_callback(panel, 20 + ((12*wx) * 10), 20 + ((hx*3) * 0),
                    2 * hx, 10 * wx, "SET", NULL, &model1443_update, (void *)args, 6,
                    font10, &c_black, &space_col);

    row = 20;
    add_label(panel, 25 + (12 * wx) * 5, row, "Paper:", font14, &c_black);
    args->file_text = add_textinput(panel, 25 + (12*wx) * 6, row, h+2, 45*wx,
                    ctx->file_name);
    row += (3*hx)+3;

    add_label(panel, 25 + (12 * wx) * 5, row, "Row:", font14, &c_black);
    add_number(panel, 25 + (12 * wx) * 6, row, h+2, 10*wx, &ctx->row, font14, &c_black, &c_white);
    row += (3*hx) + 2;
    add_label(panel, 25 + (12 * wx) * 5, row, "FCB:", font14, &c_black);
    add_combo(panel, 25 + (12 * wx) * 6, row, h+2, 12 * wx, type_label, &ctx->fcb_num,
                        font14, &c_black, &c_white);

    return (void *)panel;
}

