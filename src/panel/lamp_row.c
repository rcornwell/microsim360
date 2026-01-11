/*
 * microsim360 - GUI Draws row of lamp indicator.
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


#include <stdint.h>
#include "lamp_row.h"
#include "lamp.h"

struct _lamp_row_t {
    SDL_Rect     rect_label1[64];
    SDL_Rect     rect_label2[64];
    SDL_Rect     rect_label3[64];
    SDL_Rect     rect_lamp[64];
    SDL_Texture *label1[64];
    SDL_Texture *label2[64];
    SDL_Texture *label3[64];
    int          color[64];
    uint16_t    *value[64];
    int          shft[64];
    int          num;
};


static void
display_lamp_row(Widget wid, SDL_Renderer *render)
{
   struct _lamp_row_t *row = (struct _lamp_row_t *)wid->data;
   SDL_Rect   rect;
   int        i;
   uint16_t   bit;

   rect.w = 15;
   rect.h = 15;
   for (i = 0; i < row->num; i++) {
       if (row->label1[i] != NULL) {
          SDL_RenderCopy( render, row->label1[i], NULL, &row->rect_label1[i]);
       }
       if (row->label2[i] != NULL) {
          SDL_RenderCopy( render, row->label2[i], NULL, &row->rect_label2[i]);
       }
       if (row->label3[i] != NULL) {
          SDL_RenderCopy( render, row->label3[i], NULL, &row->rect_label3[i]);
       }
       bit = LAMP_TEST;
       if (row->value[i] != NULL) {
           bit = (*row->value[i] >> row->shft[i]) & 1;
       }
       rect.x = row->color[i] * 15;
       rect.y = (int)(15 * bit);
       SDL_RenderCopy(render, lamps, &rect, &row->rect_lamp[i]);
   }
}

static void
close_lamp_row(Widget wid)
{
   struct _lamp_row_t *row = (struct _lamp_row_t *)wid->data;
   int        i;

   for (i = 0; i < row->num; i++) {
       if (row->label1[i] != NULL) {
          SDL_DestroyTexture( row->label1[i]);
       }
       if (row->label2[i] != NULL) {
          SDL_DestroyTexture( row->label2[i]);
       }
       if (row->label3[i] != NULL) {
          SDL_DestroyTexture( row->label3[i]);
       }
   }
   free(wid->data);
}

/*
 * Add a row of lamps to window
 */
Widget
add_lamp_row(Panel win, int x, int y, Lamp_row row, int num,
               int *offsets, TTF_Font *font, SDL_Color *lab_color)
{
   Widget nwid;
   SDL_Surface *surf;
   struct _lamp_row_t *row_data;
   int                 px, py, wh, hh, wc;
   int                 wx, hx, k;
   int                 i;
   Uint32              f;


   if (TTF_SizeText(font, "M", &wx, &hx) != 0) {
        return NULL;
   }

   if ((nwid = (Widget)calloc(1, sizeof(struct _widget_t))) == NULL) {
       return NULL;
   }

   if ((row_data = (struct _lamp_row_t *)calloc(1, sizeof(struct _lamp_row_t))) == NULL) {
       free(nwid);
       return NULL;
   }

   px = x;
   row_data->num = num;
   for (i = 0; i < num; i++) {
       wc = 0;
       py = y;
       px += offsets[i];

       if (row[i].label1 != NULL) {
           surf = TTF_RenderText_Blended(font, row[i].label1, *lab_color);
           row_data->label1[i] = SDL_CreateTextureFromSurface(win->render, surf);
           SDL_QueryTexture(row_data->label1[i], &f, &k, &wh, &hh);
           SDL_FreeSurface(surf);
           row_data->rect_label1[i].x = px;
           row_data->rect_label1[i].y = py;
           row_data->rect_label1[i].w = wh;
           row_data->rect_label1[i].h = hh;
           wc = wh;
       }
       py += hx-2;
       if (row[i].label2 != NULL) {
           surf = TTF_RenderText_Blended(font, row[i].label2, *lab_color);
           row_data->label2[i] = SDL_CreateTextureFromSurface(win->render, surf);
           SDL_QueryTexture(row_data->label2[i], &f, &k, &wh, &hh);
           SDL_FreeSurface(surf);
           row_data->rect_label2[i].x = px;
           row_data->rect_label2[i].y = py;
           row_data->rect_label2[i].w = wh;
           row_data->rect_label2[i].h = hh;
           if (wh > wc) {
               wc = wh;
           }
       }
       py += hx-2;
       if (row[i].label3 != NULL) {
           surf = TTF_RenderText_Blended(font, row[i].label3, *lab_color);
           row_data->label3[i] = SDL_CreateTextureFromSurface(win->render, surf);
           SDL_QueryTexture(row_data->label3[i], &f, &k, &wh, &hh);
           SDL_FreeSurface(surf);
           row_data->rect_label3[i].x = px;
           row_data->rect_label3[i].y = py;
           row_data->rect_label3[i].w = wh;
           row_data->rect_label3[i].h = hh;
           if (wh > wc) {
               wc = wh;
           }
       }
       py += hx-2;
       row_data->rect_label1[i].x += (wc/2) - (row_data->rect_label1[i].w/2);
       row_data->rect_label2[i].x += (wc/2) - (row_data->rect_label2[i].w/2);
       row_data->rect_label3[i].x += (wc/2) - (row_data->rect_label3[i].w/2);
       if (wc > 20) {
           row_data->rect_label1[i].x -= 7;
           row_data->rect_label2[i].x -= 7;
           row_data->rect_label3[i].x -= 7;
       }
       row_data->value[i] = row[i].value;
       row_data->shft[i] = row[i].shft;
       row_data->color[i] = row[i].color;
       row_data->rect_lamp[i].x = px;
       row_data->rect_lamp[i].y = py + 5;
       row_data->rect_lamp[i].h = 15;
       row_data->rect_lamp[i].w = 15;
   }

   nwid->rect.x = x;
   nwid->rect.y = y;
   nwid->rect.w = px - x;
   nwid->rect.h = py + 20;
   nwid->data = (void *)row_data;
   nwid->draw = display_lamp_row;
   nwid->close = close_lamp_row;
   add_widget(win, nwid);
   return nwid;
}

