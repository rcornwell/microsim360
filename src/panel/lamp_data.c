/*
 * microsim360 - GUI Draws row of data indicators with parity bits.
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
#include "lamp_data.h"
#include "lamp.h"
#include "area.h"
#include "xlat.h"

struct _lamp_data_t {
    int          h[5];
    int          w[5];
    SDL_Texture *label[5];
    int          color;
    uint32_t    *value;
    int          start;
    int          offsets[36];
};



static void
display_lamp_data(Widget wid, SDL_Renderer *render)
{
   struct _lamp_data_t *row = (struct _lamp_data_t *)wid->data;
   SDL_Rect   rect;
   SDL_Rect   rect_lamp;
   SDL_Rect   rect_label;
   int        i, j;
   int        parity;
   uint32_t   bit;

   rect_lamp.x = wid->rect.x;
   rect_lamp.y = wid->rect.y;
   rect_lamp.w = 15;
   rect_lamp.h = 15;
   rect.x = row->color * 15;
   rect.w = 15;
   rect.h = 15;
   if (row->value == NULL) {
       return;
   }

   j = 0;
   for (i = 31; i >= 0; i--) {
       parity = 0;
       switch (i) {
       case 31:
              bit = odd_parity[(*row->value >> 24) & 0xff] != 0;
              parity = 1;
              break;
       case 23:
              bit = odd_parity[(*row->value >> 16) & 0xff] != 0;
              parity = 2;
              break;
       case 15:
              bit = odd_parity[(*row->value >> 8) & 0xff] != 0;
              parity = 3;
              break;
       case 7:
              bit = odd_parity[*row->value & 0xff] != 0;
              parity = 4;
              break;
       }

       if (parity != 0) {
           rect_lamp.x = row->offsets[j++];
           if (i <= row->start) {
               bit |= LAMP_TEST;
               rect_label.x = rect_lamp.x;
               rect_label.y = rect_lamp.y + 15;
               rect_label.h = 15;
               rect_label.w = 15;
               rect.y = (int)(15 * bit);
               SDL_RenderCopy(render, lamps, &rect, &rect_lamp);
               if (row->label[parity] != NULL) {
                   SDL_RenderCopy( render, row->label[parity], NULL, &rect_label);
               }
           }
       }
       rect_lamp.x = row->offsets[j++];
       if (i <= row->start) {
           bit = ((*row->value >> i) & 1) | LAMP_TEST;
           rect.y = (int)(15 * bit);
           SDL_RenderCopy(render, lamps, &rect, &rect_lamp);
       }
   }
}

static void
close_lamp_data(Widget wid)
{
   struct _lamp_data_t *row = (struct _lamp_data_t *)wid->data;
   int        i;

   for (i = 1; i < 5; i++) {
      if (row->label[i] != NULL) {
         SDL_DestroyTexture( row->label[i]);
      }
   }
   free(wid->data);
}

static char *labels[] = { NULL, "0-7", "8-15", "16-23", "24-31" };

/*
 * Add a row of data lamps to window
 */
Widget
add_lamp_data(Panel win, int x, int y, uint32_t *value, int start,
               int *offsets, int color, TTF_Font *font, SDL_Color *lab_color)
{
   Widget nwid;
   SDL_Surface *surf;
   struct _lamp_data_t *row_data;
   int                 s;
   int                 wh, hh;
   int                 wx, hx, k;
   int                 i;
   int                 pos;
   int                 j;
   Uint32              f;


   if (TTF_SizeText(font, "M", &wx, &hx) != 0) {
        return NULL;
   }

   if ((nwid = (Widget)calloc(1, sizeof(struct _widget_t))) == NULL) {
       return NULL;
   }

   if ((row_data = (struct _lamp_data_t *)calloc(1, sizeof(struct _lamp_data_t))) == NULL) {
       free(nwid);
       return NULL;
   }

   row_data->start = start;
   row_data->value = value;
   row_data->color = color;
   for (i = 1; i < 5; i++) {
       surf = TTF_RenderText_Blended(font, labels[i], *lab_color);
       row_data->label[i] = SDL_CreateTextureFromSurface(win->render, surf);
       SDL_QueryTexture(row_data->label[i], &f, &k, &wh, &hh);
       SDL_FreeSurface(surf);
       row_data->w[i] = wh;
       row_data->h[i] = hh;
   }


   pos = x;
   s = pos;
   j = 0;
   for (i = 0; i < 36; i++) {
         pos += offsets[i];
         row_data->offsets[i] = pos;
   }
   pos = x;
   s = pos;
   for (i = 31; i >= 0; i--) {
       pos += offsets[j++];
       switch (i) {
       case 31:   /* Parity, start white */
              pos += offsets[j++];
              s = pos + 7;
              break;
       case 30:
       case 29:
              break;
       case 28: /* End white */
              if (i <= start) {
                  add_area(win, s, y + 5, 4, pos - s + 10, &c_white);
              }
              break;
       case 27: /* Start black */
              s = pos + 7;
              break;
       case 26:
       case 25:
              break;
       case 24:  /* End black */
              if (i <= start) {
                  add_area(win, s, y + 5, 4, pos - s + 10, &c_black);
              }
              break;
       case 23:  /* Parity start white */
              pos += offsets[j++];
              s = pos + 7;
              break;
       case 22:
       case 21:
              break;
       case 20: /* End white */
              if (i <= start) {
                  add_area(win, s, y + 5, 4, pos - s + 10, &c_white);
              }
              break;
       case 19: /* Start black */
              s = pos + 7;
              break;
       case 18:
       case 17:
              break;
       case 16: /* End black */
              if (i <= start) {
                  add_area(win, s, y + 5, 4, pos - s + 10, &c_black);
              }
              break;
       case 15: /* Parity, start white */
              pos += offsets[j++];
              s = pos + 7;
              break;
       case 14:
       case 13:
              break;
       case 12: /* End white */
              if (i <= start) {
                  add_area(win, s, y + 5, 4, pos - s + 10, &c_white);
              }
              break;
       case 11: /* start black */
              s = pos + 7;
              break;
       case 10:
       case  9:
              break;
       case  8: /* End black */
              if (i <= start) {
                  add_area(win, s, y + 5, 4, pos - s + 10, &c_black);
              }
              break;
       case  7: /* Parity start white */
              pos += offsets[j++];
              s = pos + 7;
              break;
       case  6:
       case  5:
              break;
       case  4: /* End white */
              if (i <= start) {
                  add_area(win, s, y + 5, 4, pos - s + 10, &c_white);
              }
              break;
       case  3: /* Start black */
              s = pos + 7;
              break;
       case  2:
       case  1:
              break;
       case  0: /* End black */
              if (i < start) {
                  add_area(win, s, y + 5, 4, pos - s + 10, &c_black);
              }
              break;
       }
   }

   nwid->rect.x = x;
   nwid->rect.y = y;
   nwid->rect.w = pos - x;
   nwid->rect.h = 20 + hh;
   nwid->data = (void *)row_data;
   nwid->draw = display_lamp_data;
   nwid->close = close_lamp_data;
   add_widget(win, nwid);
   return nwid;
}

