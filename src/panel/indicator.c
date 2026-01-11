/*
 * microsim360 - GUI Draws a indicator light.
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

#include "indicator.h"

struct _indicator_t {
    SDL_Rect     recth;
    SDL_Rect     rectl;
    SDL_Texture *upper;
    SDL_Texture *lower;
    SDL_Color    color[2];
    SDL_Color    text_color;
    int          type;
    int         *value;
    int          shft;
    int          turn_off;
};


static void
display_indicator(Widget wid, SDL_Renderer *render)
{
   struct _indicator_t *ind = (struct _indicator_t *)wid->data;
   int     x, y, h, w;
   int     value = 0;

   if (ind->value != NULL) {
       value = (*ind->value >> ind->shft) & 1;
   }

   SDL_SetRenderDrawColor(render, ind->color[value].r, ind->color[value].g, ind->color[value].b, 0xff);
   SDL_RenderFillRect(render, &wid->rect);
   if (ind->upper != NULL) {
       SDL_RenderCopy(render, ind->upper, NULL, &ind->recth);
       if (ind->lower != NULL) {
           SDL_RenderCopy(render, ind->lower, NULL, &ind->rectl);
       }
   }

   SDL_SetRenderDrawColor(render, ind->text_color.r, ind->text_color.g, ind->text_color.b, 0xff);
   x = wid->rect.x;
   y = wid->rect.y;
   h = wid->rect.h;
   w = wid->rect.w;
   SDL_RenderDrawLine(render, x, y+2, x+w, y+2);
   SDL_RenderDrawLine(render, x, y+h-2, x+w, y+h-2);
}


static void
close_indicator(Widget wid)
{
   struct _indicator_t *ind = (struct _indicator_t *)wid->data;
   if (ind->upper != NULL) {
       SDL_DestroyTexture(ind->upper);
   }
   if (ind->lower != NULL) {
       SDL_DestroyTexture(ind->lower);
   }
   free(wid->data);
}


/*
 * Add indicator display.
 */
Widget
add_indicator(Panel win, int x, int y, int h, int w, char *label1, char *label2,
                   int *value, int shft, TTF_Font *font, SDL_Color *col_text,
                   SDL_Color *col_on, SDL_Color *col_off)
{
   Widget       nwid;
   SDL_Surface *surf;
   SDL_Texture *text;
   struct _indicator_t *ind;
   int          hh, wh, hl, wl, lh, k;
   Uint32       f;
   if ((nwid = (Widget)calloc(1, sizeof(struct _widget_t))) == NULL) {
       return NULL;
   }

   if ((ind = (struct _indicator_t *)calloc(1, sizeof(struct _indicator_t))) == NULL) {
       free(nwid);
       return NULL;
   }

   nwid->rect.x = x;
   nwid->rect.y = y;
   nwid->rect.w = w;
   nwid->rect.h = h;
   ind->value = value;
   ind->shft = shft;
   ind->color[1] = *col_on;
   ind->color[0] = *col_off;
   ind->text_color = *col_text;
   /* Fill in the fields */
   if (label1 != NULL) {
       surf = TTF_RenderText_Blended(font, label1, *col_text);
       text = SDL_CreateTextureFromSurface(win->render, surf);
       SDL_QueryTexture(text, &f, &k, &wh, &hh);
       SDL_FreeSurface(surf);
       ind->upper = text;
       ind->recth.x = x;
       ind->recth.y = y;
       ind->recth.w = wh;
       ind->recth.h = hh;
       lh = hh;
   }
   if (label2 != NULL) {
       surf = TTF_RenderText_Blended(font, label2, *col_text);
       text = SDL_CreateTextureFromSurface(win->render, surf);
       SDL_QueryTexture(text, &f, &k, &wl, &hl);
       SDL_FreeSurface(surf);
       ind->lower = text;
       ind->rectl.x = x;
       ind->rectl.y = y + (hh/2);
       ind->rectl.w = wl;
       ind->rectl.h = hl;
       if (wl > wh) {
          ind->recth.x += (wl - wh) / 2;
       } else {
          ind->rectl.x += (wh - wl) / 2;
       }
       ind->recth.y = y - (hh/2);
       lh += hl;
       /* Center label */
       ind->rectl.x += (w/2) - (wl/2);
       ind->rectl.y += (h/2) - (hl/2);
   }

   /* Center label */
   ind->recth.x += (w/2) - (wh/2);
   ind->recth.y += (h/2) - (hh/2);

   nwid->draw = display_indicator;
   nwid->close = close_indicator;
   nwid->data = (void *)ind;
   add_widget(win, nwid);
   return nwid;
}



